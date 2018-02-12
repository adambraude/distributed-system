#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "tree_map.h"

uint64_t hash(int key)
{
    const unsigned char ibuf[256];
    sprintf((char*)ibuf, "%d", key);
    unsigned char obuf[256];
    SHA1(ibuf, strlen((const char *)ibuf), obuf);
    int i;
    unsigned long long digest = 0;
    int s = strlen((const char *)obuf);
    for (i = 0; i < s; i++) {
        digest += obuf[i];
    }
    return digest;
}
