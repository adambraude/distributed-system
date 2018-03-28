from math import inf

def coordinator(load, vertices):
    """
    Elect a coordinator from the given list of vertices.

    :param load: load factor list
    :param vertices: list of indicies of vertices
    :return load: the new load factor list
    :return vertex: index of optimal coordinator
    """

    minimum_load = inf
    best_vertex = None

    for vertex in vertices:
        if load[vertex] < minimum_load:
            minimum_load = load[vertex]
            best_vertex = vertex

    for vertex in vertices:
        if vertex == best_vertex:
            load[vertex] += len(vertices)
        else:
            load[vertex] += 1

    return load, best_vertex
