#!/usr/bin/env python3 
import os
import sys
import shutil
import time
import argparse
import pdb
import glob

BUILD_ROOT = "@PROJECT_BINARY_DIR@"

FILES = []

CLEANUP = 0

CLANG = "@LLVM_TOOLS_BINARY_DIR@/clang++"
CLANG_PP_FLAG = "-E"

LLVM_BUILD = "@LLVM_TOOLS_BINARY_DIR@"

CLANG_OPENMP_FLAGS = "-fopenmp=libomp -O3 -fno-slp-vectorize -fno-vectorize -fno-unroll-loops"

OPENMP_INCLUDE = " " if "@OpenMP_CXX_INCLUDE_DIRS@" == "" else " -I" + "@OpenMP_CXX_INCLUDE_DIRS@".replace(";"," -I") + " "
OPENMP_LIB = " " if "@OpenMP_CXX_LIBRARIES@" == "" else " -L" + "@OpenMP_CXX_LIBRARIES@".replace(";"," -L") + " "
OPENMP_RISCV_LIB = " " # " -L" + os.path.join(LLVM_BUILD,"../sysroot_openmp_riscv/lib/")

DECOUPLING_INCLUDE = "-I " + os.path.join(BUILD_ROOT, "include")
DECOUPLING_LIB = "-L " + os.path.join(BUILD_ROOT, "lib")
DECOUPLING_LIB_NAME_X86_SHMEM =   "-lshmem_decoupling_x86"
DECOUPLING_LIB_NAME_RISCV_SHMEM = "-lshmem_decoupling_riscv"
DECOUPLING_LIB_NAME_RISCV_MAPLE = "-lmaple_decoupling_riscv"

DECOUPLING_LIB_NAME = None

ALL_INCLUDES = " ".join([OPENMP_INCLUDE, DECOUPLING_INCLUDE])

CLANG_FLAGS = "-std=c++14 -O3"
EMIT_LLVM = "-S -static -fno-slp-vectorize -fno-vectorize -fno-unroll-loops -emit-llvm -fopenmp=libomp"
OUTPUT = "-o"
OBJ_FILE = "-c"
RISCV_TARGET = "--target=riscv64-unknown-linux-gnu -mabi=lp64d"
LPTHREAD = "-lpthread"
OPT = "@LLVM_TOOLS_BINARY_DIR@/opt -S "
LLC = "@LLVM_TOOLS_BINARY_DIR@/llc -filetype=obj"
LLVM_LINK = "@LLVM_TOOLS_BINARY_DIR@/llvm-link -S"

ALWAYS_INLINER = "-load " + os.path.join(BUILD_ROOT, "lib/libAlwaysInlinerPass.so") + " -alwaysinline"
PRINT_FUNCTION_CALLS = "-load " + os.path.join(BUILD_ROOT, "lib/libPrintFunctionCallsPass.so") + " -printfunctioncalls"
FIND_NMA = "-load " + os.path.join(BUILD_ROOT, "lib/libFindNMAPass.so") + " -findnma"
DECOUPLE_SUPPLY = "-load " + os.path.join(BUILD_ROOT, "lib/libDecoupleSupplyPass.so") + " -decouplesupply"
DECOUPLE_SUPPLY_ADDR = "-load " + os.path.join(BUILD_ROOT, "lib/libDecoupleSupplyAddrPass.so") + " -decouplesupplyaddr"
DECOUPLE_COMPUTE = "-load " + os.path.join(BUILD_ROOT, "lib/libDecoupleComputePass.so") + " -decouplecompute"

NUMBA_FUNCTION_REPLACE = "-load " + os.path.join(BUILD_ROOT, "lib/libNumbaFunctionReplace.so") + " -numbafunctionreplace"

DEAD_CODE_ELIM = " -adce "
# From LLVM DOC:
# The well-known scalar replacement of aggregates transformation. This
# transform breaks up alloca instructions of aggregate type (structure
# or array) into individual alloca instructions for each member if
# possible. Then, if possible, it transforms the individual alloca
# instructions into nice clean scalar SSA form.
SCALER_REPLACE_AGG = " -sroa "
#https://llvm.org/docs/Passes.html#lowerinvoke-lower-invokes-to-calls-for-unwindless-code-generators
#QUESTION: converting invoke instructions to call...  what does that means 
LOWER_INVOKE = " -lowerinvoke "

DECADES_INCLUDE = "-I " + os.path.join(BUILD_ROOT, "lib")

