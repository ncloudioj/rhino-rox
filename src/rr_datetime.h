#ifndef _RR_DATETIME_H
#define _RR_DATETIME_H

#include <stdbool.h>

void rr_dt_now(long *sec, long *ms);

/*
 * Add a time delta (in milliseconds) to the current time
 */
void rr_dt_expire(long long delta, long *sec, long *ms);

/*
 * Compare the given time with current time
 * Return true if the given time is already past or equal to the current
 */
bool rr_dt_is_past(long sec, long ms);

#endif
