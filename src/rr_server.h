#ifndef _RR_SERVER_H
#define _RR_SERVER_H

#include "rr_network.h"
#include "rr_event.h"
#include "adlist.h"
#include "sds.h"
#include "robj.h"
#include "rr_db.h"

#include <stddef.h>
#include <stdbool.h>

#define PROTO_REPLY_MAX_LEN (16*1024)  /* max length of a reply buffer */
#define PROTO_QUERY_MAX_LEN (512*1024*1024) /* max length of the query string */
#define PROTO_IOBUF_LEN (16*1024) /* default read buffer length */
#define PROTO_INLINE_MAX_LEN (1024*64) /* max length of inline reads */
#define PROTO_MBULK_BIG_ARG (1024*32)  /* threshold of a big argument in multi bulk request */

#define PROTO_REQ_MULTIBULK 1          /* multibulk user request */
#define PROTO_REQ_INLINE 2             /* inline user request */

#define SERVER_CRON_MAX_FREQUENCY (1000) /* max frequency of server cron */
#define SERVER_RESERVED_FDS 32           /* number of reserved file descriptors */
#define SERVER_DEFAULT_PIDFILE "/var/run/rhino-rox.pid" /* default pidfile path */

#define CLIENT_PENDING_WRITE (1<<1) /* client has output to send but a write
                                       handler is yet not installed. */
#define CLIENT_CLOSE_AFTER_REPLY (1<<2) /* close once complete the entire reply */
#define CLIENT_CLOSE_ASAP (1<<3)        /* close client ASAP */

#define NET_MAX_WRITES_PER_EVENT (1024*64) /* Max reply size for each EVENT */

struct rr_server_t {
    char err[RR_NET_ERR_MAXLEN];       /* buffer for the error message */
    char *pidfile;                     /* pidfile path */
    eventloop_t *el;                   /* event loop */
    unsigned int max_clients;          /* max client size */
    unsigned long rejected;            /* counter for rejected clients */
    unsigned long served;              /* counter for served clients */
    unsigned long long max_memory;     /* max number of memory bytes to use */
    int hz;                            /* frequency of server cron */
    int lpfd;                          /* file descriptor for the listen socket */
    int lazyfree_server_del;           /* whether or not delete objects lazily */
    int shutdown;                      /* signal server to shutdown */
    size_t client_max_query_len;       /* max length for client query buffer */
    size_t stats_memory_usage;         /* current memory usage */
    rrdb_t **dbs;                      /* db array */
    int max_dbs;                       /* max number of databases */
    dict_t *commands;                  /* all commands */
    long long ncmd_complete;           /* number of command executed */
    list *clients;                     /* list of clients */
    list *clients_with_pending_writes; /* list of clients with pending writes */
    list *clients_to_close;            /* list of closable clients */
};

struct redisCommand;

typedef struct rr_client_t {
    int fd;                        /* client file descriptor */
    int req_type;                  /* request type: [inline|multibulk] */
    int multibulk_len;             /* number of multi bulk arguments left to read */
    long bulk_len;                 /* length of bulk argument in multi bulk request */
    sds query;                     /* query buffer */
    list *reply;                   /* reply list */
    struct redisCommand *cmd;      /* current cmd */
    struct redisCommand *lastcmd;  /* last cmd */
    int flags;                     /* client flags */
    int argc;                      /* number of arguments of current command. */
    robj **argv;                   /* arguments of the current command. */
    rrdb_t *db;                    /* current databasee */
    size_t replied_len;            /* total length of bytes already replied */
    size_t buf_sent_len;           /* length of bytes sent in the buffer */
    int buf_offset;                /* output buffer offset */
    char buf[PROTO_REPLY_MAX_LEN]; /* output buffer */
} rr_client_t;

/* Command flags. Please check the command table defined in the redis.c file
 * for more information about the meaning of every flag. */