DECOUPLE_LIB_DIR = os.path.join(BUILD_ROOT, "lib")
DECOUPLE_RUN = os.path.join(DECOUPLE_LIB_DIR, "run.py")
DECOUPLE_RUN_ADDR = os.path.join(DECOUPLE_LIB_DIR, "run_addr.py")
DECOUPLE_LIB_FLAG = "-lproduce_consume-static"
DECOUPLE_LIB_FLAG_RISCV = "-lproduce_consume-static-riscv"

CLANG_VISITOR = os.path.join(BUILD_ROOT, "bin/DECVisitor")

RISCV_SYSROOT = "--sysroot=/home/ts20/riscv-tool-chain/sysroot_riscv/riscv64-unknown-linux-gnu/riscv64-unknown-linux-gnu/sysroot/"
RISCV_GCC = "--gcc-toolchain=/home/ts20/riscv-tool-chain/sysroot_riscv/riscv64-unknown-linux-gnu/"

SPP = None
DEBUG = None
INCLUDE = None
LANGUAGE = None
FAST = None
THREADS = None
SYNC_MODE = 0
SCRATCHPAD_SIZE = 8192
TARGET_OUT = None
FROM_NUMBA = False
FRESH_DIRECTORY = None
DUMP_LLVMIR = False

def generate_LLVMIR(input_file, prefix):
    dir=os.path.dirname(input_file)
    file=os.path.basename(input_file)

    os.chdir(dir) 
    llvm_copy=file+"_"+prefix
    cmd=" ".join(["cp", file, llvm_copy])
    my_exec(cmd)
    cmd=" ".join(["@LLVM_TOOLS_BINARY_DIR@/opt -dot-cfg", "-f", llvm_copy, "2> /dev/null", "> /dev/null"])
    my_exec(cmd)
    kernel_dots = glob.glob("._*kernel*.dot")
    for kernel in  kernel_dots:
        cmd=" ".join(["dot", kernel, "-Tps -o", kernel[1:-4]+prefix+".eps", "2> /dev/null", "> /dev/null"])
        my_exec(cmd)
    cmd=" ".join(["rm", ".*dot"])
    my_exec(cmd)
    os.chdir("..") 
    
def create_fresh_dir(s):
    """! Creates a new directory, but first deletes it if it exists. If
    FRESH_DIRECTORY is false, then it copies llvm_combined.ll to
    llvm_combined_0.ll and llvm_combined_1.ll if they do not exist
    already. In other comments, it was mentioned that this is used for
    the NUMBA part.

    """
    if FRESH_DIRECTORY:
        if os.path.isdir(s):
            shutil.rmtree(s)
        os.mkdir(s)
    elif not os.path.exists(os.path.join(s,"llvm_combined_0.ll")):
        assert(os.path.isdir(s))
        shutil.copyfile(os.path.join(s,"llvm_combined.ll"), os.path.join(s,"llvm_combined_0.ll"))    
    elif not os.path.exists(os.path.join(s,"llvm_combined_1.ll")):
        assert(os.path.isdir(s))
        shutil.copyfile(os.path.join(s,"llvm_combined.ll"), os.path.join(s,"llvm_combined_1.ll"))
    else:
        assert(0)


def my_exec(s, visitor = False):
    """! Executes the command s and does error handling. It visitor is
    true, it means that the command s is the DECVisitor and does extra
    error handling

    """
    print("executing: ")
    print(s)
    ret = os.system(s)
    if ret != 0 and visitor:
        error_cmd = "grep ': error:' error.log"
        print("Clang visitor failed due to the following error(s):")
        os.system(error_cmd)
        print("See error.log for all details.\n")
        exit(1)
    elif ret != 0:
        exit(1)
    else:
        if (os.path.exists("error.log")):
            os.remove("error.log")


