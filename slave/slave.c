/**
 * Slave Remote Query Handling
 */

#include "../rpc/gen/slave.h"
#include "../rpc/vote.h"

#include "../master/slavelist.h"

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

char *machine_failure_msg(char *);
unsigned int slave_id;

char *machine_failure_msg(char *);

query_result *get_vector(u_int);

int *kill_slave_rpc_1_svc(copy_vector_args a, struct svc_req *req) {
    exit(0);
}

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
        snprintf(buf, 128, "Error: could not locate vector %d on machine %s", vec_id, SLAVE_ADDR[slave_id]); // TODO: also machine name?
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

query_result *rq_pipe_1_svc(rq_pipe_args query, struct svc_req *req)
{
    printf("%s: In Pipe 1 svc\n", SLAVE_ADDR[slave_id]);
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
        printf("Going to visit %s\n", query.next->machine_addr);
        while (!strcmp(query.next->machine_addr, SLAVE_ADDR[slave_id])) {
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
        char *host = query.next->machine_addr;
        CLIENT *client;
        client = clnt_create(host,
            REMOTE_QUERY_PIPE, REMOTE_QUERY_PIPE_V1, "tcp");
        if (client == NULL) {
            clnt_pcreateerror(host);
            exit_code = EXIT_FAILURE;
        }
        else {
            next_result = rq_pipe_1(*(query.next), client);
            /* give the request a time-to-live */
            struct timeval tv;
            tv.tv_sec = TIME_TO_VOTE;
            tv.tv_usec = 0;
            clnt_control(client, CLSET_TIMEOUT, &tv);
            if (next_result == NULL) {
                clnt_perror(client, "call failed:");
                this_result->exit_code = EXIT_FAILURE;
                this_result->error_message =
                    machine_failure_msg(query.next->machine_addr);
                return this_result;
            }

            clnt_destroy(client);
        }
    }

    /* Something went wrong with the recursive call. */
    if (next_result->exit_code != EXIT_SUCCESS) {
        free(this_result);
        return next_result;
    }

    /* Our final return values. */
    u_int v_len = max(this_result->vector.vector_len,
        next_result->vector.vector_len);
    u_int64_t result_val[v_len];
    memset(result_val, 0, v_len * sizeof(u_int64_t));
    u_int result_len = 0;
    query_result *res = (query_result *) malloc(sizeof(query_result));
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
        char buf[32];
        snprintf(buf, 32, "Unknown operator %c", query.op);
        res->vector.vector_val = NULL;
        res->vector.vector_len = 0;
        res->error_message = buf;
        res->exit_code = EXIT_FAILURE;
        return res;
    }

    res->vector.vector_len = result_len;
    u_int64_t res_arr[result_len];
    memset(res_arr, 0, result_len * sizeof(u_int64_t));
    res->vector.vector_val = res_arr;
    memcpy(res->vector.vector_val, result_val,
        res->vector.vector_len * sizeof(u_int64_t));
    res->exit_code = exit_code;
    res->error_message = "";
    free(this_result);
    puts("Returning Result");
    return res;
}

#define TESTING_SLOW_PROC 0

static int result;

int *commit_msg_1_svc(int message, struct svc_req *req)
{
	printf("SLAVE: VOTING\n");
    int ready = 1; // test value
    if (ready) {
        result = VOTE_COMMIT;
    }
    else {
        /* possible reasons: lack of memory, ... */
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
	printf("SLAVE: Putting vector %u\n", args.vec_id);
    FILE *fp;
    char filename_buf[128];
    snprintf(filename_buf, 128, "v_%d.dat", args.vec_id); // TODO: function to get vector filename
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
        snprintf(line_buffer, 32, "%llx\n", args.vector.vector_val[i]);
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
    result = EXIT_SUCCESS;
    printf("Registered slave %d\n", slave_id);
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
    CLIENT *cl = clnt_create(copy_args.destination_addr, TWO_PHASE_COMMIT,
        TWO_PHASE_COMMIT_V1, "tcp");
    commit_vec_args args;
    args.vec_id = copy_args.vec_id;
    query_result *qres = get_vector(copy_args.vec_id);
    memcpy(&args.vector, &qres->vector, sizeof(qres->vector));
    result = *commit_vec_1_svc(args, cl);
    free(qres);
    return &result;
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
