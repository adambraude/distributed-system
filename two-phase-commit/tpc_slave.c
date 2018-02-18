/* 2PC server (slave) */

#include "vote.h"
#include "tpc.h"
#include <stdio.h>

int result;

int *commit_msg_1_svc(void *v, struct svc_req *req)
{
    int ready = 1; // test value
    if (ready) {
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
    FILE *fp = fopen("vectors.dat", "rw");
    char buffer[128];
    snprintf(buffer, 128, "%d: %llu", args->vec_id, args->vec);
    fwrite(buffer, sizeof(char), sizeof(buffer), fp);
    perror("fwrite");
    fclose(fp);
    result = 0;
    return &result;
}
