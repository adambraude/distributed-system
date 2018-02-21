#include "master.h"
#include "messages.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/types.h>

/**
 * Master Process
 *
 * Coordinates/delegates tasks for the system.
 */
int main(int argc, char *argv[])
{
    /* Connect to message queue. */
    int msq_id = msgget(MSQ_KEY, MSQ_PERMISSIONS | IPC_CREAT);

    /* Container for messages. */
    struct put_msgbuf *request;
    struct msqid_ds buf;
    int rc;

    bool dying = false;

    sleep(2);

    while (true) {
        msgctl(msq_id, IPC_STAT, &buf);
        printf("Items in Queue: %i\n", buf.msg_qnum);

        sleep(1);

        if (buf.msg_qnum > 0) {
            printf("Popping off queue.\n");

            request = (struct put_msgbuf *) malloc(sizeof(struct put_msgbuf));

            /* Grab from queue. */
            rc = msgrcv(msq_id, request, sizeof(struct put_msgbuf), 0, 0);

            /* Error Checking */
            if (rc < 0) {
                perror( strerror(errno) );
                printf("msgrcv failed, rc = %d\n", rc);
                return EXIT_FAILURE;
            }

            /* Check for death signal. */
            if (request->mtype == mtype_kill_master) {
                dying = true;
                break;
            }

            printf("%i := 0x%llx\n", request->vector.vec_id, request->vector.vec);
        }

        if (dying) break;
    }

    if (dying) {
        struct put_msgbuf signal = {mtype_master_dying, {0,0} };
        msgsnd(msq_id, &signal, sizeof(struct put_msgbuf), 0);
    }

    //  while (queue is empty)
    //      while (queue is not empty)
    //          pop message from queue
    //          2-phase commit agreement w/ slaves
    //          if all agree, make RPC calls w/ message args
    return EXIT_SUCCESS;
}
