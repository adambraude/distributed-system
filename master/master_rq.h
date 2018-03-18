#ifndef MASTER_RQ_H_
#define MASTER_RQ_H_

int hande_query(char query_string[]);
int handle_point_query(char query_string[]);
int init_range_query(unsigned int *array_index, int num_ranges, char *ops, int array_len);

#endif /* MASTER_RQ_H_ */
