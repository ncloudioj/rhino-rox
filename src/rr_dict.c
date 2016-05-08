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
 *  - overwrite the value for the existing keys
 *  - keep track the size of dict
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
typedef struct Dict Dict;

struct dict_t {
    Dict *dict;
    unsigned long size;
    dict_free_callback free_cb;
};

/* A node with a NULL value suggests it's an internal node in which stores the
 * nodes as 'n' in the union struct. Otherwise, it's a leaf node, in which 's'
 * in the union stores the key, and v as the value
 */
struct Dict {
    union {
        Node *n;
        const char *s;
    } u;
    void *v;
};

struct Node {
    Dict child[2];  /* children could be either leaf and internal nodes */
    size_t byte_idx;  /* the byte index where the first bit differs */
    uint8_t bit_idx;  /* the bit index where two children differ */
};

/* The dict iterator implemented by linked list */
struct dict_iterator_t {
    list *stack;
};

#define EMPTY_NODE(d) ((d)->u.n == NULL)

unsigned long dict_length(dict_t *dict) {
    return dict->size;
}

void dict_set_freecb(dict_t *dict, dict_free_callback free_cb) {
    dict->free_cb = free_cb;
}

/* Get the closest key to this in a non-empty dict */
static Dict *closest(Dict *n, const char *key) {
    size_t len = strlen(key);
    const uint8_t *bytes = (const uint8_t *) key;

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

static Dict *get_prefix(Dict *dict, const char *prefix) {
    Dict *n, *top;
    size_t len = strlen(prefix);
    const uint8_t *bytes = (const uint8_t *) prefix;

    /* Empty dict -> return empty dict. */
    if (!dict->u.n) return dict;

    top = n = dict;

    /* Walk to find the top, as keep checking whether the prefix matches */
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
        static Dict empty_map;
        return &empty_map;
    }

    return top;
}

bool dict_empty(dict_t *dict) {
    return dict->size == 0;
}

dict_t *dict_create(void) {
    dict_t *d;

    d = rr_malloc(sizeof(dict_t));
    d->dict = rr_calloc(sizeof(Dict));
    if (d->dict == NULL) {
        rr_free(d);
        return NULL;
    }
    d->size = 0;
    d->free_cb = NULL;
    return d;
}

void dict_free(dict_t *dict) {
    if (!dict) return;
    dict_clear(dict);
    rr_free(dict->dict);
    rr_free(dict);
}

void *dict_get(dict_t *dict, const char *key) {
    Dict *d = dict->dict;

    if (d->u.n) {
        Dict *n = closest(d, key);
        if (strcmp(key, n->u.s) == 0)
            return n->v;
    }
    return NULL;
}

bool dict_has_prefix(dict_t *dict, const char *prefix) {
    return !EMPTY_NODE(get_prefix(dict->dict, prefix));
}

bool dict_contains(dict_t *dict, const char *key) {
    return dict_get(dict, key) != NULL;
}

bool dict_set(dict_t *dict, const char *k, void *value) {
    Dict *d = dict->dict, *n;
    size_t len = strlen(k);
    const uint8_t *bytes = (const uint8_t *) k;
    Node *newn;
    size_t byte_idx;
    uint8_t bit_idx, new_dir;
    char *key;

    if (!value) return false;
    if (!(key = rr_strdup(k))) return false;

    /* Empty dict? */
    if (!d->u.n) {
        d->u.s = key;
        d->v = value;
        dict->size++;
        return true;
    }

    /* Find closest existing key. */
    n = closest(d, key);

    /* Find where they differ. */
    for (byte_idx = 0; n->u.s[byte_idx] == key[byte_idx]; byte_idx++) {
        if (key[byte_idx] == '\0') {
            /* Found the same key! Overwrite the old value */
            if (dict->free_cb) dict->free_cb(n->v);
            n->v = value;
            rr_free(key);
            return true;
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

    /* Find where to insert: not the closest, but the first which differs */
    n = d;
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
    dict->size++;
    return true;
}

void *dict_del(dict_t *dict, const char *key) {
    size_t len = strlen(key);
    const uint8_t *bytes = (const uint8_t *) key;
    Dict *parent = NULL, *n = dict->dict;
    void *value = NULL;
    uint8_t direction;

    if (!n->u.n) return NULL;

    /* Find the closest, also keep track of the parent. */
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
    dict->size--;

    if (!parent) {
        /* Reset the last item in the dict */
        n->u.n = NULL;
        n->v = NULL;
    } else {
        Node *old = parent->u.n;
        /* Raise the other node as the parent. */
        *parent = old->child[!direction];
        rr_free(old);
    }

    return value;
}

static bool iterate(Dict *n, bool (*handle)(const char *, void *, void *), void *data) {
    if (n->v)
        return handle(n->u.s, n->v, data);

    return iterate(n->u.n->child, handle, data)
        && iterate(n->u.n->child+1, handle, data);
}

void dict_foreach(dict_t *dict, bool (*handle)(const char *, void *, void *), void *data) {
    Dict *d = dict->dict;

    if (!d->u.n) return;

    iterate(d, handle, data);
}

static void clear(Dict *n, dict_free_callback free_cb) {
    if (!n->v) {
        clear(n->u.n->child, free_cb);
        clear(n->u.n->child+1, free_cb);
        rr_free(n->u.n);
    } else {
        if (free_cb) free_cb(n->v);
        rr_free((char *) n->u.s);
    }
}

void dict_clear(dict_t *dict) {
    Dict *d = dict->dict;

    if (d->u.n)
        clear(d, dict->free_cb);
    d->u.n = NULL;
    d->v = NULL;
    dict->size = 0;
}

static bool copy(dict_t *dest, Dict *n) {
    if (!n->v)
        return copy(dest, n->u.n->child)
            && copy(dest, n->u.n->child+1);
    else
        return dict_set(dest, n->u.s, n->v);
}

bool dict_copy(dict_t *dest, dict_t *src) {
    if (!src || !src->dict->u.n) return false;

    bool rv = copy(dest, src->dict);
    dest->size = src->size;
    dest->free_cb = src->free_cb;
    return rv;
}

static dict_iterator_t *iter_create(Dict *dict, unsigned long size) {
    Dict *node;

    dict_iterator_t *iter = rr_malloc(sizeof(*iter));
    if (!iter) return NULL;
    iter->stack = listCreate();
    if (size == 0) return iter;

    node = dict;
    listAddNodeHead(iter->stack, node);
    while (!node->v) {
        node = &node->u.n->child[0];
        listAddNodeHead(iter->stack, node);
    }
    return iter;
}

dict_iterator_t *dict_get_prefix(dict_t *dict, const char *prefix) {
    Dict *d;

    d = get_prefix(dict->dict, prefix);
    return iter_create(d, dict->size);
}

dict_iterator_t *dict_iter_create(dict_t *dict) {
    return iter_create(dict->dict, dict->size);
}

bool dict_iter_hasnext(dict_iterator_t *iter) {
   return listLength(iter->stack);
}

dict_kv_t dict_iter_next(dict_iterator_t *iter) {
    dict_kv_t kv;
    listNode *ln;
    Dict *dict, *node;

    ln = listFirst(iter->stack);
    dict = (Dict*) ln->value;
    /* The toppest item in the stack must be a leaf node */
    assert(dict->v);
    kv.key = dict->u.s;
    kv.value = dict->v;
    listDelNode(iter->stack, ln);

    if (!listLength(iter->stack)) return kv;

    /* Move iterator to the next leaf node */
    ln = listFirst(iter->stack);
    dict = (Dict*) ln->value;
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
