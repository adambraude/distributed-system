#ifndef SLAVELIST_H
#define SLAVELIST_H

#define DBC1_ADDR "10.250.94.63"
#define DBC2_ADDR "10.250.94.56"
#define DBC3_ADDR "10.250.94.72"
#define DBC4_ADDR "10.250.94.80"
#define DBC5_ADDR "10.250.94.69"
#define DBC6_ADDR "10.250.94.64"
#define DBC7_ADDR "10.250.94.77"
#define DBC8_ADDR "10.250.94.75"

#define HOME_ADDR "127.0.0.1"

//#define LOCALTEST
#define FULL_TEST
//#define DBCAP_1_TEST

// #ifdef DBCAP_1_TEST
//     #define NUM_SLAVES 1
//     static char SLAVE_ADDR[NUM_SLAVES][32] = {
//         DB_CAPSTONE_1_ADDR
//     };
// #endif /* DBCAP_1_TEST */

// #ifdef LOCALTEST
//     #define NUM_SLAVES 1
//     static char SLAVE_ADDR[NUM_SLAVES][32] = {
//         HOME_ADDR
//     };
// #endif /* LOCALTEST */

#ifdef FULL_TEST /* Full test with 3 slaves */
    #define NUM_SLAVES 3
    static char SLAVE_ADDR[NUM_SLAVES][32] = {
        DBC4_ADDR,
        DBC2_ADDR,
        DBC3_ADDR
    };
#endif /* FULL_TEST */

#define NUM_MACHINES NUM_SLAVES + 1

#endif /* SLAVELIST_H */
