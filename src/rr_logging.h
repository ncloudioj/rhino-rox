#ifndef _RR_LOGGING_H
#define _RR_LOGGING_H

#define   RR_LOG_MAX_LEN    1024
#define   RR_LOG_DEBUG      0
#define   RR_LOG_INFO       1
#define   RR_LOG_WARNING    2
#define   RR_LOG_ERROR      3
#define   RR_LOG_CRITICAL   4

#ifdef NDEBUG
#define rr_debug(fmt, ...)
#else
#define rr_debug(fmt, ...) \
	fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

void rr_log(int level, const char *fmt, ...);

#endif
