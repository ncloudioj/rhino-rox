#include "rr_rhino_rox.h"
#include "rr_cmd_heapq.h"
#include "robj.h"
#include "rr_db.h"
#include "rr_minheap.h"

void rr_cmd_hqpush(rr_client_t *c) {
    robj *hp, *reply;
    hq_item_t item;

    if (getDoubleFromObjectOrReply(c, c->argv[2], &item.score, NULL)) return;
    hp = rr_db_lookup_or_create(c, c->argv[1], OBJ_HEAPQ);
    if (!hp || hp->type != OBJ_HEAPQ) {
        reply_add_obj(c, shared.wrongtypeerr);
        return;
    }

    c->argv[3] = tryObjectEncoding(c->argv[3]);
    item.obj = c->argv[3];
    if (!minheap_push(hp->ptr, &item)) {
        incrRefCount(c->argv[3]);
        reply = shared.ok;
    } else {
        reply = shared.err;
    }
    reply_add_obj(c, reply);
}

void rr_cmd_hqpop(rr_client_t *c) {
    robj *hq;
    hq_item_t *item;

    if ((hq=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, hq, OBJ_HEAPQ)) return;

    if ((item = minheap_pop(hq->ptr)) == NULL) {
        reply_add_obj(c, shared.nullbulk);
    }
    else {
        reply_add_bulk_obj(c, item->obj);
        decrRefCount(item->obj);
    }
}

void rr_cmd_hqpeek(rr_client_t *c) {
    robj *hq;
    hq_item_t *item;

    if ((hq=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, hq, OBJ_HEAPQ)) return;

    if ((item = minheap_min(hq->ptr)) == NULL)
        reply_add_obj(c, shared.nullbulk);
    else
        reply_add_bulk_obj(c, item->obj);
}

void rr_cmd_hqlen(rr_client_t *c) {
    robj *hq;

    if ((hq=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, hq, OBJ_HEAPQ)) return;

    reply_add_longlong(c, minheap_len(hq->ptr));
}

void rr_cmd_hqpopn(rr_client_t *c) {
    robj *hq;
    hq_item_t *item;
    long n, len;

    if (getLongFromObjectOrReply(c, c->argv[2], &n, NULL)) return;
    if (n < 0) {
        reply_add_err(c, "invalid non-negtive integer");
        return;
    }
    if ((hq=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, hq, OBJ_HEAPQ)) return;

    len = minheap_len(hq->ptr);
    n = n < len ?  n : len;
    reply_add_multi_bulk_len(c, n);
    while (n-- > 0 && (item=minheap_pop(hq->ptr)) != NULL) {
        reply_add_bulk_obj(c, item->obj);
        decrRefCount(item->obj);
    }
}
