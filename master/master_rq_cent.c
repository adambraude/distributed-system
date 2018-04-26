/**
 * Master Remote Query Handling
 */

#include "master_rq_cent.h"
#include "master.h"
#include "../rpc/gen/slave.h"
#include "../util/ds_util.h"
#include "../bitmap-vector/read_vec.h"
#include "../../bitmap-engine/BitmapEngine/src/seg-util/SegUtil.h"
#include "../../bitmap-engine/BitmapEngine/src/wah/WAHQuery.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#define TIME_TO_VOTE 1 // XXX make this shared between master/slave (general timeout)

query_result *rq_range_root(rq_range_root_args *query);

typedef struct coord_thread_args {
    rq_pipe_args *args;
    int query_result_index;
} coord_thread_args;

void *init_coordinator_thread(void *coord_args);
query_result **results;

#define DEBUG 1

/**
 * Executes the given (formatted) range query, returning 0 on success,
 * or the machine that failed in the query process
 */
int
cent_init_range_query(u_int *range_array, int num_ranges,
    char *ops, int array_len)
{
    puts("init range query");
    rq_range_root_args *root = (rq_range_root_args *) malloc(sizeof(rq_range_root_args));
    root->range_array.range_array_val = range_array;
    root->range_array.range_array_len = array_len;
    root->num_ranges = num_ranges;
    root->ops.ops_val = ops;
    root->ops.ops_len = num_ranges - 1;
    int i;
    for (i = 0; i < array_len; i++) {
        printf("%d\n", range_array[i]);
    }

    query_result *res = rq_range_root(root);
    if (DEBUG) {
        free(root);
        free(range_array);
        return 0; // TODO: fix bug
    }
    printf("Res len = %d\n", res->vector.vector_len);
    if (res->exit_code == EXIT_SUCCESS) {
        for (i = 1; i < res->vector.vector_len; i++) {
            printf("%llx\n",res->vector.vector_val[i]);
        }
    }
    else {
        printf("Query failed, reason: %s\n", res->error_message);
    }
    free(res);
    free(root);
    free(range_array);
    return EXIT_SUCCESS;
}

query_result *get_vector(u_int vec_id)
{
    /* Turn vec_id into the filename "vec_id.dat" */
    char filename[128];
    snprintf(filename, 128, "../tst_data/tpc/vec/v_%u.dat", vec_id);
    vec_t *vector = read_vector(filename);

    query_result *res = (query_result *) malloc(sizeof(query_result));
    if (vector == NULL) {
        puts("Couldn't find vector!");
        res->vector.vector_val = NULL;
        res->vector.vector_len = 0;
        res->exit_code = EXIT_FAILURE;
        char buf[128];
        res->error_message = buf;
        return res;
    }

    res->vector.vector_val = vector->vector;
    res->vector.vector_len = vector->vector_length;
    res->exit_code = EXIT_SUCCESS;
    res->error_message = "";
    free(vector);
    return res;
}

query_result *rq_pipe_1_cent(rq_pipe_args query, struct svc_req *req)
{
    query.next;
    query_result *this_result;
    query_result *next_result = NULL;

    u_int exit_code = EXIT_SUCCESS;
    this_result = get_vector(query.vec_id);
    /* Something went wrong with reading the vector,
     * or we're in the final call */
    if (this_result->exit_code != EXIT_SUCCESS || query.next == NULL)
        return this_result;
    /* Recursive Query */
    else {
        while (query.next != NULL) {
            next_result = get_vector(query.next->vec_id);
            u_int result_len = 0;
            u_int v_len = max(this_result->vector.vector_len,
                next_result->vector.vector_len);
            u_int64_t result_val[v_len];
            memset(result_val, 0, v_len);

            if (query.op == '|') {
                result_len = OR_WAH(result_val,
                    this_result->vector.vector_val, this_result->vector.vector_len,
                    next_result->vector.vector_val, next_result->vector.vector_len) + 1;
            }
            else if (query.op == '&') {
                result_len = AND_WAH(result_val,
                    this_result->vector.vector_val, this_result->vector.vector_len,
                    next_result->vector.vector_val, next_result->vector.vector_len) + 1;
            }
            else {
                query_result *res = (query_result *) malloc(sizeof(query_result));
                char buf[32];
                snprintf(buf, 32, "Unknown operator %c", query.op);
                res->vector.vector_val = NULL;
                res->vector.vector_len = 0;
                res->error_message = buf;
                res->exit_code = EXIT_FAILURE;
                return res;
            }
            query = *query.next;
            if (query.next == NULL) {
                query_result *res = (query_result *) malloc(sizeof(query_result));
                res->vector.vector_len = result_len;
                res->vector.vector_val = result_val;
                memcpy(res->vector.vector_val, result_val, result_len * sizeof(u_int64_t));
                res->exit_code = EXIT_SUCCESS;
                free(this_result);
                res->error_message = "";
                int i;
                for (i = 0; i < res->vector.vector_len; i++) {
                    printf("%llx\n", res->vector.vector_val[i]);
                }
                return res;
            }
            /* there are still more vectors to visit! */
            u_int64_t res_arr[result_len];
            memset(res_arr, 0, result_len * sizeof(u_int64_t));
            this_result->vector.vector_val = res_arr;
            memcpy(this_result->vector.vector_val, result_val,
                sizeof(u_int64_t) * result_len);
            this_result->vector.vector_len = result_len;


        }
    }

}

