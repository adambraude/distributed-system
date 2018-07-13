/*
 * Parameters for fault tolerance experiments
 *
 */
#ifndef FAULT_TOLERANCE_H

#define FT_NUM_QUERIES  5000
#define FT_KILL_MODULUS 200
#define FT_NUM_VEC      1280     /* NB: bitmap workload max vector id = 1271 */
#define FT_KILL_Q       FT_NUM_QUERIES / 2
#define FT_PREKILL_Q    FT_KILL_Q
#define FT_POSTKILL_Q   FT_KILL_Q
#define FT_OUT_PREFIX   "ft-out"
enum {NO_KILL, ORDERED, RANDOM_SLAVE};
#define FT_EXP_TYPE ORDERED

#endif
