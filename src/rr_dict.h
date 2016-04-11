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
void dict_free(dict_t *dict);
bool dict_empty(dict_t *dict);
unsigned long dict_length(dict_t *dict);
void dict_set_freecb(dict_t *dict, dict_free_callback);
void dict_clear(dict_t *dict);
bool dict_contains(dict_t *dict, const char *key);
bool dict_has_prefix(dict_t *dict, const char *prefix);
void *dict_get(dict_t *dict, const char *key);

/* Set a key value pair for the given dict.
 * If the key is already in the dict, the old value will be overwritten.
 * The user can specify a callback function to free the value.
 * Passing a NULL as the free callback will ignore this cleanup.
 */
bool dict_set(dict_t  *dict, const char *key, void *value);
void *dict_del(dict_t *dict, const char *key);
bool dict_copy(dict_t *dest, dict_t *src);

/* Dict iterator APIs */
dict_iterator_t *dict_iter_create(dict_t *dict);
bool dict_iter_hasnext(dict_iterator_t *iter);
dict_kv_t dict_iter_next(dict_iterator_t *iter);
void dict_iter_free(dict_iterator_t *iter);

/* Foreach was implemented in a recursive fashion, so use it sparingly for
 * large dicts.
 * The callback function returns a boolean to signal whether it should break
 * out or not. Return true means continue, otherwise, break out the loop.
 * */
void dict_foreach(dict_t *dict, bool (*handle)(const char *key, void *value, void *data), void *data);

/* Get all the key/values which match the given prefix.
 * The return value is an iterator to the key/value pairs */
dict_iterator_t *dict_get_prefix(dict_t *dict, const char *prefix);

#endif
