#ifndef _RR_SERVER_H
#define _RR_SERVER_H

#include "rr_network.h"

struct rr_server_t {
    char err[RR_NET_ERR_MAXLEN];  /* buffer for the error message */
    int lpfd;                       /* file descriptor for the listen socket */
    int shutdown;                   /* signal server to shutdown */
};

void rr_server_init(void);
void rr_server_close(void);

#endif
