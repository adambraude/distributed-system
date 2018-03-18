/* master_tpc_vector.c functions */
#ifndef TPC_MASTER_H
#define TPC_MASTER_H

#include "../types/types.h"
int commit_vector(vec_id_t , vec_t , char **, int );
void *get_commit_resp(void*);
void *push_vector(void*);

#endif /* TPC_MASTER_H */
