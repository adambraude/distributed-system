#include "master.h"
#include "tpc_master.h"
#include "master_rq.h"
#include "slavelist.h"
#include "../consistent-hash/ring/src/tree_map.h"
#include "../types/types.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/ipc.h>
#include <sys/types.h>

/* variables for use in all master functions */
slave_ll *slavelist;
unsigned int slave_id_counter = 1;
unsigned int partition_t;
unsigned int query_plan_t;
int num_slaves;
slave *dead_slave; /* slave presumed dead, waiting to be reawakened (assumes 1 dead slave at a time) */
int separation;

/* Ring CH variables */
rbt_ptr chash_table;

/* static partition variables */
int *partition_scale_1, *partition_scale_2; // partitions and backups
unsigned int num_keys; /* e.g., value of largest known key, plus 1 */

unsigned int max_vector_len;

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
        printf("Usage: -p partition_type -ml max_vector_len [-k num_keys] [-r repl_factor] -s slave_addr1 [... slave_addrn]\n");
        return 1;
    }
    */
    /* Connect to message queue. */
    partition_t = RING_CH;
    int msq_id = msgget(MSQ_KEY, MSQ_PERMISSIONS | IPC_CREAT);
    /* Container for messages. */
    struct msgbuf *request;
    struct msqid_ds buf;
    int rc;

    // SETUP

    /* setup values, acquired from slavelist.h */
    num_slaves = NUM_SLAVES;
    // index in slave list will be the machine ID (0 is master)
    slavelist = (slave_ll *) malloc(sizeof(slave_ll));
    slave_ll *head = slavelist;
    int i;
    for (i = 0; i < num_slaves; i++) {
        slave *s = new_slave(SLAVE_ADDR[i]);
        if (setup_slave(s)) { // could not connect
            printf("MASTER: Could not register machine %s",
                SLAVE_ADDR[i]);
            // dealloc(slavelist)
            return EXIT_FAILURE;
            // s->is_alive = false;
            // dead_slave = s;
            // num_living_slaves--;
        }
        else {
            slavelist->slave_node = s;
            slavelist->next = (slave_ll *) malloc(sizeof(slave_ll));
            slavelist = slavelist->next;
        }
    }
    slavelist->next = NULL;
    slavelist = head;

    /*
     * setup partitions
     */
    switch (partition_t) {
        case RING_CH: {
            chash_table = new_rbt();
            slave_ll *head = slavelist;
            while (head != NULL) {
                struct cache *cptr = (struct cache*) malloc(sizeof(struct cache));
                cptr->id = head->slave_node->id;
                cptr->cache_name = head->slave_node->address;
                cptr->replication_factor = 1;
                insert_cache(chash_table, cptr);
                free(cptr);
                head = head->next;
            }
            break;
        }

        case JUMP_CH: {
            // TODO setup jump
            break;
        }

        case STATIC_PARTITION: {
            int i;
            /* divide key space among the nodes
             * take the number of keys, divide by the number of slaves, then
             * assign each slave to a partition */
            slave_ll *head = slavelist;
            for (i = 0; i < num_slaves; i++) {
                partition_scale_1[i] = head->slave_node->id;
                partition_scale_2[(i + 1) % num_slaves] = head->slave_node->id;
                head = head->next;
            }
            separation = num_keys / num_slaves;
            break;
        }
    }

    /* message receipt loop */
    while (true) {
        msgctl(msq_id, IPC_STAT, &buf);
        heartbeat(); // TODO: this can be called elsewhere
        if (buf.msg_qnum > 0) {

            request = (struct msgbuf *) malloc(sizeof(msgbuf));
            /* Grab from queue. */
            // TODO fill in messages
            rc = msgrcv(msq_id, request, sizeof(msgbuf), 0, 0);

            /* Error Checking */
            if (rc < 0) {
                perror( strerror(errno) );
                printf("msgrcv failed, rc = %d\n", rc);
                continue;
            }

            if (request->mtype == mtype_put) {
                //slave **commit_slaves = malloc(sizeof(slave*) * replication_factor);
                slave *commit_slaves[replication_factor];
                unsigned int *slave_ids =
                    get_machines_for_vector(request->vector.vec_id);
                slave_ll *head = slavelist;
                int cs_index = 0;
                for (i = 0; i < num_slaves; i++) {
                    if (head->slave_node->id == slave_ids[0] ||
                        head->slave_node->id == slave_ids[1])
                        commit_slaves[cs_index++] = head->slave_node;
                    if (cs_index == replication_factor - 1) break;
                    head = head->next;
                }
                int commit_res = commit_vector(request->vector.vec_id, request->vector.vec,
                    commit_slaves, replication_factor);
                if (commit_res)
                    heartbeat();
            }
            else if (request->mtype == mtype_range_query) {
                range_query_contents contents = request->range_query;
                switch (query_plan_t) { // TODO: fill in cases
                    case STARFISH: {
                        while (starfish(contents))
                            heartbeat();
                        break;
                    }
                    case UNISTAR: {
                        break;
                    }
                    case MULTISTAR: {
                        break;
                    }
                    case ITER_PRIM: {
                        break;
                    }
                }
            }
            else if (request->mtype == mtype_point_query) {
                /* TODO: Call Jahrme function here */
            }
            free(request);
        }
    }

    /* deallocation */
    while (slavelist != NULL) {
        free(slavelist->slave_node);
        slave_ll *temp = slavelist->next;
        free(slavelist);
        slavelist = temp;
    }

}

