#ifndef _RR_ARRAY_H
#define _RR_ARRAY_H

#include <stddef.h>

typedef struct array_t {
    unsigned long    nelm;        /* current # of elements in array   */
    unsigned long    nrest;       /* rest # of elements in array      */
    size_t           size;        /* size of each item in array       */
    void             *elm;        /* point to the first item in array */
} array_t;

#define ARRAY_AT(a, i) ((void *)(((char *)(a)->elm + (a)->size*(i))))
#define ARRAY_LEN(a) ((a)->nelm)

array_t * array_create(unsigned long n, size_t s);
void array_free(array_t *array);
void * array_push(array_t * array);
void * array_push_n(array_t * array, unsigned long n);
unsigned long array_len(array_t * array);
void * array_at(array_t * array, long i);

#endif
