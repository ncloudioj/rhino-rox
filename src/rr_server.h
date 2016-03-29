#ifndef _RR_SERVER_H
#define _RR_SERVER_H

#include "rr_network.h"
#include "rr_event.h"
#include "adlist.h"
#include "sds.h"

#include <stddef.h>

#define PROTO_REPLY_MAX_LEN (16*1024)  /* max length of a reply buffer */
#define PROTO_QUERY_MAX_LEN (512*1024*1024) /* max length of the query string */
#define PROTO_IOBUF_LEN (16*1024) /* default read buffer length */

struct rr_server_t {
    char err[RR_NET_ERR_MAXLEN];       /* buffer for the error message */
    eventloop_t *el;                   /* event loop */
    unsigned int max_size;                 /* max client size */
    unsigned int rejected;             /* counter for rejected clients */
    unsigned int served;               /* counter for served clients */
    int lpfd;                          /* file descriptor for the listen socket */
    int shutdown;                      /* signal server to shutdown */
    size_t client_max_query_len;       /* max length for client query buffer */
    list *clients;                     /* list of clients */
    list *clients_with_pending_writes; /* list of clients with pending writes */
    list *clients_to_close;            /* list of closable clients */
};

typedef struct rr_client_t {
    int fd;                        /* client file descriptor */
    sds query;                     /* query buffer */
    list *reply;                   /* reply list */
    int flags;                     /* client flags */
    int buf_offset;                /* output buffer offset */
    char buf[PROTO_REPLY_MAX_LEN]; /* output buffer */
} rr_client_t;

void rr_server_init(void);
void rr_server_close(void);

#endif
