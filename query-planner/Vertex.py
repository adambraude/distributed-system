class Vertex(object):
    def __init__(self, key=None, par=None, children=[], vectors=[]):
        self.key = key
        self.par = par
        self.children = children
        self.vectors = vectors
        # TODO: should `vectors` be a set or a list?

    def __hash__(self):
        return hash(key) ^ hash(par)
