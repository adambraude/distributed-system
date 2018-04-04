"""
    Query Optimizer
    Takes a query of the following format:
    [(v0, (m00, m01)), (v1, (m10, m11), ..., (vn, (mn0, mn1))]
    which is duplicated various times.
    Returns a set of queries that should be made to get the result
"""
from copy import copy
import heapq
import random

subq1 = [ (0, (0, 1)), (1, (3, 4)), (2, (1, 3)) ] # vector 0 is on machines 1 and 2, etc...
subq2 = [ (3, (0, 1)), (4, (3, 4)), (5, (1, 3)) ]
subq3 = [ (3, (0, 1)), (4, (3, 4)), (5, (1, 3)) ]
subq4 = [ (3, (0, 1)), (4, (3, 4)), (5, (1, 3)) ]

# TODO more subqs

# count up all the machines to visit
query = [subq1, subq2, subq3, subq4]
needed_machines = set({})

"""
for t in query:
    needed_machines.add(t[1][0])
    needed_machines.add(t[1][1])
"""

num_machines = 10
EXTRA_LOAD = 1

# the default matrix: weight between every vertex is the same by default
# TODO: run a script to determine congestion between each node, save that
# number in a file, and parse the adjacency matrix from it
adjacency_matrix = [[1 if i != j else 0 for i in range(num_machines)] for j in range(num_machines)]

# TODO could also return the load on all the slaves as a point of comparison

#machines = [i for i in range(num_machines)] # XXX: a test example
# You don't need to visit every machine given in the query. First, optimize on the first machine
# given in the tuple, then use the higher-numbered one, and alternate.
def iter_mst(query):
    trees = []
    flipped = False
    for subq in query:
        vertices = set({})
        vert_vec_map = {}
        for tup in subq:
            mt = sorted(tup[1])
            if flipped:
                temp = mt[0]
                mt[0] = mt[1]
                mt[1] = temp
            vertices.add(mt[0])
            if mt[0] not in vert_vec_map: vert_vec_map[mt[0]] = [tup[0]]
            else: vert_vec_map[mt[0]].append(tup[0])
        # designate smallest-numbered node we'll visit as the root, or random
        root = sorted(list(vertices))[0]  #random.sample(vertices, 1)[0]) #XXX: alternatively randomize
        tree = _mst_prim(vertices, lambda x, y: adjacency_matrix[x][y], root, vert_vec_map)
        # update adjacency matrix
        _recur_update_am(root, tree)
        #print("Root: {0}, children = {1}".format(root, tree[root].children))
        flipped = not flipped
        trees.append((root, tree))
    return trees

def _recur_update_am(root, verts):
    for child in verts[root].children:
        adjacency_matrix[root][child] += EXTRA_LOAD
        adjacency_matrix[child][root] += EXTRA_LOAD
        _recur_update_am(child, verts)

def _adj(verts, v):
    t = set({})
    for w in range(num_machines): # check every vertex value
        if v != w and w in verts: t.add(w)
    return t

class Vertex(object):
    def __init__(self, key, par=None):
        self.key = key
        self.par = par
        self.children = []
        self.vectors = [] # TODO: should these be sets or lists

    def __hash__(self):
        return hash(key) ^ hash(par)

def _mst_prim(vertices, w, root, vector_map):
    """
    Perform Prim's algorithm returning the minimum spanning tree of
    the the given vertices with weights determined by the given function,
    from the given root node.
    :param vertices: the set of vertices we're running the algo on
    :w: the weight function
    :param root: the root node (can be chosen arbitrarily)
    """
    prim_verts = {}
    for v in vertices:
        prim_verts[v] = Vertex(float("inf"))
    prim_verts[root].key = 0
    queue = list(copy(vertices))
    heapq.heapify(queue) # TODO: replace with Fibonacci heap, for asymptotically optimal performance
    while queue != []:
        u = heapq.heappop(queue)
        for v in _adj(prim_verts, u):
            wgt = w(u, v)
            #print("Comparing {0} and {1}, w = {2} vs {3}, in {4}".format(u,v,wgt,prim_verts[v].key,queue))
            if wgt < prim_verts[v].key and v in queue:
                #print("Adding to the tree!\n")
                prim_verts[v].par = u
                prim_verts[u].children.append(v)
                prim_verts[v].key = wgt
    # TODO convert children/vectors into lists, if they're easier to use in C.
    for vert in prim_verts:
        prim_verts[vert].vectors = vector_map[vert]
        #print("{0} has {1}".format(vert,vector_map[vert]))
    #print(prim_verts[root].par)
    return prim_verts

#iter_mst(query)
