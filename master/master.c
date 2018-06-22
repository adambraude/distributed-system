#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <time.h>

#include <sys/ipc.h>
#include <sys/types.h>

#include "master.h"
#include "master_rq.h"
#include "tpc_master.h"
#include "../bitmap-vector/read_vec.h"
#include "../consistent-hash/ring/src/tree_map.h"
#include "../types/types.h"
#include "../util/ds_util.h"

#define TEST_NUM_VECTORS 5000
#define TEST_MAX_LINE_LEN 5120

#define MS_DEBUG false

char **slave_addresses;

/* variables for use in all master functions */
slave_ll *slavelist;
u_int slave_id_counter = 0;
partition_t partition;
query_plan_t query_plan;
u_int num_slaves;

/*
 * slave presumed dead,
 * waiting to be reawakened (assumes 1 dead slave at a time)
 */
slave *dead_slave;
int separation;

/* Ring CH variables */
rbt_ptr chash_table;

/* static partition variables */
int *partition_scale_1, *partition_scale_2; /* partitions and backups */
u_int num_keys; /* e.g., value of largest known key, plus 1 */

u_int max_vector_len;

/**
 * Master Process
 *
 * Coordinates/delegates tasks for the system.
 */
int main(int argc, char *argv[])
{
    int i, j;
    partition = RING_CH;
    query_plan = STARFISH;

    /* Holder for a dead slave */
    dead_slave = NULL;

    int c;
    num_slaves = fill_slave_arr(SLAVELIST_PATH, &slave_addresses);
    if (num_slaves == -1) {
        return 1;
    }
    if (num_slaves == 1) replication_factor = 1;

    /* index in slave list will be the machine ID (0 is master) */
    slavelist = (slave_ll *) malloc(sizeof(slave_ll));
    slave_ll *head = slavelist;
    for (i = 0; i < num_slaves; i++) {
        slave *s = new_slave(slave_addresses[i]);
        if (!setup_slave(s)) { /* connected to slave? */
            slavelist->slave_node = s;
            if (i < num_slaves - 1) {
                slavelist->next = (slave_ll *) malloc(sizeof(slave_ll));
                slavelist = slavelist->next;
            }
        }
    }
    /* terminate linked-list */
    slavelist->next = NULL;
    slavelist = head;

    /*
     * setup partitions
     */
    switch (partition) {
        case RING_CH: {
            chash_table = new_rbt();
            slave_ll *head = slavelist;
            while (head != NULL) {
                cache *cptr = (cache*) malloc(sizeof(cache));
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
            /* TODO setup jump */
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
    int msq_id = msgget(MSQ_KEY, MSQ_PERMISSIONS | IPC_CREAT), rc, qnum = 0;
    struct msgbuf *request;
    struct msqid_ds buf;
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
                printf("Putting vector %d\n", request->vector.vec_id);
                slave **commit_slaves =
                    get_machines_for_vector(request->vector.vec_id, true);
                int commit_res = commit_vector(request->vector.vec_id, request->vector.vec,
                    commit_slaves, replication_factor);
                if (commit_res)
                    heartbeat();
            }
            else if (request->mtype == mtype_range_query) {
                printf("Range query %d\n", ++qnum);
                range_query_contents contents = request->range_query;
                switch (query_plan) { // TODO: fill in cases
                    case STARFISH: {
                        while (starfish(contents))
                            heartbeat();
                        break;
                    }
                    case UNISTAR: {

                    }
                    case MULTISTAR: {

                    }
                    case ITER_PRIM: {

                    }
                    default: {

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
        free(slavelist->slave_node->address);
        free(slavelist->slave_node);
        slave_ll *temp = slavelist->next;
        free(slavelist);
        slavelist = temp;
    }
    if (dead_slave != NULL) {
        free(dead_slave->address);
        free(dead_slave);
    }
}

int starfish(range_query_contents contents)
{
    int i;
    int num_ints_needed = 0;
    for (i = 0; i < contents.num_ranges; i++) {
        u_int *range = contents.ranges[i];
        // each range needs this much data:
        // number of vectors (inside parens), a machine/vector ID
        // for each one, preceded by the number of vectors to query
        int row_len = (range[1] - range[0] + 1) * 2 + 1;
        num_ints_needed += row_len;
    }

    /* this array will eventually include data for the coordinator
       slave's RPC  as described in the distributed system wiki. */

    /* NB: freed in master_rq */
    u_int *range_array = (u_int *) malloc(sizeof(u_int) * num_ints_needed);
    int array_index = 0;
    bool flip = true;
    for (i = 0; i < contents.num_ranges; i++) {
        u_int *range = contents.ranges[i];
        vec_id_t j;
        // FIXME: clarify this code by naming range1, range0
        // start of range is number of vectors
        u_int range_len = range[1] - range[0] + 1;
        range_array[array_index++] = range_len;
        u_int **machine_vec_ptrs = (u_int **) malloc(sizeof(int *) * range_len);
        for (j = range[0]; j <= range[1]; j++) {
            slave **tuple = get_machines_for_vector(j, false);
            if (flip) {
                slave *temp = tuple[0];
                tuple[0] = tuple[1];
                tuple[1] = temp;
            }
            int mvp_addr = j - range[0];
            machine_vec_ptrs[mvp_addr] = (u_int *) malloc(sizeof(u_int) * 2);
            machine_vec_ptrs[mvp_addr][0] = tuple[0]->id;
            machine_vec_ptrs[mvp_addr][1] = j;
            free(tuple);
        }

        qsort(machine_vec_ptrs, range_len,
            sizeof(u_int) * 2, compare_machine_vec_tuple);
        flip = !flip;

        /* save machine/vec IDs into the array */
        int tuple_index;
        for (j = range[0]; j <= range[1]; j++) {
            tuple_index = j - range[0];
            range_array[array_index++] = machine_vec_ptrs[tuple_index][0];
            range_array[array_index++] = machine_vec_ptrs[tuple_index][1];
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
int heartbeat()
{
    slave_ll *head = slavelist;
    while (head != NULL) {
        char *addr = head->slave_node->address;
        int id = head->slave_node->id;
        head = head->next;
        if (!is_alive(addr)) {
            remove_slave(id);
            if (num_slaves == 0) {
                return 1;
            }
            reallocate();
        }
    }
    return 0;
}

/**
 * Removes the slave with the given ID from the list. (If it's not there,
 * a segmentation fault will occur)
 */
int remove_slave(u_int slave_id)
{
    slave_ll **head = &slavelist;
    /* Look for the address of the node to remove... */
    while ((*head)->slave_node->id != slave_id)
        head = &(*head)->next;
    dead_slave = (*head)->slave_node;
    /* ...and just remove it (Torvalds-style) */
    *head = (*head)->next;
    if (--num_slaves == 1) replication_factor = 1;
    return EXIT_SUCCESS;
}

void reallocate()
{
    switch (partition) {
        case RING_CH: {
            u_int pred_id = ring_get_pred_id(chash_table, dead_slave->id);
            u_int succ_id = ring_get_succ_id(chash_table, dead_slave->id);
            u_int sucsuc_id = ring_get_succ_id(chash_table, succ_id);
            print_tree(chash_table, chash_table->root);
            printf("Dead: %d Pred: %d Succ: %d Sucsuc: %d\n",dead_slave->id, pred_id, succ_id, sucsuc_id);
            slave_ll *head = slavelist;
            slave *pred, *succ, *sucsuc;
            // TODO could skip this step by storing the nodes in the tree
            // instead of having to find them this way
            for (; head != NULL; head = head->next) {
                if (head->slave_node->id == pred_id) pred = head->slave_node;
                if (head->slave_node->id == succ_id) succ = head->slave_node;
                if (head->slave_node->id == sucsuc_id)
                    sucsuc = head->slave_node;
            }

            slave_vector *vec = pred->primary_vector_head;
            if (pred != succ) {
                for (; vec != NULL; vec = vec->next) {
                    send_vector(pred, vec->id, succ);
                }
            }

            vec = dead_slave->primary_vector_head;
            // transfer successor's nodes to its successor as its backup
            for (; vec != NULL; vec = vec->next) {
                send_vector(succ, vec->id, sucsuc);
            }

            // join dead node's linked list with the successor
            dead_slave->primary_vector_head->next = succ->primary_vector_tail;
            succ->primary_vector_tail = dead_slave->primary_vector_tail;

            delete_entry(chash_table, dead_slave->id);
            break;
        }

        case JUMP_CH: {
            break;
        }

        case STATIC_PARTITION: {
            break;
        }
    }
}

/**
 * Returns replication_factor (currently, hard at 2)-tuple of vectors such
 * that t = (m1, m2) and m1 != m2 if there at least 2 slaves available.
 * If this is a *new* vector, updating should be true, false otherwise.
 */
slave **get_machines_for_vector(vec_id_t vec_id, bool updating)
{
    switch (partition) {
        case RING_CH: {
            u_int *tr = ring_get_machines_for_vector(chash_table, vec_id);
            if (updating) {
                // update this slave's primary vectors
                slave_ll *head = slavelist;
                while (head->slave_node->id != tr[0]) head = head->next;
                slave *slv = head->slave_node;
                // update this slave's primary vector list
                if (slv->primary_vector_head == NULL) { /* insert it at the head and tail */
                    slv->primary_vector_head = (slave_vector *) malloc(sizeof(slave_vector));
                    slv->primary_vector_head->id = vec_id;
                    slv->primary_vector_head->next = NULL;
                    slv->primary_vector_tail = slv->primary_vector_head;
                }
                else { /* insert it at the tail */
                    slave_vector *vec = (slave_vector *) malloc(sizeof(slave_vector));
                    slv->primary_vector_tail->next = vec;
                    slv->primary_vector_tail = vec;
                    vec->id = vec_id;
                    vec->next = NULL;
                }
            }
            slave **tr_slaves = (slave **) malloc(sizeof(slave*) * replication_factor);
            int i;
            for (i = 0; i < replication_factor; i++) {
                slave_ll *node = slavelist;
                for (; node != NULL; node = node->next) {
                    if (node->slave_node->id == tr[i])
                        tr_slaves[i] = node->slave_node;
                }
            }
            return tr_slaves;
        }

        case JUMP_CH: {
            // TODO Jahrme
            return NULL;
        }

        case STATIC_PARTITION: {
            u_int *machines = (u_int *)
                malloc(sizeof(u_int) * replication_factor);
            int index = vec_id / separation;
            assert(index >= 0 && index < num_slaves);
            machines[0] = partition_scale_1[index];
            machines[1] = partition_scale_2[index];
            // return machines;
            return NULL;
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
    u_int x = **((const u_int **)p);
    u_int y = **((const u_int **)q);

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
    s->address = malloc(strlen(address) + 1);
    strcpy(s->address, address);
    s->is_alive = true;
    s->primary_vector_head = NULL;
    s->primary_vector_tail = NULL;
    return s;
}

void sigint_handler(int sig)
{
    // TODO free allocated memory (RBT, ...)

    // TODO signal death to slave nodes
}
