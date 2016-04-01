#include "rr_ftmacro.h"

#include "rr_rhino_rox.h"
#include "rr_event.h"
#include "rr_logging.h"
#include "rr_network.h"
#include "rr_server.h"
#include "rr_config.h"
#include "rr_malloc.h"
#include "ini.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <sys/resource.h>

struct rr_server_t server;

static int server_cron(eventloop_t *el, void *ud);
static void before_polling(eventloop_t *el);
static void handle_accept(eventloop_t *el, int fd, void *ud, int mask);
static void add_client(int fd, int flags, char *ip);
static void client_free_async(rr_client_t *c);
static void handle_async_freed_clients(void);

static void rr_server_shutdown(int sig) {
    UNUSED(sig);
    server.shutdown = 1;
}

static void rr_server_signal(void) {
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGPIPE, &act, NULL);

    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = rr_server_shutdown;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    return;
}

void rr_server_init(rr_configuration *cfg) {
    rr_log_set_log_level(cfg->log_level);
    rr_log_set_log_file(cfg->log_file);

    server.shutdown = 0;
    server.max_memory = cfg->max_memory;
    server.max_clients = cfg->max_clients;
    rr_server_adjust_max_clients();
    server.hz = cfg->cron_frequency;
    server.served = 0;
    server.rejected = 0;
    server.stats_memory_usage = 0;
    rr_server_signal();
    if ((server.el = el_loop_create(server.max_clients)) == NULL) goto error;
    server.lpfd = rr_net_tcpserver(
            server.err, cfg->port, cfg->bind, AF_INET, cfg->tcp_backlog);
    if (server.lpfd == RR_EV_ERR) goto error;
    if (rr_net_nonblock(server.err, server.lpfd) == RR_ERROR) goto error;
    if (el_event_add(server.el, server.lpfd, RR_EV_READ, handle_accept, NULL)
          == RR_EV_ERR)
        goto error;

    if (el_timer_add(server.el, 1, server_cron, NULL) == RR_EV_ERR) {
        rr_log(RR_LOG_CRITICAL, "Can't create event loop timers.");
        exit(1);
    }
    el_loop_set_before_polling(server.el, before_polling);
    server.client_max_query_len = PROTO_QUERY_MAX_LEN;
    server.clients = listCreate();
    server.clients_with_pending_writes = listCreate();
    server.clients_to_close = listCreate();
    return;
error:
    rr_log(RR_LOG_CRITICAL, server.err);
    exit(1);
}

void rr_server_close(void) {
    el_loop_stop(server.el);
    el_loop_free(server.el);
}

static void rr_server_close_listening_sockets() {
    close(server.lpfd);
}

int rr_server_prepare_to_shutdown(void) {
    rr_server_close_listening_sockets();
    return RR_OK;
}

void rr_server_adjust_max_clients(void) {
    rlim_t maxfiles = server.max_clients + SERVER_RESERVED_FDS;
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        rr_log(RR_LOG_WARNING,
            "Fail to obtain the current NOFILE limit (%s), "
            "assuming 1024 and setting the max clients configuration accordingly.",
            strerror(errno));
        server.max_clients = 1024 - SERVER_RESERVED_FDS;
    } else {
        rlim_t oldlimit = limit.rlim_cur;

        /* Set the max number of files if the current limit is not enough
         * for our needs. */
        if (oldlimit < maxfiles) {
            rlim_t bestlimit;
            int setrlimit_error = 0;

            /* Try to set the file limit to match 'maxfiles' or at least
             * to the higher value supported less than maxfiles. */
            bestlimit = maxfiles;
            while(bestlimit > oldlimit) {
                rlim_t decr_step = 16;

                limit.rlim_cur = bestlimit;
                limit.rlim_max = bestlimit;
                if (setrlimit(RLIMIT_NOFILE, &limit) != -1) break;
                setrlimit_error = errno;

                /* We failed to set file limit to 'bestlimit'. Try with a
                 * smaller limit decrementing by a few FDs per iteration. */
                if (bestlimit < decr_step) break;
                bestlimit -= decr_step;
            }

            /* Assume that the limit we get initially is still valid if
             * our last try was even lower. */
            if (bestlimit < oldlimit) bestlimit = oldlimit;

            if (bestlimit < maxfiles) {
                int old_maxclients = server.max_clients;
                server.max_clients = bestlimit - SERVER_RESERVED_FDS;
                if (server.max_clients < 1) {
                    rr_log(RR_LOG_CRITICAL, "Your current 'ulimit -n' "
                        "of %llu is not enough for the server to start. "
                        "Please increase your open file limit to at least "
                        "%llu. Exiting",
                        (unsigned long long) oldlimit,
                        (unsigned long long) maxfiles);
                    exit(1);
                }
                rr_log(RR_LOG_WARNING, "You requested max_clients of %d "
                    "requiring at least %llu max file descriptors",
                    old_maxclients, (unsigned long long) maxfiles);
                rr_log(RR_LOG_WARNING, "Server can't set maximum open files "
                    "to %llu because of OS error: %s",
                    (unsigned long long) maxfiles, strerror(setrlimit_error));
                rr_log(RR_LOG_WARNING, "Current maximum open files is %llu. "
                    "maxclients has been reduced to %d to compensate for "
                    "low ulimit. "
                    "If you need higher maxclients increase 'ulimit -n'",
                    (unsigned long long) bestlimit, server.max_clients);
            } else {
                rr_log(RR_LOG_INFO, "Increased maximum number of open files "
                    "to %llu (it was originally set to %llu)",
                    (unsigned long long) maxfiles,
                    (unsigned long long) oldlimit);
            }
        }
    }
}

