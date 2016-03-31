#include "rr_server.h"
#include "rr_logging.h"
#include "rr_rhino_rox.h"
#include "rr_malloc.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>

extern struct rr_server_t server;

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

void reply_add_err(rr_client_t *c, const char *err) {
    reply_add_err_len(c, err, strlen(err));
}

void reply_add_sds(rr_client_t *c, sds s) {
    if (prepare_client_to_write(c) != RR_OK) {
        /* The caller expects the sds to be free'd. */
        sdsfree(s);
        return;
    }
    if (add_reply_to_buffer(c, s, sdslen(s)) == RR_OK) {
        sdsfree(s);
    } else {
        /* This method free's the sds when it is no longer needed. */
        add_reply_sds_to_list(c,s);
    }
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
