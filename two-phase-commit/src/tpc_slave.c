/* 2PC server (slave) */

#include "vote.h"
#include "tpc.h"
#include <stdio.h>
#include <stdlib.h>

#define TESTING_MASTER_DEATH 0

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
    if (TESTING_MASTER_DEATH) {
        sleep(5); /* kill master process! */
    }
    return &result;

}

int *commit_vec_1_svc(commit_vec_args *args, struct svc_req *req)
{
    printf("Commiting vector #%d: %llu\n", args->vec_id, args->vec);
    FILE *fp;
    fp = fopen("vectors.txt", "a");
    perror("fopen");
    char buffer[128];
    snprintf(buffer, 128, "%d: %llu", args->vec_id, args->vec);
    fprintf(fp, "%s\n", buffer);
    perror("fwrite");
    fclose(fp);
    result = 0;
    return &result;
}
