#ifndef DBMS_H
#define DBMS_H

static char *MASTER_EXECUTABLE = "./master";
static char *REPLICATION_FACTOR = "3";

int put_vector(int queue_id, int vec_id, unsigned long long vec);
int create_master();

#endif /* DBMS_H */
