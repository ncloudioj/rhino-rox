#include "rr_ftmacro.h"

#include "rr_rhino_rox.h"
#include "rr_event.h"
#include "rr_logging.h"
#include "rr_network.h"
#include "rr_server.h"
#include "rr_config.h"
#include "rr_malloc.h"
#include "rr_bgtask.h"
#include "rr_db.h"
#include "rr_cmd_admin.h"
#include "rr_cmd_trie.h"
#include "rr_cmd_heapq.h"
#include "rr_datetime.h"
#include "ini.h"
#include "adlist.h"
#include "util.h"
#include "sds.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <sys/resource.h>

struct rr_server_t server;

static int server_cron(eventloop_t *el, void *ud);
static void before_polling(eventloop_t *el);
static void handle_accept(eventloop_t *el, int fd, void *ud, int mask);
static void add_client(int fd, int flags, char *ip);
static void free_client_async(rr_client_t *c);
static void handle_async_freed_clients(void);
static void free_client_argv(rr_client_t *c);
static int cmd_process(rr_client_t *c);
static void call(rr_client_t *c, int flags);
static void create_pidfile(void);
static void set_protocol_error(rr_client_t *c, int pos);

/*
 * Every entry is composed of the following fields:
 *
 * name: a string representing the command name.
 * function: pointer to the C function implementing the command.
 * arity: number of arguments, it is possible to use -N to say >= N
 * sflags: command flags as string. See below for a table of flags.
 * flags: flags as bitmask. Computed by Redis using the 'sflags' field.
 * get_keys_proc: an optional function to get key arguments from a command.
 *                This is only used when the following three fields are not
 *                enough to specify what arguments are keys.
 * first_key_index: first argument that is a key
 * last_key_index: last argument that is a key
 * key_step: step to get all the keys from first to last argument. For instance
 *           in MSET the step is two since arguments are key,val,key,val,...
 * microseconds: microseconds of total execution time for this command.
 * calls: total number of calls of this command.
 *
 * The flags, microseconds and calls fields are computed by Redis and should
 * always be set to zero.
 *
 * Command flags are expressed using strings where every character represents
 * a flag. Later the populateCommandTable() function will take care of
 * populating the real 'flags' field using this characters.
 *
 * This is the meaning of the flags:
 *
 * w: write command (may modify the key space).
 * r: read command  (will never modify the key space).
 * m: may increase memory usage once called. Don't allow if out of memory.
 * a: admin command, like SAVE or SHUTDOWN.
 * p: Pub/Sub related command.
 * f: force replication of this command, regardless of server.dirty.
 * s: command not allowed in scripts.
 * R: random command. Command is not deterministic, that is, the same command
 *    with the same arguments, with the same key space, may have different
 *    results. For instance SPOP and RANDOMKEY are two random commands.
 * S: Sort command output array if called from script, so that the output
 *    is deterministic.
 * l: Allow command while loading the database.
 * t: Allow command while a slave has stale data but is not allowed to
 *    server this data. Normally no command is accepted in this condition
 *    but just a few.
 * M: Do not automatically propagate the command on MONITOR.
 * k: Perform an implicit ASKING for this command, so the command will be
 *    accepted in cluster mode if the slot is marked as 'importing'.
 * F: Fast command: O(1) or O(log(N)) command that should never delay
 *    its execution as long as the kernel scheduler is giving us time.
 *    Note that commands that may trigger a DEL as a side effect (like SET)
 *    are not fast commands.
 */
