/**
 * Server map implemented as a red-black tree
 * Based on CLRS Chap. 13
 */

#include <stdlib.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <string.h>
#include <stdint.h>
#include "tree_map.h"
#include <math.h>

/* RBT op prototypes */
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
node_ptr pred(rbt_ptr, hash_value);
node_ptr get_node_with_hv(rbt_ptr, hash_value);

/*
 * The cache_id of the node n in the tree with the smallest hash_value hv
 * such that n.hv > value.
 */
node_ptr succ(rbt_ptr t, hash_value value);
node_ptr recur_succ(rbt_ptr t, node_ptr root, node_ptr suc, hash_value value);
uint64_t hash(unsigned long);


/**
 * Insert the new cache into the red-black tree
 *
 */
void insert_cache(rbt_ptr t, struct cache* cptr)
{
    int i;
    for (i = 0; i < cptr->replication_factor; i++) {
        rbt_insert(t, new_node(t, cptr->id, hash(cptr->id + i), RED));
    }
}

static int replication_factor = 2;

cache_id *ring_get_machines_for_vector(rbt_ptr t, unsigned int vec_id)
{
    cache_id *res = (cache_id *) malloc(sizeof(cache_id) * replication_factor);
    node_ptr primary_node = succ(t, hash(vec_id));
    node_ptr backup_node = succ(t, primary_node->hv);
    res[0] = primary_node->cid;
    res[1] = backup_node->cid;
    return res;
}

cache_id ring_get_succ_id(rbt_ptr t, cache_id cid)
{
    return succ(t, hash(cid))->cid;
}

cache_id ring_get_pred_id(rbt_ptr t, cache_id cid)
{
    return pred(t, hash(cid))->cid;
}

// TODO: predecessor node function
// TODO: get node with particular key function
/**
 * returns the node in the tree that's the predecessor to the node
 * with the given key (assumes there exists a node with such a key in the tree)
 */
node_ptr pred(rbt_ptr t, hash_value value)
{
    node_ptr n = t->root;
    node_ptr pr = t->nil;
    while (n->hv != value) {
        if (value > n->hv) {
            pr = n;
            n = n->right;
        }
        else {
            n = n->left;
        }
    }
    if (n == rbt_min(t, t->root)) return rbt_max(t, t->root);
    if (pr == t->nil) { // we didn't go right: find rightmost node of left subtree
        pr = n->left;
        while (pr->right != t->nil) pr = pr->right;
    }
    return pr;
}

node_ptr succ(rbt_ptr t, hash_value value)
{
    node_ptr curr = t->root;
    if (value >= rbt_max(t, curr)->hv) {
        return rbt_min(t, curr);
    }
    return recur_succ(t, t->root, t->root, value);
}

node_ptr get_node_with_hv(rbt_ptr t, hash_value hv)
{
    node_ptr res = t->root;
    while (res->hv != hv) {
        if (res->hv < hv) res = res->right;
        else res = res->left;
    }
    return res;
}

void delete_entry(rbt_ptr t, cache_id id)
{
    rbt_delete(t, get_node_with_hv(t, hash(id)));
}

void print_tree(rbt_ptr t, node_ptr c)
{
    if (c == t->nil) return;
    printf("ID: %lld Hash: %lld\n", c->cid, c->hv);
    print_tree(t, c->right);
    print_tree(t, c->left);
}

node_ptr recur_succ(rbt_ptr t, node_ptr root, node_ptr suc, hash_value value)
{
    if (root == t->nil) {
        return suc;
    }
    if (value == root->hv) {
        /* if right is nil, look back */
        if (root->right == t->nil) {
            while (suc->parent != t->nil && suc->parent->hv < value) {
                suc = suc->parent;
            }
            return suc;
        }
        /* otherwise, return leftmost node of right subtree */
        suc = root->right;
        while (suc->left != t->nil) {
            suc = suc->left;
        }
        return suc;
    }
    if (root->hv > value) {
        suc = root;
        return recur_succ(t, root->left, suc, value);
    }
    else {
        return recur_succ(t, root->right, suc, value);
    }

}

node_ptr rbt_max(rbt_ptr t, node_ptr x)
{
    while (x->right != t->nil)
        x = x->right;
    return x;
}

rbt_ptr new_rbt(void)
{
    rbt_ptr r;
    r = (rbt_ptr) malloc(sizeof(rbt));
    r->nil = (node_ptr) malloc(sizeof(node));
    r->nil->color = BLACK;
    r->nil->hv = -1;
    r->nil->cid = -1;
    r->root = r->nil;
    r->size = 0;
    return r;
}

node_ptr new_node(rbt_ptr t, cache_id cid, hash_value hv, rbt_node_color color)
{

    node_ptr n;

    n = (node_ptr) malloc(sizeof(node));
    n->color = color;

    n->color = color;
    n->hv = hv;

    n->parent = t->nil;
    n->left = t->nil;
    n->right = t->nil;
    n->cid = cid;


    return n;
}

void left_rotate(rbt_ptr t, node_ptr x)
{
    node_ptr y = x->right;
    x->right = y->left;
    if (y->left != t->nil)
        y->left->parent = x;
    y->parent = x->parent;
    if (x->parent == t->nil)
        t->root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;
    y->left = x;
    x->parent = y;
}

