#ifndef SLAVELIST_H
#define SLAVELIST_H

#define DBC1_ADDR "10.250.94.63" /* master */

#define DBC2_ADDR "10.250.94.56"
#define DBC3_ADDR "10.250.94.72"
#define DBC4_ADDR "10.250.94.69"
#define DBC5_ADDR "10.250.94.80"
#define DBC6_ADDR "10.250.94.64"
#define DBC7_ADDR "10.250.94.77"
#define DBC8_ADDR "10.250.94.75"

#define HOME_ADDR "127.0.0.1"

//#define LOCALTEST
#define FULL_TEST
//#define DBCAP_2_TEST
//#define DUAL_SLAVE_TEST

#ifdef DBCAP_2_TEST
    #define NUM_SLAVES 1
    static char SLAVE_ADDR[NUM_SLAVES][32] = {
        DBC2_ADDR
    };
#endif /* DBCAP_1_TEST */

#ifdef DUAL_SLAVE_TEST
    #define NUM_SLAVES 2
    static char SLAVE_ADDR[NUM_SLAVES][32] = {
        DBC2_ADDR,
        DBC3_ADDR
    };
#endif

#ifdef LOCALTEST
    #define NUM_SLAVES 1
    static char SLAVE_ADDR[NUM_SLAVES][32] = {
        HOME_ADDR
    };
#endif /* LOCALTEST */

#ifdef FULL_TEST /* Full test with 7 slaves */
    #define NUM_SLAVES 7
    static char SLAVE_ADDR[NUM_SLAVES + 1][32] = {
        "",
        DBC2_ADDR,
        DBC3_ADDR,
        DBC4_ADDR,
        DBC5_ADDR,
        DBC6_ADDR,
        DBC7_ADDR,
        DBC8_ADDR
    };
#endif /* FULL_TEST */

#define NUM_MACHINES NUM_SLAVES + 1

#endif /* SLAVELIST_H */
