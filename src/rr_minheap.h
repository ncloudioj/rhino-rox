#ifndef _MIN_HEAP_H
#define _MIN_HEAP_H

#include "rr_array.h"

#include <stdint.h>

typedef int (*compare)(const void *lv, const void *rv);
typedef void *(*copy)(void *dst, const void *src);
typedef void (*swap)(void *lv, void *rv);

typedef struct minheap_t {
    uint32_t       len;    /* length of minheap, might be less than the actual size of array */
    struct array_t *array; /* dynamic array for storing the elmentes of minheap */
    compare        comp;   /* cumstomized comparator */
    copy           cpy;    /* cumstomized copy operator */
    swap           swp;    /* cumstomized swap operator */
} minheap_t;

struct minheap_t *minheap_create(uint32_t n, size_t size, compare comp, copy cpy, swap swp);
void minheap_free(minheap_t *heap);
int minheap_push(minheap_t *heap, const void *val);
void *minheap_pop(minheap_t *heap);
void *minheap_min(minheap_t *heap);
uint32_t minheap_len(const minheap_t *heap);

#endif
