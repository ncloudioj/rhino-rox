#ifndef RR_CMD_ADMIN
#define RR_CMD_ADMIN

#include "rr_server.h"

void rr_cmd_admin_ping(rr_client_t *c);
void rr_cmd_admin_echo(rr_client_t *c);
void rr_cmd_admin_shutdown(rr_client_t *c);

#endif /* ifndef RR_CMD_ADMIN */
