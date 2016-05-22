#include "rr_fts.h"
#include "rr_malloc.h"
#include "rr_stopwords.h"
#include "rr_stemmer.h"
#include "rr_logging.h"
#include "adlist.h"
#include "sds.h"
#include "rr_minheap.h"

#include <assert.h>
#include <math.h>

struct fts_iterator_t {
    minheap_t *docs;
};

static const char* puncs = ",.:;?!";

typedef struct index_item_t {
    fts_doc_t *doc;  /* pointer of the original document */
    int tf;          /* term frequency */
} index_item_t;

static fts_doc_t *fts_doc_create(robj *title, robj *doc) {
    fts_doc_t *fd = rr_malloc(sizeof(*fd));
    fd->title = title;
    fd->doc = doc;
    fd->len = 0;  /* will be populated at the index building phase */
    incrRefCount(title);
    incrRefCount(doc);
    return fd;
}

static void fts_doc_free(void *data) {
    fts_doc_t *d = (fts_doc_t *) data;
    decrRefCount(d->title);
    decrRefCount(d->doc);
    rr_free(d);
}

static void index_item_free(void *idx) {
    rr_free(idx);
}

static int fts_doc_match (void *ptr, void *key) {
    index_item_t *idx = (index_item_t *) ptr;
    return idx->doc == key;
}

static list *index_list_create() {
    list *idx = listCreate();
    listSetMatchMethod(idx, fts_doc_match);
    listSetFreeMethod(idx, index_item_free);
    return idx;
}

static void index_list_free(void *data) {
    listRelease(data);
}

static void dict_doc_score_free(void *data) {
    rr_free(data);
}

fts_t *fts_create(void) {
    fts_t *fts = rr_malloc(sizeof(*fts));
    fts->len = 0;
    fts->docs = dict_create();
    dict_set_freecb(fts->docs, fts_doc_free);
    fts->index = dict_create();
    dict_set_freecb(fts->index, index_list_free);
    return fts;
}

void fts_free(fts_t *fts) {
    dict_free(fts->docs);
    dict_free(fts->index);
    rr_free(fts);
}

static bool fts_index_add(fts_t *fts, fts_doc_t *doc) {
    int i, l, k, len, nonstopwords = 0;
    sds *terms;
    struct stemmer *stmer;
    sds raw = doc->doc->ptr;

    terms = sdssplitlen(raw, sdslen(raw), " ", 1, &len);
    if (!terms) return false;
    stmer = create_stemmer();
    for (i = 0; i < len; i++) {
        sds stemmed = NULL, term = terms[i], s = term;
        list *idx;
        listNode *ln;

        term = sdstrim(term, puncs);
        l = sdslen(term);
        if (l == 0 || rr_stopwords_check(term)) {
            sdsfree(term);
            continue;
        }
        nonstopwords++;
        sdstolower(term);
        /* note that the third argument is a zero-based index */
        k = stem(stmer, term, l-1);
        if (k < l-1) {
            stemmed = sdsnewlen(term, k+1);
            s = stemmed;
        }

        idx = dict_get(fts->index, s);
        if (!idx) {
            idx = index_list_create();
            dict_set(fts->index, s, idx);
        }
        ln = listSearchKey(idx, doc);
        if (ln) {
            index_item_t *idi = ln->value;
            idi->tf++;
        } else {
            index_item_t *idi = rr_malloc(sizeof(*idi));
            idi->doc = doc;
            idi->tf = 1;
            listAddNodeHead(idx, idi);
        }

        if (stemmed) sdsfree(stemmed);
        sdsfree(term);
    }
    rr_free(terms);
    free_stemmer(stmer);
    doc->len = nonstopwords;
    fts->len += doc->len;
    return true;
}

static void fts_index_del(fts_t *fts, fts_doc_t *doc) {
    int i, l, k, len;
    sds *terms;
    struct stemmer *stmer;
    sds raw = doc->doc->ptr;

    terms = sdssplitlen(raw, sdslen(raw), " ", 1, &len);
    if (!terms) return;
    stmer = create_stemmer();
    for (i = 0; i < len; i++) {
        sds stemmed = NULL, term = terms[i], s = term;
        list *idx;
        listNode *ln;

        term = sdstrim(term, puncs);
        l = sdslen(term);
        if (l == 0 || rr_stopwords_check(term)) {
            sdsfree(term);
            continue;
        }
        sdstolower(term);
        /* note that the third argument is a zero-based index */
        k = stem(stmer, term, l-1);
        if (k < l-1) {
            stemmed = sdsnewlen(term, k+1);
            s = stemmed;
        }

        idx = dict_get(fts->index, s);
        assert(idx);
        ln = listSearchKey(idx, doc);
        assert(ln);
        index_item_t *idi = ln->value;
        idi->tf--;
        if (!idi->tf) listDelNode(idx, ln);

        if (stemmed) sdsfree(stemmed);
        sdsfree(term);
    }
    rr_free(terms);
    free_stemmer(stmer);
    fts->len -= doc->len;
}

static void cat_fts_index(fts_t *fts) {
    dict_iterator_t *iter;

    iter = dict_iter_create(fts->index);
    while (dict_iter_hasnext(iter)) {
        dict_kv_t kv = dict_iter_next(iter);
        rr_debug("key: %s", kv.key);
        list *l = kv.value;
        listIter *liter = listGetIterator(l, AL_START_HEAD);
        listNode *node;
        
        while((node = listNext(liter)) != NULL) {
            index_item_t *item = node->value;
            rr_debug("doc title: %s, tf: %d",
                (char *) item->doc->title->ptr, item->tf);
        }
        listReleaseIterator(liter);
    }
    dict_iter_free(iter);
}

