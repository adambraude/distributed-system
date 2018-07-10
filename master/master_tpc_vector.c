#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tpc_master.h"
#include "../rpc/vote.h"
#include "../rpc/gen/slave.h"
#include "../types/types.h"

pthread_mutex_t lock;
int successes;

typedef struct push_vec_args {
    vec_t vector;
    vec_id_t vec_id;
    unsigned int vector_length;
    char *slave_addr;
} push_vec_args;

/*
 * Returns true if the result is valid: the (int) pointer is nonnull and
 * points to 0
 */
bool res_valid(int *r)
{
    return !(r == NULL || *r);
}

/*
 * Commit the given vid:vector mapping to the given array of slaves.
 */
int commit_vector(vec_id_t vec_id, vec_t vector, slave *slaves[], int num_slaves)
{
    /* 2PC Phase 1 on all Slaves */
    pthread_t tids[num_slaves];
    successes = 0;
    pthread_mutex_init(&lock, NULL);

    int i;
    void *status = 0;
    for (i = 0; i < num_slaves; i++) {
        pthread_create(&tids[i], NULL, get_commit_resp,
            (void *) slaves[i]->address);
    }

    for (i = 0; i < num_slaves; i++) {
        pthread_join(tids[i], &status);
        if (status == (void *) 1) {
            return 1;
        }
    }

    /* check if all slaves can commit */
    if (successes != num_slaves) {
        return 1;
    }
    /* 2PC Phase 2 */
    i = 0;
    for (i = 0; i < num_slaves; i++) {
        push_vec_args* ptr = (push_vec_args*) malloc(sizeof(push_vec_args));
        ptr->vector = vector;
        ptr->slave_addr = slaves[i]->address;
        ptr->vec_id = vec_id;
        pthread_create(&tids[i], NULL, push_vector, (void*) ptr);
    }
    for (i = 0; i < num_slaves; i++) {
        pthread_join(tids[i], NULL);
    }

    pthread_mutex_destroy(&lock);
    return 0;
}

void *get_commit_resp(void *slv_addr_arg)
{
    char *slv_addr = (char *) slv_addr_arg;
    CLIENT *clnt = clnt_create(slv_addr, TWO_PHASE_COMMIT,
        TWO_PHASE_COMMIT_V1, "tcp");
    if (clnt == NULL) {
        pthread_exit((void*) 1);
    }
    /* give the request a time-to-live */
    struct timeval tv;
    tv.tv_sec = TIME_TO_VOTE;
    tv.tv_usec = 0;
    clnt_control(clnt, CLSET_TIMEOUT, &tv);
    int *result = commit_msg_1(0, clnt);
    if (!(result == NULL || *result == VOTE_ABORT)) {
        pthread_mutex_lock(&lock);
        successes++;
        pthread_mutex_unlock(&lock);
    }
    clnt_destroy(clnt);
    return (void*) 0;
}

void *push_vector(void *thread_arg)
{
    push_vec_args *args = (push_vec_args *) thread_arg;
    CLIENT* cl = clnt_create(args->slave_addr, TWO_PHASE_COMMIT,
        TWO_PHASE_COMMIT_V1, "tcp");

    if (cl == NULL) {
        pthread_exit((void*) 1);
    }

    commit_vec_args *a = (commit_vec_args*) malloc(sizeof(commit_vec_args));
    a->vec_id = args->vec_id;
    u_int64_t vec[args->vector.vector_length];
    a->vector.vector_val = vec;
    memcpy(a->vector.vector_val, args->vector.vector,
        sizeof(u_int64_t) * args->vector.vector_length);
    a->vector.vector_len = args->vector.vector_length;
    int *result = commit_vec_1(*a, cl);

    clnt_destroy(cl);

    if (result == NULL) {
        return (void*) 1;
    }
    return (void*) 0;
}

/**
 * Connect to the slave process at the given address. Returns true if the
 * slave was set up, false otherwise
 */
bool setup_slave(slave *slv)
{
    CLIENT *cl = clnt_create(slv->address, SETUP_SLAVE, SETUP_SLAVE_V1,
        "tcp");
    if (cl == NULL) {
        return 1;
    }
    init_slave_args *args = (init_slave_args *)
        malloc(sizeof(init_slave_args));
    args->machine_name = slv->address;
    args->slave_id = slv->id;
    struct timeval tv;
    tv.tv_sec = 1; // TODO: it is important to come up with a reasonable value for this!
    tv.tv_usec = 0;
    clnt_control(cl, CLSET_TIMEOUT, &tv);
    int *res = init_slave_1(*args, cl);
    free(args);
    clnt_destroy(cl);
    return res_valid(res);
}

/**
 * Ask the slave at the given node if it's alive
 */
bool is_alive(char *address)
{
    CLIENT *cl = clnt_create(address, AYA, AYA_V1, "tcp");
    if (cl == NULL) {
        return false;
    }
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    clnt_control(cl, CLSET_TIMEOUT, &tv);
    int *res = stayin_alive_1(0, cl);
    clnt_destroy(cl);
    return res_valid(res);
}

int send_vector(slave *slave_1, vec_id_t vec_id, slave *slave_2)
{
    if (M_DEBUG && vec_id == 4) {
        printf("Sending v4 from %d->%d\n", slave_1->id, slave_2->id);
    }
    CLIENT *cl = clnt_create(slave_1->address, COPY_OVER_VECTOR,
        COPY_OVER_VECTOR_V1, "tcp");
    if (cl == NULL) {
        printf("Failed to connect to %s\n", slave_1->address);
        return 1;
    }
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    clnt_control(cl, CLSET_TIMEOUT, &tv);
    copy_vector_args args;
    args.vec_id = vec_id;
    args.destination_no = slave_2->id;
    int *res = send_vec_1(args, cl);
    clnt_destroy(cl);
    if (res == NULL || *res) {
        printf("Failed to send vector %u\n", vec_id);
        return 1;
    }
    return *res;
}
