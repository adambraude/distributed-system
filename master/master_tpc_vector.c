#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "tpc_master.h"
#include "../rpc/vote.h"
#include "../rpc/gen/slave.h"
// FIXME: should NOT have to import C files

#include "../types/types.h"
#include "slavelist.h"
pthread_mutex_t lock;
int successes;

typedef struct push_vec_args {
    vec_t vector;
    vec_id_t vec_id;
    unsigned int vector_length;
    char *slave_addr;
} push_vec_args;

/*
 * Commit the given vid:vector mappings to the given slaves.
 * TODO: generalize this: have it take a set of machines instead, since they
 * might not be available
 */
int commit_vector(vec_id_t vec_id, vec_t vector)
{
    /* 2PC Phase 1 on all Slaves */
    pthread_t tids[NUM_SLAVES];
    successes = 0;
    pthread_mutex_init(&lock, NULL);

    int i;
    void *status = 0;
    for (i = 0; i < NUM_SLAVES; i++) {
        //printf("pthread create %s\n", slaves[i]);
        pthread_create(&tids[i], NULL, get_commit_resp, (void *)SLAVE_ADDR[i]);
    }
    for (i = 0; i < NUM_SLAVES; i++) {
        pthread_join(tids[i], &status);
        if (status == (void *) 1) {
            printf("Failed to connect to all slaves.\n");
            return 1;
        }
    }

    /* check if all slaves can commit */
    if (successes != NUM_SLAVES) return 1;

    /* 2PC Phase 2 */
    i = 0;
    for (i = 0; i < NUM_SLAVES; i++) {
        push_vec_args* ptr = (push_vec_args*) malloc(sizeof(push_vec_args));
        ptr->vector = vector;
        ptr->slave_addr = SLAVE_ADDR[i];
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
    printf("Connecting to %s\n", slv_addr);
    CLIENT* clnt = clnt_create(slv_addr, TWO_PHASE_COMMIT_VOTE,
        TWO_PHASE_COMMIT_VOTE_V1, "tcp");

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
    CLIENT* cl = clnt_create(args->slave_addr, TWO_PHASE_COMMIT_VEC,
        TPC_COMMIT_VEC_V1, "tcp");

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
