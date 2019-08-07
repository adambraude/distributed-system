#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "read_vec.h"

/**
 * Reads up to MAX_VECTOR_LEN (defined in types.h) lines of a vector written
 * at the file in the given path. If no such file exists, returns NULL.
 */
vec_t *read_vector(char *path)
{

    u_int vector_len = 0;

    u_int64_t vector_val[MAX_VECTOR_LEN];
    memset(vector_val, 0, MAX_VECTOR_LEN * sizeof(u_int64_t));

    char buf[64];
    memset(buf, 0, 64);

    // XXX: could have flag variable: can do this if reading for query.
    // XXX: trying to get it to work with the BitmapEngine this way
    vector_val[vector_len++] = 0;

    /* Read file */
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        puts("Couldn't open vector!");
        return NULL;
    }
    else {
        while (vector_len < MAX_VECTOR_LEN && fgets(buf, 64, fp) != NULL) {
            vector_val[vector_len++] = (u_int64_t) strtoul(buf, NULL, 16);
        }
        fclose(fp);
    }

    /* Construct return vector */
    vec_t *return_vector = (vec_t *) malloc(sizeof(vec_t));
    memcpy(return_vector->vector, vector_val, vector_len * sizeof(u_int64_t));
    return_vector->vector_length = vector_len;

    return return_vector;
}

void print_vector(vec_t *vec)
{
    for (int i = 0; i < vec->vector_length; ++i) {
        printf("0x%lx\n", vec->vector[i]);
    }
}
