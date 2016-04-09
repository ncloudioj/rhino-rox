#ifndef _RR_DICT_H
#define _RR_DICT_H

#include <stdbool.h>

/* Call back function to be used to free the value part in the dict.
 * If setting it as NULL, nothing will be done.
 */
typedef void (*dict_free_callback)(void *value);

typedef struct dict_t dict_t;
typedef struct dict_iterator_t dict_iterator_t;
typedef struct dict_kv_t {
    const char *key;
    void *value;
} dict_kv_t;

dict_t *dict_create(void);
void dict_free(dict_t *dict, dict_free_callback free_cb);
bool dict_empty(dict_t *dict);
void dict_clear(dict_t *dict, dict_free_callback free_cb);
bool dict_contains(dict_t *dict, const char *prefix);
void *dict_get(dict_t *dict, const char *key);

/* Set a key value pair for the given dict.
 * If the key is already in the dict, the old value will be overwritten.
 * The user can specify a callback function to free the value.
 * Passing a NULL as the free callback will ignore this cleanup.
 */
bool dict_set(dict_t  *dict, const char *key, void *value, dict_free_callback free_cb);
void *dict_del(dict_t *dict, const char *key);
void *dict_get_closest(dict_t *dict, const char *prefix);
dict_t *dict_get_prefix(dict_t *dict, const char *prefix);
bool dict_copy(dict_t *dest, dict_t *src, dict_free_callback free_cb);

/* Foreach was implemented in a recursive fashion, use it sparingly for large dicts */
void dict_foreach(dict_t *dict, bool (*handle)(const char *key, void *value, void *data), void *data);

/* Dict iterator APIs */
dict_iterator_t *dict_iter_create(dict_t *dict);
bool dict_iter_hasnext(dict_iterator_t *iter);
dict_kv_t dict_iter_next(dict_iterator_t *iter);
void dict_iter_free(dict_iterator_t *iter);

#endif
