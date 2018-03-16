/**
 * Master Remote Query Handling
 */

#include "master_rq.h"
#include "../rpc/gen/slave.h"
#include "slavelist.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int init_range_query(unsigned int *range_array, int num_ranges, char *ops, int array_len)
{
    rq_range_root_args *root = (rq_range_root_args *) malloc(sizeof(rq_range_root_args));
    root->range_array.range_array_val = range_array;
    root->range_array.range_array_len = array_len;
    root->num_ranges = num_ranges;
    root->ops.ops_val = ops;
    root->ops.ops_len = num_ranges - 1;
    char *coordinator = SLAVE_ADDR[0]; // arbitrary for now
    CLIENT *cl = clnt_create(coordinator, REMOTE_QUERY_ROOT,
        REMOTE_QUERY_ROOT_V1, "tcp");
    if (cl == NULL) {
        printf("Error: could not connect to coordinator %s.\n", coordinator);
    }
    query_result *res = rq_range_root_1(*root, cl);
    printf("Result->vector.vector_val[0] is: %llu\n", res->vector.vector_val[0]);
    free(root);
    return EXIT_SUCCESS;
}
