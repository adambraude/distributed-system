#include <sys/types.h>
#ifndef READ_VEC_H
#define READ_VEC_H
#include "../types/types.h"
vec_t *read_vector(char *);
void print_vector(u_int64_t *vec, u_int vec_len);
#endif /* READ_VEC_H */
