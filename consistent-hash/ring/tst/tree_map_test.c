#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "tree_map_test.h"

int main(int argc, char *argv[])
{
    FILE *fp;
    fp = fopen("server_file", "r");
    size_t read;
    char *line = NULL;
    int cache_id, dupli;
    uint64_t hash_value;
    rbt_ptr tree = new_rbt(); /* first node in the tree. */
    node_ptr nodes[20];
    int counter = 0;
    while ((read = getline(&line, &read, fp)) != -1) {
        if (line[0] != '#') {
            cache_id = atoi(strtok(line, " "));
            dupli = atoi(strtok(NULL, " "));
            int i;
            for (i = 0; i < dupli; i++) {
                hash_value = hash(cache_id + hash(i));
                node_ptr n = new_node(tree, cache_id, hash_value, RED);
                nodes[counter++] = n;
                rbt_insert(tree, n);
            }
        }
    }
    bst_test(tree->root);
    printf("BST Test after inserting passed\n");

    /* test successor function */
    int i;
    for (i = 0; i < 17; i++) {
        printf("Successor of %d is %d\n", i, succ(tree, hash(i)));
    }

    /* test deletions */
    int j;
    for (j = 0; j < 7; j++) {
        rbt_delete(tree, nodes[j]);
    }

    bst_test(tree->root);
    printf("BST Test after deletion passed\n");
    print(tree, tree->root);
    printf("Free RBT\n");
    free_rbt(tree);
    printf("Successfully freed\n");
    return 0;
}

/*
 * Verifies that the tree is an actual binary search tree.
 */
void bst_test(node_ptr c)
{
    int right = c->right->hv;
    assert((c->left->hv <= c->hv));
    assert((right >= c->hv || right == 0));
    if (c->left->hv != 0)
        bst_test(c->left);
    if (c->right->hv != 0)
        bst_test(c->right);
}