def get_llvm_combined_from_c(dir_name, inputs, rm_inputs = False):
    """! Compiles all the C++ inputs down to LLVM IR and creates their
    combined LLVM IR form.


    @param dir_name output directory 
    @param inputs C++ input files 
    @param rm_inputs if CLEANUP and rm_inputs are true, the input
    files are deleted

    """
    outputs = []

    debug_command = " "
    include_command = " "    

    if DEBUG:
        debug_command = "-g"
    #TODO: INCLUDE should be is a list 
    if INCLUDE is not None:
        include_command = "-I" + INCLUDE

    for f in inputs:
        llvm_file_output = os.path.join(dir_name, os.path.basename(f) + ".ll")
        outputs.append(llvm_file_output)
        if TARGET_OUT == "riscv64":
            cmd = " ".join([CLANG, RISCV_TARGET, CLANG_FLAGS, EMIT_LLVM, debug_command, include_command, DECADES_INCLUDE, ALL_INCLUDES, RISCV_SYSROOT, RISCV_GCC, f, OUTPUT, llvm_file_output])
        else:
            cmd = " ".join([CLANG, CLANG_FLAGS, EMIT_LLVM, debug_command, include_command, DECADES_INCLUDE, ALL_INCLUDES, f, OUTPUT, llvm_file_output])
        my_exec(cmd)

    llvm_output = os.path.join(dir_name, "llvm_combined.ll")
    cmd = " ".join([LLVM_LINK] + outputs + [OUTPUT, llvm_output])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_output, "base")
    
    if CLEANUP:
        for f in outputs:
            os.remove(f)                
        if rm_inputs:
            for f in inputs:
                os.remove(f)
    return llvm_output


def get_llvm_combined_from_numba(dir_name, input_dirs, rm_inputs = False):
    """! Gets the LLVM IR from the numba. 

    Although input_dirs is a list, only the first entry is used.

    @param dir_name output directory 
    @param input_dirs input_directory, but only the first one is used
    @param rm_inputs if CLEANUP and rm_inputs are true, the input
    files are deleted

    @TODO what are the extra llvm and how is it used? 
    """
    if FRESH_DIRECTORY:
        cmd = "cp " + os.path.join(BUILD_ROOT, "extra_llvm") + "/*.llvm " + input_dirs[0]  + "/"
        my_exec(cmd)
    
    llvm_output = os.path.join(dir_name, "llvm_combined.ll")
    cmd = " ".join([LLVM_LINK] + [(input_dirs[0] + "/*.llvm")] + [OUTPUT, llvm_output])
    my_exec(cmd)

    return llvm_output


def get_llvm_combined(dir_name, inputs, rm_inputs = False):
    """! Gets the LLVM IR either from C++ sources or from NUMBA 
    """
    if FROM_NUMBA:
        return get_llvm_combined_from_numba(dir_name, inputs, rm_inputs)        
    return get_llvm_combined_from_c(dir_name, inputs, rm_inputs)


def get_visited_cpp_file(dir_name):
    """! Goes through all the input files FILES and first applies the
    clang preprocessor (is it the same as gcc??) and then calls the
    DECVisitor to each one of them. The only compilation method that
    does not call this function is native. 

    """
    outputs = []
    to_clean = []
    include_command = " "
    biscuit_sync = "-DBISCUIT_SYNC_MODE=" + str(SYNC_MODE)
    scratchpad_size = "-DSCRATCHPAD_SIZE=" + str(SCRATCHPAD_SIZE)

    #TODO here INCLUDE should be a list and add all of them with separate -I
    # or find a portable way to add several with one -I, using commma for example  
    if INCLUDE is not None:
        include_command = "-I" + INCLUDE

    for f in FILES:
        preprocessed_file_output = os.path.join(dir_name, os.path.splitext(os.path.basename(f))[0] + "_pp.cpp")
        visited_file_output = os.path.join(dir_name, os.path.splitext(os.path.basename(f))[0] + "_visited.cpp")
        outputs.append(visited_file_output)
        to_clean.append(preprocessed_file_output)
        
        if (TARGET_OUT == "x86" or TARGET_OUT == "simulator"):
            pp_cmd = " ".join([CLANG, CLANG_PP_FLAG, biscuit_sync, scratchpad_size, include_command, DECADES_INCLUDE, ALL_INCLUDES, f, OUTPUT, preprocessed_file_output])
        elif (TARGET_OUT == "riscv64"):
            pp_cmd = " ".join([CLANG, RISCV_TARGET, CLANG_PP_FLAG, "-std=c++14", biscuit_sync, scratchpad_size, include_command, DECADES_INCLUDE, ALL_INCLUDES, RISCV_SYSROOT, RISCV_GCC, f, OUTPUT, preprocessed_file_output])
        my_exec(pp_cmd)
        visitor_cmd = " ".join([CLANG_VISITOR, preprocessed_file_output, "-- 2> error.log > ", visited_file_output])
        my_exec(visitor_cmd, True)

    if CLEANUP:
        for f in to_clean:
            os.remove(f)

    return outputs

