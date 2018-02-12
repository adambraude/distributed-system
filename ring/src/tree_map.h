#ifndef TREE_MAP_H
#define TREE_MAP_H

#define NIL_HV 0

typedef int cache_id;
typedef int hash_value;

typedef enum {RED, BLACK} rbt_node_color;

typedef struct node *node_ptr;
typedef struct rbt *rbt_ptr;

typedef struct node
{
    hash_value hv; /* key used to identify the node */
    cache_id cid;
    rbt_node_color color;
    node_ptr left;
    node_ptr right;
    node_ptr parent;
} node;

typedef struct rbt
{
    node_ptr root;
    node_ptr nil;
    int size;
} rbt;

/* RBT initialization functions */
rbt_ptr new_rbt(void);
node_ptr new_node(rbt_ptr, cache_id, hash_value, rbt_node_color);

/* tree destructor */
void free_rbt(rbt_ptr);

/* RBT operations (from CLSR) */
void left_rotate(rbt_ptr, node_ptr);
void right_rotate(rbt_ptr, node_ptr);
void rbt_insert(rbt_ptr, node_ptr);
void rbt_insert_fixup(rbt_ptr, node_ptr);
void rbt_transplant(rbt_ptr, node_ptr, node_ptr);
node_ptr rbt_min(rbt_ptr, node_ptr);
node_ptr rbt_max(rbt_ptr, node_ptr);
void rbt_delete(rbt_ptr, node_ptr);
void rbt_delete_fixup(rbt_ptr, node_ptr);

/*
 * The cache_id of the node n in the tree with the smallest hash_value hv
 * such that n.hv > value.
 */
cache_id succ(rbt_ptr t, hash_value value);
cache_id recur_succ(rbt_ptr t, node_ptr root, node_ptr suc, hash_value value);
void print(rbt_ptr t, node_ptr c);

#endif