struct redisCommand redisCommandTable[] = {
    {"get",rr_cmd_get,2,"rF",0,NULL,1,1,1,0,0},
    {"len",rr_cmd_len,1,"rF",0,NULL,1,1,1,0,0},
    {"set",rr_cmd_set,3,"wm",0,NULL,1,1,1,0,0},
    {"del",rr_cmd_del,2,"wF",0,NULL,1,-1,1,0,0},
    {"exists",rr_cmd_exists,2,"rF",0,NULL,1,-1,1,0,0},
    {"rget",rr_cmd_rget,3,"rF",0,NULL,1,1,1,0,0},
    {"rpget",rr_cmd_rpget,3,"rF",0,NULL,1,1,1,0,0},
    {"rlen",rr_cmd_rlen,2,"rF",0,NULL,1,1,1,0,0},
    {"rset",rr_cmd_rset,4,"wm",0,NULL,1,1,1,0,0},
    {"rdel",rr_cmd_rdel,3,"wF",0,NULL,1,-1,1,0,0},
    {"rkeys",rr_cmd_rkeys,2,"rF",0,NULL,1,1,1,0,0},
    {"rvalues",rr_cmd_rvalues,2,"rF",0,NULL,1,1,1,0,0},
    {"rgetall",rr_cmd_rgetall,2,"rF",0,NULL,1,1,1,0,0},
    {"rexists",rr_cmd_rexists,3,"rF",0,NULL,1,-1,1,0,0},
    {"qpush",rr_cmd_hqpush,4,"wF",0,NULL,1,1,1,0,0},
    {"qpop",rr_cmd_hqpop,2,"wF",0,NULL,1,1,1,0,0},
    {"qpopn",rr_cmd_hqpopn,3,"wm",0,NULL,1,1,1,0,0},
    {"qpeek",rr_cmd_hqpeek,2,"rF",0,NULL,1,1,1,0,0},
    {"qlen",rr_cmd_hqlen,2,"rF",0,NULL,1,1,1,0,0},
    /*  {"select"lectCommand,2,"rlF",0,NULL,0,0,0,0,0}, */
    {"type",rr_cmd_type,2,"rF",0,NULL,1,1,1,0,0},
    {"ping",rr_cmd_admin_ping,-1,"rtF",0,NULL,0,0,0,0,0},
    {"echo",rr_cmd_admin_echo,2,"rF",0,NULL,0,0,0,0,0},
    {"shutdown",rr_cmd_admin_shutdown,-1,"arlt",0,NULL,0,0,0,0,0},
    {"info",rr_cmd_admin_info,0,"ar",0,NULL,0,0,0,0,0},
    /*  {"command",commandCommand,0,"rlt",0,NULL,0,0,0,0,0}, */
};

static void rr_server_shutdown(int sig) {
    UNUSED(sig);
    server.shutdown = 1;
}

static void rr_server_signal(void) {
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGPIPE, &act, NULL);

    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = rr_server_shutdown;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    return;
}

void populateCommandTable(void) {
    int j;
    int numcommands = sizeof(redisCommandTable)/sizeof(struct redisCommand);

    for (j = 0; j < numcommands; j++) {
        struct redisCommand *c = redisCommandTable+j;
        char *f = c->sflags;
        int retval;

        while(*f != '\0') {
            switch(*f) {
            case 'w': c->flags |= CMD_WRITE; break;
            case 'r': c->flags |= CMD_READONLY; break;
            case 'm': c->flags |= CMD_DENYOOM; break;
            case 'a': c->flags |= CMD_ADMIN; break;
            case 'p': c->flags |= CMD_PUBSUB; break;
            case 's': c->flags |= CMD_NOSCRIPT; break;
            case 'R': c->flags |= CMD_RANDOM; break;
            case 'S': c->flags |= CMD_SORT_FOR_SCRIPT; break;
            case 'l': c->flags |= CMD_LOADING; break;
            case 't': c->flags |= CMD_STALE; break;
            case 'M': c->flags |= CMD_SKIP_MONITOR; break;
            case 'k': c->flags |= CMD_ASKING; break;
            case 'F': c->flags |= CMD_FAST; break;
            default:
                rr_log(RR_LOG_ERROR, "Unsupported command flag");
                exit(1);
                break;
            }
            f++;
        }

        retval = dict_set(server.commands, sdsnew(c->name), c);
        assert(retval);
    }
}

static void create_pidfile(void) {
    if (!server.pidfile) server.pidfile = rr_strdup(SERVER_DEFAULT_PIDFILE);

    /* Try to write the pid file in a best-effort way. */
    FILE *fp = fopen(server.pidfile, "w");
    if (fp) {
        fprintf(fp, "%d\n", (int)getpid());
        fclose(fp);
    }
}