def get_decades_base(combined_llvm):
    """! Transforms the LLM IR code to the basicdecoupled mode.

    Applies multiple passes to the combined LLVM IR code. In the end
    the output is decoupled, but non inter-core communications are
    performed.

    Passes: LOWER_INVOKE (unkown) -> ALWAYS_INLINER (inlines all
    function call) -> PRINT_FUNCTION_CALLS (unknown) ->
    SCALER_REPLACE_AGG (unknown)

    """
    cmd = " ".join([OPT, LOWER_INVOKE, combined_llvm, OUTPUT, combined_llvm])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(combined_llvm, "lower_invoke")
    
    cmd = " ".join([OPT, ALWAYS_INLINER, combined_llvm, OUTPUT, combined_llvm])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(combined_llvm, "inliner")

    cmd = " ".join([OPT, PRINT_FUNCTION_CALLS, combined_llvm, OUTPUT, combined_llvm])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(combined_llvm, "print_func_call")

    if FROM_NUMBA:
        cmd = " ".join([OPT, NUMBA_FUNCTION_REPLACE, combined_llvm, OUTPUT, combined_llvm])
        my_exec(cmd)

    cmd = " ".join([OPT, SCALER_REPLACE_AGG, combined_llvm, OUTPUT, combined_llvm])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(combined_llvm, "scaler_replace")

    if DEBUG:
        cmd = " ".join([OPT, "--dwarf64", combined_llvm, OUTPUT, combined_llvm])
        my_exec(cmd)

    
    return combined_llvm

