import numpy as np
from numba import int64, float32, float64, jitclass, njit, types, generated_jit
from numba.extending import overload
from numba.types import Tuple, UniTuple
from scipy.sparse import csr_matrix, csc_matrix
from DEC_Pipeline import DEC_Pipeline, DEC_Options
import os

####   Decades Sparse Graph   ####
spec_sparse = [('indptr', int64[:]), ('indices', int64[:]),
               ('node_attr', int64[:]), ('num_nodes', int64)]

@jitclass(spec_sparse)
class mk_DecSparseGraph:
    def __init__(self, graph_indptr, graph_indices, node_attr, num_nodes):
        self.indptr = graph_indptr
        self.indices = graph_indices
        self.node_attr = node_attr
        self.num_nodes = num_nodes
        
def DecSparseGraphSpec():
    return mk_DecSparseGraph.class_type.instance_type

@njit(DecSparseGraphSpec()(int64[:], int64[:], int64[:], int64), nogil=True, pipeline_class=DEC_Pipeline)
def DecSparseGraph(i, j, k, l):
    return mk_DecSparseGraph(i, j, k, l)

def LoadDecSparseGraph(data_path, vertical=False):
    filename='/'.join([data_path,'edge_list.tsv'])
    with open(filename) as f:
        graph_edge_data = np.fromfile(f, count=-1, sep='\t',dtype=np.int64)
        graph_edge_data = np.reshape(graph_edge_data, (int(len(graph_edge_data) / 2), 2))

    num_nodes = np.max(graph_edge_data) + 1
    graph_edge_data = np.concatenate((graph_edge_data,np.flip(graph_edge_data, axis=1)),axis=0)
    data=np.ones(graph_edge_data.shape[0])
    row=graph_edge_data[:,0]
    column=graph_edge_data[:,1]
    if vertical:
        graph=csc_matrix((data,(row,column)),shape=(num_nodes,num_nodes))
    else:
        graph=csr_matrix((data,(row,column)),shape=(num_nodes,num_nodes))

    filename='/'.join([data_path,'node_attr.tsv'])
    if os.path.isfile(filename):
        with open(filename) as f:
            graph_node_attr = np.fromfile(f, count=-1, sep='\t',dtype=np.int64)
            graph_node_attr = np.reshape(graph_node_attr, (int(len(graph_node_attr) / 2), 2))
            graph_node_attr = graph_node_attr[:,1]
    else:
        graph_node_attr = np.zeros(num_nodes, dtype=np.int64)
        
    sparse_graph = DecSparseGraph(graph.indptr, graph.indices, graph_node_attr,num_nodes)

    return sparse_graph


####   Decades Bipartite Graph   ####
spec_bipartite = [
    ('x_nodes', int64),                     # number of x nodes
    ('y_nodes', int64),                     # number of y nodes
    ('edges', int64),                       # number of edges
    ('node_array', int64[:]),               # vector of node_ids
    ('edge_array', int64[:]),               # vector of edge_ids
    ('edge_data', float32[:]),              # vector of edge_weights
    ('projection_size', int64),             # size of one dim of projection array
    ('first_val', int64),                   # first neighbor node id
    ('graph_edge_data', float32[:, ::1]),   # projected graph edge weights
    ('tri_size', int64)                     # size of projected triangular matrix
]

@jitclass(spec_bipartite)
class mk_DecBipartiteGraph():
    def __init__(self, edges, x_nodes, y_nodes, graph_edge_data):
        self.x_nodes = x_nodes
        self.y_nodes = y_nodes
        self.edges = edges
        self.node_array = np.zeros(self.x_nodes+1, dtype=np.int64)
        self.edge_array = np.zeros(self.edges, dtype=np.int64)
        self.edge_data = np.zeros(self.edges, dtype=np.float32)
        self.projection_size = np.int64(((y_nodes * y_nodes) - y_nodes) / 2)
        i = 0
        node = 0
        while i < self.edges:
            first, second, weight = graph_edge_data[i, :]
            if np.int64(first) != node:
                node += 1
                self.node_array[node] = i
            self.edge_array[i] = np.int64(second)
            self.edge_data[i] = weight
            i += 1
        self.node_array[self.x_nodes] = self.edges
        self.tri_size = np.int64(y_nodes * (y_nodes - 1) / 2)

        
