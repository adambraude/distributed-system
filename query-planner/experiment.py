from hashlib import md5
from random import randint
import sys
import timeit
import time

sys.path.insert(0, './iter-mst/')
import mst_planner

sys.path.insert(0, './stars/')
import path_star, uni_star, multi_star

exp_fns = [
    path_star.planner,
    uni_star.planner,
    multi_star.planner,
    mst_planner.iter_mst,
]

"""
    Experiment 1: generate some random queries and print how long each
    function takes to satisfy them.
"""
num_machines = 10
num_queries = 10
num_vectors = 1000000000
query_len = 100
num_experiments = 1000
#vec_ids = [i for i in range(num_vectors)]
global querys

querys = []
rand_vec_ids = [[randint(0, num_vectors - 1) for i in range(query_len)] for k in range(num_experiments)]

_vec_mn_hfn = lambda k: int(md5(str(k).encode()).hexdigest(), 16) % num_machines
current_milli_time = lambda : time.time() * 1000

for ids in rand_vec_ids:
    subq = [(i, (_vec_mn_hfn(i), _vec_mn_hfn(i + 1))) for i in ids]
    querys.append(subq)

# cache warmup (don't want to treat early funtion unfairly)
for f in exp_fns: f(querys)

for f in exp_fns:
    t = current_milli_time()
    f(querys)
    print('{0} completed in {1} ms'.format(f.__name__, current_milli_time() - t))
