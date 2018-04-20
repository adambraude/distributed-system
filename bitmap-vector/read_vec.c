#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
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
    if (fp == NULL)
        return NULL;
    else {
        u_int64_t vector_val[128];
        memset(vector_val, 0, 128);
        u_int vector_len = 0;
        vec_t *return_vector = (vec_t *) malloc(sizeof(vec_t));
        char buf[32];
        // XXX: could have flag variable: can do this if reading for query.
        vector_val[vector_len++] = 0; // XXX: trying to get it to work with the BitmapEngine this way
        while (vector_len < 128 && fgets(buf, 32, fp) != NULL)
            vector_val[vector_len++] = (u_int64_t) strtoul(buf, NULL, 16);
        fclose(fp);
        memcpy(return_vector->vector, vector_val,
            sizeof(u_int64_t) * vector_len);
        return_vector->vector_length = vector_len;
        return return_vector;
    }
}
