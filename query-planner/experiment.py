from hashlib import md5
from random import randint
import sys
import timeit
import time
# TODO Jahrme query planners
sys.path.insert(0,'./iter-mst/')
import mst_planner
# create experiment data

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
# emulate consistent hashing, sort of

_vec_mn_hfn = lambda k: int(md5(str(k).encode()).hexdigest(), 16) % num_machines
current_milli_time = lambda : time.time() * 1000

for ids in rand_vec_ids:
    subq = [(i, (_vec_mn_hfn(i), _vec_mn_hfn(i + 1))) for i in ids]
    querys.append(subq)

exp_fns = [mst_planner.iter_mst] # TODO add Jahrme function

# cache warmup (don't want to treat early funtion unfairly)
for f in exp_fns: f(querys)

for f in exp_fns:
    t = current_milli_time()
    f(querys)
    print('{0} completed in {1} ms'.format(f.__name__, current_milli_time() - t))
