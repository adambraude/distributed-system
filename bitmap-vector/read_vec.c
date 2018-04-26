#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "read_vec.h"

/**
 * Reads up to max_vector_len (defined in master.c) lines of a vector written
 * at the file in the given path.
 * If no such file exists, returns NULL.
 */
vec_t *read_vector(char *path)
{
    FILE *fp = fopen(path, "r");
    puts(path);
    if (fp == NULL) {
        puts("vector not found!");
        return NULL;
    }
    else {
        u_int64_t vector_val[MAX_VECTOR_LEN];
        memset(vector_val, 0, MAX_VECTOR_LEN * sizeof(u_int64_t));
        u_int vector_len = 0;
        vec_t *return_vector = (vec_t *) malloc(sizeof(vec_t));
        char buf[64];
        memset(buf, 0, 64);
        // XXX: could have flag variable: can do this if reading for query.
        // XXX: trying to get it to work with the BitmapEngine this way
        vector_val[vector_len++] = 0;
        while (vector_len < MAX_VECTOR_LEN && fgets(buf, 64, fp) != NULL) {
            vector_val[vector_len++] = (u_int64_t) strtoul(buf, NULL, 16);
        }
        fclose(fp);
        memcpy(return_vector->vector, vector_val,
            sizeof(u_int64_t) * vector_len);
        return_vector->vector_length = vector_len;
        int i;
        return return_vector;
    }
}

void print_vector(u_int64_t *vec, u_int vec_len)
{
    int i;
    for (i = 0; i < vec_len; i++) {
        printf("%llx\n", vec[i]);
    }
}
