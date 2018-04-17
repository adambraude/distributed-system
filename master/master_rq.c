/**
 * Master Remote Query Handling
 */

#include "master_rq.h"
#include "master.h"
#include "../rpc/gen/slave.h"
#include "slavelist.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * Executes the given (formatted) range query, returning 0 on success,
 * or the machine that failed in the query process
 */
int
init_range_query(unsigned int *range_array, int num_ranges,
    char *ops, int array_len)
{
    rq_range_root_args *root = (rq_range_root_args *) malloc(sizeof(rq_range_root_args));
    root->range_array.range_array_val = range_array;
    root->range_array.range_array_len = array_len;
    root->num_ranges = num_ranges;
    root->ops.ops_val = ops;
    root->ops.ops_len = num_ranges - 1;
    char *coordinator = SLAVE_ADDR[0]; // arbitrary for now TODO: pick at random
    CLIENT *cl = clnt_create(coordinator, REMOTE_QUERY_ROOT,
        REMOTE_QUERY_ROOT_V1, "tcp");
    if (cl == NULL) {
        printf("Error: could not connect to coordinator %s.\n", coordinator);
        return 1;
    }
    printf("Calling RPC\n");
    query_result *res = rq_range_root_1(*root, cl);
    if (res == NULL) {
        printf("No response from coordinator.\n");
        return 1;
    }
    int i;
    if (res->exit_code == EXIT_SUCCESS) {
        for (i = 1; i < res->vector.vector_len - 1; i++) {
            printf("%llx\n",res->vector.vector_val[i]);
        }
        printf("%llx\n", res->vector.vector_val[i]);
    }
    else {
        printf("Query failed, reason: %s\n", res->error_message);
    }
    free(root);
    //if (res != NULL) free(res);
    return EXIT_SUCCESS;
}
