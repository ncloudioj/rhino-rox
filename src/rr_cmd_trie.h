#ifndef _RR_CMD_TRIE_H
#define _RR_CMD_TRIE_H

#include "rr_server.h"

void rr_cmd_rget(rr_client_t *c);
void rr_cmd_rset(rr_client_t *c);
void rr_cmd_rdel(rr_client_t *c);
void rr_cmd_rpget(rr_client_t *c);
void rr_cmd_rlen(rr_client_t *c);
void rr_cmd_rexists(rr_client_t *c);

#endif /* ifndef _RR_CMD_TRIE_H */
