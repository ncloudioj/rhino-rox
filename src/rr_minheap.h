#ifndef _MIN_HEAP_H
#define _MIN_HEAP_H

#include "rr_array.h"

typedef int (*compare)(const void *lv, const void *rv);
typedef void *(*copy)(void *dst, const void *src);
typedef void (*swap)(void *lv, void *rv);
typedef void (*minheap_free_callback)(void *p);

typedef struct minheap_t {
    unsigned long  len;    /* length of minheap, might be less than the actual size of array */
    struct array_t *array; /* dynamic array for storing the elmentes of minheap */
    compare        comp;   /* cumstomized comparator */
    copy           cpy;    /* cumstomized copy operator */
    swap           swp;    /* cumstomized swap operator */
    minheap_free_callback free_cb;
} minheap_t;

struct minheap_t *minheap_create(unsigned long n, size_t size, compare comp, copy cpy, swap swp);
void minheap_set_freecb(minheap_t *heap, minheap_free_callback fn);
void minheap_free(minheap_t *heap);
int minheap_push(minheap_t *heap, const void *val);
void *minheap_pop(minheap_t *heap);
void *minheap_min(minheap_t *heap);
unsigned long minheap_len(const minheap_t *heap);

#endif
