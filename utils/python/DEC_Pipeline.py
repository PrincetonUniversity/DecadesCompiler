
import os, sys, subprocess
import pdb
from numba import types, prange
from numba import jit
from numba.compiler import BasePipeline, compile_result, _LowerResult

from numba import (bytecode, interpreter, funcdesc, postproc,
                   typing, typeinfer, lowering, pylowering, utils, config,
                   errors, types, ir, rewrites, transforms)

import numba.npyufunc.parallel

from tabulate import tabulate

import llvmlite.binding as ll
# for ll.parse_assembly

from llvmlite.llvmpy.core import Constant, Type, Builder

import shutil
from time import time

import re

func_cache = []
LLVM_FILES = 0
FIRST_DEC_RUN = True
DECADES_OUTDIR = "DECADES_Numba_out"
DECADES_BASE = "decades_base"
DECpp_output = "llvm_combined.ll"
LOCAL_KERNEL = None
COMPUTE_KERNEL = None
SUPPLY_KERNEL = None
CACHED_MODS = []
LINKING_LIBS_CACHES = []
LIBS_TO_SRC = {}
TO_COMPILE = []
SIMULATION_MULTIPLIER = 1
DECADES_CHIP = None
SIMULATOR_OPTIONS = ""

# Simulator output parsing regex
TITLE_REGEX = re.compile(r'''^-+([^-]+?)-+$''')
CORE_TITLE_REGEX = re.compile(r'''Core \d+''')
# <name> : <value> <units>
# name = [\w ]+
# value = \d+(\.\d+)?(e[+-]?\d+)?
# units = .*
STAT_REGEX = re.compile(r'''^([\w ]+?)\s*:\s*(\d+(?:\.\d+)?(?:[eE][+-]?\d+)?).*$''')    

def pipe_line_print(s):
    pass

def create_fresh_dir(s):
    if os.path.isdir(s):
        shutil.rmtree(s)
    os.mkdir(s)

create_fresh_dir(DECADES_OUTDIR)

#Default options
DEC_options = {
               "llvm_outfile"  : "DEC_pipeline_out.llvm",
               "pythia_home"   : None,
               "openmp_lib"    : None,
               "DEC++"         : "DEC++",
               "numba_flag"    : "-fn 1",
               "numba_target"  : "--target numba",
               "simulator"     : False,
               "num_threads"   : 1,
               "compiler_mode" : "db",
               "multiplier"    : 1
}

# Cached simulator results
DEC_sim_stats = {}

def find_pythia_home():
    global DEC_options
    tmp = os.path.abspath(os.path.join(os.popen("which PDEC++").read().replace("\n",''), "../../"))
    DEC_options["pythia_home"] = tmp

find_pythia_home()

def find_openmp_lib():
    global DEC_options
    tmp = os.path.abspath(os.path.join(os.popen("which clang++").read().replace("\n",''), "../../lib/libomp.so"))
    DEC_options["openmp_lib"] = tmp

find_openmp_lib()

