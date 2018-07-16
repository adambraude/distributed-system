#ifndef DBMS_H
#define DBMS_H

#include "../types/types.h"

/* XXX: executables should be in the same directory? */
static char *MASTER_EXECUTABLE = "./master";
static char *REPLICATION_FACTOR = "3";

int put_vector(int, vec_id_t, vec_t*);
int point_query(int, char *);
int range_query(int, char *);

enum {BASIC_TEST, POLITICAL_DATA_TEST, WORLD_TEST, TPCORG_C_TEST};
/* TPC C benchmarking test named TPCORG to avoid confusion with
 * two-phase commit */

enum {RING_CH, JUMP_CH, STATIC_PART};

int master_exit(int);

#endif /* DBMS_H */
