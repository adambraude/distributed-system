/**
 * Types to be used by every module.
 */
#ifndef TYPES_H
#define TYPES_H
#define MAX_VECTOR_LEN 128
typedef struct vec_t {
    unsigned long long vector[MAX_VECTOR_LEN];
    unsigned int vector_length;
} vec_t;
typedef unsigned int vec_id_t;
#endif /* TYPES_H */
