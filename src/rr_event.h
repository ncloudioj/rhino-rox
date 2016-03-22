#ifndef _RR_EVENT_H
#define _RR_EVENT_H

#include "rr_minheap.h"

#define   RR_EV_OK      0
#define   RR_EV_ERR     -1

#define   RR_EV_NONE    0
#define   RR_EV_READ    1
#define   RR_EV_WRITE   2

struct eventloop_t;

typedef void ev_callback(struct eventloop_t *el, int fd, void *ud, int mask);
typedef int timer_callback(struct eventloop_t *el, void *ud);

/* Event context */
typedef struct event_t {
    int mask;              /* one of RR_EV_[READ|WRITE] */
    ev_callback *read_cb;  /* callback for read event   */
    ev_callback *write_cb; /* callback for write event  */
    void *ud;              /* user data                 */
} event_t;

/* Fired event */
typedef struct fired_event_t {
    int fd;
    int mask;
} fired_event_t;

/* Timer event */
typedef struct timer_t {
    long sec;                 /* fire timer at: second part */
    long ms;                  /* fire timer at: millisecond part */
    void *ud;                 /* user data */
    timer_callback *timer_cb; /* timer callback */
} timer_t;

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
    *lv = *(timer_t *) rv;
    *rv = tmp;
}

/* State of an event loop */
typedef struct eventloop_t {
    int maxfd;            /* highest file descriptor currently registered */
    int size;             /* the capacity of the loop forfile descriptors */
    event_t *events;      /* registered events                            */
    fired_event_t *fired; /* fired events                                 */
    minheap_t *timers;    /* timer events                                 */
    int stop;             /* flag for stopping the event loop             */
    void *context;        /* wrap the context for epoll, kqueue etc.      */
} eventloop_t;

/* Create a event loop with the given size */
eventloop_t *el_loop_create(int size);

/* Release the event loop and free the resource */
void el_loop_free(eventloop_t *el);

/* Stop the event loop */
void el_loop_stop(eventloop_t *el);

/* Get event loop size */
int el_loop_get_size(eventloop_t *el);

/*
 * Poll and process all the fired events
 * Returns the total number of events processed
 */
int el_loop_poll(eventloop_t *el);

/*
 * Add a new event to event loop
 * params:
 *     fd: the file descriptor
 *     mask: RR_EV_READ or RR_EV_WRITE
 *     proc: event callback
 *     ud: user data
 * return: RR_EV_OK or RR_EV_ERR
 */
int el_event_add(eventloop_t *el, int fd, int mask, ev_callback *proc, void *ud);

/* Delete an event given the fd and event mask */
void el_event_del(eventloop_t *el, int fd, int mask);

/* Get the event mask for the given fd */
int el_event_get(eventloop_t *el, int fd);

/* wrap up the event loop */
void el_main(eventloop_t *el);

#endif
