#include "rr_server.h"
#include "rr_logging.h"
#include "rr_rhino_rox.h"
#include "rr_malloc.h"
#include "robj.h"
#include "util.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
#include <stdarg.h>

static int prepare_client_to_write(rr_client_t *c);
static int add_reply_to_buffer(rr_client_t *c, const char *s, size_t len);
void add_reply_sds_to_list(rr_client_t *c, sds s);
void add_reply_str_to_list(rr_client_t *c, const char *s, size_t len);

bool client_has_pending_replies(rr_client_t *c) {
    return c->buf_offset || listLength(c->reply);
}

/*  Set the flag for this client, and add it to the client list with pending write */
static int prepare_client_to_write(rr_client_t *c) {
    if (c->fd <= 0) return RR_ERROR;

    /* Schedule the client to write the output buffers to the socket only
     * if not already done (there were no pending writes already and the client
     * was yet not flagged) */
    if (!client_has_pending_replies(c) && !(c->flags & CLIENT_PENDING_WRITE)) {
        /* Here instead of installing the write handler, we just flag the
         * client and put it into a list of clients that have something
         * to write to the socket. This way before re-entering the event
         * loop, we can try to directly write to the client sockets avoiding
         * a system call. We'll only really install the write handler if
         * we'll not be able to write the whole reply at once. */
        c->flags |= CLIENT_PENDING_WRITE;
        listAddNodeHead(server.clients_with_pending_writes, c);
    }

    /* Authorize the caller to queue in the output buffer of this client. */
    return RR_OK;
}


static int add_reply_to_buffer(rr_client_t *c, const char *s, size_t len) {
    size_t available = sizeof(c->buf)-c->buf_offset;

    if (c->flags & CLIENT_CLOSE_AFTER_REPLY) return RR_OK;

    /* If there already are entries in the reply list, we cannot
     * add anything more to the static buffer. */
    if (listLength(c->reply) > 0) return RR_ERROR;

    /* Check that the buffer has enough space available for this string. */
    if (len > available) return RR_ERROR;

    memcpy(c->buf+c->buf_offset, s, len);
    c->buf_offset += len;
    return RR_OK;
}

static void add_reply_object_to_list(rr_client_t *c, robj *o) {
    if (c->flags & CLIENT_CLOSE_AFTER_REPLY) return;

    if (listLength(c->reply) == 0) {
        sds s = sdsdup(o->ptr);
        listAddNodeTail(c->reply,s);
        c->replied_len += sdslen(s);
    } else {
        listNode *ln = listLast(c->reply);
        sds tail = listNodeValue(ln);

        /* Append to this object when possible. If tail == NULL it was
         * set via addDeferredMultiBulkLength(). */
        if (tail && sdslen(tail)+sdslen(o->ptr) <= PROTO_REPLY_MAX_LEN) {
            tail = sdscatsds(tail,o->ptr);
            listNodeValue(ln) = tail;
            c->replied_len += sdslen(o->ptr);
        } else {
            sds s = sdsdup(o->ptr);
            listAddNodeTail(c->reply,s);
            c->replied_len += sdslen(s);
        }
    }
}

/* This method takes responsibility on the sds. When it is no longer
 * needed it will be free'd, otherwise it ends up in a robj. */
void add_reply_sds_to_list(rr_client_t *c, sds s) {
    if (c->flags & CLIENT_CLOSE_AFTER_REPLY) {
        sdsfree(s);
        return;
    }

    c->replied_len += sdslen(s);
    if (listLength(c->reply) == 0) {
        listAddNodeTail(c->reply, s);
    } else {
        listNode *ln = listLast(c->reply);
        sds tail = listNodeValue(ln);

        /* Append to this object when possible. If tail == NULL it was
         * set via addDeferredMultiBulkLength(). */
        if (tail && sdslen(tail)+sdslen(s) <= PROTO_REPLY_MAX_LEN) {
            tail = sdscatsds(tail,s);
            listNodeValue(ln) = tail;
            sdsfree(s);
        } else {
            listAddNodeTail(c->reply, s);
        }
    }
}

void add_reply_str_to_list(rr_client_t *c, const char *s, size_t len) {
    if (c->flags & CLIENT_CLOSE_AFTER_REPLY) return;

    c->replied_len += len;
    if (listLength(c->reply) == 0) {
        sds node = sdsnewlen(s, len);
        listAddNodeTail(c->reply,node);
    } else {
        listNode *ln = listLast(c->reply);
        sds tail = listNodeValue(ln);

        /* Append to this object when possible. If tail == NULL it was
         * set via addDeferredMultiBulkLength(). */
        if (tail && sdslen(tail)+len <= PROTO_REPLY_MAX_LEN) {
            tail = sdscatlen(tail, s, len);
            listNodeValue(ln) = tail;
        } else {
            sds node = sdsnewlen(s, len);
            listAddNodeTail(c->reply, node);
        }
    }
}

