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
    rbt *tree = new_rbt(); /* first node in the tree. */
    node *nodes[20];
    int counter = 0;
    while ((read = getline(&line, &read, fp)) != -1) {
        if (line[0] != '#') {
            cache_id = atoi(strtok(line, " "));
            dupli = atoi(strtok(NULL, " "));
            int i;
            for (i = 0; i < dupli; i++) {
                hash_value = hash(cache_id + hash(i));
                node *n = new_node(tree, cache_id, hash_value, RED);
                nodes[counter++] = n;
                rbt_insert(tree, n);
            }
        }
    }
    bst_test(tree->root);

    /* test deletions */
    int j;
    for (j = 0; j < 7; j++) {
        rbt_delete(tree, nodes[j]);
    }

    bst_test(tree->root);
    print(tree, tree->root);
    free_rbt(tree);
    return 0;
}

/*
 * Verifies that the tree is an actual binary search tree.
 */
void bst_test(node *c)
{
    int right = c->right->hv;
    assert((c->left->hv <= c->hv));
    assert((right >= c->hv || right == 0));
    if (c->left->hv != 0)
        bst_test(c->left);
    if (c->right->hv != 0)
        bst_test(c->right);
}
