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
    char *timestamp = asctime(timeinfo);
    timestamp[strlen(timestamp) - 7] = '\0'; /* trim year */
    if (strlen(timestamp) > sz) return EXIT_FAILURE;
    int i;
    for (i = 0; i < strlen(timestamp); i++) {
        if (timestamp[i] == ' ') timestamp[i] = '_';
    }
    strcpy(caller_ts, timestamp);
    return EXIT_SUCCESS;
}
