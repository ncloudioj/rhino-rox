#ifndef _RR_DATETIME_H
#define _RR_DATETIME_H

void rr_dt_now(long *sec, long *ms);

/*
 * Add a delta (in milliseconds) to the current time
 */
void rr_dt_until(long long delta, long *sec, long *ms);

#endif
