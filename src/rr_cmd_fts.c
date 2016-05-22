#include "rr_cmd_fts.h"
#include "rr_fts.h"

void rr_cmd_dset(rr_client_t *c) {
    robj *fts, *reply;

    fts = rr_db_lookup_or_create(c, c->argv[1], OBJ_FTS);
    if (!fts || fts->type != OBJ_FTS) {
        reply_add_obj(c, shared.wrongtypeerr);
        return;
    }

    c->argv[2] = tryObjectEncoding(c->argv[2]);
    if (fts_add(fts->ptr, c->argv[2], c->argv[3])) {
        /* fts_add will be responsible for incrementing the ref counts */
        reply = shared.ok;
    } else {
        reply = shared.err;
    }
    reply_add_obj(c, reply);
}

void rr_cmd_dget(rr_client_t *c) {
    robj *fts;
    fts_doc_t *doc;

    if ((fts=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, fts, OBJ_FTS)) return;

    if ((doc=fts_get(fts->ptr, c->argv[2])) == NULL)
        reply_add_obj(c, shared.nullbulk);
    else
        reply_add_bulk_obj(c, doc->doc);
}

void rr_cmd_dsearch(rr_client_t *c) {
    robj *fts;
    unsigned long size;
    struct fts_iterator_t *iter;

    if ((fts=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, fts, OBJ_FTS)) return;

    iter = fts_search(fts->ptr, c->argv[2], &size);
    reply_add_multi_bulk_len(c, size * 2);
    while (fts_iter_hasnext(iter)) {
        fts_doc_score_t *fds = fts_iter_next(iter);
        reply_add_bulk_obj(c, fds->doc->title);
        reply_add_bulk_obj(c, fds->doc->doc);
    }
    fts_iter_free(iter);
}

void rr_cmd_dlen(rr_client_t *c) {
    robj *fts;

    if ((fts=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, fts, OBJ_FTS)) return;

    reply_add_longlong(c, fts_size(fts->ptr));
}

void rr_cmd_ddel(rr_client_t *c) {
    robj *fts, *reply;

    if ((fts=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, fts, OBJ_FTS)) return;

    reply = fts_del(fts->ptr, c->argv[2])? shared.cone : shared.czero;
    reply_add_obj(c, reply);
}
