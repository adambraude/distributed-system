/**
 * Types to be used by every module.
 */
#ifndef TYPES_H
#define TYPES_H
#define MAX_VECTOR_LEN 128
#include <sys/types.h>

typedef struct vec_t {
    u_int64_t vector[MAX_VECTOR_LEN];
    u_int vector_length;
} vec_t;
typedef u_int vec_id_t;
#endif /* TYPES_H */
