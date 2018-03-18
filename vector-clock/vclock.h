#ifndef VCLOCK_H
#define VCLOCK_H

#include "vclock_type.h"

#ifndef MAX
#define MAX(a, b) (a < b ? b : a)
#endif /* MAX */

vector_clock create_vclock(void);
void destroy_vclock(vector_clock);
int compare_vclock(vector_clock, vector_clock);
void handle_recvd_vclock(vector_clock, vector_clock, int);
void prep_send_vclock(vector_clock, int);

#endif /* VCLOCK_H */
