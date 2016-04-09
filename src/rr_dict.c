/*
 * Crit-bit tree based dictionary which supports inserts,
 * lookups (both exact match or prefix match), delete, and iterator
 *
 * Based on public domain code from Rusty Russel, Adam Langley
 * and D. J. Bernstein.
 *
 * Also based on the implemention in project vis:
 * http://github.com/martanne/vis
 *
 * Added features:
 *  - non-recursive style iterator
 *  - custom memory cleanup callback when emptying dict
 *  - overwrite the existing keys with value cleanup callback
 *
 * Further information about the data structure can be found at:
 *  http://cr.yp.to/critbit.html
 *  http://github.com/agl/critbit
 *  http://ccodearchive.net/info/strmap.html
 */

#include "adlist.h"
#include "rr_dict.h"
#include "rr_malloc.h"

#include <assert.h>
#include <string.h>
#include <stdint.h>

typedef struct Node Node;

/* The NULL value 'v' suggests it's an internal node which stores nodes as 'n'
 * in the union struct. Otherwise, it's a leaf node, where 's' in the union
 * will be used as the key, and v as the value.
 */
struct dict_t {
    union {
        Node *n;
        const char *s;
    } u;
    void *v;
};

struct Node {
    dict_t child[2];  /* children could be both leaf and internal nodes */
    size_t byte_idx;  /* The byte index where the first bit differs */
    uint8_t bit_idx;  /* The bit index where two children differ */
};

/* The dict iterator implemented by linked list */
struct dict_iterator_t {
    list *stack;
};

/* Get the closest key to this in a non-empty dict */
static dict_t *closest(dict_t *n, const char *key) {
    size_t len = strlen(key);
    const uint8_t *bytes = (const uint8_t *)key;

    while (!n->v) {
        uint8_t direction = 0;

        if (n->u.n->byte_idx < len) {
            uint8_t c = bytes[n->u.n->byte_idx];
            direction = (c >> n->u.n->bit_idx) & 1;
        }
        n = &n->u.n->child[direction];
    }
    return n;
}

bool dict_empty(dict_t *dict) {
    return dict->u.n == NULL;
}

dict_t *dict_create(void) {
    return rr_calloc(sizeof(dict_t));
}

void dict_free(dict_t *dict, dict_free_callback free_cb) {
    if (!dict) return;
    dict_clear(dict, free_cb);
    rr_free(dict);
}

void *dict_get(dict_t *dict, const char *key) {
    /* Not empty dict? */
    if (dict->u.n) {
        dict_t *n = closest(dict, key);
        if (strcmp(key, n->u.s) == 0)
            return n->v;
    }
    return NULL;
}

void *dict_get_closest(dict_t *dict, const char *prefix) {
    void *v = dict_get(dict, prefix);
    if (v) return v;

    dict_t *m = dict_get_prefix(dict, prefix);
    if (dict_empty(m)) return NULL;
    return m->v;
}

bool dict_contains(dict_t *dict, const char *prefix) {
    return !dict_empty(dict_get_prefix(dict, prefix));
}

bool dict_set(dict_t *dict, const char *k, void *value, dict_free_callback free_cb) {
    size_t len = strlen(k);
    const uint8_t *bytes = (const uint8_t *) k;
    dict_t *n;
    Node *newn;
    size_t byte_idx;
    uint8_t bit_idx, new_dir;
    char *key;

    if (!value) return false;
    if (!(key = rr_strdup(k))) return false;

    /* Empty dict? */
    if (!dict->u.n) {
        dict->u.s = key;
        dict->v = (void *) value;
        return true;
    }

    /* Find closest existing key. */
    n = closest(dict, key);

    /* Find where they differ. */
    for (byte_idx = 0; n->u.s[byte_idx] == key[byte_idx]; byte_idx++) {
        if (key[byte_idx] == '\0') {
            /* All identical! Overwrite the old value */
            if (free_cb) free_cb(n->v);
            n->v = value;
            rr_free(key);
            return false;
        }
    }

    /* Find which bit differs */
    uint8_t diff = (uint8_t)n->u.s[byte_idx] ^ bytes[byte_idx];
    for (bit_idx = 0; diff >>= 1; bit_idx++);

    /* Which direction do we go at this bit? */
    new_dir = ((bytes[byte_idx]) >> bit_idx) & 1;

    /* Allocate new node. */
    newn = rr_malloc(sizeof(*newn));
    if (!newn) {
        rr_free(key);
        return false;
    }
    newn->byte_idx = byte_idx;
    newn->bit_idx = bit_idx;
    newn->child[new_dir].v = value;
    newn->child[new_dir].u.s = key;

    /* Find where to insert: not closest, but first which differs! */
    n = dict;
    while (!n->v) {
        uint8_t direction = 0;

        if (n->u.n->byte_idx > byte_idx) break;
        /* Subtle: bit numbers are "backwards" for comparison */
        if (n->u.n->byte_idx == byte_idx && n->u.n->bit_idx < bit_idx) break;

        if (n->u.n->byte_idx < len) {
            uint8_t c = bytes[n->u.n->byte_idx];
            direction = (c >> n->u.n->bit_idx) & 1;
        }
        n = &n->u.n->child[direction];
    }

    newn->child[!new_dir] = *n;
    n->u.n = newn;
    n->v = NULL;
    return true;
}

