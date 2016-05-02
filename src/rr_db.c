#include "rr_db.h"
#include "rr_server.h"
#include "robj.h"
#include "sds.h"
#include "rr_malloc.h"
#include "rr_bgtask.h"

#include <stdlib.h>
#include <string.h>

rrdb_t *rr_db_create(int id) {
    rrdb_t *db;

    db = rr_malloc(sizeof(*db));
    db->dict = dict_create();
    dict_set_freecb(db->dict, rr_obj_free_callback);
    db->id = id;

    return db;
}

robj *rr_db_lookup(rrdb_t *db, robj *key) {
    return dict_get(db->dict, key->ptr);
}

/* Add a key/value pair to the database. Unlike rr_db_set_key,
 * it won't increment the ref count of the value.*/
bool rr_db_add(rrdb_t *db, robj *key, robj *val) {
    bool ret;

    ret = dict_set(db->dict, key->ptr, val);
    return ret;
}

bool rr_db_del(rrdb_t *db, robj *key) {
    return server.lazyfree_server_del ? \
        rr_db_del_async(db, key) : rr_db_del_sync(db, key);
}

bool rr_db_del_sync(rrdb_t *db, robj *key) {
    robj *val;

    if ((val=dict_del(db->dict, key->ptr)) != NULL) {
        decrRefCount(val);
        return true;
    } else {
        return false;
    }
}

/* High level Set operation. This function can be used in order to set
 * a key, whatever it was existing or not, to a new object.
 *
 * 1) The ref count of the value object is incremented.
 */
bool rr_db_set_key(rrdb_t *db, robj *key, robj *val) {
    bool ret = rr_db_add(db, key, val);
    if (ret) incrRefCount(val);
    return ret; 
}

/* Return the amount of work needed in order to free an object.
 * The return value is not always the actual number of allocations the
 * object is compoesd of, but a number proportional to it.
 *
 * For strings the function always returns 1.
 *
 * For aggregated objects represented by hash tables or other data structures
 * the function just returns the number of elements the object is composed of.
 *
 * Objects composed of single allocations are always reported as having a
 * single item even if they are actaully logical composed of multiple
 * elements.
 */
static size_t get_lazyfree_effort(robj *obj) {
    if (obj->encoding == OBJ_ENCODING_HT) {
        dict_t *dict = obj->ptr;
        return dict_length(dict);
    } else {
        return 1; /* Everything else is a single allocation. */
    }
}

/* Delete a key/value in async if necessary from the DB.
 * If the value is composed of a few allocations, to free in a lazy way
 * is actually just slower... So under a certain limit we just free
 * the object synchronously. */
#define LAZYFREE_THRESHOLD 64
bool rr_db_del_async(rrdb_t *db, robj *key) {
    robj *val;

    val = dict_del(db->dict, key->ptr);
    if (val) {
        size_t free_effort = get_lazyfree_effort(val);
        if (free_effort > LAZYFREE_THRESHOLD) {
            rr_bgt_add_task(TASK_LAZY_FREE, SUBTYPE_FREE_OBJ, val);
        } else {
            decrRefCount(val);
        }
        return true;
    } else {
        return false;
    }
}

void rr_cmd_get(struct rr_client_t *c) {
    robj *o;

    if ((o=rr_db_lookup(c->db, c->argv[1])) == NULL) {
        reply_add_bulk_obj(c, shared.nullbulk);
        return;
    }

    if (o->type != OBJ_STRING)
        reply_add_obj(c, shared.wrongtypeerr);
    else
        reply_add_bulk_obj(c, o); 
}

void rr_cmd_set(struct rr_client_t *c) {
    /* If the lazyfree is required, explicitely try to delete the existing
     * key. Otherwise, rr_db_set_key will overwrite the existing key in
     * a sync way. */
    if (server.lazyfree_server_del) rr_db_del(c->db, c->argv[1]);

    c->argv[2] = tryObjectEncoding(c->argv[2]);
    if (rr_db_set_key(c->db, c->argv[1], c->argv[2]))
        reply_add_obj(c, shared.ok);
    else
        reply_add_obj(c, shared.err);
}

void rr_cmd_len(struct rr_client_t *c) {
    unsigned long l = dict_length(c->db->dict);
    reply_add_longlong(c, l);
}

void rr_cmd_exists(struct rr_client_t *c) {
    robj *reply;

    reply = dict_contains(c->db->dict, c->argv[1]->ptr) ? shared.cone : shared.czero;
    reply_add_obj(c, reply);
}

void rr_cmd_del(struct rr_client_t *c) {
    robj *reply;

    reply = rr_db_del(c->db, c->argv[1]) ? shared.cone : shared.czero;
    reply_add_obj(c, reply);
}

void rr_cmd_type(struct rr_client_t *c) {
    robj *o;
    char *type;

    o = rr_db_lookup(c->db, c->argv[1]);
    if (o == NULL) {
        type = "none";
    } else {
        switch(o->type) {
        case OBJ_STRING: type = "string"; break;
        case OBJ_HASH: type = "trie"; break;
        default: type = "unknown"; break;
        }
    }
    reply_add_status(c,type);
}
