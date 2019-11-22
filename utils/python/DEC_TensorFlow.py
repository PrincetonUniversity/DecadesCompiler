import tensorflow as tf
from tensorflow.python.framework import tensor_util
import os

arg_ops = ["Const"]
#excluded_ops = ["NoOp", "Const", "Identity", "VariableV2", "Assign"]

inputs_one = ["relu", "tanh", "sigmoid", "elu", "softmax", "asinh", "selu", "leakyrelu", "maxpool", "avgpool", "biasaddgrad", "avgpoolgrad", "lrn"]
inputs_two = ["prelu", "relugrad", "tanhgrad", "sigmoidgrad", "elugrad", "softmaxgrad", "asinhgrad", "selugrad", "leakyrelugrad", "biasadd", "conv2d", "matmul", "add", "sub", "mul", "div"]
inputs_three = ["prelugrad", "maxpoolgrad", "batchnorm", "conv2dbackpropinput", "conv2dbackpropfilter"]

software_activation = ["prelu", "tanh", "sigmoid"]
activation_fuse = {"relu": 0, "tanh": 1, "sigmoid": 2}
activation = ["elu", "softmax", "asinh", "selu", "leakyrelu"] 
pool = ["maxpool", "avgpool"]
pool_fuse = {"maxpool": 0, "avgpool": 1}
pool_grad = ["maxpoolgrad", "avgpoolgrad"]
convolution = ["conv2d"]
matmul = ["matmul"]
bias_add = ["biasadd"]
arithmetic = ["add", "sub", "mul", "div"]
scalar = ["scalar_add", "scalar_sub", "scalar_mul", "scalar_div"]
sdp = {"add": "0", "sub": "1", "mul": "2", "div": "3", "scalar_add": "4", "scalar_sub": "5", "scalar_mul": "6", "scalar_div": "7", "relu": "8"}
batch_normalization = ["batchnorm"]
lrn = ["lrn"]
activation_grad = ["relugrad", "prelugrad", "tanhgrad", "sigmoidgrad", "elugrad", "softmaxgrad", "asinhgrad", "selugrad", "leakyrelugrad"]
grad = ["biasaddgrad", "maxpoolgrad", "avgpoolgrad", "relugrad", "prelugrad", "tanhgrad", "sigmoidgrad", "elugrad", "softmaxgrad", "asinhgrad", "selugrad", "leakyrelugrad"]
backprop = ["conv2dbackpropinput", "conv2dbackpropfilter"]

# Fusing
fuse_activation = ["relu", "prelu", "tanh", "sigmoid"]
layer = ["conv2d_layer", "dense_layer"]

THRESHOLD = 256*1024/4
BATCH_SIZE = 1

'''
API FUNCTIONS
'''

def run(filename, graph):
    cpp = open(filename, "w+")
    graph_def = graph.as_graph_def(add_shapes=True).node
    ops, funcs, func_dims = parse_graph(graph_def)

    intro(cpp)
    kernel(cpp, ops, funcs, func_dims)
    main(cpp, ops)
    batch(filename)

def print_trace(graph):
    graph_def = graph.as_graph_def(add_shapes=True).node

    for node in graph_def:
        #print(node.name, node.op)
        if (is_kernel_op(graph_def, node)):
            dim = node.attr['_output_shapes'].list.shape[0].dim
            string = node.name + "("
            index = 0
            for i in node.input:
                if (index != 0):
                    string = string + ","
                index = index + 1
                nodes = [n for n in graph_def if n.name == i.split(":")[0]]
                list = [n.attr['_output_shapes'].list.shape[0].dim for n in graph_def if n.name == i.split(":")[0]]
                for dim in list:
                    dim_index = 0
                    for d in range(len(dim)):
                        if (dim[d].size != -1):
                            if dim_index == 0:
                                string = string + str(dim[d].size)
                            else:
                                string = string + "x" + str(dim[d].size)
                        else:
                            if dim_index == 0:
                                string = string + "1"
                            else:
                                string = string + "x" + "1"
                        dim_index = dim_index + 1
            string = string + ")"
            print(string)