class DEC_Options():
    
    @staticmethod
    def set_simulator_target():
        global DEC_options
        DEC_options["simulator"] = True
        
    @staticmethod
    def set_multiplier(m):
        global SIMULATION_MULTIPLIER
        to_store = int(m)
        DEC_options["multiplier"] = m

    @staticmethod
    def set_num_threads(i):
        global DEC_options
        DEC_options["num_threads"] = i

    @staticmethod
    def set_num_tiles(i):
        global DEC_options
        DEC_options["num_threads"] = i


    @staticmethod
    def get_num_tiles():
        return DEC_options["num_threads"]


    @staticmethod
    def get_num_threads():
        return DEC_options["num_threads"]

    @staticmethod
    def set_decoupled_mode(status=True):
        global DEC_options
        global DECADES_BASE
        if status:
            DEC_options["compiler_mode"] = "di"
            DECADES_BASE = "decades_decoupled_implicit"
        else:
            DEC_options["compiler_mode"] = "db"
            DECADES_BASE = "decades_base"
            


    @staticmethod
    def is_base_mode():
        return DEC_options["compiler_mode"] == "db"

    @staticmethod
    def is_decoupled_mode():
        return DEC_options["compiler_mode"] == "di"

    @staticmethod
    def get_decpp_line():
        cmd_list = [DEC_options["DEC++"], DEC_options["numba_flag"], DEC_options["numba_target"], "-t " + str(DEC_options["num_threads"]), "-m " + DEC_options["compiler_mode"]]
        if DEC_options["simulator"]:
            cmd_list.append("-spp " + os.path.join(DEC_options["pythia_home"], "pass", "preproc.sh"))
        if not FIRST_DEC_RUN:
            cmd_list.append("-fd 0")
        cmd_list.append(DECADES_OUTDIR)
        std_out = os.path.join(DECADES_OUTDIR, "dec_compile_log_std.txt")
        std_err = os.path.join(DECADES_OUTDIR, "dec_compile_log_stderr.txt")
        #cmd_list.append("> " + std_out + " 2> " + std_err)
        return " ".join(cmd_list)

    @staticmethod
    def preset_config():
        if "DECADES_CONFIG" in os.environ:
            tmp = os.environ["DECADES_CONFIG"]
        else:
            tmp = "SINGLE_THREAD"

        print("Loading Pythia preset: " + tmp)

        if tmp == "SINGLE_THREAD":
            DEC_Options.set_num_threads(1)
            
        elif tmp == "TEST_CHIP":
            DEC_Options.set_num_threads(1)
            #DEC_Options.set_decouled_mode()
            DEC_Options.set_simulator_target()

        elif tmp == "FUTURE_CHIP":
            DEC_Options.set_num_threads(1)
            #DEC_Options.set_decoupled_mode() 
            DEC_Options.set_simulator_target()

        else:
            print("No decades preset config found")
            assert(False)

    @staticmethod
    def preset_config_explicit(s):
        global DECADES_CHIP
        global SIMULATOR_OPTIONS
        if s == "SDH_baseline":
            DECADES_CHIP = "SDH_baseline"
            DEC_Options.set_num_threads(24)
            SIMULATOR_OPTIONS = "-sc sim_darpa -cc core_darpa"
        elif s == "DECADES_small":
            DECADES_CHIP = "DECADES_small"
            DEC_Options.set_num_threads(8)
            SIMULATOR_OPTIONS = "-sc sim_test"
        elif s == "DECADES_big":
            DECADES_CHIP = "DECADES_big"
            DEC_Options.set_num_threads(24)
            SIMULATOR_OPTIONS = "-sc sim_2023"
        elif s == "DECADES_commercial":
            DECADES_CHIP = "DECADES_commercial"
            DEC_Options.set_num_threads(24)
            SIMULATOR_OPTIONS = "-sc sim_comm"
        elif s == "ASIC":
            DECADES_CHIP = "ASIC"
            DEC_Options.set_num_threads(5)
            SIMULATOR_OPTIONS = "-sc sim_test -cc core_ASIC"

        else:
            assert(False)

    @staticmethod
    def set_experiment_parameters(v):
        v = int(v)
        if v == 0:
            DEC_Options.set_num_threads(1)
            DEC_Options.set_decoupled_mode(False)

        if v == 1:            
            DEC_Options.set_num_threads(min(16, DEC_Options.get_num_threads()))
            DEC_Options.set_decoupled_mode(False)

        if v == 2:
            DEC_Options.set_num_threads(1)
            DEC_Options.set_decoupled_mode(True)

        if v == 3:
            DEC_Options.set_num_threads(int(min(8, int(DEC_Options.get_num_threads() / 2))))
            DEC_Options.set_decoupled_mode(True)