void rr_server_init(rr_configuration *cfg) {
    int i;

    rr_log_set_log_level(cfg->log_level);
    rr_log_set_log_file(cfg->log_file);

    server.shutdown = 0;
    server.pidfile = NULL;
    server.max_memory = cfg->max_memory;
    server.max_clients = cfg->max_clients;
    server.max_dbs = cfg->max_dbs;
    server.lazyfree_server_del = cfg->lazyfree_server_del;
    rr_server_adjust_max_clients();
    server.hz = cfg->cron_frequency;
    server.cronloops = 0;
    server.served = 0;
    server.rejected = 0;
    server.stats_memory_usage = 0;
    rr_server_signal();
    if ((server.el = el_loop_create(server.max_clients)) == NULL) goto error;
    server.lpfd = rr_net_tcpserver(
            server.err, cfg->port, cfg->bind, AF_INET, cfg->tcp_backlog);
    if (server.lpfd == RR_EV_ERR) goto error;
    if (rr_net_nonblock(server.err, server.lpfd) == RR_ERROR) goto error;
    if (el_event_add(server.el, server.lpfd, RR_EV_READ, handle_accept, NULL)
          == RR_EV_ERR)
        goto error;

    if (el_timer_add(server.el, 1, server_cron, NULL) == RR_EV_ERR) {
        rr_log(RR_LOG_CRITICAL, "Can't create event loop timers.");
        exit(1);
    }
    el_loop_set_before_polling(server.el, before_polling);
    server.client_max_query_len = PROTO_QUERY_MAX_LEN;
    server.clients = listCreate();
    server.clients_with_pending_writes = listCreate();
    server.clients_to_close = listCreate();

    server.dbs = rr_malloc(sizeof(rrdb_t*)*server.max_dbs);
    for (i = 0; i < server.max_dbs; i++) {
        server.dbs[i] = rr_db_create(i);
    }

    server.commands = dict_create();
    server.ncmd_complete = 0;
    populateCommandTable();

    createSharedObjects();

    /* light up background task runners */
    rr_bgt_init();
    /* create pidfile if necessary */
    if (cfg->pidfile[0] != '\0') {
        server.pidfile = rr_strdup(cfg->pidfile);
        create_pidfile();
    }
    return;
error:
    rr_log(RR_LOG_CRITICAL, server.err);
    exit(1);
}

sds rr_server_get_info(void) {
    char used_mem_human[64], max_mem_human[64], system_mem_human[64];
    size_t used_mem = rr_get_used_memory();
    size_t system_mem = rr_get_system_memory_size();
    unsigned long long max_mem = server.max_memory ? server.max_memory : system_mem;
    sds info = sdsempty();

    info = sdscatprintf(info,
        "# Server\r\n"
        "current_clients:%lu\r\n"
        "clients_served:%lu\r\n"
        "clients_rejected:%lu\r\n"
        "\r\n",
        listLength(server.clients), server.served, server.rejected);

    bytesToHuman(used_mem_human, used_mem);
    bytesToHuman(system_mem_human, system_mem);
    bytesToHuman(max_mem_human, max_mem);
    info = sdscatprintf(info,
        "# Memory\r\n"
        "used_memory:%zu\r\n"
        "used_memory_human:%s\r\n"
        "system_memory:%zu\r\n"
        "system_memory_human:%s\r\n"
        "max_memory:%lld\r\n"
        "max_memory_human:%s\r\n",
        used_mem, used_mem_human, system_mem, system_mem_human,
        max_mem, max_mem_human);

    return info;
}

struct redisCommand *cmd_lookup(sds name) {
    sdstolower(name);
    return dict_get(server.commands, name);
}

struct redisCommand *cmd_lookup_cstr(char *s) {
    struct redisCommand *cmd;
    sds name = sdsnew(s);

    cmd = dict_get(server.commands, name);
    sdsfree(name);
    return cmd;
}

void rr_server_close(void) {
    el_loop_stop(server.el);
    el_loop_free(server.el);
}

static void rr_server_close_listening_sockets() {
    close(server.lpfd);
}

int rr_server_prepare_to_shutdown(void) {
    rr_log(RR_LOG_INFO, "User required to shutdown the service, preparing...");

    /* nuke the pidfile */
    if (server.pidfile) {
        rr_log(RR_LOG_INFO, "Deleting the pidfile %s", server.pidfile);
        unlink(server.pidfile);
    }

    rr_server_close_listening_sockets();
    rr_log(RR_LOG_INFO, "Ready to exit, bye!");
    return RR_OK;
}

