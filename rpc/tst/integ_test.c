#include "integ_test.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 * Integration tests
 * Tests involving multiple node functions should go here.
 * Note that this may not run on the first attempt.
 */

int main(void)
{
    test_2pc_procedure();
}

/**
 * End-to-end test of two-phase bit vector commit.
 * Make sure that the master executable exists in src/
 * and that the slave node is running.
 */
void test_2pc_procedure(void)
{
    /* parameters: bit vector array,
     * addresses of running slaves
     */
    char *argv[] = {MASTER_EXEC, DB_CAPSTONE_2_ADDR};
    int status = 0;
    int chpid = fork();
    if (chpid < 0) {
        return;
    }
    else if (chpid == 0) {
        execv(MASTER_EXEC, argv);
    }
    else {
        wait(&status);
    }
}