def decades_launch_kernel(k, *args):
    global LOCAL_KERNEL
    global COMPUTE_KERNEL
    global SUPPLY_KERNEL
    global NUM_THREADS
    function_args = ",".join(["i" + str(i) for i in range(len(k.signatures[0]))])
    _kernel_args = ",".join(["i" + str(i) for i in range(len(k.signatures[0]) - 2)])
    LOCAL_KERNEL = k

    compute_wrapper = """
def _kernel_compute({args}):
    LOCAL_KERNEL({args})
    """.format(args = function_args)

    exec(compute_wrapper, globals())
    COMPUTE_KERNEL = jit(k.signatures[0],nogil=True, nopython=True, pipeline_class=DEC_Pipeline)(_kernel_compute)

    supply_wrapper = """
def _kernel_supply({args}):
    LOCAL_KERNEL({args})
    """.format(args    = function_args)

    exec(supply_wrapper, globals())
    SUPPLY_KERNEL = jit(k.signatures[0],nogil=True, nopython=True, pipeline_class=DEC_Pipeline)(_kernel_supply)

    if (DEC_Options.is_base_mode()):
        total_threads = DEC_Options.get_num_threads()
        k_threads = DEC_Options.get_num_threads()
        compute_pred = True
    elif (DEC_Options.is_decoupled_mode()):
        total_threads = DEC_Options.get_num_threads() * 2
        k_threads = DEC_Options.get_num_threads()
        compute_pred = "i < " + str(k_threads)
    else:
        print("found an unsupported mode")
        assert(False)


    numba.npyufunc.parallel.__dict__["NUM_THREADS"] = total_threads
    
#    wrapper = """
#config.THREADING_LAYER = 'omp'
#config.NUMBA_NUM_THREADS = {threads}
#def tile_launcher({args}):
#    for i in prange({threads}):
#        if({compute_pred}):
#           COMPUTE_KERNEL({k_args},i,{threads})
#        else:
#           SUPPLY_KERNEL({k_args},i - {threads},{threads})
#    """.format(threads      = str(threads / thread_div),
#               args         = function_args,
#               k_args       = _kernel_args,
#               compute_pred = compute_pred)

    wrapper = """
def tile_launcher({args}):
    for i in prange({threads}):
        if({compute_pred}):
            COMPUTE_KERNEL({k_args},i,{k_threads})
        else:
            SUPPLY_KERNEL({k_args},i - {k_threads},{k_threads})
    """.format(threads       = total_threads,
               k_threads     = k_threads,
               compute_pred  = compute_pred,
               args          = function_args,
               k_args        = _kernel_args)

    exec(wrapper, globals())

    os.environ["OMP_NUM_THREADS"] = str(total_threads)
    config.THREADING_LAYER = 'omp'
    config.NUMBA_NUM_THREADS = total_threads

    jit_wrapper = jit(k.signatures[0],nogil=True, parallel = True, nopython=True, pipeline_class=DEC_Pipeline)(tile_launcher)

    print("\nCompilation finished.\nStarting kernel execution\n")
    t=time()
    jit_wrapper(*args, 0, 1)
    return time() - t
    

