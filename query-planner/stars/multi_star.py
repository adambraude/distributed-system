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
Node(machine_id=coord, vectors=vectors)
    Children:
    Node(machine_id=subcoord0, vectors=vectors)
        Children:
            Node(machine_id=mach0, vectors=vectors)
            Node(machine_id=mach1, vectors=vectors)
            ...
            Node(machine_id=machk, vectors=vectors)
    Node(machine_id=subcoord1, vectors=vectors)
        Children:
            Node(machine_id=mach0, vectors=vectors)
            Node(machine_id=mach1, vectors=vectors)
            ...
            Node(machine_id=machk, vectors=vectors)
    ...
```
where coord and sub_coord are both machine ids.
"""

num_machines = 0

import time
current_milli_time = lambda : time.time() * 1000

def planner(query, machine_count=16):
    global num_machines
    num_machines = machine_count

    assert(num_machines > 0)

    # Sort the query so that we work from largest range to smallest.
    query.sort(key=len, reverse=True)

    load = [0 for i in range(num_machines)]

    # List to collest plan for each range.
    root = Node()

    t = current_milli_time()
    for range_query in query:
        range_plan, load = range_planner(range_query, load)
        root.children.append(range_plan)
    # print('Ranges took {0} ms'.format(current_milli_time() - t))

    root.children.sort(key=lambda m: load[m.machine_id])

    root.machine_id = root.children[0].machine_id

    return root

def range_planner(query, load):
    # Mapping from machine_id to vectors available on that machine
    m_to_vs = dict()
    for v, ms in query:
        for m in ms:
            vs = m_to_vs.get(m, set())
            vs.add(v)
            m_to_vs[m] = vs

    # List of machines sorted based on their load factor
    ms = list(set([m for v, m_tup in query for m in m_tup]))
    ms.sort(key=lambda m: (load[m], -len(m_to_vs[m])))

    # Set of vectors
    vs = set(v for v, ms in query)

    range_plan = Node()

    while vs and ms:
        m = ms.pop()
        provided_vs = m_to_vs.get(m, {}).intersection(vs)
        if provided_vs:
            leaf = Node(machine_id=m, vectors=provided_vs)
            range_plan.children.append(leaf)
            vs = vs - provided_vs

    # If this fails, we have a bug that made us run out of machines without satisfying the vectors.
    assert(not vs)

    # range_plan.children.sort(key=lambda m: load[m.machine_id])

    subcoord = range_plan.children.pop(0)
    range_plan.machine_id = subcoord.machine_id
    range_plan.vectors = subcoord

    return range_plan, load

class Node(object):
    def __init__(self, machine_id=None, children=[], vectors=[]):
        self.machine_id = machine_id
        self.children = children
        self.vectors = vectors

    def __hash__(self):
        return hash(machine_id)

if __name__ == '__main__':
    pass
