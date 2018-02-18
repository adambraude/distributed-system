/* 2PC server (slave) */

#include "vote.h"
#include "tpc.h"
#include <stdio.h>

int result;

int *commit_msg_1_svc(void * v, struct svc_req *req)
{
    int ready = 0; // test value
    if (ready) {
        // TODO: prepare for commit?
        result = VOTE_COMMIT;
    }
    else {
        result = VOTE_ABORT;
    }
    return &result;

}

int *commit_vec_1_svc(commit_vec_args * args, struct svc_req *req)
{
    printf("Commiting vector #%d: %llu\n", args->vec_id, args->vec);
    result = 0;
    return &result;
}
