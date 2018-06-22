#ifndef MASTER_RQ_H_
#define MASTER_RQ_H_

#include "../ipc/messages.h"

int init_btree_range_query(range_query_contents);
int hande_query(char query_string[]);
int handle_point_query(char query_string[]);
int init_range_query(unsigned int *array_index, int num_ranges, char *ops, int array_len);
int kill_random_slave(int num_slaves);

#ifdef __APPLE__
    #include </usr/include/python2.7/Python.h>
#elif defined __linux__
    //#include </usr/include/python2.7/Python.h>
#endif

#endif
