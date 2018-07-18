#include "ipc_util.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

int getcurr_timestamp(char *caller_ts, int sz)
{
    if (caller_ts == NULL) return EXIT_FAILURE;
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char *timestamp = asctime(timeinfo) + 11; /* skip month and day */
    timestamp[strlen(timestamp) - 6] = '\0'; /* trim year */
    if (strlen(timestamp) > sz) return EXIT_FAILURE;
    strcpy(caller_ts, timestamp);
    return EXIT_SUCCESS;
}
