/**
 * Slave Remote Query Handling
 */

#include "../rpc/gen/slave.h"
#include "slave.h"
#include "../rpc/vote.h"

#include "../../bitmap-engine/BitmapEngine/src/seg-util/SegUtil.h"
#include "../../bitmap-engine/BitmapEngine/src/wah/WAHQuery.h"

#include "../bitmap-vector/read_vec.h"
#include "../types/types.h"
#include "../util/ds_util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

char **slave_addresses = NULL;

#define SLAVE_DEBUG true

query_result *get_vector(u_int vec_id)
{
    /* Turn vec_id into the filename "vec_id.dat" */
    char filename[16];
    snprintf(filename, 16, "v_%u.dat", vec_id);
    vec_t *vector = read_vector(filename);
    query_result *res = (query_result *) malloc(sizeof(query_result));
    if (vector == NULL) {
        res->vector.vector_val = NULL;
        res->vector.vector_len = 0;
        res->exit_code = EXIT_FAILURE;
        char buf[128];
        snprintf(buf, 128, "Error: could not locate vector %d on machine %s\n",
            vec_id, slave_addresses[slave_id]);
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

bool query_res_valid(rq_pipe_args *query_ptr, u_int64_t **result_val,
    query_result *this_result, query_result *next_result,
    u_int *result_len_ptr)
{
    u_int v_len = max(this_result->vector.vector_len,
        next_result->vector.vector_len);
    u_int64_t arr[v_len];
    *result_val = arr;
    memset(*result_val, 0, v_len * sizeof(u_int64_t));
    rq_pipe_args query = *query_ptr;
    if (query.op == '|') {
        *result_len_ptr = OR_WAH(*result_val,
            this_result->vector.vector_val, this_result->vector.vector_len,
            next_result->vector.vector_val, next_result->vector.vector_len) + 1;
        return true;
    }
    else if (query.op == '&') {
        *result_len_ptr = AND_WAH(*result_val,
            this_result->vector.vector_val, this_result->vector.vector_len,
            next_result->vector.vector_val, next_result->vector.vector_len) + 1;
        return true;
    }
    return false;
}

query_result *query_err_res(rq_pipe_args *query_ptr)
{
    query_result *res = (query_result *)
        malloc(sizeof(query_result));
    char buf[32];
    snprintf(buf, 32, "Unknown operator %c", query_ptr->op);
    res->vector.vector_val = NULL;
    res->vector.vector_len = 0;
    res->error_message = buf;
    res->exit_code = EXIT_FAILURE;
    return res;
}

/**
 * take the given result values result_len, result_val, insert it into
 * query_result vector struct values
 */
void set_vec(u_int result_len, u_int64_t *result_val, query_result *res)
{
    u_int64_t res_arr[result_len];
    res->vector.vector_val = res_arr;
    memcpy(res->vector.vector_val, result_val,
        sizeof(u_int64_t) * result_len);
    res->vector.vector_len = result_len;
}

query_result *failed_slave_res(query_result *res, char *addr)
{
    res->exit_code = EXIT_FAILURE;
    char error_message[64];
    snprintf(error_message, 64,
        "Error: No response from machine %s\n", addr);
    res->error_message = error_message;
    return res;
}

query_result *success_res(query_result *res)
{
    res->exit_code = EXIT_SUCCESS;
    res->error_message = "";
    return res;
}

query_result *rq_pipe_1_svc(rq_pipe_args query, struct svc_req *req)
{
    query_result *this_result = get_vector(query.vec_id), *next_result = NULL;

    /* Something went wrong with reading the vector,
     * or we're in the final call */
    if (this_result->exit_code != EXIT_SUCCESS || query.next == NULL) {
        if (SLAVE_DEBUG) puts("Query tail");
        return this_result;
    }

    /* Recursive Query */
    else {
        query = *(query.next);
        /* process vectors on this machine */
        while (query.machine_no == slave_id) {
            next_result = get_vector(query.vec_id);
            if (next_result->exit_code == EXIT_FAILURE) {
                printf("Error: vector %u not found\n", query.vec_id);
                return next_result;
            }

            u_int64_t *result_val = NULL;
            u_int result_len;

            if (!query_res_valid(&query, &result_val, this_result, next_result,
                &result_len)) {
                free(this_result);
                free(next_result);
                return query_err_res(&query);
            }

            set_vec(result_len, result_val, this_result);
            if (query.next == NULL) {
                free(next_result);
                if (SLAVE_DEBUG) puts("completed all-local rq");
                return success_res(this_result);
            }
            query = *(query.next);
        }

        /* complete query with downstream slaves */
        int host = query.machine_no;
        CLIENT *client;
        client = clnt_create(slave_addresses[host],
            REMOTE_QUERY_PIPE, REMOTE_QUERY_PIPE_V1, "tcp");
        if (client == NULL) {
            clnt_pcreateerror(slave_addresses[host]);
            return failed_slave_res(this_result,
                slave_addresses[query.machine_no]);
        }
        else {
            /* give the request a time-to-live */
            struct timeval tv;
            tv.tv_sec = TIME_TO_VOTE * 5;
            tv.tv_usec = 0;
            clnt_control(client, CLSET_TIMEOUT, &tv);
            if (SLAVE_DEBUG)
               printf("RPCing %s, vec %u\n", slave_addresses[query.machine_no], query.vec_id);
            next_result = rq_pipe_1(query, client);
            clnt_destroy(client);
            if (next_result == NULL) {
                clnt_perror(client, "Recursive pipe call failed");
                return failed_slave_res(this_result,
                    slave_addresses[query.machine_no]);
            }
        }
    }

    /* Something went wrong with the recursive call. */
    if (next_result->exit_code != EXIT_SUCCESS) {
        if (SLAVE_DEBUG)
            printf("Result response: %s", next_result->error_message);
        free(this_result);
        puts("recursive call failed");
        return next_result;
    }

    /* Our final return values. */
    u_int64_t *result_val = NULL;
    u_int result_len;
    if (!query_res_valid(&query, &result_val, this_result, next_result,
        &result_len)) {
        free(this_result);
        return query_err_res(&query);
    }
    set_vec(result_len, result_val, this_result);
    if (SLAVE_DEBUG) puts("completed rq w/ remote slave");
    return success_res(this_result);
}

#define TESTING_SLOW_PROC 0

static int result;

int *commit_msg_1_svc(int message, struct svc_req *req)
{
    int ready = 1; // test value
    if (ready) {
        result = VOTE_COMMIT;
    }
    else {
        result = VOTE_ABORT;
    }
    /* make the process run slow, to test failure */
    if (TESTING_SLOW_PROC) {
        sleep(TIME_TO_VOTE + 1);
    }
    return &result;

}

int *commit_vec_1_svc(struct commit_vec_args args, struct svc_req *req)
{
    FILE *fp;
    char filename_buf[128];
    if (SLAVE_DEBUG)
        printf("Recieved vector %d\n", args.vec_id);
    snprintf(filename_buf, 128, "v_%d.dat", args.vec_id);
    fp = fopen(filename_buf, "wb");
    char buffer[1024];
    memset(buffer, 0, 1024);
    buffer[0] = '\0';
    char line_buffer[32];
    memset(line_buffer, 0, 32);
    int i;
    /* first element of the should be 0, to work with WAHQuery.c
     so don't bother storing it*/
    for (i = 1; i < args.vector.vector_len; i++) {
        snprintf(line_buffer, 32, "%lx\n", args.vector.vector_val[i]);
        strcat(buffer, line_buffer);
    }
    fprintf(fp, "%s", buffer);
    fclose(fp);
    result = EXIT_SUCCESS;
    return &result;
}

int *init_slave_1_svc(init_slave_args args, struct svc_req *req)
{
    slave_id = args.slave_id; /* assign this slave its ID */
    free(slave_addresses);
    if (fill_slave_arr(SLAVELIST_PATH, &slave_addresses) < 0) {
        result = EXIT_FAILURE;
        return &result;
    }
    if (SLAVE_DEBUG)
        printf("On machine %s, assigned slave number %d\n",
            slave_addresses[slave_id], slave_id);
    result = EXIT_SUCCESS;
    return &result;
}

int *stayin_alive_1_svc(int x, struct svc_req *req)
{
    result = 0;
    return &result;
}

/**
 * hand the machine with the given address the vector with the given
 * ID
 */
int *send_vec_1_svc(copy_vector_args copy_args, struct svc_req *req)
{
    CLIENT *cl = clnt_create(slave_addresses[copy_args.destination_no],
        TWO_PHASE_COMMIT, TWO_PHASE_COMMIT_V1, "tcp");
    commit_vec_args args;
    args.vec_id = copy_args.vec_id;
    query_result *qres = get_vector(copy_args.vec_id);
    if (qres->vector.vector_val == NULL) {
        printf("Error: could not find vector %u\n", copy_args.vec_id);
        result = -1;
        return &result;
    }
    memcpy(&args.vector, &qres->vector, sizeof(qres->vector));
    if (SLAVE_DEBUG)
        printf("Sending vector %u to %s\n", copy_args.vec_id, slave_addresses[copy_args.destination_no]);
    int *res = commit_vec_1(args, cl);
    free(qres);
    return res;
}

/**
 * Make this slave die. Used for fault tolerance testing if having a slave
 * crash at a particular point is desirable.
 */
int *kill_order_1_svc(int arg, struct svc_req *req)
{
    printf("Slave %d exiting\n", slave_id);
    exit(0);
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