def native_lowering_stage(targetctx, library, interp, typemap, restype,
                          calltypes, flags, metadata):

    global LLVM_FILES
    global FIRST_DEC_RUN
    global func_cache
    
    # Lowering
    fndesc = funcdesc.PythonFunctionDescriptor.from_specialized_function(
        interp, typemap, restype, calltypes, mangler=targetctx.mangler,
        inline=flags.forceinline, noalias=flags.noalias)


    with targetctx.push_code_library(library):
        lower = lowering.Lower(targetctx, library, fndesc, interp,
                               metadata=metadata)
        lower.lower()

        if not flags.no_cpython_wrapper:
            lower.create_cpython_wrapper(flags.release_gil)
        env = lower.env
        call_helper = lower.call_helper
        del lower

    if flags.no_compile:
        return _LowerResult(fndesc, call_helper, cfunc=None, env=env)
    else:
        # Prepare for execution


        functions = [x.name for x in library.get_defined_functions()]
        is_kernel_compute = "_kernel_compute" in functions[0]
        is_kernel_supply = "_kernel_supply" in functions[0]
        is_tile_launcher = "tile_launcher" in functions[0]
        call_decpp = None
        if (DEC_Options.is_base_mode()):
            call_decpp = is_kernel_compute or is_tile_launcher
        if (DEC_Options.is_decoupled_mode()):
            call_decpp = is_kernel_compute or is_kernel_supply or is_tile_launcher

        assert(call_decpp is not None)


        if not call_decpp:
            pipe_line_print("")
            pipe_line_print("-----")
            pipe_line_print("DEC++ Pipeline compilation output")
            pipe_line_print("")

            pipe_line_print(functions)

            for l in library._linking_libraries:
                LINKING_LIBS_CACHES.append(l)


            LIBS_TO_SRC[library] = library.get_llvm_str()
            pipe_line_print("-----")
            pipe_line_print("")


        

        if call_decpp:
            #create_fresh_dir(DECADES_OUTDIR)
            if FIRST_DEC_RUN:
                ll.load_library_permanently(os.path.join(DEC_options["pythia_home"],"tools", "tracer.so"))
                ll.load_library_permanently(DEC_options["openmp_lib"])
                ll.load_library_permanently(os.path.join(BUILD_ROOT, "./utils/DecoupleServer/libproduce_consume-shared.so"))
                #pdb.set_trace()
                ll.load_library_permanently(os.path.join(BUILD_ROOT, "./utils/DECLib/libDECADES-numba.so"))

            pipe_line_print("")
            pipe_line_print("-----")
            pipe_line_print("DEC++ Pipeline compilation output")
            pipe_line_print("")

            functions = [x for x in library.get_defined_functions()]
            pipe_line_print("identified kernel:")
            pipe_line_print(functions[0].name)
            pipe_line_print("")


            def rec_mk_llvm_from_libs(lib):
                global LLVM_FILES
                if lib in func_cache:
                    return
                else:
                    if lib in LIBS_TO_SRC:
                        to_print = LIBS_TO_SRC[lib]
                    else:
                        to_print = lib.get_llvm_str()
                    llvm_output_file = str(LLVM_FILES) + "_" + DEC_options["llvm_outfile"]
                    fh = open(os.path.join(DECADES_OUTDIR,llvm_output_file), 'w')
                    fh.write(to_print)
                    fh.close()
                    LLVM_FILES += 1
                    func_cache.append(lib)
                    for l in lib._linking_libraries:
                        rec_mk_llvm_from_libs(l)
                
            rec_mk_llvm_from_libs(library)
            for kernel in TO_COMPILE:
                rec_mk_llvm_from_libs(kernel)
            TO_COMPILE.append(library)

            for l in library._linking_libraries:
                LINKING_LIBS_CACHES.append(l)



            cmd = DEC_Options.get_decpp_line()
            #cmd = "DEC++ -fn 1 --target simulator -spp ~/pythia_git/pythia/pass/preproc.sh " + DECADES_OUTDIR
            print("\nexecuting DEC++:")
            print(cmd + "\n")
            ret = os.system(cmd)
            if ret != 0:
                print("")
                print("Error executing DEC++")
                exit(0)

            assert(os.path.exists(os.path.join(DECADES_BASE, DECpp_output)))
            cmd = "sed -e s/immarg//g -i " + os.path.join(DECADES_BASE, DECpp_output)
            pipe_line_print(cmd)
            os.system(cmd)

            fh = open(os.path.join(DECADES_BASE, DECpp_output))
            llvm_src = fh.read()
            fh.close()

            reg_labels = re.findall(r"^\d+:\s*; preds =.*$", llvm_src, re.MULTILINE)
            for rl in reg_labels:
                new_line = "; <label>:" + rl
                llvm_src = llvm_src.replace(rl, new_line)

            
            codegen = library.codegen
            library2 = codegen.create_library('DEC++')
            #from pathlib import Path
            #contents = Path(os.path.join(DECADES_BASE, DECpp_output)).read_text()
            new_module = ll.parse_assembly(llvm_src)
            library2.add_llvm_module(new_module)
            library = library2
            #pdb.set_trace()
            #library._linking_libraries = linking_libs_cache
            LLVM_FILES += 1
            FIRST_DEC_RUN = False

        
        cfunc = targetctx.get_executable(library, fndesc, env)
        
        # Insert native function for use by other jitted-functions.
        # We also register its library to allow for inlining.
        targetctx.insert_user_function(cfunc, fndesc, [library])
        return _LowerResult(fndesc, call_helper, cfunc=cfunc, env=env)