void rr_server_adjust_max_clients(void) {
    rlim_t maxfiles = server.max_clients + SERVER_RESERVED_FDS;
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        rr_log(RR_LOG_WARNING,
            "Fail to obtain the current NOFILE limit (%s), "
            "assuming 1024 and setting the max clients configuration accordingly.",
            strerror(errno));
        server.max_clients = 1024 - SERVER_RESERVED_FDS;
    } else {
        rlim_t oldlimit = limit.rlim_cur;

        /* Set the max number of files if the current limit is not enough
         * for our needs. */
        if (oldlimit < maxfiles) {
            rlim_t bestlimit;
            int setrlimit_error = 0;

            /* Try to set the file limit to match 'maxfiles' or at least
             * to the higher value supported less than maxfiles. */
            bestlimit = maxfiles;
            while(bestlimit > oldlimit) {
                rlim_t decr_step = 16;

                limit.rlim_cur = bestlimit;
                limit.rlim_max = bestlimit;
                if (setrlimit(RLIMIT_NOFILE, &limit) != -1) break;
                setrlimit_error = errno;

                /* We failed to set file limit to 'bestlimit'. Try with a
                 * smaller limit decrementing by a few FDs per iteration. */
                if (bestlimit < decr_step) break;
                bestlimit -= decr_step;
            }

            /* Assume that the limit we get initially is still valid if
             * our last try was even lower. */
            if (bestlimit < oldlimit) bestlimit = oldlimit;

            if (bestlimit < maxfiles) {
                int old_maxclients = server.max_clients;
                server.max_clients = bestlimit - SERVER_RESERVED_FDS;
                if (server.max_clients < 1) {
                    rr_log(RR_LOG_CRITICAL, "Your current 'ulimit -n' "
                        "of %llu is not enough for the server to start. "
                        "Please increase your open file limit to at least "
                        "%llu. Exiting",
                        (unsigned long long) oldlimit,
                        (unsigned long long) maxfiles);
                    exit(1);
                }
                rr_log(RR_LOG_WARNING, "You requested max_clients of %d "
                    "requiring at least %llu max file descriptors",
                    old_maxclients, (unsigned long long) maxfiles);
                rr_log(RR_LOG_WARNING, "Server can't set maximum open files "
                    "to %llu because of OS error: %s",
                    (unsigned long long) maxfiles, strerror(setrlimit_error));
                rr_log(RR_LOG_WARNING, "Current maximum open files is %llu. "
                    "maxclients has been reduced to %d to compensate for "
                    "low ulimit. "
                    "If you need higher maxclients increase 'ulimit -n'",
                    (unsigned long long) bestlimit, server.max_clients);
            } else {
                rr_log(RR_LOG_INFO, "Increased maximum number of open files "
                    "to %llu (it was originally set to %llu)",
                    (unsigned long long) maxfiles,
                    (unsigned long long) oldlimit);
            }
        }
    }
}

#define CLIENTS_CRON_MIN_ITERATIONS 5
static void client_cron(void) {
}

static int server_cron(eventloop_t *el, void *ud) {
    UNUSED(el);
    UNUSED(ud);

    if (server.shutdown) {
        if (rr_server_prepare_to_shutdown() == RR_OK) exit(0);
        rr_log(RR_LOG_INFO,
            "SIGTERM received but errors trying to shut down the server");
        server.shutdown = 0;
    }

    handle_async_freed_clients();

    client_cron();

    server.stats_memory_usage = rr_get_used_memory();

    server.cronloops++;
    return 1000/server.hz;
}

static void handle_async_freed_clients(void) {
    while (listLength(server.clients_to_close)) {
        listNode *ln = listFirst(server.clients_to_close);
        rr_client_t *c = listNodeValue(ln);
        c->flags &= ~CLIENT_CLOSE_ASAP;
        rr_client_free(c);
        listDelNode(server.clients_to_close, ln);
    }
}

static void free_client_async(rr_client_t *c) {
    if (c->flags & CLIENT_CLOSE_ASAP) return;
    c->flags |= CLIENT_CLOSE_ASAP;
    listAddNodeTail(server.clients_to_close,c);
}

static int handle_clients_with_pending_writes() {
    listIter li;
    listNode *ln;
    int processed = listLength(server.clients_with_pending_writes);

    listRewind(server.clients_with_pending_writes, &li);
    while((ln = listNext(&li))) {
        rr_client_t *c = listNodeValue(ln);
        c->flags &= ~CLIENT_PENDING_WRITE;
        listDelNode(server.clients_with_pending_writes, ln);

        /* Try to write buffers to the client socket. */
        if (reply_write_to_client(c->fd, c, 0) == RR_ERROR) continue;

        /* If there is nothing left, do nothing. Otherwise install
         * the write handler. */
        if (client_has_pending_replies(c) &&
            el_event_add(server.el, c->fd, RR_EV_WRITE, reply_write_callback, c)
            == RR_EV_ERR) {
            free_client_async(c);
        }
    }
    return processed;
}

static void before_polling(eventloop_t *el) {
    UNUSED(el);
    handle_clients_with_pending_writes();
}

