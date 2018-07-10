/**
 * Types to be used by every module.
 */
#ifndef TYPES_H
#define TYPES_H
#define MAX_VECTOR_LEN 128
#include <sys/types.h>
#include <stdbool.h>

typedef struct vec_t {
    u_int64_t vector[MAX_VECTOR_LEN];
    u_int vector_length;
} vec_t;

typedef struct slave_vector {
    unsigned int id;
    struct slave_vector *next;
} slave_vector;

typedef struct slave {
    unsigned int id;
    char *address;
    bool is_alive;
    slave_vector *primary_vector_head; /* vectors that were assigned to this slave */
    slave_vector *primary_vector_tail;
} slave;

typedef u_int vec_id_t;
#endif /* TYPES_H */
