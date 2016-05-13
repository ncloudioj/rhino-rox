#ifndef _RR_CMD_HEAPQ_H
#define _RR_CMD_HEAPQ_H

#include "rr_server.h"

void rr_cmd_hqpush(rr_client_t *c);
void rr_cmd_hqpeek(rr_client_t *c);
void rr_cmd_hqpop(rr_client_t *c);
void rr_cmd_hqpopn(rr_client_t *c);
void rr_cmd_hqlen(rr_client_t *c);

#endif /* ifndef RR_CMD_HEAPQ_H */
