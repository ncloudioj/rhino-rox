#include "rr_cmd_trie.h"
#include "robj.h"
#include "rr_db.h"
#include "rr_dict.h"
#include "rr_array.h"

void rr_cmd_rget(rr_client_t *c) {
    robj *trie, *o;

    if ((trie=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, trie, OBJ_HASH)) return;

    if ((o = dict_get(trie->ptr, c->argv[2]->ptr)) == NULL)
        reply_add_obj(c, shared.nullbulk);
    else
        reply_add_bulk_obj(c, o);
}

void rr_cmd_rexists(rr_client_t *c) {
    robj *trie;

    if ((trie=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, trie, OBJ_HASH)) return;

    if (dict_contains(trie->ptr, c->argv[2]->ptr))
        reply_add_obj(c, shared.cone);
    else
        reply_add_obj(c, shared.czero);
}

void rr_cmd_rlen(rr_client_t *c) {
    robj *trie;
    unsigned long len;

    if ((trie=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, trie, OBJ_HASH)) return;

    len = dict_length(trie->ptr);
    reply_add_longlong(c, len);
}

void rr_cmd_rset(rr_client_t *c) {
    robj *trie, *reply;

    trie = rr_db_lookup_or_create(c, c->argv[1], OBJ_HASH);
    if (!trie || checkType(c, trie, OBJ_HASH)) return;

    c->argv[3] = tryObjectEncoding(c->argv[3]);
    if (dict_set(trie->ptr, c->argv[2]->ptr, c->argv[3])) {
        incrRefCount(c->argv[3]);
        reply = shared.ok;
    } else {
        reply = shared.err;
    }
    reply_add_obj(c, reply);
}

static void get_prefix(rr_client_t *c, int flags) {
    robj *trie;
    dict_iterator_t *iter;
    array_t *kvs;
    long n = 0, multiplier = 0, i, total;

    if ((trie=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, trie, OBJ_HASH)) return;

    if (flags & DICT_KEY) multiplier++;
    if (flags & DICT_VAL) multiplier++;

    iter = dict_get_prefix(trie->ptr, c->argv[2]->ptr);
    kvs = array_create(8, sizeof(dict_kv_t));
    while (dict_iter_hasnext(iter)) {
        dict_kv_t *kv = array_push(kvs);
        *kv = dict_iter_next(iter);
        n++;
    }

    total = n * multiplier;
    reply_add_multi_bulk_len(c, total);
    for (i = 0; i < n; i++) {
        dict_kv_t *kv = ARRAY_AT(kvs, i);
        if (flags & DICT_KEY) reply_add_bulk_cstr(c, kv->key);
        if (flags & DICT_VAL) reply_add_bulk_obj(c, kv->value);
    }
    array_free(kvs);
    dict_iter_free(iter);
}

void rr_cmd_rpget(rr_client_t *c) {
    get_prefix(c, DICT_KEY|DICT_VAL);
}

static void get_all(rr_client_t *c, int flags) {
    robj *trie;
    dict_iterator_t *iter;
    array_t *kvs;
    long n = 0, multiplier = 0, i, total;

    if ((trie=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, trie, OBJ_HASH)) return;

    if (flags & DICT_KEY) multiplier++;
    if (flags & DICT_VAL) multiplier++;

    iter = dict_iter_create(trie->ptr);
    kvs = array_create(8, sizeof(dict_kv_t));
    while (dict_iter_hasnext(iter)) {
        dict_kv_t *kv = array_push(kvs);
        *kv = dict_iter_next(iter);
        n++;
    }

    total = n * multiplier;
    reply_add_multi_bulk_len(c, total);
    for (i = 0; i < n; i++) {
        dict_kv_t *kv = array_at(kvs, i);
        if (flags & DICT_KEY) reply_add_bulk_cstr(c, kv->key);
        if (flags & DICT_VAL) reply_add_bulk_obj(c, kv->value);
    }
    array_free(kvs);
    dict_iter_free(iter);
}

void rr_cmd_rkeys(rr_client_t *c) {
    get_all(c, DICT_KEY);
}

void rr_cmd_rvalues(rr_client_t *c) {
    get_all(c, DICT_VAL);
}

void rr_cmd_rgetall(rr_client_t *c) {
    get_all(c, DICT_KEY|DICT_VAL);
}

void rr_cmd_rdel(rr_client_t *c) {
    robj *reply, *trie, *del;

    if ((trie=rr_db_lookup_or_reply(c, c->argv[1], shared.nullbulk)) == NULL ||
        checkType(c, trie, OBJ_HASH)) return;

    del = dict_del(trie->ptr, c->argv[2]->ptr);
    reply = !del ? shared.czero : shared.cone;
    rr_obj_free_callback(del);
    reply_add_obj(c, reply);
}