bool fts_add(fts_t *fts, robj *title, robj *doc) {
    if (dict_contains(fts->docs, title->ptr)) fts_del(fts, title);

    fts_doc_t *fd = fts_doc_create(title, doc);
    if (!dict_set(fts->docs, title->ptr, fd)) {
        fts_doc_free(fd);
        return false;
    }

    /* update the index for this doc */
    if (!fts_index_add(fts, fd)) {
        fts_doc_free(dict_del(fts->docs, title->ptr));
        return false;
    }
    cat_fts_index(fts);
    return true;
}

fts_doc_t *fts_get(fts_t *fts, robj *title) {
    return dict_get(fts->docs, title->ptr); 
}

bool fts_del(fts_t *fts, robj *title) {
    fts_doc_t *doc = dict_del(fts->docs, title->ptr);

    if (!doc) return false;
    fts_index_del(fts, doc);
    fts_doc_free(doc);
    return true;
}

unsigned long fts_size(fts_t *fts) {
    return dict_length(fts->docs);
}

#define BM25_B (.75)
#define BM25_K (1.2)
static void calculate_bm25(fts_t *fts, list *indices, dict_t *scores) {
    listIter *iter;
    listNode *node;
    unsigned long doc_size = fts_size(fts);
    unsigned long list_size = listLength(indices);
    doc_size = doc_size ? doc_size : 1;
    double avgdl = (fts->len * 1.0) / doc_size;

    iter = listGetIterator(indices, AL_START_HEAD);
    while((node = listNext(iter)) != NULL) {
        index_item_t *idx = node->value;
        int dl = idx->doc->len;
        fts_doc_score_t *fds = dict_get(scores, idx->doc->title->ptr);
        if (!fds) {
            fds = rr_malloc(sizeof(*fds));
            fds->doc = idx->doc;
            fds->score = .0f;
            dict_set(scores, idx->doc->title->ptr, fds);
        }
        double idf = log((doc_size - list_size + 0.5) / (list_size + 0.5));
        double tf = idx->tf * (BM25_K + 1) / (idx->tf + BM25_K * (1 - BM25_B + BM25_B*dl/avgdl));
        fds->score += tf * idf;
    }
    listReleaseIterator(iter);
}

static dict_t *search_with_bm25_score(fts_t *fts, robj *query) {
    int i, l, k, len;
    sds *terms;
    struct stemmer *stmer;
    sds raw = query->ptr;
    /* dict of (title, fts_doc_score_t) */
    dict_t *scores = dict_create();
    dict_set_freecb(scores, dict_doc_score_free);
    dict_t *queried_terms = dict_create();

    terms = sdssplitlen(raw, sdslen(raw), " ", 1, &len);
    if (!terms) return scores;
    stmer = create_stemmer();
    for (i = 0; i < len; i++) {
        sds stemmed = NULL, term = terms[i], s = term;
        list *idx;

        term = sdstrim(term, puncs);
        l = sdslen(term);
        if (l == 0 || rr_stopwords_check(term)) {
            sdsfree(term);
            continue;
        }
        sdstolower(term);
        /* note that the third argument is a zero-based index */
        k = stem(stmer, term, l-1);
        if (k < l-1) {
            stemmed = sdsnewlen(term, k+1);
            s = stemmed;
        }
        if (dict_contains(queried_terms, s)) goto free_sds;
        dict_set(queried_terms, s, (void *)1);
        idx = dict_get(fts->index, s);
        if (!idx) goto free_sds;
        calculate_bm25(fts, idx, scores);
free_sds:
        if (stemmed) sdsfree(stemmed);
        sdsfree(term);
    }
    dict_free(queried_terms);
    rr_free(terms);
    free_stemmer(stmer);
    return scores;
}

/* minheap callbacks - copy, compare, swap */
static inline void *fts_cpy(void *dst, const void *src) {
    *(fts_doc_score_t *) dst = *(fts_doc_score_t *) src;
    return dst;
}

static inline int fts_cmp(const void *lv, const void *rv) {
    const fts_doc_score_t *l = (const fts_doc_score_t *) lv;
    const fts_doc_score_t *r = (const fts_doc_score_t *) rv;

    /* higher score items stay at the beginning of heap, i.e. a maxheap */
    if (l->score > r->score)
        return -1;
    else if (l->score < r->score)
        return 1;
    else
        return 0;
}

static inline void fts_swp(void *lv, void *rv) {
    fts_doc_score_t tmp;

    tmp = *(fts_doc_score_t *) lv;
    *(fts_doc_score_t *) lv = *(fts_doc_score_t *) rv;
    *(fts_doc_score_t *) rv = tmp;
}

static struct fts_iterator_t *create_fts_iterator(unsigned long size) {
    struct fts_iterator_t *it = rr_malloc(sizeof(*it));
    it->docs = minheap_create(size, sizeof(fts_doc_score_t), fts_cmp, fts_cpy, fts_swp);
    return it;
}

struct fts_iterator_t *fts_search(fts_t *fts, robj *query, unsigned long *size) {
    dict_t * scores = search_with_bm25_score(fts, query);
    *size = dict_length(scores);
    struct fts_iterator_t *it = create_fts_iterator(*size);
    dict_iterator_t *dict_it = dict_iter_create(scores);
    while (dict_iter_hasnext(dict_it)) {
        dict_kv_t score = dict_iter_next(dict_it);
        minheap_push(it->docs, score.value);
    }
    dict_free(scores);
    return it;
}

bool fts_iter_hasnext(struct fts_iterator_t *it) {
    return minheap_len(it->docs) > 0;
}

fts_doc_score_t *fts_iter_next(struct fts_iterator_t *it) {
    return minheap_pop(it->docs);
}

void fts_iter_free(struct fts_iterator_t *it) {
    minheap_free(it->docs);
    rr_free(it);
}
