/**
 * Master Remote Query Handling
 */

#include "master_rq.h"
#include "master.h"
#include "../rpc/gen/slave.h"
#include "../util/ds_util.h"
#include "../../bitmap-engine/BitmapEngine/src/seg-util/SegUtil.h"
#include "../../bitmap-engine/BitmapEngine/src/wah/WAHQuery.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>

char **slave_addresses;

#define TIME_TO_VOTE 1 // XXX make this shared between master/slave (general timeout)

query_result *rq_range_root(rq_range_root_args *query);

typedef struct coord_thread_args {
    rq_pipe_args *args;
    int query_result_index;
} coord_thread_args;

void *init_coordinator_thread(void *coord_args);
query_result **results;

int kill_random_slave(int num_slaves) {
    srand(time(NULL));
    int death_index = rand() % num_slaves;
    printf("Killing slave %d\n", death_index);
    CLIENT *cl = clnt_create(slave_addresses[death_index],
        KILL_SLAVE, KILL_SLAVE_V1, "tcp");
    if (cl == NULL) return -1;
    int *res = kill_order_1(0, cl);
    clnt_destroy(cl);
    return 0;
}


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

    query_result *res = rq_range_root(root);

    free(root);
    free(range_array);
    return EXIT_SUCCESS;
}

query_result *rq_range_root(rq_range_root_args *query)
{
    int num_threads = query->num_ranges;
    unsigned int *range_array = query->range_array.range_array_val;

    results = (query_result **) malloc(sizeof(query_result *) * num_threads);
    int i, array_index = 0;
    for (i = 0; i < num_threads; i++) {
        int num_nodes = range_array[array_index++];
        rq_pipe_args *pipe_args = (rq_pipe_args *)
            malloc(sizeof(rq_pipe_args) * num_nodes);
        rq_pipe_args *head_args = pipe_args;
        int j;
        for (j = 0; j < num_nodes; j++) {
            pipe_args->machine_no = range_array[array_index++];
            pipe_args->vec_id = range_array[array_index++];
            pipe_args->op = '|';
            if (j < num_nodes - 1) {
                rq_pipe_args *next_args = (rq_pipe_args *)
                    malloc(sizeof(rq_pipe_args) * num_nodes);
                pipe_args->next = next_args;
                pipe_args = next_args;
            }
            else {
                pipe_args->next = NULL;
            }
        }

        /* allocate the appropriate number of args */
        coord_thread_args *thread_args = (coord_thread_args *)
            malloc(sizeof(coord_thread_args));

        thread_args->query_result_index = i;
        thread_args->args = head_args;

        init_coordinator_thread((void*) thread_args);
    }

    query_result *res = (query_result *) malloc(sizeof(query_result));

    /*
     * Conclude the query. Join each contributing thread,
     * and in doing so, report error if there is one, or report largest vector size
     */
    u_int result_vector_len = 0, largest_vector_len = 0;
    for (i = 0; i < num_threads; i++) {
        /* assuming a single point of failure, report on the failed slave */
        if (results[i]->exit_code != EXIT_SUCCESS) {
            return results[i];
        }
        largest_vector_len = max(largest_vector_len,
            results[i]->vector.vector_len);
    }

    /* all results found! */
    res->exit_code = EXIT_SUCCESS;
    res->error_message = "";
    if (num_threads == 1) { /* there are no vectors to AND together */
        memcpy(&res->vector, &results[0]->vector, sizeof(results[0]->vector));
        free(results);
        return res;
    }

    u_int64_t *result_vector = (u_int64_t *)
        malloc(sizeof(u_int64_t) * largest_vector_len);
    memset(result_vector, 0, largest_vector_len * sizeof(u_int64_t));
    /* AND the first 2 vectors together */
    result_vector_len = AND_WAH(result_vector,
        results[0]->vector.vector_val, results[0]->vector.vector_len,
        results[1]->vector.vector_val, results[1]->vector.vector_len) + 1;

    /* AND the subsequent vectors together */
    for (i = 2; i < num_threads; i++) {
        u_int64_t *new_result_vector = (u_int64_t *)
            malloc(sizeof(u_int64_t) * largest_vector_len);
        memset(new_result_vector, 0, sizeof(u_int64_t) * largest_vector_len);
        result_vector_len = AND_WAH(new_result_vector, result_vector,
            result_vector_len, results[i]->vector.vector_val,
            results[i]->vector.vector_len) + 1;
        free(result_vector);
        result_vector = new_result_vector;
    }

    /* deallocate the results */
    free(results);
    u_int64_t arr[result_vector_len];
    memset(arr, 0, result_vector_len * sizeof(u_int64_t));
    res->vector.vector_val = arr; // XXX is this actually necessary
    memcpy(res->vector.vector_val, result_vector,
        result_vector_len * sizeof(u_int64_t));
    free(result_vector);
    res->vector.vector_len = result_vector_len;
    return res;
}

/**
 * Local helper function, returning a no-response message from the machine
 * of the given name.
 */
char *machine_failure_msg(char *machine_name)
{
    char *error_message = (char *) malloc(sizeof(char) * 64);
    snprintf(error_message, 64,
        "Error: No response from machine %s\n", machine_name);
    return error_message;
}

void *init_coordinator_thread(void *coord_args) {
    coord_thread_args *args = (coord_thread_args *) coord_args;

    CLIENT *clnt = clnt_create(slave_addresses[args->args->machine_no],
        REMOTE_QUERY_PIPE, REMOTE_QUERY_PIPE_V1, "tcp");

    if (clnt == NULL) {
        clnt_pcreateerror(slave_addresses[args->args->machine_no]);
    }
    /* give the request a time-to-live */
    struct timeval tv;
    tv.tv_sec = TIME_TO_VOTE;
    tv.tv_usec = 0;
    clnt_control(clnt, CLSET_TIMEOUT, &tv);
    query_result *res = rq_pipe_1(*(args->args), clnt);
    if (res == NULL) {
        clnt_perror(clnt, "call failed: ");
        /* Report that this machine failed */
        res = (query_result *) malloc(sizeof(query_result));
        res->exit_code = EXIT_FAILURE;
        res->error_message = machine_failure_msg(slave_addresses[args->args->machine_no]);
    }
    results[args->query_result_index] = res;
    clnt_destroy(clnt);
    /* deallocate the coordinator arguments */
    rq_pipe_args *node = args->args;
    while (node != NULL) {
        rq_pipe_args *head = node->next;
        free(node);
        node = head;
    }
    free(coord_args);
    return (void *) EXIT_SUCCESS;
}
