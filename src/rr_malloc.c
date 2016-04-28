#include "rr_malloc.h"
#include "rr_atomic.h"
#include "jemalloc.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Overwrite the allocator from libc */
#define malloc(size) je_malloc(size)
#define calloc(count,size) je_calloc(count,size)
#define realloc(ptr,size) je_realloc(ptr,size)
#define free(ptr) je_free(ptr)

#define rr_malloc_size(ptr) je_malloc_usable_size(ptr)

#define update_malloc_stat_alloc(size) do {        \
    ATOM_INC(&used_memory,(size),&used_memory_mutex); \
} while(0)

#define update_malloc_stat_free(size) do {         \
    ATOM_DEC(&used_memory,(size),&used_memory_mutex); \
} while(0)

/* memory usage and its mutex */
static size_t used_memory = 0;
pthread_mutex_t used_memory_mutex = PTHREAD_MUTEX_INITIALIZER;

static void rr_default_oom(size_t size) {
    fprintf(stderr, "Out of memory: trying to allocate %zu bytes\n", size);
    fflush(stderr);
    abort();
}

static void (*rr_oom_handler)(size_t) = rr_default_oom;

void *rr_malloc(size_t size) {
    void *ptr = malloc(size);

    if (!ptr) rr_oom_handler(size);
    update_malloc_stat_alloc(rr_malloc_size(ptr));
    return ptr;
}

void *rr_calloc(size_t size) {
    void *ptr = calloc(1, size);

    if (!ptr) rr_oom_handler(size);
    update_malloc_stat_alloc(rr_malloc_size(ptr));
    return ptr;
}

void *rr_realloc(void *ptr, size_t size) {
    size_t oldsize;
    void *newptr;

    if (ptr == NULL) return rr_malloc(size);
    oldsize = rr_malloc_size(ptr);
    newptr = realloc(ptr, size);
    if (!newptr) rr_oom_handler(size);

    update_malloc_stat_free(oldsize);
    update_malloc_stat_alloc(rr_malloc_size(newptr));
    return newptr;
}

void rr_free(void *ptr) {
    if (ptr == NULL) return;
    update_malloc_stat_free(rr_malloc_size(ptr));
    free(ptr);
}

char *rr_strdup(const char *s) {
    size_t l = strlen(s) + 1;
    char *p = rr_malloc(l);

    memcpy(p, s, l);
    return p;
}

size_t rr_get_used_memory(void) {
    size_t size;

    ATOM_GET(&used_memory,size,&used_memory_mutex);
    return size;
}

void rr_set_oom_handler(void (*oom_handler)(size_t)) {
    rr_oom_handler = oom_handler;
}
