#ifndef _RR_STOPWORDS_H
#define _RR_STOPWORDS_H

#include <stdbool.h>

void rr_stopwords_load(void);
bool rr_stopwords_check(const char *word);

#endif /* ifndef _RR_STOPWORDS_H */
