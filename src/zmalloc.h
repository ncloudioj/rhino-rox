/*
 * Adapt those z functions used in Redis
 */

#ifndef __ZMALLOC_H__
#define __ZMALLOC_H__

#include "rr_malloc.h"

#define zmalloc(size) rr_malloc(size)
#define zfree(size) rr_free(size)
#define zcalloc(count,size) rr_calloc(count,size)
#define zrealloc(ptr,size) rr_realloc(ptr,size)

#endif