def dump_trace(filename, graph):

    filedir = "./"
    
    run(filedir + filename + ".cpp", graph)

'''
C++ FILE FUNCTIONS
'''

def intro(cpp):
    cpp.write("#include \"stdio.h\"\n")
    cpp.write("#include \"stdlib.h\"\n")
    cpp.write("#include <iostream>\n")
    cpp.write("#include <chrono>\n")
    cpp.write("#include \"DECADES/DECADES_TensorFlow.h\"\n")
    cpp.write("\n")

def kernel(cpp, ops, funcs, func_dims):
    cpp.write("__attribute__((noinline))\n")
    cpp.write("void _kernel_(")

    for op in ops:
        cpp.write("float* " + op + ", ")
    cpp.write("int tid, int num_threads)\n")
    cpp.write("{\n")

    for f in range(len(funcs)):
        func = func_dims[f]
        func_name = next(iter(func))
        dims = func[func_name]
        if func_name not in sdp:
            name = "decadesTF_" + func_name
            cpp.write("\t" + name + "(")
        
        if (len(dims) == 1):
            if func_name in (software_activation + activation + activation_grad):
                dim_tot = 1
                for d in dims[0]:
                    dim_tot = dim_tot * d
                cpp.write(str(dim_tot) + ", ")
                if func_name == "elu" or func_name == "elugrad":
                    cpp.write("1.0, ")
            elif func_name in sdp:
                working_mode = sdp[func_name]
                dim_tot = 1
                for d in dims[0]:
                    dim_tot = dim_tot * d
                if int(working_mode) < 4:
                    size_tot = 3*dim_tot
                else:
                    size_tot = 2*dim_tot
                if size_tot >= THRESHOLD:
                    cpp.write("\tdecadesTF_sdp(")
                    cpp.write(working_mode + ", " + str(dim_tot) + ", ")
                else:
                    name = "decadesTF_" + func_name
                    cpp.write("\t" + name + "(")
                    if func_name == "relu":
                        cpp.write(str(dim_tot) + ", ")
                    elif func_name in scalar:
                        cpp.write(str(dim_tot) + ", 1, ")
                    else:
                        cpp.write(str(dim_tot) + ", " + str(dim_tot) + ", ")
        else:
            if func_name in (pool + matmul + convolution + backprop + grad + layer):
                for d in dims[1]:
                    for dim in d:
                        cpp.write(str(dim) + ", ")
        
        func_var = funcs[f]
        func_var_name = next(iter(func_var))
        if func_name == "conv2dbackpropinput":
            for a in range(1, len(func_var[func_var_name])):
                arg = func_var[func_var_name][a]
                cpp.write(arg + ", ")
        elif func_name == "conv2dbackpropfilter":
            for a in range(len(func_var[func_var_name])):
                if (a != 1):
                    arg = func_var[func_var_name][a]
                    cpp.write(arg + ", ")
        else:
            for arg in func_var[func_var_name]:
                cpp.write(arg + ", ")
            if func_name == "relu" and 2*dim_tot >= THRESHOLD:
                cpp.write(arg + ", ")
        cpp.write(func_var_name + ", ")
        if func_name in layer and len(func_var[func_var_name]) == 2:
            cpp.write(func_var_name + ", ")
        
        cpp.write("tid, num_threads);\n")
        cpp.write("\tDECADES_BARRIER();\n")
    cpp.write("}\n")

    cpp.write("\n")

def main(cpp, ops):
    cpp.write("int main() {\n")
    start_time(cpp)
    allocate(cpp, ops)
    invoke_kernel(cpp, ops)
    end_time(cpp)
    cpp.write("\treturn 0;\n")
    cpp.write("}\n")

def batch(filename):
    global BATCH_SIZE

    batch_filename = filename.replace(".cpp", "_batchsize.txt")
    batch_file = open(batch_filename, "w+")
    batch_file.write(str(BATCH_SIZE))
    batch_file.close()
