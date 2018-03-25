#ifndef DBMS_H
#define DBMS_H

#include "../types/types.h"

/* XXX: executables should be in the same directory? */
static char *MASTER_EXECUTABLE = "./master";
static char *REPLICATION_FACTOR = "3";

int put_vector(int, vec_id_t, vec_t*);
int point_query(int, char *);
int range_query(int, char *);

#define BASIC_TEST 0
#define POLITICAL_DATA_TEST 1

#define RING_CH 0
#define JUMP_CH 1
#define STATIC_PART 2

#define PARITION_TYPE

#endif /* DBMS_H */
