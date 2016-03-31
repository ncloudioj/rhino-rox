#ifndef _RR_SERVER_H
#define _RR_SERVER_H

#include "rr_network.h"
#include "rr_event.h"
#include "adlist.h"
#include "sds.h"

#include <stddef.h>
#include <stdbool.h>

#define PROTO_REPLY_MAX_LEN (16*1024)  /* max length of a reply buffer */
#define PROTO_QUERY_MAX_LEN (512*1024*1024) /* max length of the query string */
#define PROTO_IOBUF_LEN (16*1024) /* default read buffer length */
#define PROTO_INLINE_MAX_LEN (1024*64) /* max length of inline reads */

#define SERVER_CRON_MAX_FREQUENCY (1000) /* max frequency of server cron */
#define SERVER_RESERVED_FDS 32           /* number of reserved file descriptors */

#define CLIENT_PENDING_WRITE (1<<1) /* Client has output to send but a write
                                       handler is yet not installed. */
#define CLIENT_CLOSE_AFTER_REPLY (1<<2) /* Close once complete the entire reply */
#define CLIENT_CLOSE_ASAP (1<<3)        /* Close client ASAP */

#define NET_MAX_WRITES_PER_EVENT (1024*64) /* Max reply size for each EVENT */

struct rr_server_t {
    char err[RR_NET_ERR_MAXLEN];       /* buffer for the error message */
    eventloop_t *el;                   /* event loop */
    unsigned int max_clients;          /* max client size */
    unsigned int rejected;             /* counter for rejected clients */
    unsigned int served;               /* counter for served clients */
    unsigned long long max_memory;     /* Max number of memory bytes to use */
    int hz;                            /* frequency of server cron */
    int lpfd;                          /* file descriptor for the listen socket */
    int shutdown;                      /* signal server to shutdown */
    size_t client_max_query_len;       /* max length for client query buffer */
    size_t stats_memory_usage;         /* current memory usage */
    list *clients;                     /* list of clients */
    list *clients_with_pending_writes; /* list of clients with pending writes */
    list *clients_to_close;            /* list of closable clients */
};

typedef struct rr_client_t {
    int fd;                        /* client file descriptor */
    sds query;                     /* query buffer */
    list *reply;                   /* reply list */
    int flags;                     /* client flags */
    size_t replied_len;            /* total length of bytes already replied */
    size_t buf_sent_len;           /* length of bytes sent in the buffer */
    int buf_offset;                /* output buffer offset */
    char buf[PROTO_REPLY_MAX_LEN]; /* output buffer */
} rr_client_t;

struct rr_configuration;
void rr_server_init(struct rr_configuration *cfg);
void rr_server_close(void);
void rr_server_adjust_max_clients(void);
int rr_server_prepare_to_shutdown(void);

rr_client_t *rr_client_create(int fd);
void rr_client_free(rr_client_t *c);

/* Replying related functions */
void reply_add_str(rr_client_t *c, const char *s, size_t len);
void reply_add_err_len(rr_client_t *c, const char *s, size_t len);
void reply_add_err(rr_client_t *c, const char *err);
void reply_add_sds(rr_client_t *c, sds s);
int reply_write_to_client(int fd, rr_client_t *c, int handler_installed);

/* Callback for write event */
void reply_write_callback(eventloop_t *el, int fd, void *ud, int mask);

bool client_has_pending_replies(rr_client_t *c);

#endif