void *dict_del(dict_t *dict, const char *key) {
    size_t len = strlen(key);
    const uint8_t *bytes = (const uint8_t *)key;
    dict_t *parent = NULL, *n;
    void *value = NULL;
    uint8_t direction;

    /* Empty dict? */
    if (!dict->u.n) {
        return NULL;
    }

    /* Find closest, but keep track of parent. */
    n = dict;
    /* Anything with NULL value is a node. */
    while (!n->v) {
        uint8_t c = 0;

        parent = n;
        if (n->u.n->byte_idx < len) {
            c = bytes[n->u.n->byte_idx];
            direction = (c >> n->u.n->bit_idx) & 1;
        } else {
            direction = 0;
        }
        n = &n->u.n->child[direction];
    }

    /* Did we find it? */
    if (strcmp(key, n->u.s)) return NULL;

    rr_free((char *) n->u.s);
    value = n->v;

    if (!parent) {
        /* We deleted the last node. */
        dict->u.n = NULL;
    } else {
        Node *old = parent->u.n;
        /* Raise other node to parent. */
        *parent = old->child[!direction];
        rr_free(old);
    }

    return value;
}

static bool iterate(dict_t n, bool (*handle)(const char *, void *, void *), void *data) {
    if (n.v) return handle(n.u.s, n.v, data);

    return iterate(n.u.n->child[0], handle, data)
        && iterate(n.u.n->child[1], handle, data);
}

void dict_foreach(dict_t *dict, bool (*handle)(const char *, void *, void *), void *data) {
    if (!dict->u.n) return;

    iterate(*dict, handle, data);
}

dict_t *dict_get_prefix(dict_t *dict, const char *prefix) {
    dict_t *n, *top;
    size_t len = strlen(prefix);
    const uint8_t *bytes = (const uint8_t *) prefix;

    /* Empty dict -> return empty dict. */
    if (!dict->u.n) return dict;

    top = n = dict;

    /* We walk to find the top, but keep going to check prefix matches */
    while (!n->v) {
        uint8_t c = 0, direction;

        if (n->u.n->byte_idx < len)
            c = bytes[n->u.n->byte_idx];

        direction = (c >> n->u.n->bit_idx) & 1;
        n = &n->u.n->child[direction];
        if (c) top = n;
    }

    if (strncmp(n->u.s, prefix, len)) {
        /* Convenient return for prefixes which do not appear in dict */
        static dict_t empty_map;
        return &empty_map;
    }

    return top;
}

static void clear(dict_t n, dict_free_callback free_cb) {
    if (!n.v) {
        clear(n.u.n->child[0], free_cb);
        clear(n.u.n->child[1], free_cb);
        rr_free(n.u.n);
    } else {
        if (free_cb) free_cb(n.v);
        rr_free((char*)n.u.s);
    }
}

void dict_clear(dict_t *dict, dict_free_callback free_cb) {
    if (dict->u.n) clear(*dict, free_cb);
    dict->u.n = NULL;
    dict->v = NULL;
}

static bool copy(dict_t *dest, dict_t n, dict_free_callback free_cb) {
    if (!n.v)
        return copy(dest, n.u.n->child[0], free_cb)
            && copy(dest, n.u.n->child[1], free_cb);
    else
        return dict_set(dest, n.u.s, n.v, free_cb);
}

bool dict_copy(dict_t *dest, dict_t *src, dict_free_callback free_cb) {
    if (!src || !src->u.n) return true;

    return copy(dest, *src, free_cb);
}

dict_iterator_t *dict_iter_create(dict_t *dict) {
    dict_t *node;

    dict_iterator_t *iter = rr_malloc(sizeof(*iter));
    iter->stack = listCreate();
    if (dict_empty(dict)) {
        /* if it's either a single node or an empty dict */
        if (dict->v) listAddNodeHead(iter->stack, dict);
        return iter;
    }

    node = dict;
    listAddNodeHead(iter->stack, node);
    while (!node->v) {
        node = &node->u.n->child[0];
        listAddNodeHead(iter->stack, node);
    }
    return iter;
}

bool dict_iter_hasnext(dict_iterator_t *iter) {
   return listLength(iter->stack);
}

dict_kv_t dict_iter_next(dict_iterator_t *iter) {
    dict_kv_t kv;
    listNode *ln;
    dict_t *dict, *node;

    ln = listIndex(iter->stack, 0);
    dict = (dict_t *) ln->value;
    /* The toppest item in the stack must be a leaf node */
    assert(dict->v);
    listDelNode(iter->stack, ln);
    kv.key = dict->u.s;
    kv.value = dict->v;

    if (!listLength(iter->stack)) return kv;

    /* Move iterator to the next leaf node */
    ln = listIndex(iter->stack, 0);
    dict = (dict_t *) ln->value;
    listDelNode(iter->stack, ln);
    /* This node must be an internal node */
    assert(!dict->v);
    node = &dict->u.n->child[1];
    listAddNodeHead(iter->stack, node);
    while (!node->v) {
        node = &node->u.n->child[0];
        listAddNodeHead(iter->stack, node);
    }

    return kv;
}

void dict_iter_free(dict_iterator_t *iter) {
    listRelease(iter->stack);
    rr_free(iter);
}
