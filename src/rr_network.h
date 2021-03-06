#ifndef _RR_NETWORK_H
#define _RR_NETWORK_H

#include <stddef.h>
#include <sys/types.h>

#define   RR_NET_OK       0
#define   RR_NET_ERR      -1

#define   RR_NET_ERR_MAXLEN   256
#define   RR_NET_BACKLOG      512
#define   RR_NET_MAXACCEPT    1000
#define   RR_NET_MAXIPLEN     46

int rr_net_nonblock(char *err, int fd);
int rr_net_keepalive(char *err, int fd);
int rr_net_nodelay(char *err, int fd);

/*
 * accept and copy remote ip and port
 */
int rr_net_accept(char *err, int socket, char *ip, size_t ip_len, int *port);

/*
 * accept the unix domain socket
 */
int rr_unix_accept(char *err, int socket);

/*
 * create a tcp server (i.e bind & listen)
 * bindaddr: if NULL, it'll bind to "0.0.0.0"
 * af: af specifies the ai_family, e.g. AF_INET or AF_INET6
 * backlog: the queue size of listen socket
 */
int rr_net_tcpserver(char *err, int port, char *bindaddr, int af, int backlog);

/*
 * create a unix domain socket server (i.e bind & listen)
 * path: path of unix domain socket
 * perm: permission for chmod
 * backlog: the queue size of listen socket
 */
int rr_net_unixserver(char *err, char *path, mode_t perm, int backlog);
#endif
