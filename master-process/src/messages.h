#ifndef MESSAGES_H
#define MESSAGES_H

#include <sys/msg.h>

/**
 * TODO:
 * Do not hard code key so that we can (hopefully) avoid conflicts.
 * Beej recommends using:
 *      key_t ftok(const char *path, int id);
 */
static key_t MSQ_KEY = 440440;

/* Equivalent to "rw-rw-rw-" */
static int MSQ_PERMISSIONS = 0666;

/* Message types */
static long mtype_put = 1;
static long mtype_kill_master = 98;
static long mtype_master_dying = 99;

typedef struct assigned_vector {
    int vec_id;
    unsigned long long vec;
} assigned_vector;

typedef struct put_msgbuf {
    long mtype;
    assigned_vector vector;
} put_msgbuf;

#endif /* MESSAGES_H */