class DEC_Pipeline(BasePipeline):
    """
    The default compiler pipeline
    """
    def define_pipelines(self, pm):
        if not self.flags.force_pyobject:
            self.define_nopython_pipeline(pm)
    
    def add_lowering_stage(self, pm):
        """
        Add the lowering (code-generation) stage for nopython-mode
        """
        pm.add_stage(self.stage_nopython_backend, "nopython mode backend")
        pm.add_stage(self.DECADES_pass, "DECADES passes")

    def stage_nopython_backend(self):
        """
        Do lowering for nopython
        """
        lowerfn = self.backend_nopython_mode
        self._backend(lowerfn, objectmode=False)

    def _backend(self, lowerfn, objectmode):
        """
        Back-end: Generate LLVM IR from Numba IR, compile to machine code
        """
        if self.library is None:
            codegen = self.targetctx.codegen()
            self.library = codegen.create_library(self.func_id.func_qualname)
            # Enable object caching upfront, so that the library can
            # be later serialized.
            self.library.enable_object_caching()
            

        lowered = lowerfn()
        signature = typing.signature(self.return_type, *self.args)
        self.cr = compile_result(
            typing_context=self.typingctx,
            target_context=self.targetctx,
            entry_point=lowered.cfunc,
            typing_error=self.status.fail_reason,
            type_annotation=self.type_annotation,
            library=self.library,
            call_helper=lowered.call_helper,
            signature=signature,
            objectmode=objectmode,
            interpmode=False,
            lifted=self.lifted,
            fndesc=lowered.fndesc,
            environment=lowered.env,
            metadata=self.metadata,
            )

    def backend_nopython_mode(self):
        """Native mode compilation"""
        msg = ("Function %s failed at nopython "
               "mode lowering" % (self.func_id.func_name,))
        with self.fallback_context(msg):
            return native_lowering_stage(
                self.targetctx,
                self.library,
	        self.func_ir,
                self.typemap,
                self.return_type,
                self.calltypes,
                self.flags,
                self.metadata)

    def DECADES_pass(self):
        pass


def _parse_num(s):
    try:
        return int(s)
    except ValueError:
        return float(s)

def decades_simulate():
    if not DEC_options["simulator"]:
        print('ERROR: Run kernel with DEC_Options.set_simulator_target() in order to launch the simulator', file=sys.stderr)
        return

    print("Using simulator mulplier of: ", DEC_options["multiplier"])
    args = [os.path.join(DEC_options["pythia_home"], 'tools', 'pythiarun'), '-n', str(DEC_Options.get_num_tiles())]
    if DEC_Options.is_decoupled_mode():
        args.append('-d')

    args.append('.')
    if SIMULATOR_OPTIONS != "":
        args += SIMULATOR_OPTIONS.split(' ')

    try:
        print('Running Simulator:', ' '.join(args))
        result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding='utf-8')

        current_section = current_title = None
        text_file = open("getStats.txt", "w")
        text_file.write(str(result.stdout))
        text_file.close()
        
        clock_freq = int(re.findall("chip_freq: (\d+)", result.stdout)[0])
        DEC_sim_stats["clock_freq"] = clock_freq
        time_units = str(re.findall("Total Simulation Time: \d+ (\D+$)", result.stdout, re.M)[0].replace(' ',''))
        assert (time_units == "secs" or time_units == "mins")
        mult = 1
        if time_units == "mins":
            mult = 60

        time_value = int(re.findall("Total Simulation Time: (\d+)", result.stdout)[0])
        DEC_sim_stats["simulation_seconds"] = str(int(time_value) * mult)
        for line in result.stdout.split('\n'):
            t_match = TITLE_REGEX.match(line)

            if t_match:
                t_match = CORE_TITLE_REGEX.search(t_match.group(1))

                if t_match:
                    current_section = 'CORE'
                    current_title = t_match.group(0).upper()
                    DEC_sim_stats[current_title] = {}
                elif 'GLOBAL' in line.upper():
                    current_section = current_title = 'GLOBAL'
                    DEC_sim_stats[current_title] = {}

            elif current_section is not None:
                s_match = STAT_REGEX.match(line)

                if s_match:
                    stat_name = s_match.group(1).replace(' ', '_').lower()
                    stat_value = s_match.group(2)
                    DEC_sim_stats[current_title][stat_name] = _parse_num(stat_value)

        result.check_returncode()

    except subprocess.CalledProcessError as e:
        print('ERROR: Simulator run failed with exit code: {}'.format(e.returncode), file=sys.stderr)

