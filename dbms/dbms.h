#ifndef DBMS_H
#define DBMS_H

#define DBMS_DEBUG false

enum {RING_CH, JUMP_CH, STATIC_PART};

int experiment_load_vectors(int);
int experiment_fault_tolerance(int);
int experiment_introduce_slaves(int);

#endif /* DBMS_H */
