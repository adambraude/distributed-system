#ifndef MASTER_RQ_H_
#define MASTER_RQ_H_

#include "../ipc/messages.h"

int init_btree_range_query(range_query_contents);
int hande_query(char[]);
int handle_point_query(char[]);
int init_range_query(unsigned int *, int, char *, int);
int kill_slave(int);
int kill_random_slave(int);

#ifdef __APPLE__
    #include </usr/include/python2.7/Python.h>
#elif defined __linux__
    //#include </usr/include/python2.7/Python.h>
#endif

#endif
