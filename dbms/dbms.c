#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../ipc/messages.h"
#include "../types/types.h"
#include "../bitmap-vector/read_vec.h"
#include "../experiments/exper.h"
#include "../experiments/fault_tolerance.h"
#include "../experiments/load_vec.h"
#include "../util/ipc_util.h"

#include "dbms.h"
#include "dbms-helper.h"

/**
 * Database Management System (DBMS)
 *
 * Imitate the interactions coming from a true DBMS.
 */
int main(int argc, char *argv[])
{
    /* Create status check. */
    int status = EXIT_SUCCESS;

    /* Create message queue. */
    int msq_id = msgget(MSQ_KEY, MSQ_PERMISSIONS | IPC_CREAT);

    /* Perform correct experiment. */
    switch (EXPERIMENT_TYPE) {
        case LOAD_VECTORS: {
            status = experiment_load_vectors(msq_id);
            break;
        }
        case FAULT_TOLERANCE: {
            status = experiment_fault_tolerance(msq_id);
            break;
        }
        case SLAVE_INTRO: {
            status = experiment_introduce_slaves(msq_id);
            break;
        }
        default: {
            break;
        }
    }

    /* Destroy message queue. */
    msgctl(msq_id, IPC_RMID, NULL);

    return status;
}

int experiment_load_vectors(int msq_id)
{
    puts("DBMS: starting experiment: load vectors");

    /* Accounting Variables */
    char buf[64];
    int i, j;
    int num_vecs = LV_START;
    int status = EXIT_SUCCESS;
    pid_t master_pid = -1;

    /* Time Variables */
    // TODO: current time string function
    struct timespec start, stop;
    int64_t dt_s, dt_ns;

    char timestamp[32];
    getcurr_timestamp(timestamp, sizeof(timestamp));

    /* Output Variables */
    char outfile_nmbuf[36];
    snprintf(outfile_nmbuf, 36, "%s-%s.csv", LV_OUTFILE, timestamp);
    FILE *fp = fopen(outfile_nmbuf, "w");
    fprintf(fp, "num_vecs,time(sec)\n");

    for (i = 0; i < LV_NUM_TRIALS; i++) {
        printf("Starting trial %d; distributing %d vectors.\n", i, num_vecs);

        // Spawn a new master for each trial.
        master_pid = spawn_master(master_pid);

        // Time each trial.
        clock_gettime(CLOCK_REALTIME, &start);
        for (j = 0; j < num_vecs; j++) {
            snprintf(buf, 64, "/root/dbie/dbie-data/tpc/vecs/v_%d.dat", j);
            put_vector(msq_id, j, read_vector(buf));
        }
        master_exit(msq_id);
        waitpid(master_pid, &status, 0);
        clock_gettime(CLOCK_REALTIME, &stop);

        /* Calculate time difference */
        dt_s = stop.tv_sec - start.tv_sec;
        dt_ns = stop.tv_nsec - start.tv_nsec;

        /* Adjust if negative nanosecond change */
        if (dt_ns < 0) {
            dt_ns += 1e9;
            dt_s += -1;
        }

        // Record trial time.
        fprintf(fp, "%d,%ld.%09ld\n", num_vecs, dt_s, dt_ns);
        printf("Trial took %ld.%09ld seconds.\n", dt_s, dt_ns);

        // Next trial.
        num_vecs *= LV_MULTIPLIER;
    }

    fclose(fp);

    return status;
}

int experiment_fault_tolerance(int msq_id)
{
    puts("DBMS: starting experiment: fault tolerance");

    /* Accounting Variables */
    int i;
    char buf[64];
    int status = EXIT_SUCCESS;
    pid_t master_pid = -1;
    char *line = NULL;
    size_t n = 0;
    int qnum = 0;
    char data_dir[] = "/root/dbie/dbie-data/tpc";

    /* Open Query File */
    FILE *fp;
    switch(QUERY_TYPE) {
        case POINT_QUERIES: {
            snprintf(buf, 64, "%s/qs/point.dat", data_dir);
            break;
        }
        case RANGE_QUERIES: {
            snprintf(buf, 64, "%s/qs/range.dat", data_dir);
            break;
        }
        case MIXED_QUERIES: {
            snprintf(buf, 64, "%s/qs/mixed.dat", data_dir);
            break;
        }
        default: {
            break;
        }
    }
    fp = fopen(buf, "r");
    if (fp == NULL) {
        puts("DBMS: error: could not open query file");
        return 1;
    }

    /* Open Query File */
    master_pid = spawn_master(master_pid);

    /* Open Query File */
    puts("DBMS: Sending vectors...");
    for (i = 0; i < FT_NUM_VEC; i++) {
        snprintf(buf, 64, "%s/vecs/v_%d.dat", data_dir, i);
        put_vector(msq_id, i, read_vector(buf));
    }

    /* Open Query File */
    puts("DBMS: Sending queries...");
    while (getline(&line, &n, fp) != -1 && qnum++ < FT_NUM_QUERIES) {
        query(msq_id, line);
    }

    /* Wait for master completion. */
    master_exit(msq_id);
    puts("DBMS: Waiting on master...");
    waitpid(master_pid, &status, 0);
    return status;
}

int experiment_introduce_slaves(int msq_id)
{
    puts("DBMS: starting experiment: slave introduction");
    
    int status = EXIT_SUCCESS;

    puts("DBMS: Sending vectors...");
    int i;
    char buf[64];
    for (i = 0; i < FT_NUM_VEC; i++) {
        snprintf(buf, 64, "../tst_data/vecs/v_%d.dat", i);
        put_vector(msq_id, i, read_vector(buf));
    }

    if (DBMS_DEBUG) puts("DBMS: Adding slaves by address...");
    
    char addr1[16] = "172.17.0.7";
    char addr2[16] = "172.17.0.8";
    char addr3[16] = "172.17.0.9";
    char addr4[16] = "172.17.0.10";

    slave_intro(msq_id, addr1);
    slave_intro(msq_id, addr2);
    slave_intro(msq_id, addr3);
    slave_intro(msq_id, addr4);

    master_exit(msq_id);
    return status;
}