void right_rotate(rbt_ptr t, node_ptr y)
{
    node_ptr x = y->left;
    y->left = x->right;
    if (x->right != t->nil)
        x->right->parent = y;
    x->parent = y->parent;
    if (y->parent == t->nil)
        t->root = x;
    else if (y == y->parent->right)
        y->parent->right = x;
    else
        y->parent->left = x;
    x->right = y;
    y->parent = x;
}

void rbt_insert(rbt_ptr t, node_ptr z)
{
    t->size++;
    node_ptr x = t->root;
    node_ptr y = t->nil;
    while (x != t->nil) {
        y = x;
        if (z->hv < x->hv)
            x = x->left;
        else
            x = x->right;
    }
    z->parent = y;
    if (y == t->nil)
        t->root = z;
    else if (z->hv < y->hv)
        y->left = z;
    else
        y->right = z;
    z->left = t->nil;
    z->right = t->nil;
    z->color = RED;
    rbt_insert_fixup(t, z);
}

void rbt_insert_fixup(rbt_ptr t, node_ptr z)
{
    node_ptr y;
    while (z->parent->color == RED) {
        if (z->parent == z->parent->parent->left) {
            y = z->parent->parent->right;
            if (y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            }
            else {
                if (z == z->parent->right) {
                    z = z->parent;
                    left_rotate(t, z);
                }
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                right_rotate(t, z->parent->parent);
            }
        }
        else {
            y = z->parent->parent->left;
            if (y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            }
            else {
                if (z == z->parent->left) {
                    z = z->parent;
                    right_rotate(t, z);
                }
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                left_rotate(t, z->parent->parent);
            }
        }
    }
    t->root->color = BLACK;
}

void rbt_transplant(rbt_ptr t, node_ptr u, node_ptr v)
{
    if (u->parent == t->nil) {
        t->root = v;
    }
    else if (u == u->parent->left) {
        u->parent->left = v;
    }
    else
        u->parent->right = v;
    v->parent = u->parent;
}

node_ptr rbt_min(rbt_ptr t, node_ptr x)
{
    while (x->left != t->nil)
        x = x->left;
    return x;
}

void rbt_delete(rbt_ptr t, node_ptr z)
{
    t->size--;
    node_ptr x, y = z;
    rbt_node_color y_orig_col = y->color;
    if (z->left == t->nil) {
        x = z->right;
        rbt_transplant(t, z, z->right);
    }
    else if (z->right == t->nil) {
        x = z->left;
        rbt_transplant(t, z, z->left);
    }
    else {
        y = rbt_min(t, z->right);
        y_orig_col = y->color;
        x = y->right;
        if (y->parent == z) {
            x->parent = y;
        }
        else {
            rbt_transplant(t, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        rbt_transplant(t, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }
    if (y_orig_col == BLACK)
        rbt_delete_fixup(t, x);
    free(z);
}

void rbt_delete_fixup(rbt_ptr t, node_ptr x)
{
    node_ptr w;
    while (x != t->root && x->color == BLACK) {
        if (x == x->parent->left) {
            w = x->parent->right;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                left_rotate(t, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == BLACK && w->right->color == BLACK) {
                w->color = RED;
                x = x->parent;
            }
            else {
                if (w->right->color == BLACK) {
                    w->left->color = BLACK;
                    w->color = RED;
                    right_rotate(t, w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->right->color = BLACK;
                left_rotate(t, x->parent);
                x = t->root;
            }
        }
        else {
            w = x->parent->left;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                right_rotate(t, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == BLACK && w->left->color == BLACK) {
                w->color = RED;
                x = x->parent;
            }
            else {
                if (w->left->color == BLACK) {
                    w->right->color = BLACK;
                    w->color = RED;
                    left_rotate(t, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->left->color = BLACK;
                right_rotate(t, x->parent);
                x = t->root;
            }
        }
    }
    x->color = BLACK;
}

/**
 * Computes a 64-bit SHA1 hash of the given key.
 * source: https://memset.wordpress.com/2010/10/06/using-sha1-function/
 */
uint64_t hash(unsigned long key)
{
    int i;
    unsigned char temp[SHA_DIGEST_LENGTH];
    char buf[SHA_DIGEST_LENGTH * 2];
    char ibuf[32];
    snprintf(ibuf, 32, "%lu", key);

    memset(buf, 0x0, SHA_DIGEST_LENGTH * 2);
    memset(temp, 0x0, SHA_DIGEST_LENGTH);

    SHA1((unsigned char *) ibuf, strlen(ibuf), temp);

    for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf((char *) &(buf[i * 2]), "%02x", temp[i]);
    }
    int buflen = 15; // we can only take the first 15 bytes due to C technical limits
    char lbuf[buflen];
    for (i = 0; i < buflen; i++) lbuf[i] = buf[i];
    lbuf[buflen] = '\0';

    return strtoull(lbuf, NULL, 16); // TODO: could force this into positive range, but it doesn't matter
}


void recur_free_rbt(rbt_ptr t, node_ptr n)
{
    if (n->left != t->nil) recur_free_rbt(t, n->left);
    if (n->right != t->nil) recur_free_rbt(t, n->right);
    free(n);
}

void free_rbt(rbt_ptr t)
{
    recur_free_rbt(t, t->root);
    free(t->nil);
    free(t);
}
