#ifndef _RR_MALLOC_H
#define _RR_MALLOC_H

#include <stdlib.h>

void *rr_malloc(size_t size);
void *rr_calloc(size_t size);
void *rr_realloc(void *ptr, size_t size);
void rr_free(void *ptr);
char *rr_strdup(const char *s);
void rr_set_oom_handler(void (*oom_handler)(size_t));
size_t rr_get_used_memory(void);

#endif
