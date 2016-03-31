#include "rr_rhino_rox.h"
#include "rr_config.h"
#include "rr_logging.h"
#include "rr_server.h"
#include "ini.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

typedef struct cfg_enum_t {
    const char *name;
    const int val;
} cfg_enum_t;

/* Get enum value from name, return false if the name is missing */
static bool cfg_enum_get_value(cfg_enum_t *ce, const char *name, int *val) {
    while(ce->name != NULL) {
        if (!strcasecmp(ce->name, name)) {
            *val = ce->val;
            return true;
        }
        ce++;
    }
    return false;
}

cfg_enum_t LOG_LEVEL_ENUM[] = {
    {"debug", RR_LOG_DEBUG},
    {"info", RR_LOG_INFO},
    {"warning", RR_LOG_WARNING},
    {"error", RR_LOG_ERROR},
    {"critical", RR_LOG_CRITICAL},
    {NULL, 0}
};

/* Convert a string representing an amount of memory into the number of
 * bytes, so for instance memtoll("1Gb") will return 1073741824 that is
 * (1024*1024*1024).
 *
 * On parsing error, if *err is not NULL, it's set to 1, otherwise it's
 * set to 0. On error the function return value is 0, regardless of the
 * fact 'err' is NULL or not. */
static long long memtoll(const char *p, int *err) {
    const char *u;
    char buf[128];
    long mul; /* unit multiplier */
    long long val;
    unsigned int digits;

    if (err) *err = 0;

    /* Search the first non digit character. */
    u = p;
    if (*u == '-') u++;
    while(*u && isdigit(*u)) u++;
    if (*u == '\0' || !strcasecmp(u,"b")) {
        mul = 1;
    } else if (!strcasecmp(u,"k")) {
        mul = 1000;
    } else if (!strcasecmp(u,"kb")) {
        mul = 1024;
    } else if (!strcasecmp(u,"m")) {
        mul = 1000*1000;
    } else if (!strcasecmp(u,"mb")) {
        mul = 1024*1024;
    } else if (!strcasecmp(u,"g")) {
        mul = 1000L*1000*1000;
    } else if (!strcasecmp(u,"gb")) {
        mul = 1024L*1024*1024;
    } else {
        if (err) *err = 1;
        return 0;
    }

    /* Copy the digits into a buffer, we'll use strtoll() to convert
     * the digit (without the unit) into a number. */
    digits = u-p;
    if (digits >= sizeof(buf)) {
        if (err) *err = 1;
        return 0;
    }
    memcpy(buf,p,digits);
    buf[digits] = '\0';

    char *endptr;
    errno = 0;
    val = strtoll(buf,&endptr,10);
    if ((val == 0 && errno == EINVAL) || *endptr != '\0') {
        if (err) *err = 1;
        return 0;
    }
    return val*mul;
}

static int handler(void *config,
                   const char *section,
                   const char *name,
                   const char *value) {
    char *err;
    char msg[1024];
    rr_configuration *cfg = (rr_configuration *)config;

    #define MATCH(s,n) (strcmp(section,s) == 0 && strcmp(name,n) == 0)
    if (MATCH("server", "max_clients")) {
        cfg->max_clients = atoi(value);
        if (cfg->max_clients < 0) {
            err = "Invalid value for max_clients";
            goto error;
        }
    } else if (MATCH("server", "cron_frequency")) {
        cfg->cron_frequency = atoi(value);
        if (cfg->cron_frequency < 0 || cfg->cron_frequency > SERVER_CRON_MAX_FREQUENCY) {
            err = "Invalid value for cron_frequency";
            goto error;
        }
    } else if (MATCH("server", "max_memory")) {
        cfg->max_memory = memtoll(value, NULL);
        if (cfg->cron_frequency < 0) {
            err = "Invalid value for max_memory";
            goto error;
        }
    } else if (MATCH("logging", "log_level")) {
        if (!cfg_enum_get_value(LOG_LEVEL_ENUM, value, &cfg->log_level)
            || cfg->log_level > RR_LOG_CRITICAL
            || cfg->log_level < RR_LOG_DEBUG) {
            err = "Invalid value for log_level";
            goto error;
        }
    } else if (MATCH("network", "port")) {
        cfg->port = atoi(value);
        if (cfg->port < 0 || cfg->port > 65535) {
            err = "Invalid value for port";
            goto error;
        }
    } else if (MATCH("network", "bind")) {
        cfg->bind = strdup(value);
        if (cfg->port < 0 || cfg->port > 65535) {
            err = "Invalid value for port";
            goto error;
        }
    } else if (MATCH("network", "tcp_backlog")) {
        cfg->tcp_backlog = atoi(value);
        if (cfg->port < 0) {
            err = "Invalid value for tcp_backlog";
            goto error;
        }
    } else {
        snprintf(msg, sizeof(msg), "Unknown item: \"%s\" in section: [%s]", name, section);
        err = msg;
        goto error;
    }
    return 1;
error:
    rr_log(RR_LOG_ERROR, err);
    return 0;
}

int rr_config_load(const char *path, rr_configuration *cfg) {
    return ini_parse(path, handler, cfg) == 0 ? RR_OK : RR_ERROR;
}
