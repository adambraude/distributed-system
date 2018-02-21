#include "dbms.h"
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
 * Database Management System (DBMS)
 *
 * Imitate the interactions coming from a true DBMS.
 */
int main(int argc, char *argv[])
{
    printf("Starting DBMS...\n");

    /* SPAWN MASTER */

    bool master_exists = false;
    pid_t master_pid = -1;
    printf("Ready to fork...\n");
    if (!master_exists) switch (master_pid = fork()) {
        case -1:
            printf("Master process creation failed.\n");
            perror("fork");
            exit(EXIT_FAILURE);
        case 0:
            printf("Becomming master...\n");
            master_exists = true;
            char *argv[3] = {MASTER_EXECUTABLE, REPLICATION_FACTOR};
            int master_exit_status = execv(MASTER_EXECUTABLE, argv);
            exit(master_exit_status);
        default:
            printf("Resuming DBMS role...\n");
            master_exists = true;
    }

    /* SETUP IPC */

    /* Create message queue. */
    int msq_id = msgget(MSQ_KEY, MSQ_PERMISSIONS | IPC_CREAT);

    printf("Ready to send vectors...\n");
    put_vector(msq_id, 1, 0x00ab);
    put_vector(msq_id, 2, 0x10c0);
    put_vector(msq_id, 3, 0x0781);
    put_vector(msq_id, 4, 0x99ff);

    sleep(10);

    /* CLEANING UP */

    /* Destroy message queue. */
    msgctl(msq_id, IPC_RMID, NULL);
    /* Reap master. */
    int master_result = 0;
    wait(&master_result);
    printf("Master returned: %i\n", master_result);

    printf("Stopping DBMS...\n");
    return EXIT_SUCCESS;
}

int put_vector(int queue_id, int vec_id, unsigned long long vec)
{
    put_msgbuf *put = (struct put_msgbuf *) malloc(sizeof(struct put_msgbuf));
    put->mtype = mtype_put;
    put->vector = (assigned_vector) { vec_id, vec };
    printf("SEND: data at %llu\n", put);
    msgsnd(queue_id, put, sizeof(put_msgbuf), 0);
    return EXIT_SUCCESS;
}
