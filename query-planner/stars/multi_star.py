"""
A range is a list of tuples where the first entry is a vector and the second is a tuple of the machines upon which the vector is available. A query is a list of ranges.

Thus, range r_i is of the form:
```
[(v0, (m00, m01)),
 (v1, (m10, m11)),
 ...,
 (vn, (mn0, mn1))]
```
Further, the following is an example of a query:
```
[r_1, r_2, ..., r_n]
```

The following implementation assumes that both ranges and queries are held in lists. In contrast, we assume that it is a tuple that holds a vector and the associated tuple of machines upon which it resides.

The output will be of the following form:
```
(coord, [vectors], [
    (sub_coord, [vectors], [
        (machine, [vectors]),
        (machine, [vectors]),
        ...
        (machine, [vectors]),
    ])
])
```
where coord and sub_coord are both machine ids.
"""

from collections import deque

import sys
sys.path.insert(0, '..')
from Vertex import *


num_machines = 0

def planner(query, machine_count=16):
    global num_machines
    num_machines = machine_count

    assert(num_machines > 0)

    # Sort the query so that we work from largest range to smallest.
    query.sort(key=len, reverse=True)

    # Load matrix (essentially an adjacency matrix).
    load = [[
        0 if i == j else 1
        for i in range(num_machines)]
        for j in range(num_machines)]

    # List to collest plan for each range.
    range_plans = []

    for range_query in query:
        range_plan, load = range_planner(range_query, load)
        range_plans.append(range_plan)



    return

def range_planner(query, load):
    # Mapping from machine to available vectors
    m_to_vs = dict()

    for v, ms in query:
        for m in ms:
            vs = m_to_vs.get(m, set())
            vs.add(v)
            m_to_vs[m] = vs


    # List of machines sorted based on their load factor
    ms = list(set([m for v, m_tup in query for m in m_tup]))
    ms.sort(key=lambda m: (weight(m, load), -len(m_to_vs[m])))

    # Set of vectors
    vs = set(v for v, ms in query)

    plan = deque([])

    while vs and ms:
        m = ms.pop()
        provided_vs = m_to_vs.get(m, {}).intersection(vs)
        if provided_vs:
            plan.appendleft((m, provided_vs))
            vs = vs - provided_vs

    if vs:
        print("ERROR: Exhausted machines without satisfying vectors.")

    return plan, load

def weight(machine, load):
    global num_machines
    total = sum(load[machine][i] + load[i][machine]
        for i in range(num_machines))
    return total

if __name__ == '__main__':
    print("This is the multi-star query planner.")
