/**
 * Slave Remote Query Handling
 */

#include "../rpc/gen/slave.h"
#include "../rpc/vote.h"

#include "../master/slavelist.h"

#include "../vector-clock/vclock.h"

#include "../../bitmap-engine/BitmapEngine/src/seg-util/SegUtil.h"
#include "../../bitmap-engine/BitmapEngine/src/wah/WAHQuery.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define TESTING_SLOW_PROC 0

typedef struct coord_thread_args {
    rq_pipe_args *args;
    int query_result_index;
} coord_thread_args;

query_result **results;
int result;

/* TODO: Figure out how to have slave nodes know their id. */
int slave_id = 0;

/* TODO: Currently we do not have a way to destroy the clocks. */
vector_clock slave_clock;

void *init_coordinator_thread(void *coord_args) {
    coord_thread_args *args = (coord_thread_args *) coord_args;
    CLIENT *clnt = clnt_create(args->args->machine_addr, REMOTE_QUERY_PIPE,
        REMOTE_QUERY_PIPE_V1, "tcp");

    if (clnt == NULL) {
        printf("client is null\n");
    }

    query_result *res = rq_pipe_1_svc(*(args->args), slave_clock, clnt);

    if (res == NULL) {
        printf("Query to %s failed\n", args->args->machine_addr);
        return (void *) 1;
    }

    else {
        results[args->query_result_index] = res;
    }

    return (void *) 0;
}

query_result *get_vector(u_int vec_id)
{
    /* Turn vec_id into the filename "v_[vec_id].dat" */
    char filename[16];
    snprintf(filename, 16, "v_%u.dat", vec_id);

    /* Necessary Variables */
    FILE *fp = NULL;
    u_int64_t *vector_val = NULL;
    u_int vector_len = 0;
    u_int exit_code = EXIT_SUCCESS;

    /* Open file in binary mode. */
    fp = fopen(filename, "r");

    if (fp == NULL) {
        exit_code = EXIT_FAILURE ^ vec_id;
    }

    else {
        /* TODO: should be empirically determined average vector length */
        int num_elts = 4;
        vector_val = (u_int64_t *) malloc(sizeof(u_int64_t) * num_elts);
        char buf[32];

        while (fgets(buf, 32, fp) != NULL) {
            if (vector_len > num_elts) {
                num_elts *= 2;
                vector_val = (u_int64_t *)
                    realloc(vector_val, num_elts * sizeof(u_int64_t));
            }

            vector_val[vector_len++] = (u_int64_t) strtol(buf, NULL, 10);
        }

        fclose(fp);
    }

    query_result *vector = (query_result *) malloc(sizeof(query_result));
    vector->vector.vector_val = vector_val;
    vector->vector.vector_len = vector_len;
    vector->exit_code = exit_code;

    return vector;
}

query_result
*rq_pipe_1_svc(rq_pipe_args query, vector_clock recv_clock,
    struct svc_req *req)
{
    if (slave_clock == NULL) {
        slave_clock = create_vclock();
    }

    handle_recvd_vclock(slave_clock, recv_clock, slave_id);

    query_result *this_result;
    query_result *next_result = NULL;

    u_int exit_code = EXIT_SUCCESS;
    this_result = get_vector(query.vec_id);

    prep_send_vclock(slave_clock, slave_id);
    this_result->time_at_completion = slave_clock;

    /* Something went wrong with reading the vector. */
    if (this_result->exit_code != EXIT_SUCCESS) {
        return this_result;
    }

    /* We are the final call. */
    else if (query.next == NULL) {
        return this_result;
    }

    /* Recursive Query */
    else {
        char *host = query.machine_addr;
        CLIENT *client;
        client = clnt_create(host, REMOTE_QUERY_PIPE, REMOTE_QUERY_PIPE_V1,
            "tcp");

        if (client == NULL) {
            clnt_pcreateerror(host);
            exit_code = EXIT_FAILURE;
        }

        else {
            next_result = rq_pipe_1_svc(*(query.next), slave_clock, client);

            if (next_result == NULL) {
                clnt_perror(client, "call failed:");
            }

            clnt_destroy(client);
        }
    }

