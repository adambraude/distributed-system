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
#include "master_rq_cent.h"
#include "tpc_master.h"
#include "../bitmap-vector/read_vec.h"
#include "../consistent-hash/ring/src/tree_map.h"
#include "../types/types.h"

#define TEST_NUM_VECTORS 5000
#define TEST_MAX_LINE_LEN 5120

#define MS_DEBUG false

/* Ring CH variables */
rbt *chash_table;

/* static partition variables */
u_int num_keys; /* e.g., value of largest known key, plus 1 */

u_int max_vector_len;

/**
 * Master Process
 *
 * Coordinates/delegates tasks for the system.
 */
int main(int argc, char *argv[])
{
    /* READ queries */
    FILE *fp = fopen("../tst_data/tpc/qs/qs2.dat", "r");
    char *query_str;
    char *ops;

    int num_queries = atoi(argv[1]);

    int query,i,j;
    for (query = 0; query < num_queries; query++) {
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
            ranges[num_ranges][0] = r1;
            ranges[num_ranges][1] = r2;
            token = strtok(NULL, delim);
            if (token != NULL)
                ops[num_ranges] = token[0];
            token = strtok(NULL, delim);
            num_ranges++;
        }
        range_query_contents *contents = (range_query_contents *)
            malloc(sizeof(range_query_contents));
        /* fill in the data */
        // TODO move to above `while` loop!
        for (i = 0; i < num_ranges; i++) {
            contents->ranges[i][0] = ranges[i][0];
            contents->ranges[i][1] = ranges[i][1];
            if (i != num_ranges - 1) contents->ops[i] = ops[i];
        }

        contents->num_ranges = num_ranges;
        starfish(*contents);

        free(contents);
        free(ops);
    }
    fclose(fp);
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
    /* NB: freed in master_rq */
    u_int *range_array = (u_int *) malloc(sizeof(u_int) * num_ints_needed);
    int array_index = 0;
    bool flip = true;
    for (i = 0; i < contents.num_ranges; i++) {
        u_int *range = contents.ranges[i];
        vec_id_t j;
        u_int range_len = range[1] - range[0] + 1;
        range_array[array_index++] = range_len;
        u_int machine_vec_ptrs[range_len][2];
        for (j = range[0]; j <= range[1]; j++) {
            int mvp_addr = j - range[0];
            machine_vec_ptrs[mvp_addr][0] = 0;
            machine_vec_ptrs[mvp_addr][1] = j;
        }
        /* save machine/vec IDs into the array */
        int tuple_index;
        for (j = range[0]; j <= range[1]; j++) {
            tuple_index = j - range[0];
            range_array[array_index++] = machine_vec_ptrs[tuple_index][0];
            range_array[array_index++] = machine_vec_ptrs[tuple_index][1];
        }
    }
    return cent_init_range_query(range_array, contents.num_ranges,
        contents.ops, array_index);
}
