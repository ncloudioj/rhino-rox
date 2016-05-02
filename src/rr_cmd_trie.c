#include "rr_cmd_trie.h"
#include "robj.h"
#include "rr_db.h"
#include "rr_dict.h"

void rr_cmd_rget(rr_client_t *c) {
    robj *o;

    if ((o=rr_db_lookup(c->db, c->argv[1])) == NULL) {
        reply_add_bulk_obj(c, shared.nullbulk);
        return;
    }

    if (o->type != OBJ_STRING) {
        reply_add_obj(c, shared.wrongtypeerr);
        return;
    } else {
        reply_add_bulk_obj(c, o);
        return;
    }
}

void rr_cmd_rset(rr_client_t *c) {
    c->argv[2] = tryObjectEncoding(c->argv[2]);
    if (rr_db_set_key(c->db, c->argv[1], c->argv[2]))
        reply_add_obj(c, shared.ok);
    else
        reply_add_obj(c, shared.err);
}

void rr_cmd_rpget(rr_client_t *c) {
    if (!dict_has_prefix(c->db->dict, c->argv[1]->ptr)) {
        reply_add_obj(c, shared.nullbulk);
        return;
    }
    dict_iterator_t *iter = dict_get_prefix(c->db->dict, c->argv[1]->ptr);
    while (dict_iter_hasnext(iter)) {
        dict_kv_t v = dict_iter_next(iter);
        reply_add_bulk_obj(c, v.value);
    }
    dict_iter_free(iter);
}

void rr_cmd_rdel(rr_client_t *c) {
    robj *reply;

    reply = rr_db_del(c->db, c->argv[1]) ? shared.ok : shared.nokeyerr;
    reply_add_obj(c, reply);
}
