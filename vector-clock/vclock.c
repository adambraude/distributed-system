#include <stdlib.h>
#include <assert.h>
#include "../master/slavelist.h"
#include "vclock.h"

/**
 * Create a new vector clock.
 */
vector_clock create_vclock(void)
{
    vector_clock return_val = (vector_clock)
        malloc(sizeof(lamport_timestamp) * (NUM_MACHINES));
    int i;
    for (i = 0; i < NUM_MACHINES; i++) {
        return_val[i] = 0; /* initially, all clocks are 0 */
    }
    return return_val;
}

/**
 * Updates current clock given a received clock.
 */
void
handle_recvd_vclock(vector_clock current_clock, vector_clock received_clock, int receiver_id)
{
    assert(receiver_id >= 0 && receiver_id < NUM_MACHINES);
    int i;
    for (i = 0; i < NUM_MACHINES; i++) {
        current_clock[i] = MAX(current_clock[i], received_clock[i]);
    }
    current_clock[receiver_id]++;
}

/**
 * Incrementing clock in preparation for sending.
 */
void prep_send_vclock(vector_clock current_clock, int receiver_id)
{
    assert(receiver_id >= 0 && receiver_id < NUM_MACHINES);
    current_clock[receiver_id]++;
}
