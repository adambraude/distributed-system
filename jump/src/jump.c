/**
 * "Jump Consistent Hash" is a fast, minimal memory, consistent hash algorithm.
 *
 * Source:
 * John Lamping and Eric Veach,
 * "A Fast, Minimal Memory, Consistent Hash Algorithm"
 * https://arxiv.org/abs/1406.2294
 */

#include "jump.h"

static const uint64_t seed = 2862933555777941757;

int32_t jump_consistent_hash (uint64_t key, int32_t num_buckets)
{
    int64_t b = -1;
    int64_t j = 0;

    double divisor, dividend;

    while (j < num_buckets) {
        b = j;
        key = key * seed + 1;
        divisor = (double)(1LL << 31);
        dividend = (double)((key >> 33) + 1);
        j = (int64_t) ((b + 1) * (divisor / dividend));
    }
    return (int32_t) b;
}
