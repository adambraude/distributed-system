/**
 * Master Remote Query Handling
 */

#include "master_rq.h"
#include "master.h"
#include "../rpc/gen/slave.h"
#include "slavelist.h"
#include "../util/ds_util.h"
#include "../../bitmap-engine/BitmapEngine/src/seg-util/SegUtil.h"
#include "../../bitmap-engine/BitmapEngine/src/wah/WAHQuery.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#define TIME_TO_VOTE 1 // XXX make this shared between master/slave (general timeout)

query_result *rq_range_root(rq_range_root_args *query);
typedef struct coord_thread_args {
    rq_pipe_args *args;
    int query_result_index;
} coord_thread_args;
void *init_coordinator_thread(void *coord_args);
query_result **results;


/**
 * Executes the given (formatted) range query, returning 0 on success,
 * or the machine that failed in the query process
 */
int
init_range_query(unsigned int *range_array, int num_ranges,
    char *ops, int array_len)
{
    int j;
    puts("Printing Array");
    for (j = 0; j < array_len; j++) {
        printf("arr[%d] = %d\n", j, range_array[j]);
    }
    rq_range_root_args *root = (rq_range_root_args *) malloc(sizeof(rq_range_root_args));
    root->range_array.range_array_val = range_array;
    root->range_array.range_array_len = array_len;
    root->num_ranges = num_ranges;
    root->ops.ops_val = ops;
    root->ops.ops_len = num_ranges - 1;
    // char *coordinator = SLAVE_ADDR[0]; // arbitrary for now TODO: pick at random
    // CLIENT *cl = clnt_create(coordinator, REMOTE_QUERY_ROOT,
    //     REMOTE_QUERY_ROOT_V1, "tcp");
    // if (cl == NULL) {
    //     printf("Error: could not connect to coordinator %s.\n", coordinator);
    //     return 1;
    // }
    printf("Range Root Query\n");
    query_result *res = rq_range_root(root);
    // if (res == NULL) {
    //     printf("No response from coordinator.\n");
    //     return 1;
    // }
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
    free(res);
    //if (res != NULL) free(res);
    return EXIT_SUCCESS;
}

query_result *rq_range_root(rq_range_root_args *query)
{
    int num_threads = query->num_ranges;
    unsigned int *range_array = query->range_array.range_array_val;
    pthread_t tids[num_threads];

    results = (query_result **) malloc(sizeof(query_result *) * num_threads);
    int i;
    int array_index = 0;
    for (i = 0; i < num_threads; i++) {
        int num_nodes = range_array[array_index++];
        rq_pipe_args *pipe_args = (rq_pipe_args *)
            malloc(sizeof(rq_pipe_args) * num_nodes);
        rq_pipe_args *head_args = pipe_args;
        int j;
        for (j = 0; j < num_nodes; j++) {
            pipe_args->machine_addr = SLAVE_ADDR[range_array[array_index++]];
            printf("Address = %s\n", pipe_args->machine_addr);
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

        pthread_create(&tids[i], NULL, init_coordinator_thread,
            (void *) thread_args);
    }

    query_result *res = (query_result *) malloc(sizeof(query_result));

    /*
     * Conclude the query. Join each contributing thread,
     * and in doing so, report error if there is one, or report largest vector size
     */
    u_int result_vector_len = 0;
    u_int largest_vector_len = 0;
    for (i = 0; i < num_threads; i++) {
        pthread_join(tids[i], NULL);
        /* assuming a single point of failure, report on the failed slave */
        if (results[i]->exit_code != EXIT_SUCCESS) {
            /*
            res->exit_code = results[i]->exit_code;
            char *msg = results[i]->error_message;
            res->error_message = (char *) malloc(sizeof(msg));
            strcpy(res->error_message, msg);
            res->failed_machine_id = results[i]->failed_machine_id;
            return res;
            */
            return results[i]; // TODO: probably want to free the other stuff too
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
        //free_res(num_threads);
        return res;
    }
    u_int64_t *result_vector = (u_int64_t *)
        malloc(sizeof(u_int64_t) * largest_vector_len); // XXX: is dynamically allocating this necessary??

    /* AND the first 2 vectors together */
    result_vector_len = AND_WAH(result_vector,
        results[0]->vector.vector_val, results[0]->vector.vector_len,
        results[1]->vector.vector_val, results[1]->vector.vector_len);
    /* AND the subsequent vectors together */
    for (i = 2; i < num_threads; i++) {
        u_int64_t *new_result_vector = (u_int64_t *)
            malloc(sizeof(u_int64_t) * largest_vector_len);
        result_vector_len = AND_WAH(new_result_vector, result_vector,
            result_vector_len, results[i]->vector.vector_val,
            results[i]->vector.vector_len);
        free(result_vector);
        result_vector = new_result_vector;
    }
    /* deallocate the results */
    free(results);
    //free_res(num_threads);
    res->vector.vector_val = result_vector;
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

    CLIENT *clnt = clnt_create(args->args->machine_addr,
        REMOTE_QUERY_PIPE, REMOTE_QUERY_PIPE_V1, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror(args->args->machine_addr);
    }
    query_result *res = rq_pipe_1(*(args->args), clnt);
    /* give the request a time-to-live */
    struct timeval tv;
    tv.tv_sec = TIME_TO_VOTE;
    tv.tv_usec = 0;
    clnt_control(clnt, CLSET_TIMEOUT, &tv);
    if (res == NULL) {
        clnt_perror(clnt, "call failed: ");
        /* Report that this machine failed */
        res = (query_result *) malloc(sizeof(query_result));
        res->exit_code = EXIT_FAILURE;
        res->error_message = machine_failure_msg(args->args->machine_addr);
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
