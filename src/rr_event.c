#include "rr_event.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifdef __linux__
#include "rr_epoll.c"
#endif

#if defined(__APPLE__) || defined(__FREEBSD__) || defined(__OPENBSD__) || defined(__NETBSD__)
#include "rr_kqueue.c"
#endif

eventloop_t *el_loop_create(int size) {
    eventloop_t *el;
    int i;

    if ((el = malloc(sizeof(*el))) == NULL) goto err;
    el->events = malloc(sizeof(event_t)*size);
    el->fired = malloc(sizeof(fired_event_t)*size);
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
        free(el->events);
        free(el->fired);
        minheap_free(el->timers);
        free(el);
    }
    return NULL;
}

void el_loop_free(eventloop_t *el) {
    el_context_free(el);
    minheap_free(el->timers);
    free(el->events);
    free(el->fired);
    free(el);
}

int el_loop_get_size(eventloop_t *el) {
    return el->size;
}

void el_loop_stop(eventloop_t *el) {
    el->stop = 1;
}

int el_event_add(eventloop_t *el, int fd, int mask, ev_callback *proc, void *ud)
{
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

void el_event_del(eventloop_t *el, int fd, int mask)
{
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

int el_loop_poll(eventloop_t *el) {
    int processed = 0, nevents;
    int j;
    struct timeval *tvp = NULL;

    nevents = el_context_poll(el, NULL);
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

    return processed; /* return the number of processed file/time events */
}

void el_main(eventloop_t *el) {
    while (!el->stop) {
        el_loop_poll(el);
    }
}
