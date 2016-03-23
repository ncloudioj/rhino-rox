#include "rr_rhino_rox.h"

#include "rr_event.h"
#include "rr_logging.h"
#include "rr_network.h"
#include "rr_server.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

struct rr_server_t server;


static void handle_accept(eventloop_t *el, int fd, void *ud, int mask);
static void free_client(rr_client_t *c);

static void rr_server_shutdown(int sig) {
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

void rr_server_init(void) {
    server.shutdown = 0;
    server.size = 10000;
    server.nrejected = 0;
    rr_server_signal();
    if ((server.el = el_loop_create(server.size)) == NULL) goto error;
    if ((server.lpfd = rr_net_tcpserver(server.err, 6000, NULL, AF_INET, RR_NET_BACKLOG)) == RR_ERROR) goto error;
    if (rr_net_nonblock(server.err, server.lpfd) == RR_ERROR) goto error;
    if (el_event_add(server.el, server.lpfd, RR_EV_READ, handle_accept, NULL) == RR_EV_ERR) goto error;
    return;
error:
    rr_log(RR_LOG_CRITICAL, server.err);
    exit(1);
}

void rr_server_close(void) {
    if (server.shutdown) {
        el_loop_stop(server.el);
        el_loop_free(server.el);
    }
}

static void process_user_input(rr_client_t *c) {

}

static void handle_read_from_client(eventloop_t *el, int fd, void *ud, int mask) {
   rr_client_t *c = (rr_client_t *) ud;

    int nread, readlen;
    size_t qblen;
    UNUSED(el);
    UNUSED(mask);

    readlen = RR_NET_BUFSIZE;
    nread = read(fd, c->in->elm, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) {
            return;
        } else {
            rr_log(RR_LOG_ERROR, "Reading from client: %s", strerror(errno));
            free_client(c);
            return;
        }
    } else if (nread == 0) {
        rr_log(RR_LOG_INFO, "Client closed connection");
        free_client(c);
        return;
    }

    process_user_input(c);
}

static rr_client_t *create_client(int fd) {
    rr_client_t *c = malloc(sizeof(rr_client_t));
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
    c->in = array_create(RR_NET_BUFSIZE, sizeof(char));
    c->out = array_create(RR_NET_BUFSIZE, sizeof(char));
    if (c->in == NULL || c->out == NULL) goto error;
    c->flags = 0;
    if (fd != -1) listAddNodeTail(server.clients, c);
    return c;
error:
    free(c);
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

static void free_client(rr_client_t *c) {
    unlink_client(c);
    array_free(c->in);
    array_free(c->out);
    free(c);
}

static void add_client(int fd, int flags, char *ip) {
    rr_client_t *c;
    if ((c = create_client(fd)) == NULL) {
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
    if (listLength(server.clients) > server.size) {
        char *err = "-ERR max number of clients reached\r\n";

        /* That's a best effort error message, don't check write errors */
        if (write(c->fd, err, strlen(err)) == -1) {
            /* Nothing to do, Just to avoid the warning... */
        }
        server.nrejected++;
        free_client(c);
        return;
    }

    server.nserved++;
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
    rr_server_init();
    rr_server_close();
}
