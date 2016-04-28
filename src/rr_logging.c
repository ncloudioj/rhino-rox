#include "rr_ftmacro.h"

#include "rr_rhino_rox.h"
#include "rr_logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

static const char *LOG_LEVEL[] ={
    "[DEBUG]",
    "[INFO]",
    "[WARN]",
    "[ERROR]",
    "[CRITICAL]"
};

static int log_level = RR_LOG_INFO;
static char log_file[256] = {0};

int rr_log_set_log_level(int level) {
    int current = log_level;

    if (level < RR_LOG_DEBUG || level > RR_LOG_CRITICAL) return -1;
    log_level = level;
    return current;
}

int rr_log_set_log_file(const char *f) {
    size_t len;

    len = strnlen(f, sizeof(log_file));
    if (len >= sizeof(log_file)) {
        return RR_ERROR;
    }
    strncpy(log_file, f, sizeof(log_file));
    return RR_OK;
}

void rr_log(int level, const char *fmt, ...) {
    va_list ap;
    char msg[RR_LOG_MAX_LEN];

    level &= 0xFF;
    if (level > RR_LOG_CRITICAL || level < RR_LOG_DEBUG || level < log_level)
        return;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    FILE *fp;
    char buf[64];
    int off, log_stdout = 0;
    struct timeval tv;

    if (log_file[0] == '\0') log_stdout = 1;
    fp = log_stdout ? stdout : fopen(log_file, "a");
    if (fp == NULL) return;

    gettimeofday(&tv, NULL);
    off = strftime(buf, sizeof(buf), "%F %T.", localtime(&tv.tv_sec));
    snprintf(buf+off, sizeof(buf)-off, "%03d", (int)tv.tv_usec/1000);
    fprintf(fp, "%s %s %s\n", buf, LOG_LEVEL[level], msg);
    fflush(fp);
    if (!log_stdout) fclose(fp);
}