void reply_add_obj(rr_client_t *c, robj *obj) {
    if (prepare_client_to_write(c) != RR_OK) return;

    /* This is an important place where we can avoid copy-on-write
     * when there is a saving child running, avoiding touching the
     * refcount field of the object if it's not needed.
     *
     * If the encoding is RAW and there is room in the static buffer
     * we'll be able to send the object to the client without
     * messing with its page. */
    if (sdsEncodedObject(obj)) {
        if (add_reply_to_buffer(c, obj->ptr, sdslen(obj->ptr)) != RR_OK)
            add_reply_object_to_list(c, obj);
    } else if (obj->encoding == OBJ_ENCODING_INT) {
        /* Optimization: if there is room in the static buffer for 32 bytes
         * (more than the max chars a 64 bit integer can take as string) we
         * avoid decoding the object and go for the lower level approach. */
        if (listLength(c->reply) == 0 && (sizeof(c->buf) - c->buf_offset) >= 32) {
            char buf[32];
            int len;

            len = ll2string(buf, sizeof(buf), (long)obj->ptr);
            if (add_reply_to_buffer(c, buf, len) == RR_OK) return;
            /* else... continue with the normal code path, but should never
             * happen actually since we verified there is room. */
        }
        obj = getDecodedObject(obj);
        if (add_reply_to_buffer(c, obj->ptr, sdslen(obj->ptr)) != RR_OK)
            add_reply_object_to_list(c, obj);
        decrRefCount(obj);
    } else {
        assert(0);
    }
}

void reply_add_str(rr_client_t *c, const char *s, size_t len) {
    if (prepare_client_to_write(c) != RR_OK) return;
    if (add_reply_to_buffer(c, s, len) != RR_OK)
        add_reply_str_to_list(c, s, len);
}

void reply_add_err_len(rr_client_t *c, const char *s, size_t len) {
    reply_add_str(c, "-ERR ", 5);
    reply_add_str(c, s, len);
    reply_add_str(c, "\r\n", 2);
}

void reply_add_err_format(rr_client_t *c, const char *fmt, ...) {
    size_t l, j;
    va_list ap;
    va_start(ap,fmt);
    sds s = sdscatvprintf(sdsempty(), fmt, ap);
    va_end(ap);
    /* Make sure there are no newlines in the string, otherwise invalid protocol
     * is emitted. */
    l = sdslen(s);
    for (j = 0; j < l; j++) {
        if (s[j] == '\r' || s[j] == '\n') s[j] = ' ';
    }
    reply_add_err_len(c, s, sdslen(s));
    sdsfree(s);
}

void reply_add_err(rr_client_t *c, const char *err) {
    reply_add_err_len(c, err, strlen(err));
}


void reply_add_status_len(rr_client_t *c, const char *s, size_t len) {
    reply_add_str(c,"+",1);
    reply_add_str(c,s,len);
    reply_add_str(c,"\r\n",2);
}

void reply_add_status(rr_client_t *c, const char *status) {
    reply_add_status_len(c, status, strlen(status));
}

/* This function will take the ownership of the sds argument */
void reply_add_sds(rr_client_t *c, sds s) {
    if (prepare_client_to_write(c) != RR_OK) {
        /* The caller expects the sds to be freed. */
        sdsfree(s);
        return;
    }
    if (add_reply_to_buffer(c, s, sdslen(s)) == RR_OK) {
        sdsfree(s);
    } else {
        /* This method free's the sds when it is no longer needed. */
        add_reply_sds_to_list(c, s);
    }
}

/* Add a long long as integer reply or bulk len / multi bulk count.
 * Basically this is used to output <prefix><long long><crlf>. */
static void reply_add_longlong_with_prefix(rr_client_t *c, long long ll, char prefix) {
    char buf[128];
    int len;

    if (prefix == '*' && ll < OBJ_SHARED_BULKHDR_LEN && ll >= 0) {
        reply_add_obj(c, shared.mbulkhdr[ll]);
        return;
    } else if (prefix == '$' && ll < OBJ_SHARED_BULKHDR_LEN && ll >= 0) {
        reply_add_obj(c, shared.bulkhdr[ll]);
        return;
    }

    buf[0] = prefix;
    len = ll2string(buf+1, sizeof(buf)-1, ll);
    buf[len+1] = '\r';
    buf[len+2] = '\n';
    reply_add_str(c, buf, len+3);
}

void reply_add_longlong(rr_client_t *c, long long ll) {
    reply_add_longlong_with_prefix(c, ll, ':');
}

void reply_add_multi_bulk_len(rr_client_t *c, long length) {
    if (length < OBJ_SHARED_BULKHDR_LEN)
        reply_add_obj(c, shared.mbulkhdr[length]);
    else
        reply_add_longlong_with_prefix(c, length, '*');
}

