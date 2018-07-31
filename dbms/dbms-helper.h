#ifndef DBMS_HELPER_H
#define DBMS_HELPER_H

#include "../types/types.h"

/* XXX: executables should be in the same directory? */
static char MASTER_EXECUTABLE[] = "./master";
static char REPLICATION_FACTOR[] = "3";

int slave_intro(int, char *);
int put_vector(int, vec_id_t, vec_t *);
int query(int, char *);
int point_query(int, char *);
int range_query(int, char *);
int master_exit(int);
pid_t spawn_master(pid_t);

#endif /* DBMS_HELPER_H */
