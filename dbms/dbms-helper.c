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

#include "../bitmap-vector/read_vec.h"
#include "../ipc/messages.h"
#include "../types/types.h"

#include "dbms-helper.h"

int slave_intro(int queue_id, char address[16])
{
    message_buffer *intro = (message_buffer *) malloc(sizeof(message_buffer));
    intro->mtype = mtype_slave_intro;

    // memset(intro->address, 0, sizeof(intro->address));
    strncpy(intro->address, address, sizeof(intro->address));
    intro->address[15] = '\0';

    msgsnd(queue_id, intro, sizeof(message_buffer), 0);

    free(intro);

    return EXIT_SUCCESS;
}

int put_vector(int queue_id, vec_id_t vec_id, vec_t *vec)
{
    message_buffer *put = (message_buffer *) malloc(sizeof(message_buffer));
    put->mtype = mtype_put;

    assigned_vector *av = (assigned_vector *) malloc(sizeof(assigned_vector));
    av->vec_id = vec_id;
    av->vec = *vec;

    put->vector = *av;

    msgsnd(queue_id, put, sizeof(message_buffer), 0);

    return EXIT_SUCCESS;
}

int query(int queue_id, char *query_str)
{
    if (query_str[0] == 'P') {
        return point_query(queue_id, query_str);
    } else if (query_str[0] == 'R') {
        return range_query(queue_id, query_str);
    } else {
        return EXIT_FAILURE;
    }
}

int point_query(int queue_id, char *query_str)
{
    message_buffer *point = (message_buffer *) malloc(sizeof(message_buffer));

    point->mtype = mtype_point_query;
    point->point_vec_id = (vec_id_t) atoi(strtok(query_str, "P:"));

    msgsnd(queue_id, point, sizeof(message_buffer), 0);
    free(point);
    return EXIT_SUCCESS;
}

/*
* for now, assume that all interbin ops are &
* Format: "R:[b1,b2]<op1>[b3,b4]<op2>..."
* where op is & or |
*/
int range_query(int queue_id, char *query_str)
{
   message_buffer *range = (message_buffer *) malloc(sizeof(message_buffer));
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
   msgsnd(queue_id, range, sizeof(message_buffer), 0);
   free(contents);
   free(ops);
   for (i = 0; i < num_ranges; i++)
       free(ranges[i]); // free bounds
   free(ranges);
   free(range);
   return EXIT_SUCCESS;
}

int master_exit(int queue_id)
{
    message_buffer *buf = (message_buffer *) malloc(sizeof(message_buffer));
    buf->mtype = mtype_kill_master;
    msgsnd(queue_id, buf, sizeof(message_buffer), 0);
    free(buf);
    return 0;
}

pid_t spawn_master(pid_t master_pid)
{
    switch (master_pid = fork()) {
        case -1: {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        case 0: {
            char *master_args[3];
            master_args[0] = MASTER_EXECUTABLE;
            master_args[1] = REPLICATION_FACTOR;
            master_args[2] = NULL;
            int master_exit_status = execv(MASTER_EXECUTABLE, master_args);
            exit(master_exit_status);
        }
        default:
            break;
   }
    return master_pid;
}
