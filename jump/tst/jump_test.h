#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "../src/jump.h"
#include "../src/jump.c"

typedef struct distribution_stats {
    double variance;
    double standard_deviation;
    double standard_error;
} distribution_stats;

uint64_t long_random ();

void run_distrobution_tests
    (int base, int min_power, int max_power, int step, int printing);

distribution_stats test_distribution_random_keys
    (int num_keys, int num_buckets);

distribution_stats test_distribution_sequen_keys
    (int num_keys, int num_buckets);

distribution_stats calculate_distribution_stats
    (int num_keys, int num_buckets, int bucket_items[]);
