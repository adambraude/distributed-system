def vecstrings(l):
    """
    Returns a list of n-bit hex strings from the given list of integers
    """
    vec = ''.join([str(a) for a in l])
    #print(vec)
    n = 63 # split the string into 63-bit literal words starting with 0
    # TODO: move this into a top-level module so that all test functions can use this
    # instead of having to rewrite it
    return [hex(int(vec[i:i + n], 2)).replace('0x', '0x0') for i in range(0, len(vec), n)]