class DEC_Simulation():
    @staticmethod
    def export_stats_json():
        import json
        print(json.dumps(DEC_sim_stats, indent=2))

    @staticmethod
    def get_stat(stat, core=None):
        if core is None:
            if not 'GLOBAL' in DEC_sim_stats:
                print('ERROR: Cannot get simulator stats before executing decades_simulate() successfully', file=sys.stderr)
                return None
            
            return DEC_sim_stats['GLOBAL'][stat]
        else:
            core_title = 'CORE {}'.format(core)

            if not core_title in DEC_sim_stats:
                print('ERROR: No stats for core {} have been collected'.format(core), file=sys.stderr)
                return None

            return DEC_sim_stats[core_title][stat]

    @staticmethod
    def get_ipc(core=None):
        return DEC_Simulation.get_stat('ipc', core)

    @staticmethod
    def get_freq(core=None):
        return DEC_sim_stats["clock_freq"]
        
    @staticmethod
    def get_average_bw(core=None):
        return DEC_Simulation.get_stat('average_bw', core)
        
    @staticmethod
    def get_l1_miss_rate(core=None):
        return DEC_Simulation.get_stat('l1_miss_rate', core)
        
    @staticmethod
    def get_l2_miss_rate(core=None):
        return DEC_Simulation.get_stat('l2_miss_rate', core)
        
    @staticmethod
    def get_accelerator(core=None):
        return DEC_Simulation.get_stat('accelerator', core)
        
    @staticmethod
    def get_barrier(core=None):
        return DEC_Simulation.get_stat('barrier', core)
        
    @staticmethod
    def get_bs_done(core=None):
        return DEC_Simulation.get_stat('bs_done', core)
        
    @staticmethod
    def get_bs_vector_inc(core=None):
        return DEC_Simulation.get_stat('bs_vector_inc', core)
        
    @staticmethod
    def get_bs_wake(core=None):
        return DEC_Simulation.get_stat('bs_wake', core)
        
    @staticmethod
    def get_call_bs(core=None):
        return DEC_Simulation.get_stat('call_bs', core)
        
    @staticmethod
    def get_cast(core=None):
        return DEC_Simulation.get_stat('cast', core)
        
    @staticmethod
    def get_core_interrupt(core=None):
        return DEC_Simulation.get_stat('core_interrupt', core)
        
    @staticmethod
    def get_fp_addsub(core=None):
        return DEC_Simulation.get_stat('fp_addsub', core)
        
    @staticmethod
    def get_fp_div(core=None):
        return DEC_Simulation.get_stat('fp_div', core)
        
    @staticmethod
    def get_fp_mult(core=None):
        return DEC_Simulation.get_stat('fp_mult', core)
        
    @staticmethod
    def get_fp_rem(core=None):
        return DEC_Simulation.get_stat('fp_rem', core)
        
    @staticmethod
    def get_gep(core=None):
        return DEC_Simulation.get_stat('gep', core)
        
    @staticmethod
    def get_invalid(core=None):
        return DEC_Simulation.get_stat('invalid', core)
        
    @staticmethod
    def get_i_addsub(core=None):
        return DEC_Simulation.get_stat('i_addsub', core)
        
    @staticmethod
    def get_i_div(core=None):
        return DEC_Simulation.get_stat('i_div', core)
        
    @staticmethod
    def get_i_mult(core=None):
        return DEC_Simulation.get_stat('i_mult', core)
        
    @staticmethod
    def get_i_rem(core=None):
        return DEC_Simulation.get_stat('i_rem', core)
        
    @staticmethod
    def get_ld(core=None):
        return DEC_Simulation.get_stat('ld', core)
        
    @staticmethod
    def get_ld_prod(core=None):
        return DEC_Simulation.get_stat('ld_prod', core)
        
    @staticmethod
    def get_logical(core=None):
        return DEC_Simulation.get_stat('logical', core)
        
    @staticmethod
    def get_phi(core=None):
        return DEC_Simulation.get_stat('phi', core)
        
    @staticmethod
    def get_recv(core=None):
        return DEC_Simulation.get_stat('recv', core)
        
    @staticmethod
    def get_send(core=None):
        return DEC_Simulation.get_stat('send', core)
        
    @staticmethod
    def get_st(core=None):
        return DEC_Simulation.get_stat('st', core)
        
    @staticmethod
    def get_staddr(core=None):
        return DEC_Simulation.get_stat('staddr', core)
        
    @staticmethod
    def get_stval(core=None):
        return DEC_Simulation.get_stat('stval', core)
        
    @staticmethod
    def get_terminator(core=None):
        return DEC_Simulation.get_stat('terminator', core)
        
    @staticmethod
    def get_bytes_read(core=None):
        return DEC_Simulation.get_stat('bytes_read', core)
        
    @staticmethod
    def get_bytes_write(core=None):
        return DEC_Simulation.get_stat('bytes_write', core)
        
    @staticmethod
    def get_cache_access(core=None):
        return DEC_Simulation.get_stat('cache_access', core)
        
    @staticmethod
    def get_cache_evicts(core=None):
        return DEC_Simulation.get_stat('cache_evicts', core)
        
    @staticmethod
    def get_cache_pending(core=None):
        return DEC_Simulation.get_stat('cache_pending', core)
        
    @staticmethod
    def get_comp_issue_success(core=None):
        return DEC_Simulation.get_stat('comp_issue_success', core)
        
    @staticmethod
    def get_comp_issue_try(core=None):
        return DEC_Simulation.get_stat('comp_issue_try', core)
        
    @staticmethod
    def get_contexts(core=None):
        return DEC_Simulation.get_stat('contexts', core)
        
    @staticmethod
    def get_cycles(core=None):
        return DEC_options["multiplier"] * DEC_Simulation.get_stat('cycles', core)
        
    @staticmethod
    def get_dram_accesses(core=None):
        return DEC_Simulation.get_stat('dram_accesses', core)
        
    @staticmethod
    def get_forwarded(core=None):
        return DEC_Simulation.get_stat('forwarded', core)
        
    @staticmethod
    def get_l1_hits(core=None):
        return DEC_Simulation.get_stat('l1_hits', core)
        
    @staticmethod
    def get_l1_misses(core=None):
        return DEC_Simulation.get_stat('l1_misses', core)
        
    @staticmethod
    def get_l2_hits(core=None):
        return DEC_Simulation.get_stat('l2_hits', core)
        
    @staticmethod
    def get_l2_misses(core=None):
        return DEC_Simulation.get_stat('l2_misses', core)
        
    @staticmethod
    def get_ld_prod_issue_success(core=None):
        return DEC_Simulation.get_stat('ld_prod_issue_success', core)
        
    @staticmethod
    def get_ld_prod_issue_try(core=None):
        return DEC_Simulation.get_stat('ld_prod_issue_try', core)
        
    @staticmethod
    def get_load_issue_success(core=None):
        return DEC_Simulation.get_stat('load_issue_success', core)
        
    @staticmethod
    def get_load_issue_try(core=None):
        return DEC_Simulation.get_stat('load_issue_try', core)
        
    @staticmethod
    def get_lsq_insert_fail(core=None):
        return DEC_Simulation.get_stat('lsq_insert_fail', core)
        
    @staticmethod
    def get_lsq_insert_success(core=None):
        return DEC_Simulation.get_stat('lsq_insert_success', core)
        
    @staticmethod
    def get_misspeculated(core=None):
        return DEC_Simulation.get_stat('misspeculated', core)
        
    @staticmethod
    def get_recv_issue_success(core=None):
        return DEC_Simulation.get_stat('recv_issue_success', core)
        
    @staticmethod
    def get_recv_issue_try(core=None):
        return DEC_Simulation.get_stat('recv_issue_try', core)
        
    @staticmethod
    def get_send_issue_success(core=None):
        return DEC_Simulation.get_stat('send_issue_success', core)
        
    @staticmethod
    def get_send_issue_try(core=None):
        return DEC_Simulation.get_stat('send_issue_try', core)
        
    @staticmethod
    def get_speculated(core=None):
        return DEC_Simulation.get_stat('speculated', core)
        
    @staticmethod
    def get_speculatively_forwarded(core=None):
        return DEC_Simulation.get_stat('speculatively_forwarded', core)
        
    @staticmethod
    def get_staddr_issue_success(core=None):
        return DEC_Simulation.get_stat('staddr_issue_success', core)
        
    @staticmethod
    def get_staddr_issue_try(core=None):
        return DEC_Simulation.get_stat('staddr_issue_try', core)
        
    @staticmethod
    def get_store_issue_success(core=None):
        return DEC_Simulation.get_stat('store_issue_success', core)
        
    @staticmethod
    def get_store_issue_try(core=None):
        return DEC_Simulation.get_stat('store_issue_try', core)
        
    @staticmethod
    def get_stval_issue_success(core=None):
        return DEC_Simulation.get_stat('stval_issue_success', core)
        
    @staticmethod
    def get_stval_issue_try(core=None):
        return DEC_Simulation.get_stat('stval_issue_try', core)
        
    @staticmethod
    def get_total_instructions(core=None):
        return DEC_options["multiplier"] * DEC_Simulation.get_stat('total_instructions', core)
        
    @staticmethod
    def get_global_energy(core=None):
        return DEC_options["multiplier"] * DEC_Simulation.get_stat('global_energy', core)
        
    @staticmethod
    def get_global_avg_power(core=None):
        return DEC_Simulation.get_stat('global_avg_power', core)
        
    @staticmethod
    def get_total_simulation_time(core=None):
        return DEC_Simulation.get_stat('total_simulation_time', core)
        
    @staticmethod
    def get_average_global_simulation_speed(core=None):
        return DEC_Simulation.get_stat('average_global_simulation_speed', core)

    @staticmethod
    def get_simulated_time(core=None):
        return float(DEC_Simulation.get_cycles(core)) / float(DEC_sim_stats["clock_freq"] * 1000000)

    @staticmethod
    def get_energy_delay(core=None):
        return DEC_Simulation.get_simulated_time(core) * DEC_Simulation.get_global_energy(core)

    @staticmethod
    def get_energy_delay_squared(core=None):
        return (DEC_Simulation.get_simulated_time(core)**2) * DEC_Simulation.get_global_energy(core)

    @staticmethod
    def get_gops_per_watt(core=None):
        return (DEC_Simulation.get_total_instructions(core) * 1e-9) / DEC_Simulation.get_global_energy(core)

    @staticmethod
    def get_nice_SDH_table():
        header = "\nSDH table begin\n--\n"
        chip = "chip: " + DECADES_CHIP + "\n"
        threads = "threads: " + str(DEC_Options.get_num_threads()) + "\n"
        
        decoupled = "decoupled: " + str(DEC_options["compiler_mode"] == "di") + "\n"
        
        tab_list = []
        tab_list.append(["cycles:", DEC_Simulation.get_cycles()])
        tab_list.append(["operations:", DEC_Simulation.get_total_instructions()])
        tab_list.append(["time(s):", DEC_Simulation.get_simulated_time()])
        tab_list.append(["energy:", DEC_Simulation.get_global_energy()])
        tab_list.append(["gops/watt:", DEC_Simulation.get_gops_per_watt()])
        tab_list.append(["energy-delay:", DEC_Simulation.get_energy_delay()])
        tab_list.append(["energy-delay-squared:", DEC_Simulation.get_energy_delay_squared()])
        tab_list.append(["time to simulate(s):", DEC_sim_stats["simulation_seconds"]])
        footer = "\nSDH table end\n"
        return header + chip + threads + decoupled + tabulate(tab_list) + footer


