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
rbt *new_rbt(void);
node *new_node(rbt *, cache_id, hash_value, rbt_node_color);

/* tree destructor */
void free_rbt(rbt *);

/* RBT operations (from CLSR) */
void left_rotate(rbt *, node *);
void right_rotate(rbt *, node *);
void rbt_insert(rbt *, node *);
void rbt_insert_fixup(rbt *, node *);
void rbt_transplant(rbt *, node *, node *);
node *rbt_min(rbt *, node *);
node *rbt_max(rbt *, node *);
void rbt_delete(rbt *, node *);
void rbt_delete_fixup(rbt *, node *);
node *pred(rbt *, hash_value);
node *get_node_with_hv(rbt *, hash_value);

/*
 * The cache_id of the node n in the tree with the smallest hash_value hv
 * such that n.hv > value.
 */
node *succ(rbt *t, hash_value value);
node *recur_succ(rbt *t, node *root, node *suc, hash_value value);
uint64_t hash(unsigned long);


/**
 * Insert the new cache into the red-black tree
 *
 */
void insert_cache(rbt *t, struct cache* cptr)
{
    int i;
    for (i = 0; i < cptr->replication_factor; i++) {
        rbt_insert(t, new_node(t, cptr->id, hash(cptr->id + i), RED));
    }
}

/**
 * Insert node with the given slave into the tree.
 */
void insert_slave(rbt *t, slave *s)
{
    node *n = new_node(t, s->id, hash(s->id), RED);
    n->slv = s;
    rbt_insert(t, n);
}

slave **ring_get_machines_for_vector(rbt *t, unsigned int vec_id,
    int replication_factor)
{
    slave **res = (slave **) malloc(sizeof(slave *) * replication_factor);
    int i;
    for (i = 0; i < replication_factor; i++) {
        if (i == 0) {
            res[0] = ring_get_succ_slave(t, vec_id);
        }
        else {
            res[i] = ring_get_succ_slave(t, res[i - 1]->id);
        }
    }
    return res;
}

slave *ring_get_succ_slave(rbt *t, cache_id cid)
{
    return succ(t, hash(cid))->slv;
}

slave *ring_get_pred_slave(rbt *t, cache_id cid)
{
    return pred(t, hash(cid))->slv;
}

void recur_flatten_slavelist(rbt *t, node *r)
{
    t->fs[t->slavelist_index++] = r->slv;
    if (r->right != t->nil) recur_flatten_slavelist(t, r->right);
    if (r->left != t->nil) recur_flatten_slavelist(t, r->left);
}

void flatten_slavelist(rbt *t)
{
    t->slavelist_index = 0;
    free(t->fs);
    t->fs = (slave **) malloc(sizeof(slave *) * t->size);
    recur_flatten_slavelist(t, t->root);
    int i;
    puts("printing slavelist");
    for (i = 0; i < t->size; i++) printf("%d\n", t->fs[i]->id);
}

slave **ring_flattened_slavelist(rbt *t)
{
    return t->fs;
}

// TODO: predecessor node function
// TODO: get node with particular key function
/**
 * returns the node in the tree that's the predecessor to the node
 * with the given key (assumes there exists a node with such a key in the tree)
 */
node *pred(rbt *t, hash_value value)
{
    node *n = t->root;
    node *pr = t->nil;
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

node *succ(rbt *t, hash_value value)
{
    node *curr = t->root;
    if (value >= rbt_max(t, curr)->hv) {
        return rbt_min(t, curr);
    }
    return recur_succ(t, t->root, t->root, value);
}

node *get_node_with_hv(rbt *t, hash_value hv)
{
    node *res = t->root;
    while (res->hv != hv) {
        if (res->hv < hv) res = res->right;
        else res = res->left;
    }
    return res;
}

void delete_entry(rbt *t, cache_id id)
{
    rbt_delete(t, get_node_with_hv(t, hash(id)));
    flatten_slavelist(t);
}

void print_tree(rbt *t, node *c)
{
    if (c == t->nil) return;
    print_tree(t, c->left);
    printf("ID: %u Hash: %lld\n", c->slv->id, c->hv);
    print_tree(t, c->right);
}

node *recur_succ(rbt *t, node *root, node *suc, hash_value value)
{
    if (root == t->nil) {
        return suc;
    }
    else if (value == root->hv) {
        /* if right is nil, look back */
        if (root->right == t->nil) {
            suc = root->parent;
            while (suc->parent != t->nil && suc->hv < value) {
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
    else if (root->hv > value) {
        return recur_succ(t, root->left, root, value);
    }
    else {
        return recur_succ(t, root->right, suc, value);
    }

}

node *rbt_max(rbt *t, node *x)
{
    while (x->right != t->nil)
        x = x->right;
    return x;
}

rbt *new_rbt(void)
{
    rbt *r;
    r = (rbt *) malloc(sizeof(rbt));
    r->nil = (node *) malloc(sizeof(node));
    r->nil->color = BLACK;
    r->nil->hv = -1;
    r->nil->cid = -1;
    r->root = r->nil;
    r->size = 0;
    r->fs = NULL;
    return r;
}

node *new_node(rbt *t, cache_id cid, hash_value hv, rbt_node_color color)
{

    node *n;

    n = (node *) malloc(sizeof(node));
    n->color = color;

    n->color = color;
    n->hv = hv;

    n->parent = t->nil;
    n->left = t->nil;
    n->right = t->nil;
    n->cid = cid;


    return n;
}

void left_rotate(rbt *t, node *x)
{
    node *y = x->right;
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

void right_rotate(rbt *t, node *y)
{
    node *x = y->left;
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

void rbt_insert(rbt *t, node *z)
{
    t->size++;
    node *x = t->root;
    node *y = t->nil;
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
    flatten_slavelist(t);
}

void rbt_insert_fixup(rbt *t, node *z)
{
    node *y;
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

void rbt_transplant(rbt *t, node *u, node *v)
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

node *rbt_min(rbt *t, node *x)
{
    while (x->left != t->nil)
        x = x->left;
    return x;
}

void rbt_delete(rbt *t, node *z)
{
    t->size--;
    node *x, *y = z;
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

void rbt_delete_fixup(rbt *t, node *x)
{
    node *w;
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


void recur_free_rbt(rbt *t, node *n)
{
    if (n->left != t->nil) recur_free_rbt(t, n->left);
    if (n->right != t->nil) recur_free_rbt(t, n->right);
    free(n);
}

void free_rbt(rbt *t)
{
    recur_free_rbt(t, t->root);
    free(t->nil);
    free(t);
}