static int process_inline_input(rr_client_t *c) {
    char *newline;
    int i, argc;
    sds *argv, aux;
    size_t querylen;

    /* Search for end of line */
    newline = strchr(c->query, '\n');

    /* Nothing to do without a \r\n */
    if (newline == NULL) {
        if (sdslen(c->query) > PROTO_INLINE_MAX_LEN) {
            reply_add_err(c, "Protocol error: too big inline request");
        }
        return RR_ERROR;
    }

    /* Handle the \r\n case. */
    if (newline && newline != c->query && *(newline-1) == '\r')
        newline--;

    /* Split the input buffer up to the \r\n */
    querylen = newline - c->query;
    aux = sdsnewlen(c->query, querylen);
    argv = sdssplitargs(aux, &argc);
    sdsfree(aux);

    if (argv == NULL) {
        reply_add_err(c, "Protocol error: unbalanced quotes in request");
        set_protocol_error(c, 0);
        return RR_ERROR;
    }

    /* Slice the query buffer to delete the processed line */
    sdsrange(c->query, querylen+2, -1);

    if (argc) {
        if (c->argv) rr_free(c->argv);
        c->argv = rr_malloc(sizeof(robj*)*argc);
    }

    /* Create redis objects for all arguments. */
    for (c->argc = 0, i = 0; i < argc; i++) {
        if (sdslen(argv[i])) {
            c->argv[c->argc] = createObject(OBJ_STRING, argv[i]);
            c->argc++;
        } else {
            sdsfree(argv[i]);
        }
    }
    rr_free(argv);
    return RR_OK;
}

/* Trims the query buffer to make the function that processes
 * multi bulk requests idempotent */
static void set_protocol_error(rr_client_t *c, int pos) {
    c->flags |= CLIENT_CLOSE_AFTER_REPLY;
    sdsrange(c->query, pos, -1);
}

static int process_multi_bulk_input(rr_client_t *c) {
    char *newline = NULL;
    int pos = 0, ok;
    long long ll;

    if (c->multibulk_len == 0) {
        /* The client should have been reset */
        assert(c->argc == 0);

        /* Multi bulk length cannot be read without a \r\n */
        newline = strchr(c->query, '\r');
        if (newline == NULL) {
            if (sdslen(c->query) > PROTO_INLINE_MAX_LEN) {
                reply_add_err(c, "Protocol error: too big mbulk count string");
                set_protocol_error(c, 0);
            }
            return RR_ERROR;
        }

        /* Buffer should also contain \n */
        if (newline-(c->query) > ((signed)sdslen(c->query)-2))
            return RR_ERROR;

        /* We know for sure there is a whole line since newline != NULL,
         * so go ahead and find out the multi bulk length. */
        ok = string2ll(c->query+1, newline-(c->query+1), &ll);
        if (!ok || ll > 1024*1024) {
            reply_add_err(c, "Protocol error: invalid multibulk length");
            set_protocol_error(c, pos);
            return RR_ERROR;
        }

        pos = (newline-c->query) + 2;
        if (ll <= 0) {
            sdsrange(c->query,pos,-1);
            return RR_OK;
        }

        c->multibulk_len = ll;

        /* Setup argv array on client structure */
        if (c->argv) rr_free(c->argv);
        c->argv = rr_malloc(sizeof(robj*)*c->multibulk_len);
    }

    assert(c->multibulk_len > 0);
    while(c->multibulk_len) {
        /* Read bulk length if unknown */
        if (c->bulk_len == -1) {
            newline = strchr(c->query+pos, '\r');
            if (newline == NULL) {
                if (sdslen(c->query) > PROTO_INLINE_MAX_LEN) {
                    reply_add_err(c,
                        "Protocol error: too big bulk count string");
                    set_protocol_error(c,0);
                    return RR_ERROR;
                }
                break;
            }

            /* Buffer should also contain \n */
            if (newline-(c->query) > ((signed)sdslen(c->query)-2))
                break;

            if (c->query[pos] != '$') {
                reply_add_err_format(c,
                    "Protocol error: expected '$', got '%c'", c->query[pos]);
                set_protocol_error(c, pos);
                return RR_ERROR;
            }

            ok = string2ll(c->query+pos+1, newline-(c->query+pos+1), &ll);
            if (!ok || ll < 0 || ll > 512*1024*1024) {
                reply_add_err(c, "Protocol error: invalid bulk length");
                set_protocol_error(c,pos);
                return RR_ERROR;
            }

            pos += newline-(c->query+pos)+2;
            if (ll >= PROTO_MBULK_BIG_ARG) {
                size_t qblen;

                /* If we are going to read a large object from network
                 * try to make it likely that it will start at c->query
                 * boundary so that we can optimize object creation
                 * avoiding a large copy of data. */
                sdsrange(c->query, pos, -1);
                pos = 0;
                qblen = sdslen(c->query);
                /* Hint the sds library about the amount of bytes this string is
                 * going to contain. */
                if (qblen < (size_t)ll+2)
                    c->query = sdsMakeRoomFor(c->query, ll+2-qblen);
            }
            c->bulk_len = ll;
        }

        /* Read bulk argument */
        if (sdslen(c->query)-pos < (unsigned)(c->bulk_len+2)) {
            /* Not enough data (+2 == trailing \r\n) */
            break;
        } else {
            /* Optimization: if the buffer contains JUST our bulk element
             * instead of creating a new object by *copying* the sds we
             * just use the current sds string. */
            if (pos == 0 &&
                c->bulk_len >= PROTO_MBULK_BIG_ARG &&
                (signed) sdslen(c->query) == c->bulk_len+2)
            {
                c->argv[c->argc++] = createObject(OBJ_STRING,c->query);
                sdsIncrLen(c->query,-2); /* remove CRLF */
                c->query = sdsempty();
                /* Assume that if we saw a fat argument we'll see another one
                 * likely... */
                c->query = sdsMakeRoomFor(c->query, c->bulk_len+2);
                pos = 0;
            } else {
                c->argv[c->argc++] =
                    createStringObject(c->query+pos, c->bulk_len);
                pos += c->bulk_len+2;
            }
            c->bulk_len = -1;
            c->multibulk_len--;
        }
    }

    /* Trim to pos */
    if (pos) sdsrange(c->query, pos, -1);

    /* We're done when c->multibulk == 0 */
    if (c->multibulk_len == 0) return RR_OK;

    /* Still not read to process the command */
    return RR_ERROR;
}

