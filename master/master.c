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
#include "slavelist.h"
#include "tpc_master.h"
#include "../bitmap-vector/read_vec.h"
#include "../consistent-hash/ring/src/tree_map.h"
#include "../types/types.h"

#define TEST_NUM_VECTORS 100
#define TEST_NUM_QUERIES 45
#define TEST_MAX_LINE_LEN 5120

#define MS_DEBUG false

/* variables for use in all master functions */
slave_ll *slavelist;
u_int slave_id_counter = 1;
u_int partition_t;
u_int query_plan_t;
int num_slaves;

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

    /* Connect to message queue. */
    partition_t = RING_CH;
    query_plan_t = STARFISH;

    /* Holder for a dead slave */
    dead_slave = NULL;

    /* setup values, acquired from slavelist.h */
    num_slaves = NUM_SLAVES;
    if (num_slaves == 1) replication_factor = 1;

    /* index in slave list will be the machine ID (0 is master) */
    slavelist = (slave_ll *) malloc(sizeof(slave_ll));
    slave_ll *head = slavelist;
    for (i = 1; i <= num_slaves; i++) {
        /* TODO: when we use CLI args, change this array */
        slave *s = new_slave(SLAVE_ADDR[i]);
        /* could not connect */
        if (setup_slave(s)) {
            printf("MASTER: Could not register machine %s\n", SLAVE_ADDR[i]);
        }
        else {
            slavelist->slave_node = s;
            printf("Added slave %s\n", SLAVE_ADDR[i]);
            /* if there is another slave to add to the linked-list,
             * allocate a node for it */
            if (i < num_slaves - 1) {
                slavelist->next = (slave_ll *) malloc(sizeof(slave_ll));
                slavelist = slavelist->next;
            }
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

    /* For measuring query time. */
    clock_t start_time = clock();

    /* PUT vectors */
    int vec_id;
    char buf[64];
    for (vec_id = 0; vec_id < TEST_NUM_VECTORS; vec_id++) {
        if (vec_id % 1000 == 0) printf("PUT(v%d)\n", vec_id);

        snprintf(buf, 64, "../tst_data/tpc/vec/v_%d.dat", vec_id);
        vec_t *vec = read_vector(buf);

        slave *commit_slaves[replication_factor];
        u_int *slave_ids = get_machines_for_vector(vec_id, true);
        slave_ll *head = slavelist;

        int cs_index = 0;
        while (head != NULL) {
            if (head->slave_node->id == slave_ids[0] ||
                head->slave_node->id == slave_ids[1])
                commit_slaves[cs_index++] = head->slave_node;
            if (cs_index == replication_factor) break;
            head = head->next;
        }

        int commit_res = commit_vector(vec_id, *vec, commit_slaves, replication_factor);
        if (commit_res) heartbeat();

        free(slave_ids);
    }

    /* READ queries */
    FILE *fp = fopen("../tst_data/tpc/qs/qs2.dat", "r");
    char *query_str;
    char *ops;

    int query;
    for (query = 0; query < TEST_NUM_QUERIES; query++) {
        printf("QUERY(%d)\n", query);

        char query_str[TEST_MAX_LINE_LEN];

        fgets(query_str, TEST_MAX_LINE_LEN - 1, fp);
        /* Get rid of CR or LF at end of line */
        for (j = strlen(query_str) - 1;
            j >= 0 && (query_str[j] == '\n' || query_str[j] == '\r');
            j--) query_str[j + 1] = '\0';

        /**
         * first pass: figure out how many ranges are in the query,
         * to figure out how much memory to allocate.
         */
        unsigned int num_ranges = 0;
        i = 0;
        while (query_str[i] != '\0') {
            if (query_str[i++] == ',') num_ranges++;
        }

        static char delim[] = "[,]";
        char *token = strtok(query_str, "R:");

        /* grab first element */
        token = strtok(token, delim);
        ops = (char *) malloc(sizeof(char) * num_ranges);
        puts("ops malloc");
        /* Allocate 2D array of range pointers */
        unsigned int range_count = num_ranges;
        vec_id_t ranges[128][2];

        /* parse out ranges and operators */
        num_ranges = 0;
        int r1, r2;
        while (token != NULL) {
            r1 = atoi(token);
            token = strtok(NULL, delim);
            r2 = atoi(token);
            //vec_id_t *bounds = (vec_id_t *) malloc(sizeof(vec_id_t) * 2);
            // bounds[0] = r1;
            // bounds[1] = r2;
            ranges[num_ranges][0] = r1;
            ranges[num_ranges][1] = r2;
            token = strtok(NULL, delim);
            if (token != NULL)
                ops[num_ranges] = token[0];
            token = strtok(NULL, delim);
            num_ranges++;
        }
        puts("while done");
        range_query_contents *contents = (range_query_contents *)
            malloc(sizeof(range_query_contents));
        puts("filling in data");
        /* fill in the data */
        for (i = 0; i < num_ranges; i++) {
            contents->ranges[i][0] = ranges[i][0];
            contents->ranges[i][1] = ranges[i][1];
            if (i != num_ranges - 1) contents->ops[i] = ops[i];
        }

        contents->num_ranges = num_ranges;
        puts("Entering switch stmt");

        switch (query_plan_t) {
            /* TODO: fill in cases */
            case STARFISH: {
                puts("calling starfish");
                while (starfish(*contents))
                    heartbeat();
                break;
            }
            case UNISTAR:
            case MULTISTAR:
            case ITER_PRIM: {
                /* init_btree_range_query(contents); */
                /* TODO (for conference version) */
                break;
            }
        }

        /* free bounds */
        // for (i = 0; i < range_count; i++)
        //     free(ranges[i]);
        // free(ranges);
        //free(query_str);
        free(contents);
        free(ops);
    }
    fclose(fp);

    clock_t end_time = clock();

    long double elapsed_time =
        (long double) (end_time - start_time) / CLOCKS_PER_SEC;
    printf("Time elapsed: %Lf\n", elapsed_time);

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
            u_int *tuple = get_machines_for_vector(j, false);
            if (flip) {
                u_int temp = tuple[0];
                tuple[0] = tuple[1];
                tuple[1] = temp;
            }
            int mvp_addr = j - range[0];
            machine_vec_ptrs[mvp_addr] = (u_int *) malloc(sizeof(u_int) * 2);
            machine_vec_ptrs[mvp_addr][0] = tuple[0];
            machine_vec_ptrs[mvp_addr][1] = j;
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
    puts("init_range_query");
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
            printf("Machine %s failed\n", addr);
            remove_slave(id);
            if (num_slaves == 0) {
                puts("MASTER: there are no more slaves");
                return 1;
            }
            reallocate();
        }
        else {
            if (MS_DEBUG) printf("%s is alive!\n", addr);
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
    dead_slave = *head;
    /* ...and just remove it (Torvalds-style) */
    *head = (*head)->next;
    num_slaves--;
    if (num_slaves == 1) replication_factor = 1;
    return EXIT_SUCCESS;
}

void reallocate()
{
    // take the dead slave, reallocate its vectors to other machines
    // assumes that none of the vectors die in the process
    printf("Something died\n");
    switch (partition_t) {
        case RING_CH: {
            u_int pred_id = ring_get_pred_id(chash_table, dead_slave->id);
            u_int succ_id = ring_get_succ_id(chash_table, dead_slave->id);
            u_int sucsuc_id = ring_get_succ_id(chash_table, succ_id);
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
            // TODO: have send_vector take an *integer* instead, and get the
            // address out of the slavelist array
            // transfer predecessor's vectors to dead node's successor
            slave_vector *vec = pred->primary_vector_head;
            if (pred != succ) {
                while (vec != NULL) {
                    send_vector(pred, vec->id, succ); // TODO: code this RPC
                    vec = vec->next;
                }
            }
            vec = dead_slave->primary_vector_head;
            // transfer successor's nodes to its successor as its backup
            while (vec != NULL) {
                send_vector(dead_slave, vec->id, sucsuc);
                vec = vec->next;
            }
            // join dead node's linked list with the successor
            succ->primary_vector_tail->next = dead_slave->primary_vector_head;
            succ->primary_vector_tail = dead_slave->primary_vector_tail;

            delete_entry(chash_table, dead_slave->id);
        }
    }
}

/**
 * Returns replication_factor (currently, hard at 2)-tuple of vectors such
 * that t = (m1, m2) and m1 != m2 if there at least 2 slaves available.
 * If this is a *new* vector, updating should be true, false otherwise.
 */
u_int *get_machines_for_vector(vec_id_t vec_id, bool updating)
{
    switch (partition_t) {
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
            return tr;
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
