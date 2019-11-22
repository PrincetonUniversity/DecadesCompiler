import numpy as np
from numba import int64, float32, float64, jitclass, njit, types, generated_jit, int32
from numba.extending import overload
from numba.types import Tuple, UniTuple, unicode_type, List
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
        
    sparse_graph = DecSparseGraph(np.array(graph.indptr, dtype=np.int64), np.array(graph.indices, dtype=np.int64), graph_node_attr,num_nodes)

    return sparse_graph


####   Decades Bipartite Graph   ####
spec_bipartite = [
    ('x_nodes', int64),                     # number of x nodes
    ('y_nodes', int64),                     # number of y nodes
    ('edges', int64),                       # number of edges
    ('node_array', int64[:]),               # vector of node_ids
    ('edge_array', int64[:]),               # vector of edge_ids
    ('edge_data', float64[:]),              # vector of edge_weights
    ('projection_size', int64),             # size of one dim of projection array
    ('first_val', int64),                   # first neighbor node id
    ('graph_edge_data', float64[:, ::1]),   # projected graph edge weights
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
        self.edge_data = np.zeros(self.edges, dtype=np.float64)
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

@njit(DecBipartiteGraphSpec()(int64, int64, int64, float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def DecBipartiteGraph(i, j, k, l):
    return mk_DecBipartiteGraph(i, j, k, l)


def LoadDecBipartiteGraph(filename):
    with open(filename) as f:
        _ = f.readline()
        line = f.readline().split()
        edges = np.int64(line[1])
        x_nodes = np.int64(line[2])
        y_nodes = np.int64(line[3])
        edge_data = np.loadtxt(f, dtype=np.float64)
    bipartite_graph = DecBipartiteGraph(edges, x_nodes, y_nodes, edge_data)
    
    return bipartite_graph


# Decades class implementation for csr, csc, coo
# sparse matrix data structures (see below for matrix operations)
spec_sparse_float64 = [('indptr', int64[:]), ('indices', int64[:]),
                       ('data', float64[:]), ('shape', int64[:]), ('size', int64)]

@jitclass(spec_sparse_float64)
class mk_DecCSR:
    def __init__(self, indptr, indices, data, shape):
        self.indptr = indptr
        self.indices = indices
        #self.shape = np.array([len(indptr)-1, np.max(indptr)], dtype=np.int64)
        self.shape = shape
        self.size = indptr[-1]
        self.data = data


@jitclass(spec_sparse_float64)
class mk_DecCSC:
    def __init__(self, indptr, indices, data, shape):
        self.indptr = indptr
        self.indices = indices
        # np.array([len(indptr), np.max(indptr)+1], dtype=np.int64)
        self.shape = shape
        self.size = indptr[-1]
        self.data = data


spec_sparse_coo_float64 = [('rows', int64[:]), ('cols', int64[:]),
                       ('data', float64[:]), ('shape', int64[:]), ('size', int64)]            

@jitclass(spec_sparse_coo_float64)
class mk_DecCOO:
    def __init__(self, rows, cols, data, shape):
        self.rows = rows
        self.cols = cols
        # shape = np.array([np.max(rows)+1, np.max(cols)+1], dtype=np.int64)
        self.shape =shape
        self.size = np.int64(len(rows))
        self.data = data

        
def DecCSRSpec():
    return mk_DecCSR.class_type.instance_type


def DecCSCSpec():
    return mk_DecCSC.class_type.instance_type


def DecCOOSpec():
    return mk_DecCOO.class_type.instance_type

@njit(DecCSRSpec()(int64[:], int64[:], float64[:], int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
def DecCSR(i, j, k, l):
    return mk_DecCSR(i,j,k, l)


@njit(DecCSCSpec()(int64[:], int64[:], float64[:], int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
def DecCSC(i, j, k, l):
    return mk_DecCSC(i, j, k, l)


@njit(DecCOOSpec()(int64[:], int64[:], float64[:], int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
def DecCOO(i, j, k, l):
    return mk_DecCOO(i, j, k, l)


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
    data = np.zeros(A.size, dtype=np.float64) 
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

    return DecCSR(indptr, indices, data, A.shape)

@njit(DecCOOSpec()(DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def csr_to_coo(A):
    n_rows, n_cols = A.shape
    cols = A.indices
    rows = np.empty(len(cols), dtype=A.indices.dtype)
    
    for i in range(n_rows):
        for j in range(A.indptr[i],A.indptr[i+1]):
            rows[j] = i
    
    return DecCOO(rows, cols, A.data, A.shape)

@njit(DecCSCSpec()(DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def csr_to_csc(A):
    n_rows,n_cols= A.shape
    indptr= np.zeros(n_cols+1, dtype=np.int64)
    indices= np.zeros(A.size, dtype=np.int64)
    data = np.zeros(A.size, dtype=np.float64)
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

    return DecCSC(indptr, indices, data, A.shape)


@njit(DecCSRSpec()(DecCSCSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def csc_to_csr(A):
    B = DecCSR(A.indptr, A.indices, A.data, A.shape)
    B = csr_to_csc(B)
    return DecCSR(B.indptr, B.indices, B.data, A.shape)

@njit(DecCSRSpec()(DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def transpose_csr(A):
    B = csr_to_csc(A)
    return DecCSR(B.indptr, B.indices, B.data, A.shape[::-1])


@njit(DecCSCSpec()(DecCSCSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def transpose_csc(A):
    B = DecCSR(A.indptr,A.indices,A.data, A.shape[::-1])
    B = csr_to_csc(B)
    return B


@njit(DecCOOSpec()(DecCOOSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def transpose_coo(A):
    return DecCOO(A.cols, A.rows, A.data, A.shape[::-1])


@njit(float64[:,:](float64[:,:],float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_dot(x,y):
    m,d1=x.shape
    d2,n=y.shape
    if d2!=d1:
        print('matrices must have (m x k) and (k x n) dimensions')
        assert(False)
    d = d1
    z= np.zeros((m,n), dtype=np.float64)
    for i in range(m):
        for j in range(n):
            ij = 0
            for k in range(d):
                ij +=x[i,k]*y[k,j]
            z[i,j]= ij
    return z

@njit(float64[:,:](float64[:,:],float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_mult(x,y):
    m,n=x.shape
    m2,n2 =y.shape
    if m2!=m or n2!=n:
        print('matrices must be the same dimensions')
        assert(False)
    z= np.zeros((m,n), dtype=np.float64)
    for i in range(m):
        for j in range(n):
            z[i,j] =x[i,j]*y[i,j]
    return z


@njit(float64[:,:](float64[:,:],float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_div(x,y):
    m,n=x.shape
    m2,n2 =y.shape
    if m2!=m or n2!=n:
        print('matrices must be the same dimensions')
        assert(False)
    z= np.zeros((m,n), dtype=np.float64)
    for i in range(m):
        for j in range(n):
            z[i,j] =x[i,j]/y[i,j]
    return z

@njit(float64[:,:](float64[:,:],float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_add(x,y):
    m,n=x.shape
    m2,n2 =y.shape
    if m2!=m or n2!=n:
        print('matrices must be the same dimensions')
        assert(False)
    z= np.zeros((m,n), dtype=np.float64)
    for i in range(m):
        for j in range(n):
            z[i,j] =x[i,j]+y[i,j]
    return z


@njit(float64[:,:](float64[:,:],float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_subt(x,y):
    m,n=x.shape
    m2,n2 =y.shape
    if m2!=m or n2!=n:
        print('matrices must be the same dimensions')
        assert(False)
    z= np.zeros((m,n), dtype=np.float64)
    for i in range(m):
        for j in range(n):
            z[i,j] =x[i,j]-y[i,j]
    return z

@njit(float64[:,:](float64[:,:],float64), nogil=True, pipeline_class=DEC_Pipeline)
def dec_mul_scalar(x,y):
    m,n=x.shape
    z= np.zeros((m,n), dtype=np.float64)
    for i in range(m):
        for j in range(n):
            z[i,j] = y*x[i,j]
    return z

@njit(DecCSRSpec()(DecCSRSpec(),DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def sparse_sparse_dot(A,B):
    if (A.shape[1]!=B.shape[0]):
        print('Matrix dimensions do not match!Make sure you are multiplying (m x k) and (k x n) matrices.')
        print('Extending one of the sparse matrix to match dimensions, beware if this is not intended')
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
    data = np.zeros(indptr[-1], dtype=np.float64)

    for i in range(n_rows):
        sums = np.zeros(n_cols, dtype=np.float64)

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
    
        
    return DecCSR(indptr, indices, data, np.array([ n_rows, n_cols], dtype=np.int64))

@njit(DecCSRSpec()(DecCSRSpec(), DecCSCSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def dot_sparse_sparse(A, B):
    if (A.shape[1]!=B.shape[0]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x k) and (k x n) matrices.')
        print('Extending one of the sparse matrix to match dimensions, beware if this is not intended')        
    return sparse_sparse_dot(A,csc_to_csr(B))


@njit((float64[:,:])(DecCSRSpec(),float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def dot_sparse_dense(A,B):
    if (A.shape[1]!=B.shape[0]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x k) and (k x n) matrices.')
        if (A.shape[1] > B.shape[0]):
            assert(False)
        else:
            print('Extending the sparse matrix (2nd argument) to match dimensions, beware if this is not intended')

    n_rows = A.shape[0]
    n_cols = np.int64(B.shape[1])
    C = np.zeros((n_rows, n_cols), dtype=np.float64)
    
    for i in range(n_rows):
        for j in range(n_cols):
            tmp = 0.0
            for k in range(A.indptr[i],A.indptr[i+1]):
                tmp += B[A.indices[k],j] * A.data[k]
            C[i,j] = tmp

        #C[i,:] = np.sum(np.multiply(A_row.reshape(-1,1), B[A.indices[A.indptr[i]:A.indptr[i+1]],:]), axis=0)
        #C[i,:] = np.dot(A.data[A.indptr[i]:A.indptr[i+1]],B[A.indices[A.indptr[i]:A.indptr[i+1]],:])
    return C


@njit((float64[:,:])(float64[:,:],DecCSCSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def dot_dense_sparse(B,A):
        
    if A.shape[0]!=B.shape[1]:
        print('Matrix dimensions do not match! Make sure you are multiplying (m x k) and (k x n) matrices.')
        if (A.shape[0] > B.shape[1]):
            assert(False)
        else:
            print('Extending the sparse matrix (2nd argument) to match dimensions, beware if this is not intended')

    n_cols = A.shape[1]
    n_rows = np.int64(B.shape[0])
    C = np.zeros((n_rows, n_cols), dtype=np.float64)
    for i in range(n_rows):
        for j in range(n_cols):
            tmp = 0.0
            for k in range(A.indptr[j],A.indptr[j+1]):
                tmp += B[i,A.indices[k]] * A.data[k]
            C[i,j] = tmp
            #C[:,i] = np.dot(,A.data[A.indptr[i]:A.indptr[i+1]])
            
    return C

@njit((float64[:,:],DecCSCSpec(), float64[:,:], int32, int32), nogil=True, pipeline_class=DEC_Pipeline)
def dot_dense_sparse_parallel(B,A,C, tid, num_threads):

    if A.shape[0]!=B.shape[1]:
        print('Matrix dimensions do not match! Make sure you are multiplying (m x k) and (k x n) matrices.')
        if (A.shape[0] > B.shape[1]):
            assert(False)
        else:
            print('Extending the sparse matrix (2nd argument) to match dimensions, beware if this is not intended')

    n_cols = A.shape[1]
    n_rows = np.int64(B.shape[0])

    #not the best way to distribute, the best would distribute remainder evenly rather than rounding up
    if n_cols < num_threads:
        chunk = 1
    elif n_cols%num_threads>0:
        chunk = (n_cols//num_threads) +1
    else:
        chunk = (n_cols//num_threads)
        
    for i in range(n_rows):
        for j in range((tid)*chunk, min(chunk*(tid+1), n_cols)):
            tmp = 0.0
            for k in range(A.indptr[j],A.indptr[j+1]):
                tmp += B[i,A.indices[k]] * A.data[k]
            C[i,j] = tmp
            #C[:,i] = np.dot(,A.data[A.indptr[i]:A.indptr[i+1]])


@njit(Tuple((int64[:],int64[:],int64[:]))(int64[:],int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
def list_intersection(list1,list2):
    len1 = len(list1)
    len2 = len(list2)
    
    x = np.zeros(max(len1,len2), dtype=np.int64)
    xidx = np.zeros(max(len1,len2), dtype=np.int64)
    xjdx = np.zeros(max(len1,len2), dtype=np.int64)
    i, j, lenx = 0, 0, 0
    while (i < len1) and (j < len2):
        if list1[i] == list2[j]:
            x[lenx] = list1[i]
            xidx[lenx] = i
            xjdx[lenx] = j
            i += 1
            j += 1
            lenx += 1
        elif list1[i] < list2[j]:
            i += 1
        elif list1[i] > list2[j]:
            j += 1
    return x[:lenx], xidx[:lenx], xjdx[:lenx]



#@njit(Tuple((int64[:], int64[:]))(int64[:],int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
@njit(Tuple((int64[:], int64[:], int64[:,:], int64[:,:], int64[:], int64[:], int64[:]))(int64[:],int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
def list_union(list1,list2):
    len1 = len(list1)
    len2 = len(list2)
    minlen = min(len1,len2)
    intersection = np.zeros(minlen, dtype=np.int64)
    x_index = np.zeros(minlen, dtype=np.int64)
    
    union = np.zeros(len1+len2, dtype=np.int64)
    diff_list1 = np.zeros((2,len1), dtype=np.int64)
    diff_list2 = np.zeros((2,len2), dtype=np.int64)
    x_index = np.zeros(minlen, dtype=np.int64)
    xidx = np.zeros(minlen, dtype=np.int64)
    xjdx = np.zeros(minlen, dtype=np.int64)

    i, j, lenu, lenx, diff_i, diff_j, diff_ix, diff_jx = 0, 0, 0, 0, 0, 0, 0 ,0
    k=0
    while (i < len1) and (j < len2) and k<1:
        k +=1
        '''
        if list1[i] == list2[j]:
            union[lenu] = list1[i]
            intersection[lenx] = list1[i]
            x_index[lenx] = lenu
            xidx[lenx] = i
            xjdx[lenx] = j
            i += 1
            j += 1
            lenx += 1
            lenu += 1
        ''' 
         
        if list1[i] == list2[j]:
            union[lenu] = list1[i]
            diff_list1[0, diff_ix] = lenu
            diff_list1[1, diff_ix] =  i
            i += 1
            lenu += 1
            diff_ix +=1
            
        elif list1[i] > list2[j]:
            union[lenu] = list2[j]
            diff_list2[0,diff_jx] = lenu
            diff_list2[1,diff_jx] = j
            j += 1
            lenu += 1
            diff_jx += 1
            
    while i < len1:
            union[lenu] = list1[i]
            diff_list1[0,diff_ix] = lenu
            diff_list1[1,diff_ix] = i
            i += 1
            lenu += 1
            diff_ix +=1

    while j < len2:
            union[lenu] = list2[j]
            diff_list2[0,diff_jx] = lenu
            diff_list2[1,diff_jx] = j
            j += 1
            lenu += 1
            diff_jx += 1
            
    #return intersection, x_index        
    return union[:lenu], intersection[:lenx], diff_list1[:,:diff_ix], diff_list2[:,:diff_jx], x_index[:lenx], xidx[:lenx], xjdx[:lenx]

    #return union, intersection, diff_list1, diff_list2, x_index, xidx, xjdx

    #zzz=np.array([0], dtype=np.int64)
    #zzz2=np.array([[0]], dtype=np.int64)
    #return zzz[:0], zzz[:0], zzz2[:0], zzz2[:0], zzz[:0], zzz[:0], zzz[:0]


@njit(DecCSRSpec()(DecCSRSpec(), DecCSRSpec()), nogil=True,  pipeline_class=DEC_Pipeline)
def ew_add_sparse_sparse(A, B):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)
    n_rows=A.shape[0]
    n_cols = A.shape[1]
    indptr= np.zeros(n_rows+1, dtype=np.int64)
    size = A.size+B.size
    data = np.zeros(size, dtype=np.float64)
    indices = np.zeros(size, dtype=np.int64)
    counter = np.int64(0)

    union , _ , diffA, diffB, x_index, xidx, xjdx = list_union(A.indices[A.indptr[0]: A.indptr[1]], B.indices[B.indptr[0]:B.indptr[1]])
    _ , x_index = list_union(A.indices[A.indptr[0]: A.indptr[1]], B.indices[B.indptr[0]:B.indptr[1]])

    for i in range(n_rows):
        union , _ , diffA, diffB, x_index, xidx, xjdx = list_union(A.indices[A.indptr[i]: A.indptr[i+1]], B.indices[B.indptr[i]:B.indptr[i+1]])
        offset = union.size
        n = x_index.size
        
        if x_index.size>0:
            data[counter:counter+offset][x_index] = np.array([A.data[A.indptr[i]:A.indptr[i+1]][xidx[k]] + B.data[B.indptr[i]:B.indptr[i+1]][xjdx[k]] for k  in range(n)], dtype=np.float64)

        if diffA.size>0:  
            data[counter:counter+offset][diffA[0,:]]=A.data[A.indptr[i]:A.indptr[i+1]][diffA[1,:]]
        if diffB.size>0:
            data[counter:counter+offset][diffB[0,:]]=B.data[B.indptr[i]:B.indptr[i+1]][diffB[1,:]]
    
        indices[counter:counter+offset] = union
        counter += offset
        indptr[i+1] = counter

    return DecCSR(indptr, indices[:counter], data[:counter], A.shape)
    
    #return A

@njit(DecCSRSpec()(DecCSRSpec(), DecCSRSpec()), nogil=True,  pipeline_class=DEC_Pipeline)
def ew_subt_sparse_sparse(A, B):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)
    n_rows=A.shape[0]
    n_cols = A.shape[1]
    indptr= np.zeros(n_rows+1, dtype=np.int64)
    size = A.size+B.size
    data = np.zeros(size, dtype=np.float64)
    indices = np.zeros(size, dtype=np.int64)
    counter = np.int64(0)

    for i in range(n_rows):
        union , _ , diffA, diffB, x_index, xidx, xjdx = list_union(A.indices[A.indptr[i]: A.indptr[i+1]], B.indices[B.indptr[i]:B.indptr[i+1]])
        offset = union.size
        n = x_index.size
        
        if x_index.size>0:
            data[counter:counter+offset][x_index] = np.array([A.data[A.indptr[i]:A.indptr[i+1]][xidx[k]] - B.data[B.indptr[i]:B.indptr[i+1]][xjdx[k]] for k  in range(n)], dtype=np.float64)
        if diffA.size>0:  
            data[counter:counter+offset][diffA[0,:]]=A.data[A.indptr[i]:A.indptr[i+1]][diffA[1,:]]
        if diffB.size>0:
            data[counter:counter+offset][diffB[0,:]]= -1 * B.data[B.indptr[i]:B.indptr[i+1]][diffB[1,:]]
    
        indices[counter:counter+offset] = union
        counter += offset
        indptr[i+1] = counter

    return DecCSR(indptr, indices[:counter], data[:counter], A.shape)
    

@njit(DecCSRSpec()(DecCSRSpec(), DecCSRSpec(), List(unicode_type)), nogil=True,  pipeline_class=DEC_Pipeline)
def elementwise_sparse_sparse(A, B, o):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)
    n_rows = A.shape[0]
    n_cols = A.shape[1]
    indptr= np.zeros(n_rows+1, dtype=np.int64)
    size = A.size+B.size
    data = np.zeros(size, dtype=np.float64)
    indices = np.zeros(size, dtype=np.int64)
    counter = np.int64(0)


    if o[0] =='*':
        for i in range(n_rows):
            x, xidx, xjdx = list_intersection(A.indices[A.indptr[i]: A.indptr[i+1]], B.indices[B.indptr[i]:B.indptr[i+1]])
            offset = xidx.size
            if offset>0:
                data[counter:counter+offset] = np.array([A.data[A.indptr[i]:A.indptr[i+1]][xidx[k]] * B.data[B.indptr[i]:B.indptr[i+1]][xjdx[k]] for k  in range(offset)], dtype=np.float64)
            indices[counter:counter+offset] = x
            counter += offset
            indptr[i+1] = counter
            
    else:
        for i in range(n_rows):
            union , _ , diffA, diffB, x_index, xidx, xjdx = list_union(A.indices[A.indptr[i]: A.indptr[i+1]], B.indices[B.indptr[i]:B.indptr[i+1]])
            offset = union.size
            n = x_index.size
                        
            if o[0] =='+':
                if x_index.size>0:
                    data[counter:counter+offset][x_index] = np.array([A.data[A.indptr[i]:A.indptr[i+1]][xidx[k]] + B.data[B.indptr[i]:B.indptr[i+1]][xjdx[k]] for k  in range(n)], dtype=np.float64)
                if diffA.size>0:  
                    data[counter:counter+offset][diffA[0,:]]=A.data[A.indptr[i]:A.indptr[i+1]][diffA[1,:]]
                if diffB.size>0:
                    data[counter:counter+offset][diffB[0,:]]=B.data[B.indptr[i]:B.indptr[i+1]][diffB[1,:]]
                #zzz = 0
            elif o[0] == '-':
                if x_index.size>0:
                    data[counter:counter+offset][x_index] = np.array([A.data[A.indptr[i]:A.indptr[i+1]][xidx[k]] - B.data[B.indptr[i]:B.indptr[i+1]][xjdx[k]] for k  in range(n)], dtype=np.float64)
                if diffA.size>0:  
                    data[counter:counter+offset][diffA[0,:]]=A.data[A.indptr[i]:A.indptr[i+1]][diffA[1,:]]
                if diffB.size>0:
                    data[counter:counter+offset][diffB[0,:]]= -1*B.data[B.indptr[i]:B.indptr[i+1]][diffB[1,:]]

            
            else:
                print('this operation is not supported')
                assert(False)

            indices[counter:counter+offset] = union
            counter += offset
            indptr[i+1] = counter

    return DecCSR(indptr, indices[:counter], data[:counter], A.shape)
    
    
@njit(DecCSRSpec()(DecCSRSpec(), float64[:,:], List(unicode_type)), nogil=True, pipeline_class=DEC_Pipeline)
def elementwise_sparse_dense(A, B, o):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)

    #if o[0]=='/':
    #    print('Division of zero by zero is assigned to 0, instead of nan!')

    n_rows, n_cols = A.shape
    data = np.zeros(A.size, dtype=np.float64)
    indices = np.zeros(A.size, dtype=np.int64)
    indptr = np.zeros(n_rows+1, dtype=np.int64)
    counter=0
    for i in range(n_rows):
        cols=A.indices[A.indptr[i]: A.indptr[i+1]]
        offset=len(cols)
        if o[0] == '*':
            data[counter:counter+offset] =  np.array([A.data[A.indptr[i]:A.indptr[i+1]][k] * B[i,cols[k]] for k  in range(len(cols))], dtype=np.float32)
        elif o[0] == '/':
            data[counter:counter+offset] =  np.array([A.data[A.indptr[i]:A.indptr[i+1]][k] / B[i,cols[k]] for k  in range(len(cols))], dtype=np.float32)
        else:
            print('This operation is not supported')
            assert(False)
        indices[counter:counter+offset] = cols
        counter += offset
        indptr[i+1] = counter
    
    return DecCSR(indptr, indices, data, A.shape)


@njit(DecCSRSpec()(DecCSRSpec(), float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def ew_sparse_dense_div(A, B):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)

    n_rows, n_cols = A.shape
    data = np.zeros(A.size, dtype=np.float64)
    indices = np.zeros(A.size, dtype=np.int64)
    indptr = np.zeros(n_rows+1, dtype=np.int64)
    counter=0
    for i in range(n_rows):
        cols=A.indices[A.indptr[i]: A.indptr[i+1]]
        offset=len(cols)
        data[counter:counter+offset] =  np.array([A.data[A.indptr[i]:A.indptr[i+1]][k] / B[i,cols[k]] for k  in range(len(cols))], dtype=np.float32)
        indices[counter:counter+offset] = cols
        counter += offset
        indptr[i+1] = counter
    
    return DecCSR(indptr, indices, data, A.shape)

@njit((DecCSRSpec(), float64[:,:], DecCSRSpec(), int32, int32), nogil=True, pipeline_class=DEC_Pipeline)
def ew_sparse_dense_div_parallel(A, B, C, tid, num_threads):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)

    n_rows, n_cols = A.shape
    counter=0

    if n_rows< num_threads:
        chunk = 1
    elif n_rows%num_threads > 0:
        chunk = (n_rows//num_threads) +1
    else:
        chunk = (n_rows//num_threads)
            
    for i in range(tid*chunk, min((tid+1)* chunk, n_rows)):
        cols=A.indices[A.indptr[i]: A.indptr[i+1]]
        offset=len(cols)
        C.data[counter:counter+offset][:] =  np.array([A.data[A.indptr[i]:A.indptr[i+1]][k] / B[i,cols[k]] for k  in range(len(cols))], dtype=np.float32)
        C.indices[counter:counter+offset] = cols
        counter += offset
        C.indptr[i+1] = counter


@njit(DecCSRSpec()(DecCSRSpec(), float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def ew_sparse_dense_mult(A, B):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)

    n_rows, n_cols = A.shape
    data = np.zeros(A.size, dtype=np.float64)
    indices = np.zeros(A.size, dtype=np.int64)
    indptr = np.zeros(n_rows+1, dtype=np.int64)
    counter=0
    for i in range(n_rows):
        cols=A.indices[A.indptr[i]: A.indptr[i+1]]
        offset=len(cols)
        data[counter:counter+offset] =  np.array([A.data[A.indptr[i]:A.indptr[i+1]][k] * B[i,cols[k]] for k  in range(len(cols))], dtype=np.float32)
        indices[counter:counter+offset] = cols
        counter += offset
        indptr[i+1] = counter
    
    return DecCSR(indptr, indices, data, A.shape)



@njit(DecCSRSpec()(float64[:,:], DecCSRSpec(), List(unicode_type)), nogil=True, pipeline_class=DEC_Pipeline)
def elementwise_dense_sparse(A, B, o):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)
    if o[0] != '*':
        print('This operation is not supported with dense-sparse matrices')
        assert(False)
    return elementwise_sparse_dense(B, A, o)


@njit(DecCSRSpec()(DecCSRSpec(), float64[:,:]), nogil=True, pipeline_class=DEC_Pipeline)
def multiply_sparse_dense(A,B):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)
    n_rows, n_cols = A.shape
    data = np.zeros(A.size, dtype=np.float64)
    indices = np.zeros(A.size, dtype=np.int64)
    indptr = np.zeros(n_rows+1, dtype=np.int64)
    counter = np.int64(0)
    for i in range(n_rows):
        cols=A.indices[A.indptr[i]: A.indptr[i+1]]
        offset=len(cols)
        data[counter:counter+offset] =  np.array([A.data[A.indptr[i]:A.indptr[i+1]][k] * B[i,cols[k]] for k  in range(len(cols))], dtype=np.float32)
        indices[counter:counter+offset] = cols
        counter += offset
        indptr[i+1] = counter
        
    return DecCSR(indptr, indices, data, A.shape)


@njit(DecCSRSpec()(float64[:,:], DecCSRSpec()), nogil=True, pipeline_class=DEC_Pipeline)
def multiply_dense_sparse(A, B):
    if (A.shape[0]!=B.shape[0]) or (A.shape[1] != B.shape[1]):
        print('Matrix dimensions do not match! Make sure you are multiplying (m x n) and (m x n) matrices.')
        assert(False)
    return multiply_sparse_dense(B, A)

@generated_jit(nopython=True, nogil=True)
def dot(A, B):
    if A is DecCSRSpec():
        if B is DecCSRSpec():
            return lambda A,B: dot_sparse_sparse(A, B)
        elif B is DecCSCSpec():
            return lambda A,B: dot_sparse_sparse(A, csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: dot_sparse_dense(A,B)
        else:
            print('input types not supported')
            assert(False)
    elif A is DecCSCSpec():
        if B is DecCSRSpec():
            return lambda A,B: dot_sparse_sparse(csc_to_csr(A), B)
        elif B is DecCSCSpec():
            return lambda A,B: dot_sparse_sparse(csc_to_csr(A), csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: dot_sparse_dense(csc_to_csr(A), B)
        else:
            print('input types not supported')
            assert(False)
    elif isinstance(A, np.ndarray):
       	if B is DecCSRSpec():
            return lambda A,B: dot_dense_sparse(A, csr_to_csc(B))
        elif B is DecCSCSpec():
            return lambda A,B: dot_dense_sparse(A, B)
        elif isinstance(B, np.ndarray):
            return lambda A,B: dec_dot(A, B)
        else:
            print('input types not supported')
            assert(False)
    else:
        print('input types not supported')
        assert(False)


# Hadamard product
@generated_jit(nopython=True, nogil=True)
def multiply(A, B):
    if A is DecCSRSpec():
        if B is DecCSRSpec():
            return lambda A,B: multiply_sparse_sparse(A, B)
        elif B is DecCSCSpec():
            return lambda A,B: multiply_sparse_sparse(A, csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: multiply_sparse_dense(A,B)
        else:
            print('input types not supported')
            assert(False)
    elif A is DecCSCSpec():
        if B is DecCSRSpec():
            return lambda A,B: multiply_sparse_sparse(csc_to_csr(A), B)
        elif B is DecCSCSpec():
            return lambda A,B: multiply_sparse_sparse(csc_to_csr(A), csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: multiply_sparse_dense(csc_to_csr(A), B)
        else:
            print('input types not supported')
            assert(False)
    elif isinstance(A, np.ndarray):
       	if B is DecCSRSpec():
            return lambda A,B: multiply_dense_sparse(A, B)
        elif B is DecCSCSpec():
            return lambda A,B: multiply_dense_sparse(A, csc_to_csr(B))
        elif isinstance(B, np.ndarray):
            return lambda A,B: dec_mult(A, B)
        else:
            print('input types not supported')
            assert(False)
    else:
        print('input types not supported')
        assert(False)


@njit((float64[:], int64[:], int64[:], int64[:],int64[:], int64[:], int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
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

@njit(int64(int64[:]), nogil=True, pipeline_class=DEC_Pipeline)
def dec_int64_max(f):
    ret = np.int64(-2147483648)
    for i in f:
        if i > ret:
            ret = i
    return ret

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

@njit(int32(int32[:]), nogil=True, pipeline_class=DEC_Pipeline)
def PyDECADES_fetchAdd(addr):
    tmp = addr[0]
    addr[0] += 1
    return tmp

@njit(int32(int32[:], int32), nogil=True, pipeline_class=DEC_Pipeline)
def PyDECADES_atomicAdd(addr, val):
    tmp = addr[0]
    addr[0] += val
    return tmp
