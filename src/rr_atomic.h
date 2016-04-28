#ifndef _RR_ATOMIC_H
#define _RR_ATOMIC_H

#include "rr_config.h"
#include <pthread.h>

#if defined(HAVE_ATOMIC)

#define ATOM_INC(ptr,n,mutex) __sync_add_and_fetch(ptr,(n))
#define ATOM_DEC(ptr,n,mutex) __sync_sub_and_fetch(ptr,(n))
#define ATOM_GET(ptr,dst,mutex) do {   \
    dst = __sync_sub_and_fetch(ptr,0); \
} while (0)

#else

#define ATOM_INC(ptr,n,mutex) do { \
    pthread_mutex_lock(&mutex);    \
    ptr += (n);                    \
    pthread_mutex_unlock(&mutex);  \
} while (0)

#define ATOM_DEC(ptr,n,mutex) do { \
    pthread_mutex_lock(&mutex);    \
    ptr -= (n);                    \
    pthread_mutex_unlock(&mutex);  \
} while (0)

#define ATOM_GET(ptr,dst,mutex) do { \
    pthread_mutex_lock(&mutex);      \
    dst = ptr;                       \
    pthread_mutex_unlock(&mutex);    \
} while (0)
#endif

#endif
