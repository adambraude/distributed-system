#ifndef READ_VEC_H
#define READ_VEC_H
#include "../types/types.h"
vec_t *read_vector(char *);
#define INITIAL_VECTOR_LEN 4 // XXX: should be an empirically determined average vector length
#endif /* READ_VEC_H */
