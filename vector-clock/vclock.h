#ifndef VCLOCK_H
#include "vclock_type.h"
#ifndef MAX
#define MAX(a, b) (a < b ? b : a)
#endif /* MAX */

vector_clock create_vclock(void);
void handle_recvd_vclock(vector_clock current_clock, vector_clock received_clock, int receiver_id);
void prep_send_vclock(vector_clock current_clock, int receiver_id);
#endif /* VCLOCK_H */
