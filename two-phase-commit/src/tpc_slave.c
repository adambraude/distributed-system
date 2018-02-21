/* 2PC server (slave) */

#include "vote.h"
#include "tpc.h"
#include <stdio.h>
#include <stdlib.h>

#define TESTING_SLOW_PROC 0

int result;

int *commit_msg_1_svc(void *v, struct svc_req *req)
{
    int ready = 1; // test value
    if (ready) {
        result = VOTE_COMMIT;
    }
    else {
        /* possible reasons: lack of memory, ... */
        result = VOTE_ABORT;
    }
    /* make the process run slow, so that */
    if (TESTING_SLOW_PROC) {
        sleep(TIME_TO_VOTE + 1);
    }
    return &result;

}

int *commit_vec_1_svc(commit_vec_args *args, struct svc_req *req)
{
    printf("Commiting vector #%d: %lu\n", args->vec_id, args->vec);
    FILE *fp;
    fp = fopen("vectors.txt", "a");
    char buffer[128];
    snprintf(buffer, 128, "%d: %lu", args->vec_id, args->vec);
    fprintf(fp, "%s\n", buffer);
    fclose(fp);
    result = 0;
    return &result;
}
