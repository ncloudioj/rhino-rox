#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

/* Event loop context for kqueue */
typedef struct el_context_t {
   int kqfd;
   struct kevent *events;
} el_context_t;

static int el_context_create(eventloop_t *el) {
    el_context_t *context = rr_malloc(sizeof(el_context_t));

    if (!context) return RR_EV_ERR;
    context->events = rr_malloc(sizeof(struct kevent)*el->size);
    if (!context->events) {
        rr_free(context);
        return RR_EV_ERR;
    }
    context->kqfd = kqueue();
    if (context->kqfd == -1) {
        rr_free(context->events);
        rr_free(context);
        return RR_EV_ERR;
    }
    el->context = context;
    return RR_EV_OK;
}

static void el_context_free(eventloop_t *el) {
    el_context_t *context = el->context;

    close(context->kqfd);
    rr_free(context->events);
    rr_free(context);
}

static int el_context_add(eventloop_t *el, int fd, int mask) {
    el_context_t *context = el->context;
    struct kevent ke;

    if (mask & RR_EV_READ) {
        EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(context->kqfd, &ke, 1, NULL, 0, NULL) == -1) return -1;
    }
    if (mask & RR_EV_WRITE) {
        EV_SET(&ke, fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        if (kevent(context->kqfd, &ke, 1, NULL, 0, NULL) == -1) return -1;
    }
    return 0;
}

static void el_context_del(eventloop_t *el, int fd, int mask) {
    el_context_t *context = el->context;
    struct kevent ke;

    if (mask & RR_EV_READ) {
        EV_SET(&ke, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        kevent(context->kqfd, &ke, 1, NULL, 0, NULL);
    }
    if (mask & RR_EV_WRITE) {
        EV_SET(&ke, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        kevent(context->kqfd, &ke, 1, NULL, 0, NULL);
    }
    return;
}

static int el_context_poll(eventloop_t *el, struct timeval *tvp) {
    el_context_t *context = el->context;
    int retval, nevents = 0;

    if (tvp != NULL) {
        struct timespec timeout;
        timeout.tv_sec = tvp->tv_sec;
        timeout.tv_nsec = tvp->tv_usec * 1000;
        retval = kevent(context->kqfd, NULL, 0, context->events, el->size, &timeout);
    } else {
        retval = kevent(context->kqfd, NULL, 0, context->events, el->size, NULL);
    }

    if (retval > 0) {
        int j;

        nevents = retval;
        for(j = 0; j < nevents; j++) {
            int mask = 0;
            struct kevent *e = context->events+j;

            if (e->filter == EVFILT_READ) mask |= RR_EV_READ;
            if (e->filter == EVFILT_WRITE) mask |= RR_EV_WRITE;
            el->fired[j].fd = e->ident;
            el->fired[j].mask = mask;
        }
    }
    return nevents;
}
