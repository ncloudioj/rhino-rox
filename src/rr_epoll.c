#include <sys/epoll.h>

typedef struct el_context_t {
    int epfd;
    struct epoll_event *events;
} el_context_t;

static int el_context_create(eventloop_t *el) {
    el_context_t *context = rr_malloc(sizeof(el_context_t));

    if (!context) return -1;
    context->events = rr_malloc(sizeof(struct epoll_event)*el->size);
    if (!context->events) {
        rr_free(context);
        return -1;
    }
    context->epfd = epoll_create(1024); /* 1024 is just a hint for the kernel */
    if (context->epfd == -1) {
        rr_free(context->events);
        rr_free(context);
        return -1;
    }
    el->context = context;
    return 0;
}

static void el_context_free(eventloop_t *el) {
    el_context_t *context = el->context;

    close(context->epfd);
    rr_free(context->events);
    rr_free(context);
}

static int el_context_add(eventloop_t *el, int fd, int mask) {
    el_context_t *context = el->context;
    struct epoll_event ee;
    /* If the fd was already monitored for some event, we need a MOD
     * operation. Otherwise we need an ADD operation. */
    int op = el->events[fd].mask == RR_EV_NONE ?
            EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    ee.events = 0;
    mask |= el->events[fd].mask; /* Merge old events */
    if (mask & EL_EV_READ) ee.events |= EPOLLIN;
    if (mask & EL_EV_WRITE) ee.events |= EPOLLOUT;
    ee.data.u64 = 0; /* avoid valgrind warning */
    ee.data.fd = fd;
    if (epoll_ctl(context->epfd, op, fd, &ee) == -1) return -1;
    return 0;
}

static void el_context_del(eventloop_t *el, int fd, int delmask) {
    el_context_t *context = el->context;
    struct epoll_event ee;
    int mask = el->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & EL_EV_READ) ee.events |= EPOLLIN;
    if (mask & EL_EV_WRITE) ee.events |= EPOLLOUT;
    ee.data.u64 = 0; /* avoid valgrind warning */
    ee.data.fd = fd;
    if (mask != RR_EV_NONE) {
        epoll_ctl(context->epfd,EPOLL_CTL_MOD,fd,&ee);
    } else {
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL. */
        epoll_ctl(context->epfd, EPOLL_CTL_DEL, fd, &ee);
    }
}

static int el_context_poll(eventloop_t *el, struct timeval *tvp) {
    el_context_t *context = el->context;
    int retval, nevent = 0;

    retval = epoll_wait(context->epfd, context->events, el->size,
            tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1);
    if (retval > 0) {
        int j;

        nevent = retval;
        for (j = 0; j < nevent; j++) {
            int mask = 0;
            struct epoll_event *e = context->events+j;

            if (e->events & EPOLLIN) mask |= EL_EV_READ;
            if (e->events & EPOLLOUT) mask |= EL_EV_WRITE;
            if (e->events & EPOLLERR) mask |= EL_EV_WRITE;
            if (e->events & EPOLLHUP) mask |= EL_EV_WRITE;
            el->fired[j].fd = e->data.fd;
            el->fired[j].mask = mask;
        }
    }
    return nevent;
}
