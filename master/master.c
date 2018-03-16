#include "master.h"
#include "tpc_master.h"
#include "master_rq.h"
#include "../ipc/messages.h"
//#include "../rpc/gen/rq.h"
#include "slavelist.h"
#include "../consistent-hash/ring/src/tree_map.h"
#include "../types/types.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/types.h>

rbt_ptr chash_table;

/**
 * Comparator for machine-vector tuples to sort in ascending order of machine ID.
 * Source: https://en.wikipedia.org/wiki/Qsort
 */
int compare_machine_vec_tuple(const void *p, const void *q) {
    unsigned int x = **((const unsigned int **)p);
    unsigned int y = **((const unsigned int **)q);

    if (x < y)
        return -1;
    else if (x > y)
        return 1;

    return 0;
}

/**
 * Master Process
 *
 * Coordinates/delegates tasks for the system.
 */
int main(int argc, char *argv[])
{
    // XXX: running with defaults for now (r = 3, and hardcoded slave addresses)
    /*
    if (argc < 3) {
        printf("Usage: [-r repl_factor] -s slave_addr1 [... slave_addrn]\n");
        return 1;
    }
    */
    /* Connect to message queue. */
    int msq_id = msgget(MSQ_KEY, MSQ_PERMISSIONS | IPC_CREAT);
    /* Container for messages. */
    struct msgbuf *request;
    struct msqid_ds buf;
    int rc;

    /*
     * insert slaves into the tree
     */
     chash_table = new_rbt();
     int i;
     for (i = 0; i < NUM_SLAVES; i++) {
        struct cache *cptr = (struct cache*) malloc(sizeof(struct cache));
        cptr->cache_id = i;
        cptr->cache_name = SLAVE_ADDR[i];
        cptr->replication_factor = 1;
        insert_cache(chash_table, cptr);
        free(cptr);
    }
    while (true) {
        msgctl(msq_id, IPC_STAT, &buf);

        if (buf.msg_qnum > 0) {


            request = (struct msgbuf *) malloc(sizeof(msgbuf));
//            printf("allocated request\n");
            /* Grab from queue. */
            rc = msgrcv(msq_id, request, sizeof(msgbuf), 0, 0);

            /* Error Checking */
            if (rc < 0) {
                perror( strerror(errno) );
                printf("msgrcv failed, rc = %d\n", rc);
                return EXIT_FAILURE;
            }

            if (request->mtype == mtype_put) {
                // FIXME: generalize this
                /*
                vec_id_t vec_id_mult = request->vector.vec_id *
                    replication_factor;
                vec_id_t vec_id_1 = vec_id_mult;
                vec_id_t vec_id_2 = vec_id_mult + 1;
                vec_id_t vec_id_3 = vec_id_mult + 2;
                char *slave_1 = SLAVE_ADDR[
                    get_machine_for_vector(chash_table, vec_id_1)
                ];
                char *slave_2 = SLAVE_ADDR[
                    get_machine_for_vector(chash_table, vec_id_2)
                ];
                char *slave_3 = SLAVE_ADDR[
                    get_machine_for_vector(chash_table, vec_id_3)
                ];
                */
                if (NUM_SLAVES == 1) {
                    printf("about to put vector\n");
                    //printf("putting %llu\n...\n", value);
                    printf("Putting a vector %u:%llu to %s\n", request->vector.vec_id, request->vector.vec.vector[0], SLAVE_ADDR[0]);
                    printf("commit call\n");
                    commit_vector(request->vector.vec_id, request->vector.vec, SLAVE_ADDR, 1);
                }
                // TODO ensure slave_1 != slave_2 != slave_3
                // TODO: call commit_vector RPC function here
                //commit_vector(request->vector.vec_id, request->vector.vec,
                //    {slave_1, slave_2, slave_3}, 3);
            }
            else if (request->mtype == mtype_range_query) {
                range_query_contents contents = request->range_query;
                int i;
                int num_ints_needed = 0;
                for (i = 0; i < contents.num_ranges; i++) {
                    unsigned int *range = contents.ranges[i];
                    // each range needs this much data:
                    // number of vectors (inside parens), a machine/vector ID
                    // for each one, preceded by the number of vectors to query
                    int row_len = (range[1] - range[0] + 1) * 2 + 1;
                    num_ints_needed += row_len;
                }
                /* this array will eventually include data for the coordinator
                   slave's RPC  as described in the distributed system wiki. */
                unsigned int *range_array = (unsigned int *)
                    malloc(sizeof(unsigned int) * num_ints_needed);
                int array_index = 0;
                for (i = 0; i < contents.num_ranges; i++) {
                    unsigned int *range = contents.ranges[i];
                    vec_id_t j;
                    // start of range is number of vectors
                    range_array[array_index++] = range[1] - range[0] + 1;
                    unsigned int **machine_vec_ptrs = (unsigned int **)
                        malloc(sizeof(int *) * (range[1] - range[0] + 1));
                    for (j = range[0]; j <= range[1]; j++) {
                        unsigned int *tuple = (unsigned int *)
                            malloc(sizeof(unsigned int) * 2);
                        tuple[0] = get_machine_for_vector(chash_table, j);
                        tuple[1] = j;
                        machine_vec_ptrs[j - range[0]] = tuple;
                    }

                    qsort(machine_vec_ptrs, range[1] - range[0],
                        sizeof(unsigned int) * 2, compare_machine_vec_tuple);

                    /* save machine/vec IDs into the array */
                    int tuple_index;
                    for (j = range[0]; j <= range[1]; j++) {
                        tuple_index = j - range[0];
                        range_array[array_index++] =
                            machine_vec_ptrs[tuple_index][0];
                        range_array[array_index++] =
                            machine_vec_ptrs[tuple_index][1];
                    }

                    for (j = range[0]; j <= range[1]; j++) {
                        free(machine_vec_ptrs[j - range[0]]);
                    }
                    free(machine_vec_ptrs);
                }

                init_range_query(range_array, contents.num_ranges, contents.ops, array_index);
            }
            else if (request->mtype == mtype_point_query) {
                /* TODO: Call Jahrme function here */
            }
            free(request);
        }

    }

    return EXIT_SUCCESS;
}

void sigint_handler(int sig)
{
    // TODO free allocated memory (RBT, ...)

    // TODO signal death to slave nodes
}
