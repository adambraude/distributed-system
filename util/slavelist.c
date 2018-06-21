#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

char **slaves;

//#define TESTING_UTILS
#ifdef TESTING_UTILS
int main(void)
{
    // test
    int fill_slave_arr(char *, char ***);
    int res = fill_slave_arr("../SLAVELIST", &slaves), i;
    for (i = 0; i < 7; i++){
        assert(slaves[i][9] - 48 == i + 3);
    }
    assert(res == 11);
    return 0;
}
#endif

/**
 * Fills the given string array with addresses of all the slaves.
 * Assumes that the given array is uninitialized (will reallocate memory)
 * Returns the number of IP addresses written to slave_arr_ptr, -1
 * if the slv_filename is invalid.
 */
int fill_slave_arr(char *slv_filename, char ***slave_arr_ptr)
{
    if (!slave_arr_ptr) {
        printf("Error: invalid slave array pointer given\n");
        return -2;
    }
    FILE *fp;
    fp = fopen(slv_filename, "r");
    if (fp) {
        int count = 0;
        char c;
        for (c = getc(fp); c != EOF; c = getc(fp))
            if (c == '\n')
                count++;
        if (*slave_arr_ptr != NULL){
            puts("About to free");
            free(slave_arr_ptr);
            puts("freed");
        }
        *slave_arr_ptr = (char **) malloc(sizeof(char *) * count);
        int i;
        for (i = 0; i < 128; i++){
            (*slave_arr_ptr)[i] = (char *) malloc(sizeof(char) * 32);
        }
        int read;
        char *line = NULL;
        size_t len = 0;
        int slave_arr_ptr_index = 0;
        fp = fopen(slv_filename, "r");
        while ((read = getline(&line, &len, fp)) != -1) {
            if (line[0] == 'T' || line[0] == 't') {
                break;
            }
            else if (line[0] != '#') { /* ignore commented line */
                strcpy((*slave_arr_ptr)[slave_arr_ptr_index], line);
                /* trim newline character from address */
                (*slave_arr_ptr)[slave_arr_ptr_index][strlen(line) - 1] = '\0';
                slave_arr_ptr_index++;
            }
        }
        fclose(fp);
        return slave_arr_ptr_index;
    }
    else {
        printf("Error: File not found: %s\n", slv_filename);
        return -1;
    }
}