#define CLIENTS_CRON_MIN_ITERATIONS 5
static void client_cron(void) {
}

static int server_cron(eventloop_t *el, void *ud) {
    UNUSED(el);
    UNUSED(ud);

    if (server.shutdown) {
        if (rr_server_prepare_to_shutdown() == RR_OK) exit(0);
        rr_log(RR_LOG_INFO,
            "SIGTERM received but errors trying to shut down the server");
        server.shutdown = 0;
    }

    handle_async_freed_clients();

    client_cron();

    server.stats_memory_usage = rr_get_used_memory();

    return 1000/server.hz;
}

static void handle_async_freed_clients(void) {
    while (listLength(server.clients_to_close)) {
        listNode *ln = listFirst(server.clients_to_close);
        rr_client_t *c = listNodeValue(ln);
        c->flags &= ~CLIENT_CLOSE_ASAP;
        rr_client_free(c);
        listDelNode(server.clients_to_close, ln);
    }
}

static void client_free_async(rr_client_t *c) {
    if (c->flags & CLIENT_CLOSE_ASAP) return;
    c->flags |= CLIENT_CLOSE_ASAP;
    listAddNodeTail(server.clients_to_close,c);
}

static int handle_clients_with_pending_writes() {
    listIter li;
    listNode *ln;
    int processed = listLength(server.clients_with_pending_writes);

    listRewind(server.clients_with_pending_writes, &li);
    while((ln = listNext(&li))) {
        rr_client_t *c = listNodeValue(ln);
        c->flags &= ~CLIENT_PENDING_WRITE;
        listDelNode(server.clients_with_pending_writes, ln);

        /* Try to write buffers to the client socket. */
        if (reply_write_to_client(c->fd, c, 0) == RR_ERROR) continue;

        /* If there is nothing left, do nothing. Otherwise install
         * the write handler. */
        if (client_has_pending_replies(c) &&
            el_event_add(server.el, c->fd, RR_EV_WRITE, reply_write_callback, c)
            == RR_EV_ERR) {
            client_free_async(c);
        }
    }
    return processed;
}

static void before_polling(eventloop_t *el) {
    UNUSED(el);
    handle_clients_with_pending_writes();
}

/* It does nothing except echoing the query now */
static int process_inline_input(rr_client_t *c) {
    char *newline;
    sds aux;
    size_t querylen;

    /* Search for end of line */
    newline = strchr(c->query, '\n');

    /* Nothing to do without a \r\n */
    if (newline == NULL) {
        if (sdslen(c->query) > PROTO_INLINE_MAX_LEN) {
            reply_add_err(c, "Protocol error: too big inline request");
        }
        return RR_ERROR;
    }

    /* Handle the \r\n case. */
    if (newline && newline != c->query && *(newline-1) == '\r')
        newline--;

    /* Split the input buffer up to the \r\n */
    querylen = newline - c->query;
    aux = sdsnewlen(c->query, querylen);
    reply_add_sds(c, aux);

    /* Slice the query buffer to delete the processed line */
    sdsrange(c->query, querylen+2, -1);

    return RR_OK;
}

static void process_user_input(rr_client_t *c) {
    while (sdslen(c->query)) {
       if (process_inline_input(c) != RR_OK) break;
    }
}

