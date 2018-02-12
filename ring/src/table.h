#ifndef TABLE_H
#define TABLE_H

#include <openssl/sha.h>
#include <stdint.h>

typedef struct cache {
    int cache_id;
    char *cache_name;
    int replication_factor;
} cache;

/*
 * Computes a hash value of the given integer.
 */
uint64_t hash(int);

#endif