'''
HELPER FUNCTIONS
'''

def start_time(cpp):
    cpp.write("\tauto start = std::chrono::system_clock::now();\n")
    cpp.write("\n")

def allocate(cpp, ops):
    for name in ops:
        size = 1
        for d in ops[name]:
            size = size * d
        cpp.write("\tfloat* " + name + " = (float*) malloc(" + str(size) + "*sizeof(float));\n")
    cpp.write("\n")

def initialize(cpp, vals):
    for var in vals:
        if len(vals[var].shape) > 1:
            values = [col for row in vals[var] for col in row]
        else:
            values = vals[var]
        cpp.write("\tfloat " + var + "_vals[] = {")
        for i in range(len(values)):
            num = values[i]
            if i == 0:
                cpp.write(str(num))
            else:
                cpp.write(", " + str(num))
        cpp.write("};\n")

    cpp.write("\n")

def assign(cpp, vals):
    for var in vals:
        dim = len(vals[var].shape)
        for d in range(dim):
            for i in range(d+1):
                cpp.write("\t")
            cpp.write("for(int i" + str(d) + " = 0; i" + str(d) + " < " + str(vals[var].shape[d]) + "; i" + str(d) + "++) {\n")
        for i in range(d+2):
            cpp.write("\t")
        if (dim > 1):
            cpp.write("*(" + var + " + i0*" + str(vals[var].shape[1]) + " + i1) = " + var + "_vals[i0*" + str(vals[var].shape[1]) + " + i1];\n")
        else:
            cpp.write("*(" + var + " + i0) = " + var + "_vals[i0];\n")
        for d in range(dim):
            for i in range(dim-d):
                cpp.write("\t")
            cpp.write("};\n")
        cpp.write("\n")

def invoke_kernel(cpp, ops):
    cpp.write("\t_kernel_(")
    for op in ops:
        cpp.write(op + ", ")
    cpp.write("0, 1);\n")
    cpp.write("\n")

def output(cpp, ops):
    last = len(ops) - 1
    var = (list(ops.keys())[last])
    dim = len(ops[var])
    for d in range(dim):
        for i in range(d+1):
            cpp.write("\t")
        cpp.write("for(int i" + str(d) + " = 0; i" + str(d) + " < " + str(ops[var][d]) + "; i" + str(d) + "++) {\n")
    for i in range(d+2):
        cpp.write("\t")
    if (dim > 1):
        cpp.write("std::cout << " + var + "[i0*" + str(ops[var][1]) + " + i1] << \" \";\n")
    else:
        cpp.write("std::cout << " + var + "[i0];\n")
    for d in range(dim):
        for i in range(dim-d):
            cpp.write("\t")
        cpp.write("};\n")
    cpp.write("\tstd::cout << \"\\n\";\n")
    cpp.write("\n")

def end_time(cpp):
    cpp.write("\tauto end = std::chrono::system_clock::now();\n")
    cpp.write("\tstd::chrono::duration<double> elapsed_seconds = end-start;\n")
    cpp.write("\tstd::cout << \"Elapsed time: \" << elapsed_seconds.count() << \"s\\n\";\n")
    cpp.write("\n")

def is_kernel_op(graph_def, node):
    included_ops = software_activation + activation + pool + convolution + matmul + bias_add + list(sdp.keys()) + batch_normalization + lrn + grad + backprop
    
    if (node.op.lower() not in included_ops):
        return False
    
    shape = node.attr['_output_shapes'].list.shape
    if (len(shape) == 0):
        return False

    dim = shape[0].dim
    if (len(dim) == 0):
        return False

    func = node.op.lower()
    if func in inputs_one:
        length = 1
    elif func in inputs_two:
        length = 2
    elif func in inputs_three:
        length = 3
    else:
        length = -1
    if (length != -1 and length != len(node.input)):
        return False

    # Check for -1s
    d_ind = 0
    for d in dim:
        #if func in matmul and d.size == -1:
        #    return False
        if d_ind != 0 and d.size == -1:
            return False
        d_ind = d_ind + 1

    for i in node.input:
        node_list = [n.attr['_output_shapes'].list.shape[0].dim for n in graph_def if n.name == i.split(":")[0]]
        if (len(node_list) > 0):
            node_list = node_list[0]
        if (len(node_list) == 0):
            return False
 
    return True

