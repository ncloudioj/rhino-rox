#include "minunit.h"
#include "../src/rr_dict.h"
#include "../src/rr_rhino_rox.h"

#include <string.h>

static const struct {
    char * key;
    int  value;
} pairs[] = {
    {"app", 0},
    {"apple", 1},
    {"appleby", 2},
    {"apply", 3},
    {"ape", 4},
    {"bob", 5},
    {"bobby", 6},
    {"boy", 7},
    {"box", 8},
    {NULL, -1}
};

static const struct {
    char * key;
    int  value;
} in_order_pairs[] = {
    {"ape", 4},
    {"app", 0},
    {"apple", 1},
    {"appleby", 2},
    {"apply", 3},
    {"bob", 5},
    {"bobby", 6},
    {"box", 8},
    {"boy", 7},
    {NULL, -1}
};

MU_TEST(test_dict_basic) {
    dict_t *d;
    d = dict_create();
    mu_check(dict_empty(d));
    mu_assert_int_eq(0, dict_length(d));
    
    int i;
    for (i=0; pairs[i].key; i++)
        dict_set(d, pairs[i].key, (void *) &pairs[i].value);
    mu_assert_int_eq(i, dict_length(d));

    for (i=0; pairs[i].key; i++) {
        int v = *(int *) dict_get(d, pairs[i].key);
        mu_assert_int_eq(pairs[i].value, v);
    }

    mu_check(dict_contains(d, "box"));
    mu_check(dict_has_prefix(d, "ap"));
    mu_check(!dict_contains(d, "nope"));
    
    int new = 10;
    /* Overwrite the existing key */
    mu_check(dict_set(d, "box", &new));
    mu_assert_int_eq(new, *(int *) dict_get(d, "box"));

    dict_del(d, "apple");
    mu_check(!dict_contains(d, "apple"));

    dict_clear(d);
    mu_assert_int_eq(0, dict_length(d));
    mu_check(!dict_contains(d, "box"));
    mu_check(!dict_has_prefix(d, "ap"));

    dict_free(d);
}

MU_TEST(test_dict_iterator) {
    dict_t *d;
    d = dict_create();
 
    int i;
    for (i=0; pairs[i].key; i++)
        dict_set(d, pairs[i].key, (void *) &pairs[i].value);

    dict_kv_t kv;
    dict_iterator_t *iter = dict_iter_create(d);
    for(i=0; dict_iter_hasnext(iter); i++) {
        kv = dict_iter_next(iter);
        mu_check(!strcmp(kv.key, in_order_pairs[i].key));
        mu_assert_int_eq(*(int *) kv.value, in_order_pairs[i].value);
    }
    dict_iter_free(iter);

    /* Prefix iterator */
    char *expected[] = {"apple", "appleby", "apply"};
    iter = dict_get_prefix(d, "appl");
    for(i=0; dict_iter_hasnext(iter); i++) {
        kv = dict_iter_next(iter);
        mu_check(!strcmp(kv.key, expected[i]));
    }
    mu_assert_int_eq(3, i); 
    dict_iter_free(iter);

    dict_free(d);
}

MU_TEST(test_dict_copy) {
    dict_t *d, *s;
    s = dict_create();
    d = dict_create();
 
    int i;
    for (i=0; pairs[i].key; i++)
        dict_set(s, pairs[i].key, (void *) &pairs[i].value);
    dict_copy(d, s);
    mu_assert_int_eq(dict_length(s), dict_length(d));
    for (i=0; pairs[i].key; i++)
        mu_check(dict_contains(d, pairs[i].key));

    dict_free(s);
    dict_free(d);
}

MU_TEST_SUITE(test_suite) {
    MU_RUN_TEST(test_dict_basic);
    MU_RUN_TEST(test_dict_iterator);
    MU_RUN_TEST(test_dict_copy);
}

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    MU_RUN_SUITE(test_suite);
    MU_REPORT();
    return 0;
}