def get_decades_decoupled_implicit(llvm_input):
    """! Transforms the LLM IR code to the GraphAttac mode.

    Applies multiple passes to the combined LLVM IR code. In the end
    the output is decoupled, with inter-core communications with
    respect to the GraphAttack paper (single internal queue in the
    Queue structure and compute nodes do the stores.)

    Passes: FIND_NMA -> DEAD_CODE_ELIM -> DECOUPLE_SUPPLY ->
    DECOUPLE_COMPUTE -> DEAD_CODE_ELIM -> DECOUPLE_SUPPLY_ADDR ->
    DEAD_CODE_ELIM
    """

    # Do Find NMA:
    cmd = " ".join([OPT, FIND_NMA, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "find_NMA")

    # Do aggressive dead code elim (after the FindNMA pass)
    cmd = " ".join([OPT, DEAD_CODE_ELIM, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "code_elim1")
    
    # Do supply first
    cmd = " ".join([OPT, DECOUPLE_SUPPLY, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "supply")

    # Do compute
    cmd = " ".join([OPT, DECOUPLE_COMPUTE, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "compute")

    # Do aggressive dead code elim (before terminal load opt)
    cmd = " ".join([OPT, DEAD_CODE_ELIM, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "code_elim2")

    # Do terminal load opt
    cmd = " ".join([OPT, DECOUPLE_SUPPLY_ADDR, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "supply_addr")
 
    # Do aggressive dead code elim (before terminal load opt)
    cmd = " ".join([OPT, DEAD_CODE_ELIM, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "code_elim3")

    if DEBUG:
        cmd = " ".join([OPT, "--dwarf64", combined_llvm, OUTPUT, combined_llvm])
        my_exec(cmd)

    return llvm_input

def get_decades_decoupled_DeSC(llvm_input):
    """! Transforms the LLM IR code to the DESC mode.

    Applies multiple passes to the combined LLVM IR code. In the end
    the output is decoupled, with inter-core communications with
    respect to the DeSC paper (multiple queues in the Queue structure
    and supply nodes do the stores.)

    Passes: DECOUPLE_SUPPLY -> DECOUPLE_COMPUTE -> DEAD_CODE_ELIM ->
    DECOUPLE_SUPPLY_ADDR -> DEAD_CODE_ELIM

    """
    # Do supply first
    cmd = " ".join([OPT, DECOUPLE_SUPPLY, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "supply")
    
    # Do compute
    cmd = " ".join([OPT, DECOUPLE_COMPUTE, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "compute")

    # Do aggressive dead code elim (before terminal load opt)
    cmd = " ".join([OPT, DEAD_CODE_ELIM, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "code_elim1")

    # Do terminal load opt
    cmd = " ".join([OPT, DECOUPLE_SUPPLY_ADDR, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "supply_addr")
    
    # Do aggressive dead code elim (now to end things)
    cmd = " ".join([OPT, DEAD_CODE_ELIM, llvm_input, OUTPUT, llvm_input])
    my_exec(cmd)
    if DUMP_LLVMIR:
        generate_LLVMIR(llvm_input, "code_elim2")

    if DEBUG:
        cmd = " ".join([OPT, "--dwarf64", combined_llvm, OUTPUT, combined_llvm])
        my_exec(cmd)

    return llvm_input

def get_decades_spp(llvm_input, name, ids, to_link):
    """! Calles the simulator or numba preprocessing script
    """
    if TARGET_OUT in ["simulator", "numba"] and SPP is not None:
        cmd = " ".join(["bash", SPP, llvm_input, name, to_link] + ids)
        my_exec(cmd)
        #assert(False)
        return llvm_input
    else:
        return llvm_input

def compile_native(dir_name):
    """! Compiles the input files down to nativ LLVM IR.
    """
    create_fresh_dir(dir_name)

    llvm_combined = get_llvm_combined(dir_name, FILES)    
    bin_output = os.path.join(dir_name, "native")
    cmd = " ".join([CLANG, CLANG_OPENMP_FLAGS, OPENMP_LIB, llvm_combined, OUTPUT, bin_output])
    my_exec(cmd)           
    return

def compile_DECADES_base(out_dir):
    """! Compiles all the input files to basic decoupled mode.

    """
    create_fresh_dir(out_dir)
    if not FROM_NUMBA:
        visited_files = get_visited_cpp_file(out_dir)
    else:
        visited_files = FILES
    llvm_combined = get_llvm_combined(out_dir, visited_files, True)
    llvm_dbase = get_decades_base(llvm_combined)
    if FROM_NUMBA:
        to_link = "0"
        os.environ['DECADES_KERNEL_STR'] = "_kernel_compute"
    else:
        to_link = "1"
        os.environ['DECADES_KERNEL_STR'] = "_kernel_compute"
    os.environ['DECADES_KERNEL_TYPE'] = "compute"
    
    llvm_spp = get_decades_spp(llvm_dbase, "compute", [str(i) for i in range(THREADS)], to_link)
    if FROM_NUMBA:
        return
    bin_output = os.path.join(out_dir, "decades_base")
    obj_output = os.path.join(out_dir, "combined.o")
    
    if (TARGET_OUT == "x86" or TARGET_OUT == "simulator"):
        cmd = " ".join([CLANG, CLANG_OPENMP_FLAGS, OPENMP_LIB, DECOUPLING_LIB, DECOUPLING_LIB_NAME, llvm_spp, OUTPUT, bin_output])
        my_exec(cmd)
    elif (TARGET_OUT == "riscv64"):
        cmd = " ".join([CLANG, RISCV_TARGET, RISCV_SYSROOT, RISCV_GCC, CLANG_OPENMP_FLAGS, OPENMP_RISCV_LIB, DECOUPLING_LIB, DECOUPLING_LIB_NAME, llvm_spp, OUTPUT, bin_output])
        my_exec(cmd)       
    return

def compile_decoupled_DeSC(out_dir):
    """! Compiles all the input files to decoupled DeSC mode.

    """
    create_fresh_dir(out_dir)
    if not FROM_NUMBA:
        visited_files = get_visited_cpp_file(out_dir)
    else:
        visited_files = FILES

    
    llvm_combined = get_llvm_combined(out_dir, visited_files, True)
    llvm_dbase = get_decades_base(llvm_combined)

    llvm_decoupled_implicit = get_decades_decoupled_DeSC(llvm_dbase)

    # do decoupling pass
    os.environ['DECADES_KERNEL_STR'] = "_kernel_compute"
    os.environ['DECADES_KERNEL_TYPE'] = "compute"
    llvm_spp = get_decades_spp(llvm_decoupled_implicit, "compute", [str(i) for i in range(THREADS//2)], "0")

    os.environ['DECADES_KERNEL_STR'] = "_kernel_supply"
    os.environ['DECADES_KERNEL_TYPE'] = "supply"
    if FROM_NUMBA:
        to_link = "0"
    else:
        to_link = "1"
    llvm_spp = get_decades_spp(llvm_spp, "supply", [str(i) for i in range(THREADS//2, THREADS)], to_link)
    
    bin_output = os.path.join(out_dir, "decades_decoupled_implicit")
    obj_output = os.path.join(out_dir, "combined.o")

    if (TARGET_OUT == "x86" or TARGET_OUT == "simulator"):
        cmd = " ".join([CLANG, "-L", DECOUPLE_LIB_DIR, CLANG_OPENMP_FLAGS, OPENMP_LIB, DECOUPLING_LIB, DECOUPLING_LIB_NAME, llvm_spp, DECOUPLE_LIB_FLAG, OUTPUT, bin_output])
        my_exec(cmd)
    elif (TARGET_OUT == "riscv64"):
        #cmd = " ".join([CLANG, RISCV_SYSROOT, TARGET, TARGET_OUT, OBJ_FILE, visited_files[0], OUTPUT, obj_output])
        #my_exec(cmd)
        cmd = " ".join([CLANG, RISCV_TARGET, RISCV_SYSROOT, RISCV_GCC, CLANG_OPENMP_FLAGS, OPENMP_RISCV_LIB, "-L", DECOUPLE_LIB_DIR, llvm_spp, DECOUPLE_LIB_FLAG_RISCV, OUTPUT, bin_output])
        my_exec(cmd)           
    return

def compile_decoupled_implicit(out_dir):
    """! Compiles all the input files to decoupled GraphAttack mode.

    """
    create_fresh_dir(out_dir)
    if not FROM_NUMBA:
        visited_files = get_visited_cpp_file(out_dir)
    else:
        visited_files = FILES

    llvm_combined = get_llvm_combined(out_dir, visited_files, True)
    llvm_dbase = get_decades_base(llvm_combined)

    llvm_decoupled_implicit = get_decades_decoupled_implicit(llvm_dbase)

    # do decoupling pass
    os.environ['DECADES_KERNEL_STR'] = "_kernel_compute"
    os.environ['DECADES_KERNEL_TYPE'] = "compute"
    llvm_spp = get_decades_spp(llvm_decoupled_implicit, "compute", [str(i) for i in range(THREADS//2)], "0")

    os.environ['DECADES_KERNEL_STR'] = "_kernel_supply"
    os.environ['DECADES_KERNEL_TYPE'] = "supply"
    if FROM_NUMBA:
        to_link = "0"
    else:
        to_link = "1"
    llvm_spp = get_decades_spp(llvm_spp, "supply", [str(i) for i in range(THREADS//2, THREADS)], to_link)
    
    bin_output = os.path.join(out_dir, "decades_decoupled_implicit")
    obj_output = os.path.join(out_dir, "combined.o")

    if (TARGET_OUT == "x86" or TARGET_OUT == "simulator"):
        cmd = " ".join([CLANG, "-L", DECOUPLE_LIB_DIR, CLANG_OPENMP_FLAGS, OPENMP_LIB, DECOUPLING_LIB, DECOUPLING_LIB_NAME, llvm_spp, DECOUPLE_LIB_FLAG, OUTPUT, bin_output])
        my_exec(cmd)
    elif (TARGET_OUT == "riscv64"):
        #cmd = " ".join([CLANG, RISCV_SYSROOT, TARGET, TARGET_OUT, OBJ_FILE, visited_files[0], OUTPUT, obj_output])
        #my_exec(cmd)
        cmd = " ".join([CLANG, RISCV_TARGET, RISCV_SYSROOT, RISCV_GCC, CLANG_OPENMP_FLAGS, OPENMP_RISCV_LIB, "-L", DECOUPLE_LIB_DIR, llvm_spp, DECOUPLE_LIB_FLAG_RISCV, OUTPUT, bin_output])
        my_exec(cmd)           
    return


def compile_decades_biscuit(out_dir):
    """! Compiles all the input files to decoupled GraphAttack mode.

    """
    create_fresh_dir(out_dir)
    visited_files = get_visited_cpp_file(out_dir)
    llvm_combined = get_llvm_combined(out_dir, visited_files, True)
    llvm_dbase = get_decades_base(llvm_combined)

    os.environ['DECADES_KERNEL_STR'] = "_kernel_"
    os.environ['DECADES_KERNEL_TYPE'] = "compute"
    llvm_spp = get_decades_spp(llvm_dbase, "compute", ["0"], "0")

    os.environ['DECADES_KERNEL_STR'] = "_kernel_biscuit"
    os.environ['DECADES_KERNEL_TYPE'] = "biscuit"
    llvm_spp = get_decades_spp(llvm_spp, "biscuit", ["1"], "1")

    bin_output = os.path.join(out_dir, "decades_biscuit")
    obj_output = os.path.join(out_dir, "combined.o")

    if (TARGET_OUT == "x86" or TARGET_OUT == "simulator"):
        cmd = " ".join([CLANG, CLANG_OPENMP_FLAGS, llvm_spp, OUTPUT, bin_output])
        my_exec(cmd)
    elif (TARGET_OUT == "riscv64"):
        cmd = " ".join([RISCV_CC, LPTHREAD, "-fopenmp", visited_files[0], RISCV_LIB])
        my_exec(cmd)
    return

def main():
    # Add logic to parse args here.
    global FILES
    global SPP
    global DEBUG
    global INCLUDE
    global FAST
    global CLEANUP
    global THREADS
    global SYNC_MODE
    global SCRATCHPAD_SIZE
    global TARGET_OUT
    global FROM_NUMBA
    global FRESH_DIRECTORY
    global DECOUPLING_LIB_NAME
    global DUMP_LLVMIR

    parser = argparse.ArgumentParser()
    parser.add_argument('source', nargs='+', help="C++ files to compile")
    parser.add_argument("-spp", "--simulator_preprocessor_script", type=str,
                        help="A script to run on llvm source before it is compiled to a binary")
    parser.add_argument("-g", "--debug", help="Generate debugging information", action='store_true')
    parser.add_argument("-I", "--Include", type=str, help="Include directory for compilation")
    parser.add_argument("-x", "--language", type=str, help="Language for input files")
    parser.add_argument("-m", "--comp_model", help="Compilation variant: native(n), decades-base(db), decoupled-explicit(de), decoupled-implicit(di), decoupled-DeSC(ds), or biscuit(b))", default='db')
    parser.add_argument("-a", "--accelerators", type=int, default=0, help="Use accelerators for computation")
    parser.add_argument("-t", "--num_threads", type=int, help="Number of threads to use", default=1)
    parser.add_argument("-cmr", "--num_consumers", type=int, help="Number of consumers to use", default=1)
    parser.add_argument("-r", "--rmw_mode", type=int, help="RMW mode: basic (0), locked loads (1), or ALU on buffer (2)")
    parser.add_argument("-s", "--biscuit_sync_mode", type=int, default=0, help="Use synchronous mode")
    parser.add_argument("-sps", "--biscuit_scratchpad_size", help="Size of biscuit scratchpad (bytes)")
    parser.add_argument("-tr", "--target", help="Backend to target (e.g. x86 or riscv64)", default="x86")

    parser.add_argument("-c", "--cleanup", help="cleanup intermediate files", default=0)
    parser.add_argument("-fn", "--from_numba", help="take as input a directory of numba llvm files", default=0)
    parser.add_argument("-fd", "--fresh_dir", help="create a fresh director. Should always be enabled except when called from the Numba Pipeline", default="1")
    parser.add_argument("-l", "--dump_LLVMIR", help="Creates eps files with the LLVM-IR after each pass",  action='store_true')

    parser.add_argument("-mpl", "--maple", help="use the maple IS tile library for decoupling. Only works for riscv", default=0)

    args = parser.parse_args()

    # Setup the target architecture
    if args.target in ["x86", "simulator"]:
        DECOUPLING_LIB_NAME = DECOUPLING_LIB_NAME_X86_SHMEM
        assert(args.maple == 0)
    if args.target in ["riscv64"] and args.maple == "1":
        DECOUPLING_LIB_NAME = DECOUPLING_LIB_NAME_RISCV_MAPLE
    elif args.target in ["riscv64"]:
        DECOUPLING_LIB_NAME = DECOUPLING_LIB_NAME_RISCV_SHMEM
    assert(DECOUPLING_LIB_NAME is not None)
        
    # Create a new output directory
    if args.fresh_dir == "1":
        FRESH_DIRECTORY = True
    else:
        FRESH_DIRECTORY = False

    # checks if numa mode activated
    os.environ['DECADES_FROM_NUMBA'] = "0"
    if args.from_numba == '1':
        FROM_NUMBA = True
        os.environ['DECADES_FROM_NUMBA'] = "1"

    if args.cleanup:
        CLEANUP = args.cleanup

    if args.simulator_preprocessor_script is not None:
        SPP = args.simulator_preprocessor_script        
    
    DEBUG = args.debug
    DUMP_LLVMIR = args.dump_LLVMIR 
    
    
    if args.Include is not None:
        INCLUDE = args.Include

    if args.language is not None:
        LANGUAGE = args.language

    THREADS = args.num_threads
    
    FILES = args.source

    if args.biscuit_sync_mode is not None:
        SYNC_MODE = args.biscuit_sync_mode

    if args.biscuit_scratchpad_size is not None:
        SCRATCHPAD_SIZE = args.biscuit_scratchpad_size

    assert(args.target in ["x86", "simulator", "riscv64", "numba"])
    TARGET_OUT = args.target
    
    if args.accelerators:
        os.environ['DECADES_ACCELERATORS'] = str(1)
    else:
        os.environ['DECADES_ACCELERATORS'] = str(0)
    
    start = time.time()

    rmw_mode = args.rmw_mode
    if (rmw_mode == 1):
      os.environ['DECADES_RMW'] = 1
    elif (rmw_mode == 2):
      os.environ['DECADES_RMW'] = 2

    # sets up additional env variables and calls the appropriate compile_DECADES* function
    model = args.comp_model
    if (model == "native" or model == 'n'):
        out_dir = "decades_native"
        compile_native(out_dir)
    elif (model == "decades-base" or model == 'db'):
        os.environ['DECADES_THREAD_NUM'] = str(args.num_threads)
        os.environ['DECADES_NUM_CONSUMERS'] = str(args.num_consumers)
        os.environ['DECADES_DECOUPLED'] = str(0)
        os.environ['DECADES_DECOUPLED_EXPLICIT'] = str(0)
        out_dir = "decades_base"
        os.environ['DECADES_RUN_DIR'] = out_dir
        compile_DECADES_base(out_dir)
    elif (model == "decouple-implicit" or model == 'di'):
        if ((args.num_threads%2)==1):
            raise Exception("number of threads must be a multiple of 2 for decoupled implicit mode")
        os.environ['DECADES_THREAD_NUM'] = str(args.num_threads)
        os.environ['DECADES_NUM_CONSUMERS'] = str(args.num_threads/2)
        os.environ['DECADES_DECOUPLED'] = str(1)
        os.environ['DECADES_DECOUPLED_EXPLICIT'] = str(0)
        os.environ['DECADES_DECOUPLING_MODE'] = str("d");
        out_dir = "decades_decoupled_implicit"
        os.environ['DECADES_RUN_DIR'] = out_dir
        compile_decoupled_implicit(out_dir)
    elif (model == "decouple-DeSC" or model == 'ds'):
        if ((args.num_threads%2)==1):
            raise Exception("number of threads must be a multiple of 2 for decoupled implicit mode")
        os.environ['DECADES_THREAD_NUM'] = str(args.num_threads)
        os.environ['DECADES_NUM_CONSUMERS'] = str(args.num_threads/2)
        os.environ['DECADES_DECOUPLED'] = str(1)
        os.environ['DECADES_DECOUPLED_EXPLICIT'] = str(0)
        os.environ['DECADES_DECOUPLING_MODE'] = str("d");
        out_dir = "decades_decoupled_implicit"
        os.environ['DECADES_RUN_DIR'] = out_dir
        compile_decoupled_DeSC(out_dir)
    # elif (model=="decouple-conjoint" or model =="co"):
    #     if (args.num_threads<(args.num_consumers+1)):
    #         raise Exception("number of threads must be more than number of consumer threads")
    #     os.environ['DECADES_THREAD_NUM'] = str(args.num_threads)
    #     os.environ['DECADES_NUM_CONSUMERS'] = str(args.num_consumers)
    #     os.environ['DECADES_DECOUPLED'] = str(1)
    #     os.environ['DECADES_CONJOINT'] = str(1)
    #     os.environ['DECADES_DECOUPLING_MODE'] = str("c");
    #     os.environ['DECADES_DECOUPLED_EXPLICIT'] = str(0)
    #     out_dir = "decades_decoupled_conjoint"
    #     os.environ['DECADES_RUN_DIR'] = out_dir
    #     # this needs to be changed: compile_decoupled_implicit -> compile_decoupled_conjoint
    #     compile_decoupled_implicit(out_dir)
    elif (model == "decoupled-explicit" or model == 'de'):
        assert(False)
        compile_decoupled() #need to add separate method for this or args?
    elif (model == "biscuit" or model == 'b'):
        #os.environ['DECADES_THREAD_NUM'] = str(args.num_threads)
        os.environ['DECADES_DECOUPLED'] = str(0)
        os.environ['DECADES_DECOUPLED_EXPLICIT'] = str(0)
        os.environ['DECADES_BISCUIT'] = str(1)
        out_dir = "decades_biscuit"
        compile_decades_biscuit(out_dir)

    end = time.time()
    print("")
    print("total time (seconds): " + str(end - start))

if __name__ == "__main__":
    main()
