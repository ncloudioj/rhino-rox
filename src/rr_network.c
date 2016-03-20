#include "rr_ftmacro.h"

#include "rr_rhino_rox.h"
#include "rr_network.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>


static void rr_net_error(char *err, const char *fmt, ...) {
    va_list vl;
 
    if (!err) return;
    va_start(vl, fmt);
    vsnprintf(err, RR_NET_ERR_MAXLEN, fmt, vl);
    va_end(vl);
}


static int rr_net_reuseaddr(char *err, int fd) {
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        rr_net_error(err, "setsockopt(SO_REUSEADDR): %s", strerror(errno));
        return RR_NET_ERR;
    }
    return RR_NET_OK;
}

static int rr_net_create_socket(char *err, int domain) {
    int s;
    if ((s = socket(domain, SOCK_STREAM, 0)) == -1) {
        rr_net_error(err, "creating socket: %s", strerror(errno));
        return RR_NET_ERR;
    }

    if (rr_net_reuseaddr(err,s) == RR_NET_ERR) {
        close(s);
        return RR_NET_ERR;
    }
    return s;
}

int rr_net_nonblock(char *err, int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1) {
        rr_net_error(err, "fcntl(F_GETFL): %s", strerror(errno));
        return RR_NET_ERR;
    }

    if (fcntl(fd, F_SETFL, flag|O_NONBLOCK) == -1) {
        rr_net_error(err, "fcntl(F_SETFL): %s", strerror(errno));
        return RR_NET_ERR;
    }
    return RR_NET_OK;
}

int rr_net_keepalive(char *err, int fd) {
    int keepalive = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) == -1) {
        rr_net_error(err, "setsockopt(SO_KEEPALIVE): %s", strerror(errno));
        return RR_NET_ERR;
    }
    return RR_NET_OK;
}

int rr_net_nodelay(char *err, int fd) {
    int nodelay = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) == -1) {
        rr_net_error(err, "setsockopt(TCP_NODELAY): %s", strerror(errno));
        return RR_NET_ERR;
    }
    return RR_NET_OK;
}

static int rr_net_only_inet6(char *err, int socket) {
    int inet6 = 1;
    if (setsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, &inet6, sizeof(inet6)) == -1) {
        rr_net_error(err, "setsockopt(IPV6_V6ONLY): %s", strerror(errno));
        close(socket);
        return RR_NET_ERR;
    }
    return RR_NET_OK;
}

static int rr_net_listen(char *err, int socket, struct sockaddr *sa, socklen_t len, int backlog) {
    if (bind(socket, sa, len) == -1) {
        rr_net_error(err, "bind: %s", strerror(errno));
        close(socket);
        return RR_NET_ERR;
    }

    if (listen(socket, backlog) == -1) {
        rr_net_error(err, "listen: %s", strerror(errno));
        close(socket);
        return RR_NET_ERR;
    }
    return RR_NET_OK;
}

static int rr_net_generic_accpet(char *err, int socket, struct sockaddr *sa, socklen_t *len) {
    int fd;
    while(1) {
        fd = accept(socket, sa, len);
        if (fd == -1) {
            if (errno == EINTR)
                continue;
            else {
                rr_net_error(err, "accept: %s", strerror(errno));
                return RR_NET_ERR;
            }
        }
        break;
    }
    return fd;
}

int rr_net_accept(char *err, int socket, char *ip, size_t ip_len, int *port) {
    int fd;
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);
    if ((fd = rr_net_generic_accpet(err, socket, (struct sockaddr*)&sa, &salen)) == -1)
        return RR_NET_ERR;

    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sa;
        if (ip) inet_ntop(AF_INET, (void*)&(s->sin_addr), ip, ip_len);
        if (port) *port = ntohs(s->sin_port);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6, (void*)&(s->sin6_addr), ip, ip_len);
        if (port) *port = ntohs(s->sin6_port);
    }
    return fd;
}

int rr_net_tcpserver(char *err, int port, char *bindaddr, int af, int backlog) {
    int s, error;
    char _port[6];  /* strlen("65535") */
    struct addrinfo hints, *servinfo, *p;

    snprintf(_port, 6, "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    /* will bind to INADDR_ANY if bindaddr is NULL */

    if ((error = getaddrinfo(bindaddr, _port, &hints, &servinfo)) != 0) {
        rr_net_error(err, "%s", gai_strerror(error));
        return RR_NET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (af == AF_INET6 && rr_net_only_inet6(err,s) == RR_NET_ERR) goto error;
        if (rr_net_reuseaddr(err,s) == RR_NET_ERR) goto error;
        if (rr_net_listen(err, s, p->ai_addr, p->ai_addrlen, backlog) == RR_NET_ERR) goto error;
        goto end;
    }
    if (p == NULL) {
        rr_net_error(err, "failed to bind socket");
        goto error;
    }

error:
    close(s);
    s = RR_NET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}
