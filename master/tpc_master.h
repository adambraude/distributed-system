/* master_tpc_vector.c functions */
#include "../types/types.h"
#include "master.h"
int commit_vector(vec_id_t, vec_t, slave*[], int);
void *get_commit_resp(void*);
void *push_vector(void*);
