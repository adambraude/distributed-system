#include "dbms.h"
#include "../ipc/messages.h"
#include "../types/types.h"
#include "../bitmap-vector/read_vec.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>


void
test_put_vec(int queue_id, int vec_id, u_int64_t *vector, u_int vector_len)
{
    vec_t *v = (vec_t *)malloc(sizeof(vec_t));
    memcpy(v->vector, vector, sizeof(u_int64_t) * vector_len);
    v->vector_length = vector_len;
    put_vector(queue_id, vec_id, v);
}

// XXX: deprecate change to: test_put_vec, of any length
void test_put_vec_len_1(int queue_id, int vec_id, unsigned long long vec) {
    vec_t *v = (vec_t *)malloc(sizeof(vec_t));
    unsigned long long varr[] = {vec};
    memcpy(v->vector, varr, sizeof(unsigned long long));
    v->vector[0] = vec;
    v->vector_length = 1;
    put_vector(queue_id, vec_id, v);
}

void test_put_vec_len_2(int queue_id, int vec_id, unsigned long long vec, unsigned long long vec2) {
    vec_t *v = (vec_t *)malloc(sizeof(vec_t));
    unsigned long long varr[] = {vec, vec2};
    memcpy(v->vector, varr, sizeof(unsigned long long) * 2);
    v->vector[0] = vec;
    v->vector[1] = vec2;
    v->vector_length = 2;
    put_vector(queue_id, vec_id, v);
}
/**
 * Database Management System (DBMS)
 *
 * Imitate the interactions coming from a true DBMS.
 */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        return 1;
    }

    /* SPAWN MASTER */

    bool master_exists = false;
    pid_t master_pid = -1;
    if (!master_exists) switch (master_pid = fork()) {
        case -1:
            perror("fork");
            exit(EXIT_FAILURE);
        case 0:
            char *argv[3] = {MASTER_EXECUTABLE, REPLICATION_FACTOR, NULL};
            int master_exit_status = execv(MASTER_EXECUTABLE, argv);
            exit(master_exit_status);
        default:
            master_exists = true;
    }

    /* SETUP IPC */

    /* Create message queue. */
    int msq_id = msgget(MSQ_KEY, MSQ_PERMISSIONS | IPC_CREAT);

    /* TESTS */
    int test_no = atoi(argv[1]);

    int return_val = EXIT_SUCCESS;
    if (test_no == BASIC_TEST) {

        /* put some vectors */
        test_put_vec_len_1(msq_id, 1, 0x00ab);
        test_put_vec_len_1(msq_id, 2, 0x10c0);
        test_put_vec_len_1(msq_id, 3, 0x0781);
        test_put_vec_len_1(msq_id, 4, 0x99ff);

        test_put_vec_len_1(msq_id, 5, 43776);
        test_put_vec_len_1(msq_id, 6, 49168);
        test_put_vec_len_1(msq_id, 7, 0x8170);
        test_put_vec_len_1(msq_id, 8, 0xff99);

        test_put_vec_len_2(msq_id, 9, 867, 79425);
        test_put_vec_len_2(msq_id, 10, 867, 53102);

        /* testing range query */
        char range[] = "R:[1,4]&[5,8]";
        range_query(msq_id, range);
        char range2[] = "R:[1,2]&[3,4]";
        range_query(msq_id, range2);
        char range3[] = "R:[9,10]";
        range_query(msq_id, range3);

    }
    else if (test_no == POLITICAL_DATA_TEST) {
        /* PUT vectors */
        int num_test_files = 32;
        int i;
        char buf[64];
        for (i = 0; i < num_test_files; i++) {
            snprintf(buf, 64, "../tst_data/rep-votes/voting_data/v_%d.dat", i);
            /* TODO: compress the vector (just using literals for now) */
            // blockSeg *seg = (blockSeg *) malloc(sizeof(blockSeg));
            // seg->toCompress = vector->vector;
            // seg->size = vector->vector_length;
            put_vector(msq_id, i, read_vector(buf));
        }

        /* querying */
        //char range[] = "R:[0,3]&[4,6]&[15,18]&[20,31]";
        char range[] = "R:[0,1]";
        range_query(msq_id, range);
    }
    else {
        return_val = 1;
    }


    /* CLEANING UP */
    int master_result = 0;
    /* Reap master. */
    wait(&master_result);

    /* Destroy message queue. */
    msgctl(msq_id, IPC_RMID, NULL);

    return return_val;
}

int put_vector(int queue_id, vec_id_t vec_id, vec_t *vec)
{
    msgbuf *put = (msgbuf *) malloc(sizeof(msgbuf));
    put->mtype = mtype_put;
    assigned_vector *av = (assigned_vector *) malloc(sizeof(assigned_vector));
    av->vec_id = vec_id;
    av->vec = *vec;
    put->vector = *av;

    msgsnd(queue_id, put, sizeof(msgbuf), 0);
    // free(vec);
    // free(av);
    // free(put);
    return EXIT_SUCCESS;
}

int point_query(int queue_id, char *query_str)
{
    msgbuf *point = (msgbuf *) malloc(sizeof(msgbuf));
    point->mtype = mtype_point_query;
    point->point_vec_id = (vec_id_t) atoi(strtok(query_str, "P:"));
    msgsnd(queue_id, point, sizeof(point), 0);
    return EXIT_SUCCESS;
}

/*
* for now, assume that all interbin ops are &
* Format: "R:[b1,b2]<op1>[b3,b4]<op2>..."
* where op is & or |
*/
int range_query(int queue_id, char *query_str)
{
   msgbuf *range = (msgbuf *) malloc(sizeof(msgbuf));
   range->mtype = mtype_range_query;
   // first pass: figure out how many ranges are in the query, to figure out
   // how much memory to allocate
   int i;
   unsigned int num_ranges = 0;
   i = 0;
   while (query_str[i] != '\0') {
       if (query_str[i++] == ',') num_ranges++;
   }
   static char delim[] = "[,]";
   char *token = strtok(query_str, "R:");
   token = strtok(token, delim); // grab first element
   char *ops = (char *) malloc(sizeof(char) * num_ranges);
   /* Allocate 2D array of range pointers */
   vec_id_t **ranges = (vec_id_t **) malloc(sizeof(vec_id_t *) * num_ranges);

   /* parse out ranges and operators */
   num_ranges = 0;
   int r1, r2;
   while (token != NULL) {
       r1 = atoi(token);
       token = strtok(NULL, delim);
       r2 = atoi(token);
       vec_id_t *bounds = (vec_id_t *) malloc(sizeof(vec_id_t) * 2);
       bounds[0] = r1;
       bounds[1] = r2;
       ranges[num_ranges] = bounds;
       token = strtok(NULL, delim);
       if (token != NULL)
           ops[num_ranges] = token[0];
       token = strtok(NULL, delim);
       num_ranges++;
   }
   range_query_contents *contents = (range_query_contents *)
       malloc(sizeof(range_query_contents));
   // fill in the data
   for (i = 0; i < num_ranges; i++) {
       contents->ranges[i][0] = ranges[i][0];
       contents->ranges[i][1] = ranges[i][1];
       if (i != num_ranges - 1) contents->ops[i] = ops[i];
   }

   contents->num_ranges = num_ranges;
   range->range_query = *contents;
   msgsnd(queue_id, range, sizeof(msgbuf), 0);
   free(contents);
   free(ops);
   for (i = 0; i < num_ranges; i++)
       free(ranges[i]); // free bounds
   free(ranges);
   free(range);
   return EXIT_SUCCESS;
}
