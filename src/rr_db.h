#ifndef RR_DB_H
#define RR_DB_H RR_DB_H

#include "rr_dict.h"
#include "robj.h"

#include <stdbool.h>

typedef struct rrdb_t {
    int id;        /* database ID */
    dict_t *dict;  /* database keyspace */
} rrdb_t;

rrdb_t *rr_db_create(int id);
robj *rr_db_lookup(rrdb_t *db, robj *key);
bool rr_db_add(rrdb_t *db, robj *key, robj *val);
/* Generic del based on the lazyfree_server_del configuration */
bool rr_db_del(rrdb_t *db, robj *key);
bool rr_db_del_sync(rrdb_t *db, robj *key);
bool rr_db_del_async(rrdb_t *db, robj *key);

#endif /* ifndef RR_DB_H */