    /* Something went wrong with the recursive call. */
    if (next_result->exit_code != EXIT_SUCCESS) {
        this_result->exit_code = next_result->exit_code;

        return this_result;
    }

    /* Our final return values. */
    u_int64_t *result_val = (u_int64_t *)
        malloc(sizeof(u_int64_t) * this_result->vector.vector_len);
    u_int result_len = 0;

    if (query.op == '|') {
        result_len = OR_WAH(result_val,
            this_result->vector.vector_val, this_result->vector.vector_len,
            next_result->vector.vector_val, next_result->vector.vector_len);
    }

    else if (query.op == '&') {
        result_len = AND_WAH(result_val,
            this_result->vector.vector_val, this_result->vector.vector_len,
            next_result->vector.vector_val, next_result->vector.vector_len);
    }

    else {
        printf("Error: Unknown Operator\n");
    }

    query_result *vector = (query_result *) malloc(sizeof(query_result));
    vector->vector.vector_len = result_len;
    vector->vector.vector_val = result_val;
    vector->exit_code = exit_code;
    free(this_result);
    free(next_result);

    return vector;
}

query_result
*rq_range_root_1_svc(rq_range_root_args query, vector_clock recv_clock,
    struct svc_req *req)
{
    if (slave_clock == NULL) {
        slave_clock = create_vclock();
    }

    handle_recvd_vclock(slave_clock, recv_clock, slave_id);

    int num_threads = query.num_ranges;
    unsigned int *range_array = query.range_array.range_array_val;
    pthread_t tids[num_threads];

    /* the results to be anded together (in general, OP) */
    results = (query_result **) malloc(sizeof(query_result *) * num_threads);
    int array_index = 0;

    int i;
    for (i = 0; i < num_threads; i++) {
        int num_nodes = range_array[array_index++];
        rq_pipe_args *pipe_args = (rq_pipe_args *)
            malloc(sizeof(rq_pipe_args) * num_nodes);
        rq_pipe_args *head_args = pipe_args;

        int j;
        for (j = 0; j < num_nodes; j++) {
            pipe_args->machine_addr = SLAVE_ADDR[range_array[array_index++]];
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

        coord_thread_args *thread_args = (coord_thread_args *)
            malloc(sizeof(coord_thread_args));
        thread_args->query_result_index = i;
        thread_args->args = head_args;

        /* allocate the appropriate number of args */
        pthread_create(&tids[i], NULL, init_coordinator_thread,
            (void *) thread_args);
    }

    for (i = 0; i < num_threads; i++) {
        pthread_join(tids[i], NULL);
    }

    /* TODO: AND all the results together */

    prep_send_vclock(slave_clock, slave_id);
    results[0]->time_at_completion = slave_clock;

    return results[0];
}

int
*commit_msg_1_svc(int message, vector_clock recv_clock, struct svc_req *req)
{
    if (slave_clock == NULL) {
        slave_clock = create_vclock();
    }

    handle_recvd_vclock(slave_clock, recv_clock, slave_id);

    printf("SLAVE: VOTING\n");

    /* Test value. */
    int ready = 1;

    if (ready) {
        result = VOTE_COMMIT;
    }

    /* possible reasons: lack of memory, ... */
    else {
        result = VOTE_ABORT;
    }

    /* make the process run slow, to test failure */
    if (TESTING_SLOW_PROC) {
        sleep(TIME_TO_VOTE + 1);
    }

    return &result;
}

int
*commit_vec_1_svc(struct commit_vec_args args, vector_clock recv_clock,
    struct svc_req *req)
{
    if (slave_clock == NULL) {
        slave_clock = create_vclock();
    }

    handle_recvd_vclock(slave_clock, recv_clock, slave_id);

    printf("SLAVE: Putting vector %u\n", args.vec_id);

    FILE *fp;
    char filename_buf[128];

    /* TODO: function to get vector filename. */
    snprintf(filename_buf, 128, "v_%d.dat", args.vec_id);
    fp = fopen(filename_buf, "wb");
    char buffer[128];

    int i;
    for (i = 0; i < args.vector.vector_len; i++) {
        snprintf(buffer, 128, "%llu", args.vector.vector_val[i]);
    }

    fprintf(fp, "%s\n", buffer);
    fclose(fp);
    result = 0;

    return &result;
}
