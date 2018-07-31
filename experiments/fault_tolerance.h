/*
 * Parameters for fault tolerance experiments
 *
 */
#ifndef FAULT_TOLERANCE_H

/* Type of test */
enum {NO_KILL, ORDERED_BY_ID, RANDOM_SLAVE};
#define FT_EXP_TYPE NO_KILL

#define FT_KILL_MODULUS 100  /* Kill a slave after every "n" queries. */
#define FT_NUM_QUERIES  1000 /* Running 1000 (of 10,000) generated queries. */
#define FT_NUM_VEC      1000 /* Using first 1000 vectors from TPC dataset. */

/* Output file prefixes */
#define FT_QRES_PREFIX   "ft_qres"
#define FT_REALLOC_PREFIX   "ft_realloc"

#endif