static void handle_read_from_client(eventloop_t *el, int fd, void *ud, int mask) {
   rr_client_t *c = (rr_client_t *) ud;

    int nread, readlen;
    int qlen;
    UNUSED(el);
    UNUSED(mask);

    qlen = sdslen(c->query);
    readlen = PROTO_IOBUF_LEN;
    c->query = sdsMakeRoomFor(c->query, readlen);
    nread = read(fd, c->query+qlen, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) {
            return;
        } else {
            rr_log(RR_LOG_ERROR, "Reading from client: %s", strerror(errno));
            rr_client_free(c);
            return;
        }
    } else if (nread == 0) {
        rr_log(RR_LOG_INFO, "Client closed connection");
        rr_client_free(c);
        return;
    }
    sdsIncrLen(c->query, nread);

    if (sdslen(c->query) > server.client_max_query_len) {
        rr_log(RR_LOG_WARNING, "Closing client that reached max query buffer length.");
        rr_client_free(c);
        return;
    }

    process_user_input(c);
}


/* Client.reply list dup and free methods */
static void *list_reply_dup(void *val) {
    return sdsdup(val);
}

static void list_reply_free(void *val) {
    sdsfree(val);
}

rr_client_t *rr_client_create(int fd) {
    rr_client_t *c = rr_malloc(sizeof(rr_client_t));
    if (c == NULL) {
        rr_log(RR_LOG_CRITICAL, "Error creating a new client: oom");
        return NULL;
    }

    if (fd != -1) {
        if (rr_net_nonblock(server.err, fd) != RR_NET_OK) goto error;
        if (rr_net_nodelay(server.err, fd) != RR_NET_OK) goto error;
        if (rr_net_keepalive(server.err, fd) != RR_NET_OK) goto error;
        if (el_event_add(server.el, fd, RR_EV_READ, handle_read_from_client, c) == RR_EV_ERR) goto error;
    }

    c->fd = fd;
    c->query = sdsempty();
    c->replied_len = 0;
    c->buf_sent_len = 0;
    c->reply = listCreate();
    listSetFreeMethod(c->reply, list_reply_free);
    listSetDupMethod(c->reply, list_reply_dup);
    c->flags = 0;
    if (fd != -1) listAddNodeTail(server.clients, c);
    return c;
error:
    rr_free(c);
    return NULL;
}

static void unlink_client(rr_client_t *c) {
    listNode *node;

    if (c->fd != -1) {
        node = listSearchKey(server.clients, c);
        assert(node);
        listDelNode(server.clients, node);

        el_event_del(server.el, c->fd, RR_EV_READ);
        el_event_del(server.el, c->fd, RR_EV_WRITE);
        close(c->fd);
        c->fd = -1;
    }
}

void rr_client_free(rr_client_t *c) {
    unlink_client(c);
    sdsfree(c->query);
    listRelease(c->reply);
    rr_free(c);
}

static void add_client(int fd, int flags, char *ip) {
    UNUSED(ip);

    rr_client_t *c;
    if ((c = rr_client_create(fd)) == NULL) {
        rr_log(RR_LOG_WARNING,
            "Error registering fd event for the new client: %s (fd=%d)",
            strerror(errno), fd);
        close(fd); /* May be already closed, just ignore errors */
        return;
    }
    /* If maxclient directive is set and this is one client more... close the
     * connection. Note that we create the client instead to check before
     * for this condition, since now the socket is already set in non-blocking
     * mode and we can send an error for free using the Kernel I/O */
    if (listLength(server.clients) > server.max_clients) {
        char *err = "-ERR max number of clients reached\r\n";

        /* That's a best effort error message, don't check write errors */
        if (write(c->fd, err, strlen(err)) == -1) {
            /* Nothing to do, Just to avoid the warning... */
        }
        server.rejected++;
        rr_client_free(c);
        return;
    }

    server.served++;
    c->flags |= flags;
}

static void handle_accept(eventloop_t *el, int fd, void *ud, int mask) {
    int cport, cfd, max = RR_NET_MAXACCEPT;
    char cip[RR_NET_MAXIPLEN];
    UNUSED(el);
    UNUSED(mask);
    UNUSED(ud);

    while(max--) {
        cfd = rr_net_accept(server.err, fd, cip, sizeof(cip), &cport);
        if (cfd == RR_NET_ERR) {
            if (errno != EWOULDBLOCK)
                rr_log(RR_LOG_WARNING, "Accepting client connection: %s", server.err);
            return;
        }
        rr_log(RR_LOG_INFO, "Accepted %s:%d", cip, cport);
        add_client(cfd, 0, cip);
    }
}

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    rr_configuration cfg;
    if (rr_config_load("rhino-rox.ini", &cfg) == RR_ERROR) {
        rr_log(RR_LOG_CRITICAL, "Failed to load the config file");
        exit(1);
    }
    rr_server_init(&cfg);
    el_main(server.el);
    rr_server_close();
}