#define CMD_WRITE 1                   /* "w" flag */
#define CMD_READONLY 2                /* "r" flag */
#define CMD_DENYOOM 4                 /* "m" flag */
#define CMD_NOT_USED_1 8              /* no longer used flag */
#define CMD_ADMIN 16                  /* "a" flag */
#define CMD_PUBSUB 32                 /* "p" flag */
#define CMD_NOSCRIPT  64              /* "s" flag */
#define CMD_RANDOM 128                /* "R" flag */
#define CMD_SORT_FOR_SCRIPT 256       /* "S" flag */
#define CMD_LOADING 512               /* "l" flag */
#define CMD_STALE 1024                /* "t" flag */
#define CMD_SKIP_MONITOR 2048         /* "M" flag */
#define CMD_ASKING 4096               /* "k" flag */
#define CMD_FAST 8192                 /* "F" flag */

/* Command call flags, see call() function */
#define CMD_CALL_NONE 0
#define CMD_CALL_SLOWLOG (1<<0)
#define CMD_CALL_STATS (1<<1)
#define CMD_CALL_PROPAGATE_AOF (1<<2)
#define CMD_CALL_PROPAGATE_REPL (1<<3)
#define CMD_CALL_PROPAGATE (CMD_CALL_PROPAGATE_AOF|CMD_CALL_PROPAGATE_REPL)
#define CMD_CALL_FULL (CMD_CALL_SLOWLOG | CMD_CALL_STATS | CMD_CALL_PROPAGATE)

typedef void redisCommandProc(rr_client_t *c);
typedef int *redisGetKeysProc(struct redisCommand *cmd, robj **argv, int argc, int *numkeys);
struct redisCommand {
    char *name;
    redisCommandProc *proc;
    int arity;
    char *sflags; /* Flags as string representation, one char per flag. */
    int flags;    /* The actual flags, obtained from the 'sflags' field. */
    /* Use a function to determine keys arguments in a command line.
     *      * Used for Redis Cluster redirect. */
    redisGetKeysProc *getkeys_proc;
    /* What keys should be loaded in background when calling this command? */
    int firstkey; /* The first argument that's a key (0 = no keys) */
    int lastkey;  /* The last argument that's a key */
    int keystep;  /* The step between first and last key */
    long long microseconds, calls;
};

struct rr_configuration;
void rr_server_init(struct rr_configuration *cfg);
void rr_server_close(void);
void rr_server_adjust_max_clients(void);
int rr_server_prepare_to_shutdown(void);
sds rr_server_get_info(void);

rr_client_t *rr_client_create(int fd);
void rr_client_free(rr_client_t *c);
void rr_client_reset(rr_client_t *c);

/* command look up */
struct redisCommand *cmd_lookup(sds name);
struct redisCommand *cmd_lookup_cstr(char *s);

/* Replying related functions */
void reply_add_obj(rr_client_t *c, robj *obj);
void reply_add_str(rr_client_t *c, const char *s, size_t len);
void reply_add_err_len(rr_client_t *c, const char *s, size_t len);
void reply_add_err(rr_client_t *c, const char *err);
void reply_add_err_format(rr_client_t *c, const char *fmt, ...);
void reply_add_status_len(rr_client_t *c, const char *s, size_t len);
void reply_add_status(rr_client_t *c, const char *status);
void reply_add_sds(rr_client_t *c, sds s);
void reply_add_longlong(rr_client_t *c, long long ll);
void reply_add_bulk_obj(rr_client_t *c, robj *obj);
void reply_add_bulk_cbuf(rr_client_t *c, const void *p, size_t len);
void reply_add_bulk_sds(rr_client_t *c, sds s);
void reply_add_bulk_cstr(rr_client_t *c, const char *s);
void reply_add_bulk_longlong(rr_client_t *c, long long ll);
void reply_add_multi_bulk_len(rr_client_t *c, long length);
int check_obj_type(rr_client_t *c, robj *o, int type);

int reply_write_to_client(int fd, rr_client_t *c, int handler_installed);

/* Callback for write event */
void reply_write_callback(eventloop_t *el, int fd, void *ud, int mask);

bool client_has_pending_replies(rr_client_t *c);


void objectCommand(rr_client_t *c);

/* declare the global server variable */
extern struct rr_server_t server;

#endif