def parse_graph(graph_def):
    global BATCH_SIZE

    ops = {}
    op_counts = {}
    real_names = {}
    funcs = []
    func_dims = []
    fuse = 0
    fuse_name = ""
    dense = 0
    dense_name = ""

    for node in graph_def:
        dense_layer = 0
        if (is_kernel_op(graph_def, node)):
            # Get op output sizes
            op_names = node.name.lower().split('/')
            op_name = op_names[len(op_names)-1]
            if op_name == "biasadd":
                op_name = "add" # map BiasAdd to Add
            if op_name == "conv2d":
                op_name = "conv2d_layer" # map Conv2D to Conv2DLayer
            name = ''.join([i for i in node.name if (not i.isdigit() and not (i == '_'))])
            if name == "dense/MatMul":
                op_name = "dense_layer" # map dense/MatMul to DenseLayer
                dense_layer = 1
            dims = node.attr['_output_shapes'].list.shape[0].dim
            sizes = []
            for dim in dims:
                if dim.size != -1:
                    sizes.append(dim.size)
                else:
                    sizes.append(1)
            if op_name not in op_counts:
                op_counts[op_name] = 0
            else:
                op_counts[op_name] = op_counts[op_name] + 1
            op_name = op_name + "_" + str(op_counts[op_name]) 
            ops[op_name] = sizes.copy()
            real_names[node.name] = op_name

            # Get op input information (variables and their dimensions)
            arg_names = []
            arg_sizes = []
            for i in node.input:
                node_list = [n for n in graph_def if n.name == i.split(":")[0]]
                dims_list = [n.attr['_output_shapes'].list.shape[0].dim for n in graph_def if n.name == i.split(":")[0]]
                arg_size = []
                for dim in dims_list[0]:
                    if (dim.size != -1):
                        arg_size.append(dim.size)
                    else:
                        arg_size.append(1)
                arg_sizes.append(arg_size)
                
                # Get initial variables that are not outputs of kernel functions
                arg_split = i.lower().split('/')
                arg_name = arg_split[len(arg_split)-1]
                arg_name = arg_name.split(":")[0]
                if (not is_kernel_op(graph_def, node_list[0])):    
                    if arg_name not in op_counts:
                        op_counts[arg_name] = 0
                    else:
                        op_counts[arg_name] = op_counts[arg_name] + 1
                    arg_name = arg_name + "_" + str(op_counts[arg_name])
                    ops[arg_name] = arg_size.copy()
                    real_names[node_list[0].name] = arg_name
                else: #argument comes from output of a kernel
                    if node.name not in real_names: #is this possible?
                        ops[arg_name] = arg_size.copy()
                    else:
                        arg_name = real_names[node_list[0].name]
                arg_names.append(arg_name)
            
            # Get op input dimensions
            op = node.op.lower()
            if op == "biasadd":
                op = "add"
                if fuse == 1:
                    entry = funcs[len(funcs)-1]
                    entry_names = entry[fuse_name]
                    arg_names = entry_names + [arg_names[1]]
                    del funcs[len(funcs)-1]
                    op_name = fuse_name
                elif dense == 1:
                    entry = funcs[len(funcs)-1]
                    entry_names = entry[dense_name]
                    arg_names = entry_names + [arg_names[1]]
                    del funcs[len(funcs)-1]
                    op_name = dense_name
            elif fuse == 1:
                fuse = 2
            elif dense == 1:
                dense = 2

            if dense_layer:
                op = "dense_layer"

            if op in sdp or op in (software_activation + activation + activation_grad):
                if (op in software_activation or op == "relu") and fuse == 2:
                    entry = funcs[len(funcs)-1]
                    entry_args = entry[fuse_name]
                    arg_names = entry_args
                    del funcs[len(funcs)-1]
                    op_name = fuse_name
                    
                    entry = func_dims[len(func_dims)-1]
                    entry_args = entry["conv2d_layer"]
                    activation_type = activation_fuse[op]
                    entry_args[1][4] = [1, activation_type]
                    sizes = entry_args
                    del func_dims[len(func_dims)-1]
                    op = "conv2d_layer"
                    
                    fuse = 3
                    dense = 0
                elif fuse == 1:
                    entry = func_dims[len(func_dims)-1]
                    entry_args = entry["conv2d_layer"]
                    entry_args[1][3] = [1]
                    sizes = entry_args
                    del func_dims[len(func_dims)-1]
                    op = "conv2d_layer"
                    fuse = 2
                    dense = 0
                elif (op in software_activation or op == "relu") and dense == 2:
                    entry = funcs[len(funcs)-1]
                    entry_args = entry[dense_name]
                    arg_names = entry_args
                    del funcs[len(funcs)-1]
                    op_name = dense_name

                    entry = func_dims[len(func_dims)-1]
                    entry_args = entry["dense_layer"]
                    activation_type = activation_fuse[op]
                    entry_args[1][2] = [1, activation_type]
                    sizes = entry_args
                    del func_dims[len(func_dims)-1]
                    op = "dense_layer"

                    dense = 0
                    fuse = 0
                elif dense == 1:
                    entry = func_dims[len(func_dims)-1]
                    entry_args = entry["dense_layer"]
                    entry_args[1][1] = [1]
                    sizes = entry_args
                    del func_dims[len(func_dims)-1]
                    op = "dense_layer"
                    dense = 2
                    fuse = 0
                else:
                    dim_tots = [1, 1]
                    for a in range(len(arg_sizes)):
                        for i in range(len(arg_sizes[a])):
                            dim_tots[a] = dim_tots[a] * arg_sizes[a][i]
                    if ((dim_tots[0] == 1 and dim_tots[1] != 1) or (dim_tots[0] != 1 and dim_tots[1] == 1)) and op in arithmetic:
                        op_name = "scalar_" + op_name
                        op = "scalar_" + op
                        if op_name not in op_counts:
                            op_counts[op_name] = 0
                        else:
                            op_counts[op_name] = op_counts[op_name] + 1
                        op_name = op_name + "_" + str(op_counts[op_name])
                        ops[op_name] = sizes.copy()

                    fuse = 0
                    dense = 0                
                    sizes = [sizes]
            else:
                dense = 0
                if op not in pool:
                    fuse = 0
                if op in matmul:
                    fuse = 0
                    if len(arg_sizes[0]) == 2 and len(arg_sizes[1]) == 2:
                        arg_sizes.append([1])
                        if arg_sizes[0][1] != arg_sizes[1][0]:
                            if ops[op_name][0] == arg_sizes[0][0]:
                                arg_sizes[1].reverse()
                                if arg_sizes[0][1] != arg_sizes[1][0]:
                                    arg_sizes[1].reverse()
                                    arg_sizes[0][1] = arg_sizes[1][0]
                                    ops[arg_names[0]] = arg_sizes[0]
                            elif ops[op_name][1] == arg_sizes[1][1]:
                                arg_sizes[0].reverse()
                                if arg_sizes[0][1] != arg_sizes[1][0]:
                                    arg_sizes[0].reverse()
                                    arg_sizes[0][1] = arg_sizes[1][0]
                                    ops[arg_names[0]] = arg_sizes[0]
                    else:
                        BATCH_SIZE = arg_sizes[0][0]
                elif op in convolution or op in backprop:
                    pad = node.attr['padding'].s.decode('utf-8')
                    if pad == "VALID":
                        padding = 1
                    else:
                        padding = 0
                    strides = node.attr['strides'].list.i
                    vert = strides[1]
                    horiz = strides[2]
                    if op in convolution:
                        op = "conv2d_layer"
                        fuse = 1
                        fuse_name = op_name
                        del arg_sizes[1][len(arg_sizes[1])-2]
                        arg_sizes.append([padding, 1, 1])
                        arg_sizes.append([0]) # bias add
                        arg_sizes.append([0, 0]) # activation
                        arg_sizes.append([0, 0, 0, 0, 0, 0]) # pooling
                        arg_sizes.append([0, 0, 0, 0, 0]) # lrn
                    elif op == "conv2dbackpropinput":
                        del arg_sizes[0]
                        del arg_sizes[0][3]
                        arg_sizes.append([padding, 1, 1])
                    elif op == "conv2dbackpropfilter":
                        input_entry = func_dims[len(func_dims)-1]
                        input_entry_args = input_entry["conv2dbackpropinput"]
                        arg_sizes[1] = input_entry_args[1][0][0:2]
                        del arg_sizes[0][0:3]
                        arg_sizes.append([padding, 1, 1])
                elif op in pool or op in pool_grad:
                    ksizes = node.attr['ksize'].list.i
                    height = ksizes[1]
                    width = ksizes[2]
                    arg_sizes.append([height, width])
                    if op in pool and fuse == 3:
                        entry = funcs[len(funcs)-1]
                        entry_args = entry[fuse_name]
                        arg_names = entry_args
                        del funcs[len(funcs)-1]
                        op_name = fuse_name
                        
                        entry = func_dims[len(func_dims)-1]
                        entry_args = entry["conv2d_layer"]
                        pool_type = pool_fuse[op]
                        entry_args[1][5] = [1, pool_type, height, width, 1, 1]
                        arg_sizes = entry_args[1]
                        del func_dims[len(func_dims)-1]
                        op = "conv2d_layer"

                        fuse = 0 #change to 4 with lrn
                    elif op == "maxpoolgrad":
                        del arg_sizes[1:3]
                        fuse = 0
                    else:
                        fuse = 0
                elif op == "biasaddgrad":
                    if len(arg_sizes[0]) < 4:
                        tmp_arg_sizes = [[1]]
                        for i in range(len(arg_sizes[0])):
                            tmp_arg_sizes[0].append(arg_sizes[0][i])
                        for i in range(len(arg_sizes[0])+1,4):
                            tmp_arg_sizes[0].append(1)
                        arg_sizes = tmp_arg_sizes
                    elif len(arg_sizes[0]) > 4:
                        arg_sizes[0] = arg_sizes[0][0:4]
                elif op == "dense_layer":
                    if arg_sizes[0][1] != arg_sizes[1][0]:
                        if ops[op_name][0] == arg_sizes[0][0]:
                            arg_sizes[1].reverse()
                            if arg_sizes[0][1] != arg_sizes[1][0]:
                                arg_sizes[1].reverse()
                                arg_sizes[0][1] = arg_sizes[1][0]
                                ops[arg_names[0]] = arg_sizes[0]
                        elif ops[op_name][1] == arg_sizes[1][1]:
                            arg_sizes[0].reverse()
                            if arg_sizes[0][1] != arg_sizes[1][0]:
                                arg_sizes[0].reverse()
                                arg_sizes[0][1] = arg_sizes[1][0]
                                ops[arg_names[0]] = arg_sizes[0]
                    dim_tots = [1, 1]
                    for a in range(len(arg_sizes)):
                        for i in range(len(arg_sizes[a])):
                            dim_tots[a] = dim_tots[a] * arg_sizes[a][i]
                    BATCH_SIZE = arg_sizes[0][0]
                    arg_sizes = [([(arg_sizes[0][0])] + arg_sizes[1])]
                    arg_sizes.append([0]) # bias add
                    arg_sizes.append([0, 0]) # activation
                    dense = 1
                    dense_name = op_name
                #elif op == "lrn":
                #    print(node)
                sizes = [sizes, arg_sizes]

            funcs.append({op_name: arg_names})
            func_dims.append({op: sizes})

    '''
    print(real_names)
    print()
    print(ops)
    print()
    print(funcs)
    print()
    print(func_dims) 
    '''

    return ops, funcs, func_dims

