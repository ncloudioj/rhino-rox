#include "rr_array.h"
#include "rr_malloc.h"

array_t *
array_create(uint32_t n, size_t s) {
    array_t *a;
    uint32_t initial_number; /* initial number of elements in array, 1 by default */
    
    a = rr_malloc(sizeof(array_t));
    if (a == NULL) {
        return NULL;
    }

    initial_number = n==0 ? 1 : n;
    a->elm = rr_malloc(initial_number * s);
    if (a->elm == NULL) {
    	rr_free(a);
    	return NULL;
    }
    a->nelm = 0;
    a->nrest = initial_number;
    a->size = s;

    return a;
}

void
array_free(array_t *array) {
	rr_free(array->elm);
    rr_free(array);
}

void *
array_push(array_t *array) {
    void *ret, *elm;

    if (array->nrest > 0) {
        ret = ARRAY_AT(array, array->nelm);
        array->nrest--;
        array->nelm++;
        return ret;
    }

    /* double array's size, if failed, return NULL */
    if ((elm=rr_realloc(array->elm, 2*array->size*array->nelm)) == NULL) {
    	return NULL;
    } else {
        array->elm = elm;
        ret = ARRAY_AT(array, array->nelm);
        array->nrest = array->nelm - 1;
        array->nelm++;
    }

    return ret;
}

void *
array_push_n(array_t *array, uint32_t n) {
    void *ret, *elm;

    if (n == 0) return NULL;

    if (array->nrest >= n) {
        ret = ARRAY_AT(array, array->nelm);
        array->nrest -= n;
        array->nelm += n;
        return ret;
    }

    /* expand array's size, if failed, return NULL */
    size_t nslots;
    nslots = (2*array->nelm > n) ? 2*array->nelm: array->nelm+array->nrest+n;
    if ((elm=rr_realloc(array->elm, nslots*array->size)) == NULL) {
    	return NULL;
    } else {
        array->elm = elm;
        ret = ARRAY_AT(array, array->nelm);
        array->nelm += n;
        array->nrest = nslots - array->nelm;
    }
    
    return ret;
}

uint32_t
array_len(array_t *array) {
    return array->nelm;
}

void *
array_at(array_t *array, int i) {
	if (i >= (int)ARRAY_LEN(array) || (i < 0 && -i > (int)ARRAY_LEN(array))) return NULL;
	return i >= 0 ? ARRAY_AT(array, i): ARRAY_AT(array, ARRAY_LEN(array)+i);
}