/* Create the length prefix of a bulk reply, example: $2234 */
void reply_add_bulk_len(rr_client_t *c, robj *obj) {
    size_t len;

    if (sdsEncodedObject(obj)) {
        len = sdslen(obj->ptr);
    } else {
        long n = (long) obj->ptr;

        /* Compute how many bytes will take this integer as a radix 10 string */
        len = 1;
        if (n < 0) {
            len++;
            n = -n;
        }
        while((n = n/10) != 0) {
            len++;
        }
    }

    if (len < OBJ_SHARED_BULKHDR_LEN)
        reply_add_obj(c,shared.bulkhdr[len]);
    else
        reply_add_longlong_with_prefix(c,len,'$');
}

/* Add a Redis Object as a bulk reply */
void reply_add_bulk_obj(rr_client_t *c, robj *obj) {
    reply_add_bulk_len(c, obj);
    reply_add_obj(c, obj);
    reply_add_obj(c, shared.crlf);
}

/* Add a C buffer as bulk reply */
void reply_add_bulk_cbuf(rr_client_t *c, const void *p, size_t len) {
    reply_add_longlong_with_prefix(c, len, '$');
    reply_add_str(c, p, len);
    reply_add_obj(c, shared.crlf);
}

/* Add sds to reply (takes ownership of sds and frees it) */
void reply_add_bulk_sds(rr_client_t *c, sds s)  {
    reply_add_sds(c, sdscatfmt(sdsempty(), "$%u\r\n",
        (unsigned long) sdslen(s)));
    reply_add_sds(c, s);
    reply_add_obj(c, shared.crlf);
}

/* Add a C nul term string as bulk reply */
void reply_add_bulk_cstr(rr_client_t *c, const char *s) {
    if (s == NULL) {
        reply_add_obj(c, shared.nullbulk);
    } else {
        reply_add_bulk_cbuf(c, s, strlen(s));
    }
}

/* Add a long long as a bulk reply */
void reply_add_bulk_longlong(rr_client_t *c, long long ll) {
    char buf[64];
    int len;

    len = ll2string(buf, 64, ll);
    reply_add_bulk_cbuf(c, buf, len);
}

/* Write data in output buffers to client. Return RR_OK if the client
 * is still valid after the call, RR_ERROR if it was freed. */
int reply_write_to_client(int fd, rr_client_t *c, int handler_installed) {
    ssize_t nwritten = 0, totwritten = 0;
    size_t objlen;
    sds o;

    while(client_has_pending_replies(c)) {
        if (c->buf_offset > 0) {
            nwritten = write(fd,c->buf+c->buf_sent_len,c->buf_offset-c->buf_sent_len);
            if (nwritten <= 0) break;
            c->buf_sent_len += nwritten;
            totwritten += nwritten;

            /* If the buffer was sent, set buf_offset to zero to continue with
             * the remainder of the reply. */
            if ((int)c->buf_sent_len == c->buf_offset) {
                c->buf_offset = 0;
                c->buf_sent_len = 0;
            }
        } else {
            o = listNodeValue(listFirst(c->reply));
            objlen = sdslen(o);

            if (objlen == 0) {
                listDelNode(c->reply,listFirst(c->reply));
                continue;
            }

            nwritten = write(fd, o + c->buf_sent_len, objlen - c->buf_sent_len);
            if (nwritten <= 0) break;
            c->buf_sent_len += nwritten;
            totwritten += nwritten;

            /* If we fully sent the object on head go to the next one */
            if (c->buf_sent_len == objlen) {
                listDelNode(c->reply,listFirst(c->reply));
                c->buf_sent_len = 0;
                c->replied_len -= objlen;
            }
        }
        /* Note that we avoid sending more than NET_MAX_WRITES_PER_EVENT
         * bytes, in a single threaded server it's a good idea to serve
         * other clients as well, even if a very large request comes from
         * super fast link that is always able to accept data (in real world
         * scenario think about 'KEYS *' against the loopback interface).
         *
         * However if we are over the maxmemory limit we ignore that and
         * just deliver as much data as it is possible to deliver. */
        if (totwritten > NET_MAX_WRITES_PER_EVENT &&
            (server.max_memory == 0 ||
             rr_get_used_memory() < server.max_memory)) break;
    }
    if (nwritten == -1) {
        if (errno == EAGAIN) {
            nwritten = 0;
        } else {
            rr_log(RR_LOG_ERROR, "Error writing to client: %s", strerror(errno));
            rr_client_free(c);
            return RR_ERROR;
        }
    }
    if (!client_has_pending_replies(c)) {
        c->buf_sent_len = 0;
        if (handler_installed) el_event_del(server.el, c->fd, RR_EV_WRITE);

        /* Close connection after entire reply has been sent. */
        if (c->flags & CLIENT_CLOSE_AFTER_REPLY) {
            rr_client_free(c);
            return RR_ERROR;
        }
    }
    return RR_OK;
}

void reply_write_callback(eventloop_t *el, int fd, void *ud, int mask) {
    UNUSED(el);
    UNUSED(mask);
    reply_write_to_client(fd, ud, 1);
}
