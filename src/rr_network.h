#ifndef _RR_NETWORK_H
#define _RR_NETWORK_H

#define   RR_NET_OK       0
#define   RR_NET_ERR      -1

#define   RR_NET_ERR_MAXLEN   256
#define   RR_NET_BACKLOG      512

int rr_net_nonblock(char *err, int fd);
int rr_net_keepalive(char *err, int fd);
int rr_net_nodelay(char *err, int fd);

/*
 * create a tcp server (i.e bind & listen)
 * bindaddr: if NULL, it'll bind to "0.0.0.0"
 * af: af specifies the ai_family, e.g. AF_INET or AF_INET6
 * backlog: the queue size of listen socket
 */
int rr_net_tcpserver(char *err, int port, char *bindaddr, int af, int backlog);

#endif