static void process_user_input(rr_client_t *c) {
    while (sdslen(c->query)) { 
        /* CLIENT_CLOSE_AFTER_REPLY closes the connection once the reply is
         * written to the client. Make sure not let the reply grow after
         * this flag has been set (i.e. don't process more commands). */
        if (c->flags & CLIENT_CLOSE_AFTER_REPLY) break;

        /* Determine request type when unknown. */
        if (!c->req_type) {
            if (c->query[0] == '*') {
                c->req_type = PROTO_REQ_MULTIBULK;
            } else {
                c->req_type = PROTO_REQ_INLINE;
            }
        }

        if (c->req_type == PROTO_REQ_INLINE) {
            if (process_inline_input(c) != RR_OK) break;
        } else if (c->req_type == PROTO_REQ_MULTIBULK) {
            if (process_multi_bulk_input(c) != RR_OK) break;
        } else {
            rr_log(RR_LOG_WARNING, "Unknown request type");
            assert(0); 
        }

        /* Multibulk processing could see a <= 0 length. */
        if (c->argc == 0) {
            rr_client_reset(c);
        } else {
            /* Only reset the client when the command was executed. */
            if (cmd_process(c) == RR_OK)
                rr_client_reset(c);
        }
    }
}

/* If this function gets called we already read a whole
 * command, arguments are in the client argv/argc fields.
 * cmd_process() execute the command or prepare the
 * server for a bulk read from the client.
 *
 * If RR_OK is returned the client is still alive and valid and
 * other operations can be performed by the caller. Otherwise
 * if RR_ERROR is returned the client was destroyed (i.e. after QUIT). */
static int cmd_process(rr_client_t *c) {
    /* The QUIT command is handled separately. Normal command procs will
     * go through checking for replication and QUIT will cause trouble
     * when FORCE_REPLICATION is enabled and would be implemented in
     * a regular command proc. */
    if (!strcasecmp(c->argv[0]->ptr, "quit")) {
        reply_add_obj(c, shared.ok);
        c->flags |= CLIENT_CLOSE_AFTER_REPLY;
        return RR_ERROR;
    }

    /* Now lookup the command and check ASAP about trivial error conditions
     * such as wrong arity, bad command name and so forth. */
    c->cmd = c->lastcmd = cmd_lookup(c->argv[0]->ptr);
    if (!c->cmd) {
        reply_add_err_format(c, "unknown command '%s'",
            (char*) c->argv[0]->ptr);
        return RR_OK;
    } else if ((c->cmd->arity > 0 && c->cmd->arity != c->argc) ||
               (c->argc < -c->cmd->arity)) {
        reply_add_err_format(c,"wrong number of arguments for '%s' command",
            c->cmd->name);
        return RR_OK;
    }

    call(c, CMD_CALL_FULL);
    return RR_OK;
}

