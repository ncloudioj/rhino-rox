#ifndef _RR_CMD_FTS_H
#define _RR_CMD_FTS_H

#include "rr_server.h"

void rr_cmd_dset(rr_client_t *c);
void rr_cmd_dget(rr_client_t *c);
void rr_cmd_dsearch(rr_client_t *c);
void rr_cmd_ddel(rr_client_t *c);
void rr_cmd_dlen(rr_client_t *c);

#endif /* ifndef _RR_CMD_FTS_H */
