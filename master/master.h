#ifndef MASTER_H
#define MASTER_H

#include <stdbool.h>
#include "../types/types.h"
#include "../ipc/messages.h"
#define SLAVELIST_PATH "SLAVELIST"
#define M_DEBUG false /* show debugging messages */

extern char **slave_addresses;

/**
 * each slave will keep a linked list of vector IDs indicating which
 * vector it has (could also keep record of the vector itself)
 */

typedef struct slave_ll {
    slave *slave_node;
    struct slave_ll *next;
} slave_ll;

typedef enum {RING_CH, JUMP_CH, STATIC_PARTITION} partition_t; /* parition types */
typedef enum {UNISTAR, STARFISH, MULTISTAR, ITER_PRIM} query_plan_t; /* query plan algorithms */
/* The number of different machines a vector appears on */
static int replication_factor = 2;

/* master function prototypes */
bool add_slave(char *);
bool is_alive(char *);
bool setup_slave(slave *);
double stdev(u_int64_t *, double, int);
int compare_machine_vec_tuple(const void *, const void *);
int get_new_slave_id(void);
int heartbeat(void);
int ring_heartbeat(void);
int remove_slave(unsigned int);
int send_vector(slave *, vec_id_t, slave *);
int starfish(range_query_contents);
slave **get_machines_for_vector(unsigned int, bool);
slave *create_new_slave(char *);
void master_cleanup(void);
void reallocate(slave *);

#endif /* MASTER_H */
