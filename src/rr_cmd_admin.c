#include "rr_cmd_admin.h"
#include "rr_rhino_rox.h"
#include "sds.h"

#include <stdlib.h>

void rr_cmd_admin_ping(rr_client_t *c) {
    /* The command takes zero or one arguments. */
    if (c->argc > 2) {
        reply_add_err_format(c, "wrong number of arguments for '%s' command",
            c->cmd->name);
        return;
    }

    if (c->argc == 1)
        reply_add_obj(c, shared.pong);
    else
        reply_add_bulk_obj(c, c->argv[1]);
}

void rr_cmd_admin_echo(rr_client_t *c) {
    reply_add_bulk_obj(c, c->argv[1]);
}

void rr_cmd_admin_shutdown(rr_client_t *c) {
    if (rr_server_prepare_to_shutdown() == RR_OK) exit(0);
    reply_add_err(c, "Errors trying to SHUTDOWN. Check logs.");
}

void rr_cmd_admin_info(rr_client_t *c) {
    sds info = rr_server_get_info();
    reply_add_bulk_sds(c, info);
}
