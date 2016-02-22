#include "rr_rhino_rox.h"

#include "rr_logging.h"
#include "rr_network.h"
#include "rr_server.h"

#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <netinet/in.h>

struct rr_server_t server;

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
    rr_server_signal();
    if ((server.lpfd = rr_net_tcpserver(server.err, 6000, NULL, AF_INET, RR_NET_BACKLOG)) == RR_ERROR) goto error;
    if (rr_net_nonblock(server.err, server.lpfd) == RR_ERROR) goto error;
error:
    rr_log(RR_LOG_CRITICAL, server.err);
    exit(1);
    return;
}

void rr_server_close() {

}
