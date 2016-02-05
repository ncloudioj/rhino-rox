#include "rr_logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

static const char *LOG_LEVEL[] ={
    "[DEBUG]",
    "[INFO]",
    "[WARN]",
    "[ERROR]",
    "[CRITICAL]"
};

void rr_log(int level, const char *fmt, ...) { 
    va_list ap;
    char msg[LOG_MAX_LEN];

    level &= 0xFF;
    if (level > LOG_CRITICAL || level < LOG_DEBUG) return;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    FILE *fp;
    char buf[64];
    int off;
    struct timeval tv;

    fp = stdout;
    gettimeofday(&tv, NULL);
    // ISO 8601 formatted timestamp, e.g. "2016-02-05T16:36:48.649Z"
    off = strftime(buf, sizeof(buf), "%FT%T.", localtime(&tv.tv_sec));
    snprintf(buf+off, sizeof(buf)-off, "%03dZ", (int)tv.tv_usec/1000);
    fprintf(fp, "%s %s %s\n", buf, LOG_LEVEL[level], msg);
    fflush(fp);
}
