#ifndef MASTER_H
#define MASTER_H

#include <stdbool.h>
#include "../types/types.h"
#include "../ipc/messages.h"
#define SLAVELIST_PATH "../SLAVELIST"

extern char **slave_addresses;

/**
 * each slave will keep a linked list of vector IDs indicating which
 * vector it has (could also keep record of the vector itself)
 */
typedef struct slave_vector {
    unsigned int id;
    struct slave_vector *next;
} slave_vector;

typedef struct slave {
    unsigned int id;
    char *address;
    bool is_alive;
    slave_vector *primary_vector_head; /* vectors that were assigned to this slave */
    slave_vector *primary_vector_tail;
} slave;

typedef struct slave_ll {
    slave *slave_node;
    struct slave_ll *next;
} slave_ll;

typedef enum {RING_CH, JUMP_CH, STATIC_PARTITION} partition_t; /* parition types */
typedef enum {UNISTAR, STARFISH, MULTISTAR, ITER_PRIM} query_plan_t; /* query plan algorithms */
/* The number of different machines a vector appears on */
static int replication_factor = 2;

/* master function prototypes */
int setup_slave(slave*);
slave *new_slave(char*);
int heartbeat(void);
bool is_alive(char *);
slave **get_machines_for_vector(unsigned int, bool);
int send_vector(slave *, vec_id_t, slave*);
void reallocate(void);
int starfish(range_query_contents);
int get_new_slave_id(void);
slave *new_slave(char *address);
int compare_machine_vec_tuple(const void *, const void *);
int remove_slave(unsigned int);
double stdev(u_int64_t *items, double avg, int N);

#endif /* MASTER_H */
