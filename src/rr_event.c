#include "rr_event.h"
#include "rr_datetime.h"
#include "rr_logging.h"
#include "rr_malloc.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifdef __linux__
#include "rr_epoll.c"
#endif

#if defined(__APPLE__) || defined(__FREEBSD__) || defined(__OPENBSD__) || defined(__NETBSD__)
#include "rr_kqueue.c"
#endif

/* timer_t callbacks for the queue */
static inline void *timer_cpy(void *dst, const void *src) {
    *(timer_t *) dst = *(const timer_t *) src;
    return dst;
}

static inline int timer_cmp(const void *lv, const void *rv) {
    const timer_t *l = (const timer_t *) lv;
    const timer_t *r = (const timer_t *) rv;

    if ((l->sec < r->sec) || (l->sec == r->sec && l->ms < r->ms))
        return -1;
    else if (l->sec == r->sec && l->ms == r->ms)
        return 0;
    else
        return 1;
}

static inline void timer_swp(void *lv, void *rv) {
    timer_t tmp;

    tmp = *(timer_t *) lv;
    *(timer_t *) lv = *(timer_t *) rv;
    *(timer_t *) rv = tmp;
}

eventloop_t *el_loop_create(int size) {
    eventloop_t *el;
    int i;

    if ((el = rr_malloc(sizeof(*el))) == NULL) goto err;
    el->events = rr_malloc(sizeof(event_t)*size);
    el->fired = rr_malloc(sizeof(fired_event_t)*size);
    el->timers = minheap_create(1, sizeof(timer_t), timer_cmp, timer_cpy, timer_swp);
    if (el->events == NULL || el->fired == NULL || el->timers == NULL) goto err;
    el->size = size;
    el->stop = 0;
    el->maxfd = -1;
    if (el_context_create(el) == -1) goto err;
    for (i = 0; i < size; i++)
        el->events[i].mask = RR_EV_NONE;
    return el;

err:
    if (el) {
        rr_free(el->events);
        rr_free(el->fired);
        minheap_free(el->timers);
        rr_free(el);
    }
    return NULL;
}

void el_loop_free(eventloop_t *el) {
    el_context_free(el);
    minheap_free(el->timers);
    rr_free(el->events);
    rr_free(el->fired);
    rr_free(el);
}

int el_loop_get_size(eventloop_t *el) {
    return el->size;
}

void el_loop_stop(eventloop_t *el) {
    el->stop = 1;
}

int el_event_add(eventloop_t *el, int fd, int mask, ev_callback *proc, void *ud) {
    if (fd >= el->size) {
        errno = ERANGE;
        return RR_EV_ERR;
    }
    event_t *e = &el->events[fd];

    if (el_context_add(el, fd, mask) == -1) return RR_EV_ERR;
    e->mask |= mask;
    if (mask & RR_EV_READ) e->read_cb = proc;
    if (mask & RR_EV_WRITE) e->write_cb = proc;
    e->ud = ud;
    if (fd > el->maxfd) el->maxfd = fd;
    return RR_EV_OK;
}

void el_event_del(eventloop_t *el, int fd, int mask) {
    if (fd >= el->size) return;
    event_t *e = &el->events[fd];
    if (e->mask == RR_EV_NONE) return;

    el_context_del(el, fd, mask);
    e->mask = e->mask & (~mask);
    if (fd == el->maxfd && e->mask == RR_EV_NONE) {
        /* Update the max fd */
        int j;

        for (j = el->maxfd-1; j >= 0; j--)
            if (el->events[j].mask != RR_EV_NONE) break;
        el->maxfd = j;
    }
}

int el_event_get(eventloop_t *el, int fd) {
    if (fd >= el->size) return RR_EV_NONE;
    event_t *e = &el->events[fd];

    return e->mask;
}

int el_loop_poll(eventloop_t *el, struct timeval *tvp) {
    int processed = 0, nevents;
    int j;

    nevents = el_context_poll(el, tvp);
    for (j = 0; j < nevents; j++) {
        event_t *e = &el->events[el->fired[j].fd];
        int mask = el->fired[j].mask;
        int fd = el->fired[j].fd;
        int rfired = 0;

        /* note the e->mask & mask & ... code: maybe an already processed
         * event removed an element that fired and we still didn't
         * processed, so we check if the event is still valid. */
        if (e->mask & mask & RR_EV_READ) {
            rfired = 1;
            e->read_cb(el, fd, e->ud, mask);
        }
        if (e->mask & mask & RR_EV_WRITE) {
            if (!rfired || e->write_cb != e->read_cb)
                e->write_cb(el, fd, e->ud, mask);
        }
        processed++;
    }

    return processed;
}

int el_timer_add(eventloop_t *el, long long milliseconds, timer_callback *proc, void *ud) {
    timer_t t;

    rr_dt_later(milliseconds, &t.sec, &t.ms);
    t.timer_cb = proc;
    t.ud = ud;
    if (!minheap_push(el->timers, &t)) return RR_EV_ERR;
    return RR_EV_OK;
}

int el_timer_process(eventloop_t *el) {
    int processed = 0, ret;
    uint32_t len = minheap_len(el->timers);

    while (len) {
        timer_t *t = (timer_t *) minheap_min(el->timers);
        if (!rr_dt_is_past(t->sec, t->ms)) break;

        t = (timer_t *) minheap_pop(el->timers);
        long long millisecond = t->timer_cb(el, t->ud);
        /* if the timer is still active, push it back to heap */
        if (millisecond > 0) {
            rr_dt_later(millisecond, &t->sec, &t->ms);
            ret = minheap_push(el->timers, t);
            assert(!ret);
        }
        processed++;
        len--;
    }
    return processed;
}

static bool el_pool_timeout(eventloop_t *el, struct timeval *tvp) {
    long now_sec, now_ms;

    if (!minheap_len(el->timers)) return false;

    timer_t *t = (timer_t *) minheap_min(el->timers);
    rr_dt_now(&now_sec, &now_ms);

    tvp->tv_sec = t->sec - now_sec;
    if (t->ms < now_ms) {
        tvp->tv_usec = ((t->ms+1000) - now_ms)*1000;
        tvp->tv_sec--;
    } else {
        tvp->tv_usec = (t->ms - now_ms)*1000;
    }
    if (tvp->tv_sec < 0) tvp->tv_sec = 0;
    if (tvp->tv_usec < 0) tvp->tv_usec = 0;
    return true;
}

void el_main(eventloop_t *el) {
    struct timeval tvp, *tv;
    int nevent, ntimer;

    while (!el->stop) {
        if (el_pool_timeout(el, &tvp))
            tv = &tvp;
        else
            tv = NULL; /* wait forever */
        nevent = el_loop_poll(el, tv);
        rr_log(RR_LOG_INFO, "Processed %d file events.", nevent);
        ntimer = el_timer_process(el);
        rr_log(RR_LOG_INFO, "Processed %d timer events.", ntimer);
    }
}
