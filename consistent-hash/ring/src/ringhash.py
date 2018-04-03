import hashlib
def ringhash(key):
    """
    Assumes the key is an integer. returns a key modulo 2**64
    """
    digest_str = hashlib.md5(str(key).encode('latin-1')).hexdigest()
    return int(digest_str, 16) % 2**64