query_result *rq_range_root(rq_range_root_args *query)
{
    int num_threads = query->num_ranges;
    unsigned int *range_array = query->range_array.range_array_val;
    pthread_t tids[num_threads];

    results = (query_result **) malloc(sizeof(query_result *) * num_threads);
    memset(results, NULL, num_threads * sizeof(query_result *));
    int i, array_index = 0;
    for (i = 0; i < num_threads; i++) {
        int num_nodes = range_array[array_index++];
        rq_pipe_args *pipe_args = (rq_pipe_args *)
            malloc(sizeof(rq_pipe_args) * num_nodes);
        rq_pipe_args *head_args = pipe_args;
        int j;
        for (j = 0; j < num_nodes; j++) {
            array_index++;
            //printf("Finding vector %d at %d : %s\n", range_array[array_index], range_array[array_index - 1], pipe_args->machine_addr);
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
        pthread_create(&tids[i], NULL, init_coordinator_thread, (void*) thread_args);
    }

    /*
     * Conclude the query. Join each contributing thread,
     * and in doing so, report error if there is one, or report largest vector size
     */
    u_int result_vector_len = 0, largest_vector_len = 0;
    for (i = 0; i < num_threads; i++) {
        pthread_join(tids[i], NULL);
        /* assuming a single point of failure, report on the failed slave */
        if (results[i]->exit_code != EXIT_SUCCESS) {
            return results[i];
        }
        largest_vector_len = max(largest_vector_len,
            results[i]->vector.vector_len);
    }

    if (DEBUG) {
        for (i = 0; i < num_threads; i++) {
            free(results[i]);
        }
        free(results);
        return NULL;
    }
    query_result *res = (query_result *) malloc(sizeof(query_result));

    /* all results found! */
    res->exit_code = EXIT_SUCCESS;
    res->error_message = "";
    results[0]->vector;
    puts("returning");
    if (num_threads == 1) { /* there are no vectors to AND together */
        //print_vector(results[0]->vector.vector_len, results[0]->vector.vector_val);
        puts("returning");
        return results[0];
    }
    u_int64_t *result_vector = (u_int64_t *)
        malloc(sizeof(u_int64_t) * largest_vector_len);
    memset(result_vector, 0, largest_vector_len * sizeof(u_int64_t));
    /* AND the first 2 vectors together */

    result_vector_len = AND_WAH(result_vector,
        results[0]->vector.vector_val, results[0]->vector.vector_len,
        results[1]->vector.vector_val, results[1]->vector.vector_len) + 1;
    puts("AND-ed!");

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
    for (i = 0; i < num_threads; i++) {
        free(results[i]->vector.vector_val);
        free(results[i]);
    }
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



void *init_coordinator_thread(void *coord_args) {
    coord_thread_args *args = (coord_thread_args *) coord_args;
    results[args->query_result_index] = rq_pipe_1_cent(*(args->args), NULL);
    // results[args->query_result_index] = malloc(sizeof(query_result *));
    // u_int64_t arr[res->vector.vector_len];
    // memset(arr, 0, sizeof(u_int64_t) * res->vector.vector_len);
    // results[args->query_result_index]->vector.vector_val = (u_int64_t*) malloc(sizeof(u_int64_t) * args->query_result_index);
    // memcpy(results[args->query_result_index]->vector.vector_val, res->vector.vector_val, sizeof(u_int64_t) * res->vector.vector_len);
    // results[args->query_result_index]->vector.vector_len = res->vector.vector_len;
    //
    //
    // puts("finished result");
    // print_vector(results[args->query_result_index]->vector.vector_val, results[args->query_result_index]->vector.vector_len);
    // puts("done printing result");
    // results[args->query_result_index]->exit_code = EXIT_SUCCESS;
    return (void *) EXIT_SUCCESS;
}