def DecBipartiteGraphSpec():
     return mk_DecBipartiteGraph.class_type.instance_type

@njit(DecBipartiteGraphSpec()(int64, int64, int64, float32[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def DecBipartiteGraph(i, j, k, l):
    return mk_DecBipartiteGraph(i, j, k, l)


def LoadDecBipartiteGraph(filename):
    with open(filename) as f:
        _ = f.readline()
        line = f.readline().split()
        edges = np.int64(line[1])
        x_nodes = np.int64(line[2])
        y_nodes = np.int64(line[3])
        edge_data = np.loadtxt(f, dtype=np.float32)
    bipartite_graph = DecBipartiteGraph(edges, x_nodes, y_nodes, edge_data)
    
    return bipartite_graph


# Decades class implementation for csr, csc, coo
# sparse matrix data structures (see below for matrix operations)
spec_sparse_float32 = [('indptr', int64[:]), ('indices', int64[:]),
                       ('data', float32[:]), ('shape', int64[:]), ('size', int64)]

@jitclass(spec_sparse_float32)
class mk_DecCSR:
    def __init__(self, indptr, indices, data):
        self.indptr = indptr
        self.indices = indices
        self.shape = np.array([len(indptr)-1, np.max(indptr)], dtype=np.int64)
        self.size = indptr[-1]
        self.data = data


@jitclass(spec_sparse_float32)
class mk_DecCSC:
    def __init__(self, indptr, indices, data):
        self.indptr = indptr
        self.indices = indices
        self.shape = np.array([len(indptr), np.max(indptr)+1], dtype=np.int64)
        self.size = indptr[-1]
        self.data = data


spec_sparse_coo_float32 = [('rows', int64[:]), ('cols', int64[:]),
                       ('data', float32[:]), ('shape', int64[:]), ('size', int64)]            

@jitclass(spec_sparse_coo_float32)
class mk_DecCOO:
    def __init__(self, rows, cols, data):
        self.rows = rows
        self.cols = cols
        self.shape = np.array([np.max(rows)+1, np.max(cols)+1], dtype=np.int64)
        self.size = np.int64(len(rows))
        self.data = data

        
def DecCSRSpec():
    return mk_DecCSR.class_type.instance_type


def DecCSCSpec():
    return mk_DecCSC.class_type.instance_type


def DecCOOSpec():
    return mk_DecCOO.class_type.instance_type

@njit(DecCSRSpec()(int64[:], int64[:],float32[:]), nogil=True, pipeline_class=DEC_Pipeline)
def DecCSR(i, j, k):
    return mk_DecCSR(i,j,k)


@njit(DecCSCSpec()(int64[:], int64[:],float32[:]), nogil=True, pipeline_class=DEC_Pipeline)
def DecCSC(i, j, k):
    return mk_DecCSC(i,j,k)


@njit(DecCOOSpec()(int64[:], int64[:],float32[:]), nogil=True, pipeline_class=DEC_Pipeline)
def DecCOO(i, j, k):
    return mk_DecCOO(i,j,k)


# eliminate zeros in CSR matrix if any
@njit(DecCSRSpec()(DecCSRSpec()))
def eliminate_zeros(A):
    nrow, ncol = A.shape
    nnz, row_end = 0, 0
    for i in range(nrow):
        jj = row_end
        row_end = A.indptr[i+1]
        while (jj < row_end):
            j, x = A.indices[jj], A.data[jj]
            if x!=0:
                A.indices[nnz], A.data[nnz] = j, x
                nnz += 1
            jj +=1
        A.indptr[i+1] = nnz
    A.indices = A.indices[:nnz]
    A.data = A.data[:nnz]
    return A

# Decades implementation of sparse matrix operations
#
#   matrix transformations:
#        csr to csc
#        csc to csr
#        coo to csr
#        csr to coo
#        tranpose
#
#   matrix-matrix operations:
#        (for all combinations: sparse-sparse, sparse-dense, dense-sparse, dense-dense
#                              and for all three sparse matrix classes: csr, csc, coo)
#         dot product
#         Hadamard product

    
@njit(DecCSRSpec()(DecCOOSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def coo_to_csr(A):
    n_rows,n_cols= A.shape
    nnz = A.size
    indptr= np.zeros(n_cols+1, dtype=np.int64)
    indices= np.zeros(A.size, dtype=np.int64)
    data = np.zeros(A.size, dtype=np.float32) 
    last, cumsum = 0, 0
    
    for n in range(nnz):
        indptr[A.rows[n]] += 1
    
    for i in range(n_rows):
        temp = indptr[i]
        indptr[i] = cumsum
        cumsum += temp
    
    for n in range(nnz):
        row = A.rows[n]
        dest = indptr[row]
        indices[dest] = A.cols[n]
        data[dest] = A.data[n]     
        indptr[row] += 1

    for i in range(n_rows+1):
        temp = indptr[i]
        indptr[i] = last
        last = temp

    return DecCSR(indptr, indices, data)

@njit(DecCOOSpec()(DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def csr_to_coo(A):
    n_rows, n_cols = A.shape
    cols = A.indices
    rows = np.empty(len(cols), dtype=A.indices.dtype)
    
    for i in range(n_rows):
        for j in range(A.indptr[i],A.indptr[i+1]):
            rows[j] = i
    
    return DecCOO(rows, cols, A.data)

@njit(DecCSCSpec()(DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def csr_to_csc(A):
    n_rows,n_cols= A.shape
    indptr= np.zeros(n_cols+1, dtype=np.int64)
    indices= np.zeros(A.size, dtype=np.int64)
    data = np.zeros(A.size, dtype=np.float32)
    last = 0

    # enter number of elements per column:
    for i in range(A.indptr[-1]):
        indptr[A.indices[i]+1] += 1

    # cumsum number of elements per column:
    for i in range(1,len(indptr)):
        indptr[i] += indptr[i-1]

    # fill indices of rows into index array and
    # fill columnwise stacked data into data array
    for row in range(n_rows):
        for j in range(A.indptr[row], A.indptr[row+1]):
            col = A.indices[j]
            dest = indptr[col]        
            indices[dest] = row
            data[dest] = A.data[j]
            indptr[col] += 1
    
    for col in range(n_cols+1):
        temp = indptr[col]
        indptr[col] = last
        last = temp        

    return DecCSC(indptr, indices, data)


@njit(DecCSRSpec()(DecCSCSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def csc_to_csr(A):
    B = DecCSR(A.indptr,A.indices,A.data)
    B = csr_to_csc(B)
    return DecCSR(B.indptr, B.indices, B.data)

@njit(DecCSRSpec()(DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def transpose_csr(A):
    B = csr_to_csc(A)
    return DecCSR(B.indptr, B.indices, B.data)


@njit(DecCSCSpec()(DecCSCSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def transpose_csc(A):
    B = DecCSR(A.indptr,A.indices,A.data)
    B = csr_to_csc(B)
    return B


@njit(DecCOOSpec()(DecCOOSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def transpose_coo(A):
    return DecCOO(A.cols, A.rows, A.data)


@njit(float32[:,:](float32[:,:],float32[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_dot(x,y):
    m,d=x.shape
    d,n=y.shape
    z= np.zeros((m,n), dtype=np.float32)
    for i in range(m):
        for j in range(n):
            ij = 0
            for k in range(d):
                ij +=x[i,k]*y[k,j]
            z[i,j]= ij
    return z

@njit(float32[:,:](float32[:,:],float32[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_mult(x,y):
    m,n=x.shape
    m2,n2 =y.shape
    if m2!=m or n2!=n:
        print('matrices must be the same dimensions')
        assert(False)
    z= np.zeros((m,n), dtype=np.float32)
    for i in range(m):
        for j in range(n):
            z[i,j] =x[i,j]*y[i,j]
    return z


@njit(float32[:,:](float32[:,:],float32[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_div(x,y):
    m,n=x.shape
    m2,n2 =y.shape
    if m2!=m or n2!=n:
        print('matrices must be the same dimensions')
        assert(False)
    z= np.zeros((m,n), dtype=np.float32)
    for i in range(m):
        for j in range(n):
            z[i,j] =x[i,j]/y[i,j]
    return z

@njit(float32[:,:](float32[:,:],float32[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_add(x,y):
    m,n=x.shape
    m2,n2 =y.shape
    if m2!=m or n2!=n:
        print('matrices must be the same dimensions')
        assert(False)
    z= np.zeros((m,n), dtype=np.float32)
    for i in range(m):
        for j in range(n):
            z[i,j] =x[i,j]+y[i,j]
    return z


@njit(float32[:,:](float32[:,:],float32[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_subt(x,y):
    m,n=x.shape
    m2,n2 =y.shape
    if m2!=m or n2!=n:
        print('matrices must be the same dimensions')
        assert(False)
    z= np.zeros((m,n), dtype=np.float32)
    for i in range(m):
        for j in range(n):
            z[i,j] =x[i,j]-y[i,j]
    return z

@njit(float32[:,:](float32[:,:],float32), nogil=True, pipeline_class=DEC_Pipeline)
def dec_mul_scalar(x,y):
    m,n=x.shape
    z= np.zeros((m,n), dtype=np.float32)
    for i in range(m):
        for j in range(n):
            z[i,j] = y*x[i,j]
    return z

@njit(DecCSRSpec()(DecCSRSpec(),DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def sparse_sparse_dot(A,B):
    #if (A.shape[1]!=B.shape[0]):
    #    raise('Matrix dimensions do not match!Make sure you are multiplying (m x k) and (k x n) matrices.')
    n_rows, n_cols = A.shape[0], B.shape[1]
    indptr = np.zeros((n_rows+1), dtype=np.int64)
    indptr[0] = 0
    mask = np.ones(n_cols, dtype=np.int64) * -1
    
    # pass 1:
    nnz = 0
    for i in range(n_rows):
        row_nnz = 0
        for jj in range(A.indptr[i], A.indptr[i+1]):
            j = A.indices[jj]
            for kk in range(B.indptr[j],B.indptr[j+1]):
                k = B.indices[kk]
                if mask[k] != i:
                    mask[k] = i
                    row_nnz += 1
                    
        next_nnz = nnz + row_nnz
        nnz = next_nnz
        indptr[i+1] = nnz
 
    # pass 2:
    nnz = 0
    indptr[0] = 0;
    indices = np.zeros(indptr[-1], dtype=np.int64)
    data = np.zeros(indptr[-1], dtype=np.float32)

    for i in range(n_rows):
        sums = np.zeros(n_cols, dtype=np.float32)

        for jj in range(A.indptr[i], A.indptr[i+1]):
            j = A.indices[jj]
            v = A.data[jj]

            for kk in range(B.indptr[j],B.indptr[j+1]):
                k = B.indices[kk]
                sums[k] += v*B.data[kk]

        for jj in range(n_rows):
            if (sums[jj] != 0):
                indices[nnz] = jj
                data[nnz] = sums[jj]
                nnz += 1

        indptr[i+1] = nnz
    
        
    return DecCSR(indptr, indices, data)

@njit(DecCSRSpec()(DecCSRSpec(), DecCSCSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def dot_sparse_sparse(A, B):
    #if (A.shape[1]!=B.shape[0]):
    #    raise('Matrix dimensions do not match! Make sure you are multiplying (m x k) and (k x n) matrices.')
    return sparse_sparse_dot(A,csc_to_csr(B))


@njit((float32[:,:])(DecCSRSpec(),float32[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dot_sparse_dense(A,B):
    #if (A.shape[1]!=B.shape[0]):
    #    raise('Matrix dimensions do not match! Make sure you are multiplying (m x k) and (k x n) matrices.')
    n_rows = A.shape[0]
    n_vecs = np.int64(B.shape[1])
    C = np.zeros(n_rows*n_vecs, dtype=np.float32).reshape(n_rows,n_vecs)
    for i in range(n_rows):
        C[i,:] = np.dot(A.data[A.indptr[i]:A.indptr[i+1]],B[A.indices[A.indptr[i]:A.indptr[i+1]],:])
    return C


@njit((float32[:,:])(float32[:,:],DecCSCSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def dot_dense_sparse(B,A):
    #if (A.shape[0]!=B.shape[1]):
    #    raise('Matrix dimensions do not match! Make sure you are multiplying (m x k) and (k x n) matrices.')
    n_cols = A.shape[1]
    n_rows = np.int64(B.shape[0])
    C = np.zeros(n_cols*n_rows, dtype=np.float32).reshape(n_rows,n_cols)
    for i in range(n_cols):
        C[:,i] = np.dot(B[:,A.indices[A.indptr[i]:A.indptr[i+1]]],A.data[A.indptr[i]:A.indptr[i+1]])
    return C

@njit(Tuple((int64[:],int64[:],int64[:]))(int64[:],int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
def list_intersection(list1,list2):
    len1 = len(list1)
    len2 = len(list2)
    
    #if len1==0 or len2 ==0:
    #    return np.array([],dtype=np.int64),np.array([],dtype=np.int64)
    x = np.zeros(max(len1,len2), dtype=np.int64)
    xidx = np.zeros(max(len1,len2), dtype=np.int64)
    xjdx = np.zeros(max(len1,len2), dtype=np.int64)
    i, j, lenx = 0, 0, 0
    while (i < len1) and (j < len2):
        if list1[i] == list2[j]:
            x[lenx] = list1[i]
            xidx[lenx] = i
            xjdx[lenx] = i
            i += 1
            j += 1
            lenx += 1
        elif list1[i] < list2[j]:
            i += 1
        elif list1[i] > list2[j]:
            j += 1
    return x[:lenx], xidx[:lenx], xjdx[:lenx]


@njit(DecCSRSpec()(DecCSRSpec(), DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def multiply_sparse_sparse(A, B):
    #if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
    #    raise('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')

    n_rows, n_cols = A.shape
    indptr= np.zeros(n_cols+1, dtype=np.int64)
    size = max(A.size,B.size)
    data = np.zeros(size, dtype=np.float32)
    indices = np.zeros(size, dtype=np.int64)
    counter = np.int64(0)
    for i in range(n_cols):
        x, xidx, _ = list_intersection(A.indices[A.indptr[i]: A.indptr[i+1]], B.indices[B.indptr[i]:B.indptr[i+1]])
        offset = len(xidx)
        data[counter:counter+offset] = np.multiply(A.data[A.indptr[i]:A.indptr[i+1]][xidx],  B.data[B.indptr[i]:B.indptr[i+1]][xidx])
        indices[counter:counter+offset] = x
        counter += offset
    
    return DecCSR(indptr, indices[:counter], data[:counter])


@njit(DecCSRSpec()(DecCSRSpec(), float32[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def multiply_sparse_dense(A,B):
    #if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
    #    raise('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
    n_rows, n_cols = A.shape
    data = np.zeros(A.size, dtype=np.float32)
    indices = np.zeros(A.size, dtype=np.int64)
    indptr = np.zeros(n_rows+1, dtype=np.int64)
    rowidx= np.array([i for i in range(np.int64(n_cols))], dtype=np.int64)
    counter = np.int64(0)
    for i in range(n_rows):
        x, xidx, _ = list_intersection(A.indices[A.indptr[i]: A.indptr[i+1]], rowidx)
        offset = len(xidx)
        data[counter:counter+offset] =  np.multiply(A.data[A.indptr[i]:A.indptr[i+1]][xidx],np.array([B[i,j] for j in x], dtype=np.float32))
        indices[counter:counter+offset] = x
        counter += offset
        
    return DecCSR(indptr, indices, data)


@njit(DecCSRSpec()(float32[:,:], DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def multiply_dense_sparse(A, B):
    #if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
    #    raise('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
    return multiply_sparse_dense(B, A)

@generated_jit(nopython=True, pipeline_class=DEC_Pipeline)
def dot(A, B):
    if A is DecCSRSpec():
        if B is DecCSRSpec():
            return lambda A,B: dot_sparse_sparse(A, B)
        elif B is DecCSCSpec():
            return lambda A,B: dot_sparse_sparse(A, csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: dot_sparse_dense(A,B)
        else:
            raise('input types not supported')
    elif A is DecCSCSpec():
        if B is DecCSRSpec():
            return lambda A,B: dot_sparse_sparse(csc_to_csr(A), B)
        elif B is DecCSCSpec():
            return lambda A,B: dot_sparse_sparse(csc_to_csr(A), csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: dot_sparse_dense(csc_to_csr(A), B)
        else:
            raise('input types not supported')
    elif isinstance(A, np.ndarray):
       	if B is DecCSRSpec():
            return lambda A,B: dot_dense_sparse(A, B)
        elif B is DecCSCSpec():
            return lambda A,B: dot_dense_sparse(A, csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: dec_dot(A, B)
        else:
            raise('input types not supported')
    else:
        raise('input types not supported')


# Hadamard product
@generated_jit(nopython=True, pipeline_class=DEC_Pipeline)
def multiply(A, B):
    if A is DecCSRSpec():
        if B is DecCSRSpec():
            return lambda A,B: multiply_sparse_sparse(A, B)
        elif B is DecCSCSpec():
            return lambda A,B: multiply_sparse_sparse(A, csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: multiply_sparse_dense(A,B)
        else:
            raise('input types not supported')
    elif A is DecCSCSpec():
        if B is DecCSRSpec():
            return lambda A,B: multiply_sparse_sparse(csc_to_csr(A), B)
        elif B is DecCSCSpec():
            return lambda A,B: multiply_sparse_sparse(csc_to_csr(A), csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: multiply_sparse_dense(csc_to_csr(A), B)
        else:
            raise('input types not supported')
    elif isinstance(A, np.ndarray):
       	if B is DecCSRSpec():
            return lambda A,B: multiply_dense_sparse(A, B)
        elif B is DecCSCSpec():
            return lambda A,B: multiply_dense_sparse(A, csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: np_multiply(A, B)
        else:
            raise('input types not supported')
    else:
        raise('input types not supported')


@njit((float32[:], int64[:], int64[:], int64[:],int64[:], int64[:], int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
def _minimum_spanning_tree(data, cols, indptr, i_sort, rows, predecessors, rank):

    # step through the edges from smallest to largest.
    #  V1 and V2 are connected vertices.
    i, n_edges_in_mst = 0, 0
    n_verts = predecessors.shape[0]
    n_data = i_sort.shape[0]
    
    while i < n_data and n_edges_in_mst < n_verts - 1:
        j =  i_sort[i]
        V1, V2 = rows[j], cols[j]
        # progress upward to the head node of each subtree
        R1 = V1
        while predecessors[R1] != R1:
            R1 = predecessors[R1]
        R2 = V2
        while predecessors[R2] != R2:
            R2 = predecessors[R2]

        # Compress both paths.
        while predecessors[V1] != R1:
            predecessors[V1] = R1
        while predecessors[V2] != R2:
            predecessors[V2] = R2
            
        # if the subtrees are different, then we connect them and keep the
        # edge.  Otherwise, we remove the edge: it duplicates one already
        # in the spanning tree.
        if R1 != R2:
            n_edges_in_mst += 1
            
            # Use approximate (because of path-compression) rank to try
            # to keep balanced trees.
            if rank[R1] > rank[R2]:
                predecessors[R2] = R1
            elif rank[R1] < rank[R2]:
                predecessors[R1] = R2
            else:
                predecessors[R2] = R1
                rank[R1] += 1
        else:
            data[j] = 0
        
        i += 1
        
    # We may have stopped early if we found a full-sized MST so zero out the rest
    while i < n_data:
        j = i_sort[i]
        data[j] = 0
        i += 1

@njit(DecCSRSpec()(DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def dec_minimum_spanning_tree(G):
    #  Work-horse routine for computing minimum spanning tree using
    #  Kruskal's algorithm.  By separating this code here, we get more
    #  efficient indexing.
    n_verts = G.shape[0]
    i_sort = np.argsort(G.data)
    predecessors = np.arange(n_verts)
    rank = np.zeros(predecessors.shape[0], dtype=np.int64)
    rows = np.zeros(G.size, dtype=np.int64)
    for i in range(n_verts):
        for j in range(G.indptr[i], G.indptr[i + 1]):
            rows[j] = i
        
    _minimum_spanning_tree(G.data, G.indices, G.indptr, 
                                   i_sort, rows, predecessors, rank)
    eliminate_zeros(G)
    return G


####   Triangular Dense Matrix    ####

@njit(int64(int64, int64, int64), nogil=True, pipeline_class=DEC_Pipeline)
def TriDenseGraph_num_ele_i_rows(n, i, tri_size):
    return np.int64(tri_size - ((n-i)*((n-i)-1)/2))


@njit(UniTuple(int64, 2)(int64, int64), nogil=True, pipeline_class=DEC_Pipeline)
def TriDenseGraph_k_to_i_j(k, n):
    i = np.int64(n - 2 - np.floor(np.sqrt(-8 * k + 4 * n * (n - 1) - 7) / 2.0 - 0.5))
    j = np.int64(k + i + 1 - n * (n - 1) / 2 + (n - i) * ((n - i) - 1) / 2)
    return i, j


@njit((), nogil=True, pipeline_class=DEC_Pipeline)
def PyDECADES_barrier():
    assert(False)