/* Call() is the core of Redis execution of a command.
 *
 * The following flags can be passed:
 * CMD_CALL_NONE        No flags.
 * CMD_CALL_SLOWLOG     Check command speed and log in the slow log if needed.
 * CMD_CALL_STATS       Populate command stats.
 * CMD_CALL_PROPAGATE_AOF   Append command to AOF if it modified the dataset
 *                          or if the client flags are forcing propagation.
 * CMD_CALL_PROPAGATE_REPL  Send command to salves if it modified the dataset
 *                          or if the client flags are forcing propagation.
 * CMD_CALL_PROPAGATE   Alias for PROPAGATE_AOF|PROPAGATE_REPL.
 * CMD_CALL_FULL        Alias for SLOWLOG|STATS|PROPAGATE.
 *
 * The exact propagation behavior depends on the client flags.
 * Specifically:
 *
 * 1. If the client flags CLIENT_FORCE_AOF or CLIENT_FORCE_REPL are set
 *    and assuming the corresponding CMD_CALL_PROPAGATE_AOF/REPL is set
 *    in the call flags, then the command is propagated even if the
 *    dataset was not affected by the command.
 * 2. If the client flags CLIENT_PREVENT_REPL_PROP or CLIENT_PREVENT_AOF_PROP
 *    are set, the propagation into AOF or to slaves is not performed even
 *    if the command modified the dataset.
 *
 * Note that regardless of the client flags, if CMD_CALL_PROPAGATE_AOF
 * or CMD_CALL_PROPAGATE_REPL are not set, then respectively AOF or
 * slaves propagation will never occur.
 *
 * Client flags are modified by the implementation of a given command
 * using the following API:
 *
 * forceCommandPropagation(client *c, int flags);
 * preventCommandPropagation(client *c);
 * preventCommandAOF(client *c);
 * preventCommandReplication(client *c);
 *
 */
static void call(rr_client_t *c, int flags) {
    long long start, duration;

    /* Call the command. */
    start = rr_dt_ustime();
    c->cmd->proc(c);
    duration = rr_dt_ustime() - start;
    if (flags & CMD_CALL_STATS) {
        c->lastcmd->microseconds += duration;
        c->lastcmd->calls++;
    }

    server.ncmd_complete++;
}

/* prepares the client to process the next command */
void rr_client_reset(rr_client_t *c) {
    c->req_type = 0; 
    c->multibulk_len = 0;
    c->bulk_len = -1;
    free_client_argv(c);
}

static void handle_read_from_client(eventloop_t *el, int fd, void *ud, int mask) {
   rr_client_t *c = (rr_client_t *) ud;

    int nread, readlen;
    int qlen;
    UNUSED(el);
    UNUSED(mask);

    qlen = sdslen(c->query);
    readlen = PROTO_IOBUF_LEN;
    c->query = sdsMakeRoomFor(c->query, readlen);
    nread = read(fd, c->query+qlen, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) {
            return;
        } else {
            rr_log(RR_LOG_ERROR, "Reading from client: %s", strerror(errno));
            rr_client_free(c);
            return;
        }
    } else if (nread == 0) {
        rr_log(RR_LOG_INFO, "Client closed connection");
        rr_client_free(c);
        return;
    }
    sdsIncrLen(c->query, nread);

    if (sdslen(c->query) > server.client_max_query_len) {
        rr_log(RR_LOG_WARNING, "Closing client that reached max query buffer length.");
        rr_client_free(c);
        return;
    }

    process_user_input(c);
}

/* Client.reply list dup and free methods */
static void *list_reply_dup(void *val) {
    return sdsdup(val);
}

static void list_reply_free(void *val) {
    sdsfree(val);
}

rr_client_t *rr_client_create(int fd) {
    rr_client_t *c = rr_malloc(sizeof(rr_client_t));
    if (c == NULL) {
        rr_log(RR_LOG_CRITICAL, "Error creating a new client: oom");
        return NULL;
    }

    if (fd != -1) {
        if (rr_net_nonblock(server.err, fd) != RR_NET_OK) goto error;
        if (rr_net_nodelay(server.err, fd) != RR_NET_OK) goto error;
        if (rr_net_keepalive(server.err, fd) != RR_NET_OK) goto error;
        if (el_event_add(server.el, fd, RR_EV_READ, handle_read_from_client, c) == RR_EV_ERR) goto error;
    }

    c->fd = fd;
    c->req_type = 0;
    c->multibulk_len = 0;
    c->bulk_len = -1;
    c->query = sdsempty();
    c->argc = 0;
    c->argv = NULL;
    c->cmd = c->lastcmd = NULL;
    c->db = *server.dbs;  /* use db 0 by default */
    c->replied_len = 0;
    c->buf_sent_len = 0;
    c->buf_offset = 0;
    c->reply = listCreate();
    listSetFreeMethod(c->reply, list_reply_free);
    listSetDupMethod(c->reply, list_reply_dup);
    c->flags = 0;
    if (fd != -1) listAddNodeTail(server.clients, c);
    return c;
error:
    rr_free(c);
    return NULL;
}

