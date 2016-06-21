#ifndef _RR_CONFIG_H
#define _RR_CONFIG_H

#if (__i386 || __amd64 || __powerpc__) && __GNUC__
#define GNUC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if defined(__clang__)
#define HAVE_ATOMIC
#endif
#if (defined(__GLIBC__) && defined(__GLIBC_PREREQ))
#if (GNUC_VERSION >= 40100 && __GLIBC_PREREQ(2, 6))
#define HAVE_ATOMIC
#endif
#endif
#endif

#include "rr_dict.h"

typedef struct rr_configuration {
    int port;
    char *bind;
    int max_clients;
    long long max_memory;
    int cron_frequency;
    int log_level;
    char *log_file;
    char *pidfile;
    int tcp_backlog;
    int lazyfree_server_del;
    int max_dbs;
} rr_configuration;

typedef struct rr_configuration_context {
    rr_configuration *configs;
    dict_t           *options;
} rr_configuration_context;

int rr_config_load(const char *path, rr_configuration_context *cfg);

#endif
