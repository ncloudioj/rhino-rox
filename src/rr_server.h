#ifndef _RR_SERVER_H
#define _RR_SERVER_H

#include "rr_network.h"
#include "rr_event.h"
#include "rr_array.h"
#include "adlist.h"

struct rr_server_t {
    char err[RR_NET_ERR_MAXLEN];    /* buffer for the error message */
    eventloop_t *el;                /* event loop */
    unsigned int size;              /* max client size */
    unsigned int nrejected;         /* counter for rejected clients */
    unsigned int nserved;           /* counter for served clients */
    int lpfd;                       /* file descriptor for the listen socket */
    int shutdown;                   /* signal server to shutdown */
    list *clients;                  /* list of clients */
};

typedef struct rr_client_t {
    int fd; /* client file descriptor */
    array_t *in; /* input buffer */
    array_t *out; /* output buffer */
    int flags; /* client flags */
} rr_client_t;

void rr_server_init(void);
void rr_server_close(void);

#endif
