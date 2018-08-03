#include "rr_ftmacro.h"

#include "rr_rhino_rox.h"
#include "rr_config.h"
#include "rr_logging.h"
#include "rr_server.h"
#include "rr_malloc.h"
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
    const char *val;
    rr_configuration *cfg = ((rr_configuration_context *)config)->configs;
    dict_t *options = ((rr_configuration_context *)config)->options;

    #define SETVAL(n) do {            \
        val = dict_get(options, (n)); \
        val = !val ? value : val;     \
    } while (0)
    #define MATCH(s,n) (strcmp(section,s) == 0 && strcmp(name,n) == 0)
    if (MATCH("server", "max_clients")) {
        SETVAL("max_clients");
        cfg->max_clients = atoi(val);
        if (cfg->max_clients < 0) {
            err = "Invalid value for max_clients";
            goto error;
        }
    } else if (MATCH("server", "cron_frequency")) {
        SETVAL("cron_frequency");
        cfg->cron_frequency = atoi(val);
        if (cfg->cron_frequency < 0 || cfg->cron_frequency > SERVER_CRON_MAX_FREQUENCY) {
            err = "Invalid value for cron_frequency";
            goto error;
        }
    } else if (MATCH("server", "max_memory")) {
        SETVAL("max_memory");
        cfg->max_memory = memtoll(val, NULL);
        if (cfg->max_memory < 0) {
            err = "Invalid value for max_memory";
            goto error;
        }
    } else if (MATCH("server", "pidfile")) {
        SETVAL("pidfile");
        cfg->pidfile = rr_strdup(val);
        if (cfg->pidfile[0] != '\0') {
            FILE *fp;

            fp = fopen(cfg->pidfile, "a");
            if (fp == NULL) {
                snprintf(msg, sizeof(msg), "Failed to open  pidfile \"%s\": %s",
                    cfg->pidfile, strerror(errno));
                err = msg;
                goto error;
            }
            fclose(fp);
        }
    } else if (MATCH("server", "unix_domain_socket")) {
        SETVAL("unix_domain_socket");
        cfg->unix_domain_sock = rr_strdup(val);
        rr_log(RR_LOG_WARNING, "%s", cfg->unix_domain_sock);
    } else if (MATCH("server", "unix_domain_perm")) {
        SETVAL("unix_sock_perm");
        errno = 0;
        cfg->unix_sock_perm = (mode_t) strtol(val, NULL, 8);
        if (errno || cfg->unix_sock_perm > 0777) {
            err = "Invalid socket file permissions";
            goto error;
        }
    } else if (MATCH("logging", "log_level")) {
        SETVAL("log_level");
        if (!cfg_enum_get_value(LOG_LEVEL_ENUM, val, &cfg->log_level)
            || cfg->log_level > RR_LOG_CRITICAL
            || cfg->log_level < RR_LOG_DEBUG) {
            err = "Invalid value for log_level";
            goto error;
        }
    } else if (MATCH("logging", "log_file")) {
        SETVAL("log_file");
        cfg->log_file = rr_strdup(val);
        if (cfg->log_file[0] != '\0') {
            FILE *fp;

            fp = fopen(cfg->log_file, "a");
            if (fp == NULL) {
                snprintf(msg, sizeof(msg), "Failed to open log file \"%s\": %s",
                    cfg->log_file, strerror(errno));
                err = msg;
                goto error;
            }
            fclose(fp);
        }
    } else if (MATCH("network", "port")) {
        SETVAL("port");
        cfg->port = atoi(val);
        if (cfg->port < 0 || cfg->port > 65535) {
            err = "Invalid value for port";
            goto error;
        }
    } else if (MATCH("network", "bind")) {
        SETVAL("bind");
        cfg->bind = rr_strdup(val);
    } else if (MATCH("network", "tcp_backlog")) {
        SETVAL("tcp_backlog");
        cfg->tcp_backlog = atoi(val);
        if (cfg->port < 0) {
            err = "Invalid value for tcp_backlog";
            goto error;
        }
    } else if (MATCH("lazyfree", "server_del")) {
        SETVAL("server_del");
        cfg->lazyfree_server_del = atoi(val);
        if (cfg->port < 0) {
            err = "Invalid value for server_del";
            goto error;
        }
    } else if (MATCH("database", "max_dbs")) {
        SETVAL("max_dbs");
        cfg->max_dbs = atoi(val);
        if (cfg->port < 0) {
            err = "Invalid value for max_dbs";
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

int rr_config_load(const char *path, rr_configuration_context *cfg) {
    return ini_parse(path, handler, cfg) == 0 ? RR_OK : RR_ERROR;
}
