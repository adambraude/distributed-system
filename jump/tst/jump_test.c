#include "jump_test.h"

/*
 * Run tests for 2^n buckets and 2^m keys where
 * n and m are integers between `min_power` and `max_power` (inclusive)
 * and where n and m will increment by `step`.
 *
 * The default values will test sizes ranging from 64 to 32,768.
 *
 *
 */
int main (int argc, char *argv[])
{
    int base, min_power, max_power, step;

    int silent;
    if (argc > 1)
        silent = strcmp(argv[1], "-s") == 0;
    else
        silent = 0;
    int printing = silent ? 0 : 1;

    if (argc == 1 || (argc == 2 && silent)) {
        base = 2;
        min_power = 6;
        max_power = 15;
        step = 3;
    }
    else if (argc == 5) {
        base = strtol(argv[1], NULL, 10);
        min_power = strtol(argv[1], NULL, 10);
        max_power = strtol(argv[2], NULL, 10);
        step      = strtol(argv[3], NULL, 10);
    }
    else if (argc == 6 && silent) {
        base = strtol(argv[2], NULL, 10);
        min_power = strtol(argv[2], NULL, 10);
        max_power = strtol(argv[3], NULL, 10);
        step      = strtol(argv[4], NULL, 10);
    }
    else {
        printf((
            "Invalid arguments.\n"
            "Input should be of the form:\n"
            "\t./main [-s] [min_power max_power step]\n"));

        return EXIT_FAILURE;
    }

    run_distrobution_tests(base, min_power, max_power, step, printing);

    return EXIT_SUCCESS;
}

void run_distrobution_tests
(int base, int min_power, int max_power, int step, int printing)
{
    int i, j, num_buckets, num_keys;

    for (i = min_power; i <= max_power; i += step) {
        num_buckets = 1 << i;
        for (j = i; j <= max_power; j += step) {
            num_keys = 1 << j;

            /* Test hashing random data. */
            distribution_stats random =
                test_distribution_random_keys(num_keys, num_buckets);

            /* Test hashing sequential data. */
            distribution_stats sequen =
                test_distribution_sequen_keys(num_keys, num_buckets);

            if (printing) {
                printf(
                    "==========================================\n"
                    "Dividing %6i keys between %6i buckets.\n\n",
                    num_keys, num_buckets);

                printf(
                    "         Random Keys         \n"
                    "=============================\n");

                printf(
                    "          Variance: %9.5f\n"
                    "Standard Deviation: %9.5f\n"
                    "    Standard Error: %9.5f\n",
                    random.variance,
                    random.standard_deviation,
                    random.standard_error);

                printf(
                    "       Sequential Keys       \n"
                    "=============================\n");

                printf(
                    "          Variance: %9.5f\n"
                    "Standard Deviation: %9.5f\n"
                    "    Standard Error: %9.5f\n",
                    sequen.variance,
                    sequen.standard_deviation,
                    sequen.standard_error);

                printf("\n");

                if (i == max_power && j == max_power)
                    printf(
                        "========================="
                        "========================="
                        "\n");
            }
        }
    }
}

/**
 * Generate `num_keys` random keys.
 * Distribute the generated keys between `num_buckets` buckets.
 * Return stats on how evenly distributed the keys were.
 */
distribution_stats test_distribution_random_keys
(int num_keys, int num_buckets)
{
    int bucket_items[num_buckets];
    memset(bucket_items, 0, num_buckets * sizeof(int));

    /* Start keys at random value. */
    srand((unsigned int) time(0));
    int key;

    for (key = 0; key < num_keys; ++key) {
        uint64_t value = long_random();
        int bucket = jump_consistent_hash(value, num_buckets);
        ++bucket_items[bucket];
    }

    return calculate_distribution_stats(num_keys, num_buckets, bucket_items);
}

/**
 * Generate `num_keys` sequentials keys where the first key is random.
 * Distribute the generated keys between `num_buckets` buckets.
 * Return stats on how evenly distributed the keys were.
 */
distribution_stats test_distribution_sequen_keys
(int num_keys, int num_buckets)
{
    int bucket_items[num_buckets];
    memset(bucket_items, 0, num_buckets * sizeof(int));

    /* Start keys at random value. */
    srand((unsigned int) time(0));
    uint64_t value = long_random();
    int key;

    for (key = 0; key < num_keys; ++key) {
        int bucket = jump_consistent_hash(value, num_buckets);
        ++bucket_items[bucket];
        ++value;
    }

    return calculate_distribution_stats(num_keys, num_buckets, bucket_items);
}

/**
 * Given a number of keys (num_keys) and number of buckets (num_buckets),
 * calculate the variance, standard deviation, and standard error
 * of the distribution of keys between buckets.
 *
 * This provides an easy way to ensure that keys are being distributed
 * eveny across the buckets.
 */
distribution_stats calculate_distribution_stats
(int num_keys, int num_buckets, int bucket_items[])
{
    double mean = (double) num_keys / num_buckets;
    double variance = 0.0;

    int bucket;
    for (bucket = 0; bucket < num_buckets; ++bucket) {
        variance += pow((double) bucket_items[bucket] - mean, 2);
    }
    variance = variance / num_buckets;

    double standard_deviation = sqrt(variance);
    double standard_error = standard_deviation / num_keys;

    distribution_stats stats;
    stats.variance = variance;
    stats.standard_deviation = standard_deviation;
    stats.standard_error = standard_error;

    return stats;
}

/**
 * Generate a random, unsigned, 64-bit integer.
 */
uint64_t long_random ()
{
    uint64_t upper = (uint64_t) rand();
    upper = upper << 32;
    uint64_t lower = (uint64_t) rand();
    return  upper | lower;
}