int starfish(range_query_contents contents)
{
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

            tuple[0] = get_machines_for_vector(j)[0];
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
    return init_range_query(range_array, contents.num_ranges,
        contents.ops, array_index);
}
/**
 * Send out a heartbeat to every slave. If you don't get a response, perform
 * a reallocation of the vectors it held to machines that don't already
 * have them.
 *
 */
void heartbeat()
{
    slave_ll *head = slavelist;
    while (head != NULL) {
        char *addr = head->slave_node->address;
        int id = head->slave_node->id;
        head = head->next;
        if (!is_alive(addr)) {
            printf("Machine %s failed\n", addr);
            remove_slave(id);
            reallocate();
        }
    }
}

/**
 * Removes the slave with the given ID from the list. (If it's not there,
 * a segmentation fault will occur)
 */
int remove_slave(unsigned int slave_id)
{
    slave_ll **head = &slavelist;
    /* Look for the address of the node to remove... */
    while ((*head)->slave_node->id != slave_id)
        head = &(*head)->next;
    dead_slave = *head;
    /* ...and just remove it (Torvalds-style) */
    *head = (*head)->next;
    num_slaves--;
    return EXIT_SUCCESS;
}

void reallocate()
{
    // take the dead slave, reallocate its vectors to other machines
    // assumes that none of the vectors die in the process
    switch (partition_t) {
        case RING_CH: {
            unsigned int pred_id = ring_get_pred_id(chash_table, dead_slave->id);
            unsigned int succ_id = ring_get_succ_id(chash_table, dead_slave->id);
            unsigned int sucsuc_id = ring_get_succ_id(chash_table, succ_id);
            slave_ll *head = slavelist;
            slave *pred, *succ, *sucsuc;
            // TODO could skip this step by storing the nodes in the tree
            // instead of having to find them this way
            while (head != NULL) {
                if (head->slave_node->id == pred_id) pred = head->slave_node;
                if (head->slave_node->id == succ_id) succ = head->slave_node;
                if (head->slave_node->id == sucsuc_id) sucsuc = head->slave_node;
                head = head->next;
            }
            // transfer dead node's vectors from its predecessor to its successor
            slave_vector *vec = pred->vectors;
            if (pred != succ) {
                while (vec != NULL) {
                    send_vector(pred, vec->id, succ); // TODO: code this RPC
                    vec = vec->next;
                }
            }
            vec = succ->vectors;
            // transfer successor's nodes to its successor as its backup
            while (vec != NULL) {
                send_vector(succ, vec->id, sucsuc);
                vec = vec->next;
            }
            delete_entry(chash_table, dead_slave->id);
        }
    }
}

/**
 * Returns replication_factor (currently, hard at 2)-tuple of vectors such
 * that t = (m1, m2) and m1 != m2 if there at least 2 slaves available.
 */
unsigned int *get_machines_for_vector(vec_id_t vec_id)
{
    switch (partition_t) {
        case RING_CH: {
            return ring_get_machines_for_vector(chash_table, vec_id);
        }

        case JUMP_CH: {
            // TODO Jahrme
            return NULL;
        }

        case STATIC_PARTITION: {
            unsigned int *machines = (unsigned int *)
                malloc(sizeof(unsigned int) * replication_factor);
            int index = vec_id / separation;
            assert(index >= 0 && index < num_slaves);
            machines[0] = partition_scale_1[index];
            machines[1] = partition_scale_2[index];
            return machines;
        }

        default: {
            return NULL;
        }
    }
}

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

/* Ring consistent hashing variables */

int get_new_slave_id(void)
{
    return slave_id_counter++;
}

slave *new_slave(char *address)
{
    slave *s = (slave *) malloc(sizeof(slave));
    s->id = get_new_slave_id();
    memcpy(s->address, address, strlen(address));
    s->is_alive = true;
    return s;
}

void sigint_handler(int sig)
{
    // TODO free allocated memory (RBT, ...)

    // TODO signal death to slave nodes
}
