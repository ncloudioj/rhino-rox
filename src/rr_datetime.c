#include "rr_datetime.h"

#include <stddef.h>
#include <sys/time.h>
#include <stdbool.h>

void rr_dt_now(long *sec, long *ms) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *sec = tv.tv_sec;
    *ms = tv.tv_usec/1000;
}

void rr_dt_expire_at(long long delta, long *sec, long *ms) {
    long now_sec, now_ms, utl_sec, utl_ms;

    rr_dt_now(&now_sec, &now_ms);
    utl_sec = now_sec + delta/1000;
    utl_ms = now_ms + delta%1000;
    if (utl_ms >= 1000) {
        utl_sec++;
        utl_ms -= 1000;
    }
    
    *sec = utl_sec;
    *ms = utl_ms;
}

bool rr_dt_is_past(long sec, long ms) {
    long now_sec, now_ms;

    rr_dt_now(&now_sec, &now_ms);
    if (now_sec > sec || (now_sec == sec && now_ms >= ms))
        return true;
    else
        return false;
}
