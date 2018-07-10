#ifndef TREE_MAP_H
#define TREE_MAP_H

#define NIL_HV 0

#include "../../../types/types.h"

typedef long long cache_id;
typedef long long hash_value;

typedef enum {RED, BLACK} rbt_node_color;

typedef struct node {
    hash_value hv; /* key used to identify the node */
    cache_id cid;
    rbt_node_color color;
    struct node *left;
    struct node *right;
    struct node *parent;
    slave *slv;
} node;

typedef struct rbt {
    node *root;
    node *nil;
    int size;
    int slavelist_index;
    slave **fs;
} rbt;


typedef struct cache {
    cache_id id;
    char *cache_name;
    int replication_factor;
} cache;

void insert_cache(rbt *, struct cache*);
cache_id get_machine_for_vector(rbt *, unsigned int);
rbt *new_rbt();
void print_tree(rbt *, node *);
// cache_id *ring_get_machines_for_vector(rbt *, unsigned int);
slave *ring_get_succ_slave(rbt *, cache_id);
slave *ring_get_pred_slave(rbt *, cache_id);
slave **ring_get_machines_for_vector(rbt *, u_int, int);
slave **ring_flattened_slavelist(rbt *);
void delete_entry(rbt *, cache_id);
void insert_slave(rbt *, slave *);

#endif
