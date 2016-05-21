/*
 * Full text search document type for Rhino-Rox
 */

#ifndef _RR_FTS_H
#define _RR_FTS_H

#include "robj.h"
#include <stdbool.h>
#include "rr_dict.h"

typedef struct fts_t {
    dict_t *docs;
    dict_t *index;
    long len;  /* sum of the document length in words */
} fts_t;

typedef struct fts_doc_t {
    robj *title;
    robj *doc;
    int len;   /* document length in words */
} fts_doc_t;

typedef struct fts_doc_score_t {
    fts_doc_t *doc;
    double score; /* BM25 ranking score */
} fts_doc_score_t;

struct fts_iterator_t;

fts_t *fts_create(void);
void fts_free(fts_t *fts);
bool fts_add(fts_t *fts, robj *title, robj *doc);
fts_doc_t *fts_get(fts_t *fts, robj *title);
bool fts_del(fts_t *fts, robj *title);
unsigned long fts_size(fts_t *fts);
struct fts_iterator_t *fts_search(fts_t *fts, robj *query, unsigned long *size);

bool fts_iter_hasnext(struct fts_iterator_t *it);
fts_doc_score_t *fts_iter_next(struct fts_iterator_t *it);
void fts_iter_free(struct fts_iterator_t *it);

#endif /* ifndef _RR_FTS_H */