static void unlink_client(rr_client_t *c) {
    listNode *node;

    if (c->fd != -1) {
        node = listSearchKey(server.clients, c);
        assert(node);
        listDelNode(server.clients, node);

        el_event_del(server.el, c->fd, RR_EV_READ);
        el_event_del(server.el, c->fd, RR_EV_WRITE);
        close(c->fd);
        c->fd = -1;
    }
}

static void free_client_argv(rr_client_t *c) {
    int i;
    for (i = 0; i < c->argc; i++)
        decrRefCount(c->argv[i]);
    c->argc = 0;
    c->cmd = NULL;
}

void rr_client_free(rr_client_t *c) {
    unlink_client(c);
    sdsfree(c->query);
    listRelease(c->reply);
    free_client_argv(c);
    rr_free(c);
}

static void add_client(int fd, int flags, char *ip) {
    UNUSED(ip);

    rr_client_t *c;
    if ((c = rr_client_create(fd)) == NULL) {
        rr_log(RR_LOG_WARNING,
            "Error registering fd event for the new client: %s (fd=%d)",
            strerror(errno), fd);
        close(fd); /* May be already closed, just ignore errors */
        return;
    }
    /* If maxclient directive is set and this is one client more... close the
     * connection. Note that we create the client instead to check before
     * for this condition, since now the socket is already set in non-blocking
     * mode and we can send an error for free using the Kernel I/O */
    if (listLength(server.clients) > server.max_clients) {
        char *err = "-ERR max number of clients reached\r\n";

        /* That's a best effort error message, don't check write errors */
        if (write(c->fd, err, strlen(err)) == -1) {
            /* Nothing to do, Just to avoid the warning... */
        }
        server.rejected++;
        rr_client_free(c);
        return;
    }

    server.served++;
    c->flags |= flags;
}

int check_obj_type(rr_client_t *c, robj *o, int type) {
    if (o->type != type) {
        reply_add_obj(c, shared.wrongtypeerr);
        return 1;
    }
    return 0;
}

static void handle_accept(eventloop_t *el, int fd, void *ud, int mask) {
    int cport, cfd, max = RR_NET_MAXACCEPT;
    char cip[RR_NET_MAXIPLEN];
    UNUSED(el);
    UNUSED(mask);
    UNUSED(ud);

    while(max--) {
        cfd = rr_net_accept(server.err, fd, cip, sizeof(cip), &cport);
        if (cfd == RR_NET_ERR) {
            if (errno != EWOULDBLOCK)
                rr_log(RR_LOG_WARNING, "Accepting client connection: %s", server.err);
            return;
        }
        rr_log(RR_LOG_INFO, "Accepted %s:%d", cip, cport);
        add_client(cfd, 0, cip);
    }
}

/* This is a helper function for the OBJECT command. We need to lookup keys
 * without any modification of LRU or other parameters. */
/*  robj *objectCommandLookup(rr_client_t *c, robj *key) { */
    /*  dictEntry *de; */

    /*  if ((de = dictFind(c->db->dict,key->ptr)) == NULL) return NULL; */
    /*  return (robj*) dictGetVal(de); */
/*  } */

/*  robj *objectCommandLookupOrReply(rr_client_t *c, robj *key, robj *reply) { */
    /*  robj *o = objectCommandLookup(c,key); */

    /*  if (!o) reply_add_obj(c, reply); */
    /*  return o; */
/*  } */

/* Object command allows to inspect the internals of an Redis Object.
 * Usage: OBJECT <refcount|encoding|idletime> <key> */
/*  void objectCommand(rr_client_t *c) { */
    /*  robj *o; */

    /*  if (!strcasecmp(c->argv[1]->ptr,"refcount") && c->argc == 3) { */
        /*  if ((o = objectCommandLookupOrReply(c,c->argv[2],shared.nullbulk)) */
                /*  == NULL) return; */
        /*  reply_add_longlong(c,o->refcount); */
    /*  } else if (!strcasecmp(c->argv[1]->ptr,"encoding") && c->argc == 3) { */
        /*  if ((o = objectCommandLookupOrReply(c,c->argv[2],shared.nullbulk)) */
                /*  == NULL) return; */
        /*  reply_add_bulk_cstr(c, strEncoding(o->encoding)); */
    /*  } else { */
        /*  reply_add_err(c,"Syntax error. Try OBJECT (refcount|encoding|idletime)"); */
    /*  } */
/*  } */

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    rr_configuration cfg;
    if (rr_config_load("rhino-rox.ini", &cfg) == RR_ERROR) {
        rr_log(RR_LOG_CRITICAL, "Failed to load the config file");
        exit(1);
    }
    rr_server_init(&cfg);
    el_main(server.el);
    rr_server_close();
}
