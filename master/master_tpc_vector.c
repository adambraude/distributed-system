#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tpc_master.h"
#include "../rpc/vote.h"
#include "../rpc/gen/slave.h"

#include "slavelist.h"
pthread_mutex_t lock;
int successes;

typedef struct push_vec_args {
    vec_t vector;
    vec_id_t vec_id;
    unsigned int vector_length;
    char *slave_addr;
} push_vec_args;

// TODO move all master RPC functions into one file

/*
 * Commit the given vid:vector mappings to the given array of slaves.
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
        //printf("pthread create %s\n", slaves[i]);
        pthread_create(&tids[i], NULL, get_commit_resp,
            (void *) slaves[i]->address);
    }

    for (i = 0; i < num_slaves; i++) {
        pthread_join(tids[i], &status);
        if (status == (void *) 1) {
            printf("Failed to connect to all slaves.\n");
            return 1;
        }
    }

    /* check if all slaves can commit */
    if (successes != num_slaves) {
        return 1;
    }
    /* 2PC Phase 2 */
    i = 0;
    for (i = 0; i < NUM_SLAVES; i++) {
        push_vec_args* ptr = (push_vec_args*) malloc(sizeof(push_vec_args));
        ptr->vector = vector;
        ptr->slave_addr = slaves[i]->address;
        ptr->vec_id = vec_id;
        pthread_create(&tids[i], NULL, push_vector, (void*) ptr);
    }
    for (i = 0; i < NUM_SLAVES; i++) {
        pthread_join(tids[i], NULL);
    }

    pthread_mutex_destroy(&lock);
    return 0;
}

void *get_commit_resp(void *slv_addr_arg)
{
    char *slv_addr = (char *) slv_addr_arg;
    printf("Connecting to %s, creating clnt\n", slv_addr);
    CLIENT *clnt = clnt_create(slv_addr, TWO_PHASE_COMMIT,
        TWO_PHASE_COMMIT_V1, "tcp");
    if (clnt == NULL) {
        printf("Error: could not connect to slave %s.\n", slv_addr);
        pthread_exit((void*) 1);
    }
    /* give the request a time-to-live */
    struct timeval tv;
    tv.tv_sec = TIME_TO_VOTE;
    tv.tv_usec = 0;
    clnt_control(clnt, CLSET_TIMEOUT, &tv);
    int *result = commit_msg_1(0, clnt);
    if (result == NULL || *result == VOTE_ABORT) {
        printf("Couldn't commit at slave %s.\n", slv_addr);
    }
    else {
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
        printf("Error: could not connect to slave %s.\n", args->slave_addr);
        pthread_exit((void*) 1);
    }

    commit_vec_args *a = (commit_vec_args*) malloc(sizeof(commit_vec_args));
    a->vec_id = args->vec_id;
    a->vector.vector_val = args->vector.vector;
    a->vector.vector_len = args->vector.vector_length;
    int *result = commit_vec_1(*a, cl);

    if (result == NULL) {
        printf("Commit failed.\n");
        return (void*) 1;
    }
    printf("Commit succeeded.\n");
    return (void*) 0;
}

/**
 *  Connect to the slave process at the given address, returning a new slave.
 */
int setup_slave(slave *slv)
{
    CLIENT *cl = clnt_create(slv->address, SETUP_SLAVE, SETUP_SLAVE_V1,
        "tcp");
    if (cl == NULL) {
        printf("Error: couldn't connect\n");
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
    if (*res) {
        printf("RPC: Failed to initialize slave %s\n", slv->address);
        return 1;
    }
    return 0;
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
    tv.tv_sec = 1; // TODO: it is important to come up with a reasonable value for this!
    tv.tv_usec = 0;
    clnt_control(cl, CLSET_TIMEOUT, &tv);
    int *res = stayin_alive_1(0, cl);
    if (*res) {
        return false;
    }
    clnt_destroy(cl);
    return true;
}

int send_vector(slave *slave_1, vec_id_t vec_id, slave *slave_2)
{
    CLIENT *cl = clnt_create(slave_1->address, COPY_OVER_VECTOR,
        COPY_OVER_VECTOR_V1, "tcp");
    if (cl == NULL) {
        return 1;
    }
    struct timeval tv;
    tv.tv_sec = 1; // TODO: it is important to come up with a reasonable value for this!
    tv.tv_usec = 0;
    clnt_control(cl, CLSET_TIMEOUT, &tv);
    copy_vector_args args;
    args.vec_id = vec_id;
    char *addr = slave_2->address;
    args.destination_addr = (char *) malloc(sizeof(addr));
    strcpy(args.destination_addr, addr);
    int *res = send_vec_1(args, cl);
    clnt_destroy(cl);
    //free(args);
    return *res;
}
