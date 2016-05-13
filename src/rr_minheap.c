#include "rr_minheap.h"
#include "rr_malloc.h"

static void shiftdown(minheap_t *heap, long start, long at);
static void shiftup(minheap_t *heap, long start);

struct minheap_t *
minheap_create(unsigned long n, size_t size, compare pfcmp, copy pfcpy, swap pfswp) {
    minheap_t *heap;

    heap = rr_malloc(sizeof(minheap_t));
    if (heap == NULL) return NULL;
    heap->comp = pfcmp;
    heap->cpy = pfcpy;
    heap->swp = pfswp;
    heap->free_cb = NULL;
    heap->len = 0;
    heap->array = array_create(n, size);
    if (heap->array == NULL) {
        rr_free(heap);
        return NULL;
    }
    return heap;
}

void
minheap_set_freecb(minheap_t *heap, minheap_free_callback fn) {
    heap->free_cb = fn;
}

void
minheap_free(minheap_t *heap) {
    if (heap->free_cb) {
        unsigned long i;
        for (i = 0; i < heap->len; i++) {
            heap->free_cb(ARRAY_AT(heap->array, i));
        }
    }
    array_free(heap->array);
    rr_free(heap);
}

int
minheap_push(minheap_t *heap, const void *val) {
    void *item;

    if (heap->len == array_len(heap->array)) {
        if ((item=array_push(heap->array)) == NULL) {
            return -1;
        }
    } else {
        item = array_at(heap->array, heap->len);
    }

    heap->cpy(item, val);
    heap->len++;
    shiftdown(heap, 0, heap->len - 1);
    return 0;
}

void *
minheap_pop(minheap_t *heap) {
    void * root;

    if (heap->len == 0) return NULL;
    heap->swp(array_at(heap->array, 0), array_at(heap->array, heap->len-1));
    heap->len--;
    shiftup(heap, 0);
    root = array_at(heap->array, heap->len); /* note the previous first elm has been swapped to here. */
    return root;
}

void *
minheap_min(minheap_t *heap) {
    if (heap->len == 0) return NULL;
    return array_at(heap->array, 0);
}

static void
shiftdown(minheap_t *heap, long start, long end) {
    void *child, *parent;
    long i;  /* index for the parent */

    i = end;
    while (end > start) {
        child = array_at(heap->array, i);
        i = (end - 1) >> 1;
        parent = array_at(heap->array, i);
        if (heap->comp(child, parent) < 0) {
            heap->swp(child, parent);
            end = i;
        } else
            break;
    }
}

static void
shiftup(minheap_t *heap, long start) {
    long iend, istart, ichild, iright;

    iend = (long) heap->len;
    istart = start;
    ichild = 2 * istart + 1;
    while (ichild < iend) {
        iright = ichild + 1;
        if (iright < iend && heap->comp(array_at(heap->array, ichild),
                    array_at(heap->array, iright)) > 0) {
            ichild = iright;
        }
        heap->swp(array_at(heap->array, istart), array_at(heap->array, ichild));
        istart = ichild;
        ichild = 2 * istart + 1;
    }
    shiftdown(heap, start, istart);
}

unsigned long
minheap_len(const minheap_t *heap) {
    return heap->len;
}
