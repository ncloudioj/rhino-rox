#ifndef _RR_LOGGING_H
#define _RR_LOGGING_H

#define   LOG_MAX_LEN    1024
#define   LOG_DEBUG      0
#define   LOG_INFO       1
#define   LOG_WARN       2
#define   LOG_ERROR      3
#define   LOG_CRITICAL   4

#ifdef NDEBUG
#define rr_debug(fmt, ...)
#else
#define rr_debug(fmt, ...) fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

void rr_log(int level, const char *fmt, ...);

#endif
