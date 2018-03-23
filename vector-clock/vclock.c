#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../master/slavelist.h"
#include "vclock.h"

/**
 * Create a new vector clock.
 *
 * All clocks are initialized to the `0` vector.
 */
vector_clock create_vclock(void)
{
    vector_clock return_val = (vector_clock)
        malloc(sizeof(lamport_timestamp) * (NUM_MACHINES));

    int i;
    for (i = 0; i < NUM_MACHINES; i++) {
        return_val[i] = 0;
    }

    return return_val;
}

/**
 * Destroy the given vector clock.
 */
void destroy_vclock(vector_clock garbage)
{
    free(garbage);
}

/**
 * Compare two vector clocks.
 *
 * Returns:
 *      `-1` if `first` is earlier
 *      `0` if `first` and `second` are equal
 *      `1` if `second` is earlier
 */
int compare_vclock(vector_clock first, vector_clock second)
{
    bool is_before = false;
    bool are_equal = true;

    int i;
    for (i = 0; i < NUM_MACHINES; i++) {
        if (first[i] > second[i]) {
            return 1;
        }
        else if (first[i] < second[i]) {
            is_before = true;
            are_equal = false;
        }
    }

    if (are_equal) return 0;
    else if (is_before) return -1;
    else return 1;
}

/**
 * Updates `current_clock`, in-place, to the max of the two given clocks,
 * and increment the receiver's timestamp.
 */
void handle_recvd_vclock(vector_clock current_clock,
    vector_clock received_clock, int receiver_id)
{
    assert(receiver_id >= 0 && receiver_id < NUM_MACHINES);

    int i;
    for (i = 0; i < NUM_MACHINES; i++) {
        current_clock[i] = MAX(current_clock[i], received_clock[i]);
    }

    current_clock[receiver_id]++;
}

/**
 * Increment the current machine's clock in preparation for sending.
 */
void prep_send_vclock(vector_clock current_clock, int sender_id)
{
    assert(sender_id >= 0 && sender_id < NUM_MACHINES);

    current_clock[sender_id]++;
}
