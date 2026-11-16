/*
 * server.c - minimal HTTP/1.1 server, one thread per connection.
 *
 * Supported API:
 *   PUT    /v1/objects/<key>   store an object (Content-Length required)
 *   GET    /v1/objects/<key>   read an object
 *   HEAD   /v1/objects/<key>   object metadata
 *   DELETE /v1/objects/<key>   remove an object
 *   GET    /v1/objects?prefix= list objects
 *   GET    /v1/stats           cluster / disk usage
 *   GET    /v1/health          liveness probe
 */
#include "server.h"

#include "http_parse.h"
#include "net.h"
#include "util.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_HEAD_BYTES 16384
#define SOCK_TIMEOUT 60

typedef struct {
    netfd_t fd;
    store_t *store;
    char peer[64];
    char *buf;
    size_t len;
    size_t cap;
    size_t pos; /* consumed bytes (head + body already buffered) */
} conn_t;

static volatile sig_atomic_t g_stop = 0;
static netfd_t g_listen = NET_FD_BAD;
static int g_max_conns = 128;
static int g_conns = 0;
static pthread_mutex_t g_conn_lock = PTHREAD_MUTEX_INITIALIZER;

void server_request_stop(void)
{
    g_stop = 1;
}

/* ------------------------------------------------------------------ */
/* low level output                                                    */
/* ------------------------------------------------------------------ */

static int send_all(conn_t *c, const void *buf, size_t n)
{
    const char *p = (const char *)buf;
    size_t done = 0;

    while (done < n) {
        long r = net_write(c->fd, p + done, n - done);
        if (r <= 0)
            return -1;
        done += (size_t)r;
    }
    return 0;
}

static void http_date(char *dst, size_t cap)
{
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    strftime(dst, cap, "%a, %d %b %Y %H:%M:%S GMT", &tmv);
}

static const char *reason(int code)
{
    switch (code) {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default: return "Unknown";
    }
}

static int send_head(conn_t *c, int code, const char *ctype, unsigned long long len, const char *extra)
{
    char hdr[1024];
    char date[64];
    int n;

    http_date(date, sizeof(date));
    n = snprintf(hdr, sizeof(hdr),
                 "HTTP/1.1 %d %s\r\n"
                 "Server: obsd/1.0\r\n"
                 "Date: %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %llu\r\n"
                 "Connection: close\r\n"
                 "%s"
                 "\r\n",
                 code, reason(code), date, ctype, len, extra ? extra : "");
    if (n < 0 || (size_t)n >= sizeof(hdr))
        return -1;
    return send_all(c, hdr, (size_t)n);
}

static int send_json(conn_t *c, int code, const char *body, size_t len)
{
    if (send_head(c, code, "application/json", (unsigned long long)len, NULL) != 0)
        return -1;
    return send_all(c, body, len);
}

static int send_error(conn_t *c, int code, const char *msg)
{
    char body[512];
    int n = snprintf(body, sizeof(body), "{\"error\":\"%s\",\"status\":%d}\n", msg, code);

    if (n < 0)
        return -1;
    return send_json(c, code, body, (size_t)n);
}

/* ------------------------------------------------------------------ */
/* request body                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    conn_t *c;
    uint64_t remaining;
} body_t;

static int body_read(void *ctx, void *buf, size_t n, size_t *got)
{
    body_t *b = (body_t *)ctx;
    size_t want = n;
    size_t done = 0;

    if ((uint64_t)want > b->remaining)
        want = (size_t)b->remaining;

    while (done < want) {
        if (b->c->pos < b->c->len) {
            size_t avail = b->c->len - b->c->pos;
            size_t take = avail < (want - done) ? avail : (want - done);
            memcpy((char *)buf + done, b->c->buf + b->c->pos, take);
            b->c->pos += take;
            done += take;
        } else {
            long r = net_read(b->c->fd, (char *)buf + done, want - done);
            if (r <= 0)
                return -1;
            done += (size_t)r;
        }
    }
    b->remaining -= done;
    *got = done;
    return 0;
}

/* ------------------------------------------------------------------ */
/* handlers                                                            */
/* ------------------------------------------------------------------ */

static int h_health(conn_t *c)
{
    static const char body[] = "{\"status\":\"ok\"}\n";
    return send_json(c, 200, body, sizeof(body) - 1);
}

static int h_stats(conn_t *c)
{
    store_stats_t s;
    int k = 0, m = 0;
    uint32_t shard_size = 0;
    sbuf_t sb;
    int i, rc;

    if (store_stats(c->store, &s) != STORE_OK)
        return send_error(c, 500, "cannot scan data directory");

    store_geometry(c->store, &k, &m, &shard_size);

    sbuf_init(&sb);
    sbuf_printf(&sb, "{\"objects\":%llu,\"shard_files\":%llu,\"shard_bytes\":%llu,",
                (unsigned long long)s.objects, (unsigned long long)s.shard_files,
                (unsigned long long)s.shard_bytes);
    sbuf_printf(&sb, "\"ec\":\"%d+%d\",\"shard_size\":%u,\"stripe_size\":%llu,\"shards\":{",
                k, m, shard_size, (unsigned long long)shard_size * (uint64_t)k);
    for (i = 0; i < k + m; i++)
        sbuf_printf(&sb, "%s\"d%d\":%llu", i ? "," : "", i, (unsigned long long)s.shards[i]);
    sbuf_puts(&sb, "}}\n");

    rc = send_json(c, 200, sb.p, sb.len);
    sbuf_free(&sb);
    return rc;
}

static int h_list(conn_t *c, const http_req_t *req)
{
    sbuf_t sb;
    char prefix[KEY_MAX + 1];
    int rc;

    sbuf_init(&sb);
    if (http_query_get(req->query, "prefix", prefix, sizeof(prefix)) != 0)
        prefix[0] = '\0';

    rc = store_list(c->store, prefix, &sb);
    if (rc != STORE_OK) {
        sbuf_free(&sb);
        return send_error(c, 500, "cannot list objects");
    }
    rc = send_json(c, 200, sb.p, sb.len);
    sbuf_free(&sb);
    return rc;
}

static int h_put(conn_t *c, const http_req_t *req, const char *key)
{
    body_t body;
    obj_src_t src;
    uint32_t etag = 0;
    int rc;
    char out[512];
    int n;

    if (!key_valid(key))
        return send_error(c, 400, "invalid key");

    if (!req->has_content_length)
        return send_error(c, 411, "content-length required");

    if (req->expect_continue && send_all(c, "HTTP/1.1 100 Continue\r\n\r\n", 25) != 0)
        return -1;

    body.c = c;
    body.remaining = req->content_length;
    src.read = body_read;
    src.ctx = &body;

    rc = store_put(c->store, key, req->content_length, &src, &etag);
    switch (rc) {
    case STORE_OK:
        n = snprintf(out, sizeof(out), "{\"key\":\"%s\",\"size\":%llu,\"etag\":\"%08x\"}\n", key,
                     (unsigned long long)req->content_length, etag);
        return send_json(c, 200, out, (size_t)n);
    case STORE_ERR_CLIENT:
        return send_error(c, 400, "request body shorter than content-length");
    case STORE_ERR_INVALID:
        return send_error(c, 400, "invalid key or object too large");
    default:
        return send_error(c, 500, store_errstr(rc));
    }
}

typedef struct {
    conn_t *c;
    int sent; /* response headers already on the wire */
} sink_ctx_t;

static int sink_meta(void *ctx, const obj_meta_t *m)
{
    sink_ctx_t *s = (sink_ctx_t *)ctx;
    char extra[160];

    snprintf(extra, sizeof(extra), "ETag: \"%08x\"\r\nX-EC-Scheme: %d+%d\r\nX-Object-Size: %llu\r\n",
             m->etag, m->k, m->m, (unsigned long long)m->size);
    if (send_head(s->c, 200, "application/octet-stream", m->size, extra) != 0)
        return -1;
    s->sent = 1;
    return 0;
}

static int sink_data(void *ctx, const void *buf, size_t n)
{
    sink_ctx_t *s = (sink_ctx_t *)ctx;
    return send_all(s->c, buf, n);
}

static int h_get(conn_t *c, const char *key)
{
    obj_sink_t sink;
    sink_ctx_t sc;
    int rc;

    if (!key_valid(key))
        return send_error(c, 400, "invalid key");

    memset(&sc, 0, sizeof(sc));
    sc.c = c;
    sink.on_meta = sink_meta;
    sink.on_data = sink_data;
    sink.ctx = &sc;

    rc = store_get(c->store, key, &sink);
    if (rc == STORE_OK)
        return 0;

    /* Once the 200 and part of the body are on the wire we cannot send a
     * second response: drop the connection so the client sees a short read
     * instead of silently truncated data. */
    if (sc.sent) {
        log_error("aborting response for '%s': %s", key, store_errstr(rc));
        return -1;
    }

    switch (rc) {
    case STORE_ERR_NOTFOUND:
        return send_error(c, 404, "object not found");
    case STORE_ERR_INVALID:
        return send_error(c, 400, "invalid key");
    case STORE_ERR_SHORT:
        return send_error(c, 503, "not enough healthy shards to rebuild the object");
    case STORE_ERR_CORRUPT:
        return send_error(c, 500, "object data is corrupt beyond repair");
    default:
        return send_error(c, 500, store_errstr(rc));
    }
}

static int h_head(conn_t *c, const char *key)
{
    obj_meta_t m;
    char extra[160];
    int rc;

    if (!key_valid(key))
        return send_error(c, 400, "invalid key");

    meta_init(&m);
    rc = store_meta_get(c->store, key, &m);
    if (rc != STORE_OK) {
        if (rc == STORE_ERR_NOTFOUND)
            return send_error(c, 404, "object not found");
        return send_error(c, 500, store_errstr(rc));
    }
    snprintf(extra, sizeof(extra), "ETag: \"%08x\"\r\nX-EC-Scheme: %d+%d\r\nX-Stripes: %u\r\n",
             m.etag, m.k, m.m, m.num_stripes);
    rc = send_head(c, 200, "application/octet-stream", m.size, extra);
    meta_free(&m);
    return rc;
}

static int h_delete(conn_t *c, const char *key)
{
    int rc;

    if (!key_valid(key))
        return send_error(c, 400, "invalid key");

    rc = store_delete(c->store, key);
    if (rc == STORE_ERR_NOTFOUND)
        return send_error(c, 404, "object not found");
    if (rc != STORE_OK)
        return send_error(c, 500, store_errstr(rc));
    return send_head(c, 204, "text/plain", 0, NULL);
}

static int h_root(conn_t *c)
{
    static const char body[] =
        "obs-ec-store: erasure coded object store\n"
        "\n"
        "  PUT    /v1/objects/<key>\n"
        "  GET    /v1/objects/<key>\n"
        "  HEAD   /v1/objects/<key>\n"
        "  DELETE /v1/objects/<key>\n"
        "  GET    /v1/objects?prefix=<p>\n"
        "  GET    /v1/stats\n"
        "  GET    /v1/health\n";
    if (send_head(c, 200, "text/plain; charset=utf-8", sizeof(body) - 1, NULL) != 0)
        return -1;
    return send_all(c, body, sizeof(body) - 1);
}

/* ------------------------------------------------------------------ */
/* dispatch                                                            */
/* ------------------------------------------------------------------ */

static int dispatch(conn_t *c, http_req_t *req)
{
    const char *path = req->path;
    const char *key;
    int is_get = strcmp(req->method, "GET") == 0;
    int is_head = strcmp(req->method, "HEAD") == 0;

    if (strcmp(path, "/") == 0 && is_get)
        return h_root(c);
    if (strcmp(path, "/v1/health") == 0)
        return is_get ? h_health(c) : send_error(c, 405, "method not allowed");
    if (strcmp(path, "/v1/stats") == 0)
        return is_get ? h_stats(c) : send_error(c, 405, "method not allowed");
    if (strcmp(path, "/v1/objects") == 0 || strcmp(path, "/v1/objects/") == 0)
        return is_get ? h_list(c, req) : send_error(c, 405, "method not allowed");

    if (strncmp(path, "/v1/objects/", 12) != 0)
        return send_error(c, 404, "unknown endpoint");

    key = path + 12;
    if (strcmp(req->method, "PUT") == 0)
        return h_put(c, req, key);
    if (is_get)
        return h_get(c, key);
    if (is_head)
        return h_head(c, key);
    if (strcmp(req->method, "DELETE") == 0)
        return h_delete(c, key);
    return send_error(c, 405, "method not allowed");
}

/* ------------------------------------------------------------------ */
/* connection handling                                                 */
/* ------------------------------------------------------------------ */

static int read_head(conn_t *c, http_req_t *req, size_t *head_len)
{
    for (;;) {
        int r = http_parse_head(c->buf, c->len, req, head_len);
        long n;

        if (r == 1)
            return 0;
        if (r < 0)
            return -1;
        if (c->len >= MAX_HEAD_BYTES)
            return -1;
        if (c->cap < MAX_HEAD_BYTES) {
            size_t cap = c->cap ? c->cap * 2 : 2048;
            char *nb;
            if (cap > MAX_HEAD_BYTES)
                cap = MAX_HEAD_BYTES;
            nb = (char *)realloc(c->buf, cap);
            if (nb == NULL)
                return -1;
            c->buf = nb;
            c->cap = cap;
        }
        n = net_read(c->fd, c->buf + c->len, c->cap - c->len);
        if (n <= 0)
            return -1;
        c->len += (size_t)n;
    }
}

static void *conn_thread(void *arg)
{
    conn_t *c = (conn_t *)arg;
    http_req_t req;
    size_t head_len = 0;
    int rc;

    net_set_timeout(c->fd, SOCK_TIMEOUT);
    net_set_nodelay(c->fd);

    if (read_head(c, &req, &head_len) != 0) {
        send_error(c, 400, "malformed request");
        goto done;
    }
    c->pos = head_len;

    rc = dispatch(c, &req);
    log_info("%s %s %s%s%s -> %s", c->peer, req.method, req.path,
             req.query[0] ? "?" : "", req.query, rc == 0 ? "done" : "error");

done:
    if (c->buf)
        free(c->buf);
    net_close(c->fd);
    pthread_mutex_lock(&g_conn_lock);
    g_conns--;
    pthread_mutex_unlock(&g_conn_lock);
    free(c);
    return NULL;
}

int server_run(store_t *st, const char *host, int port, int max_conns)
{
    g_max_conns = max_conns > 0 ? max_conns : 128;

    if (net_init() != 0) {
        log_error("cannot initialise the socket layer");
        return -1;
    }

    g_listen = net_listen(host, port, 256);
    if (g_listen == NET_FD_BAD) {
        log_error("cannot listen on %s:%d", host && *host ? host : "0.0.0.0", port);
        return -1;
    }

    log_info("listening on %s:%d", host && *host ? host : "0.0.0.0", port);

    while (!g_stop) {
        char peer[64] = "";
        netfd_t fd = net_accept(g_listen, peer, sizeof(peer));
        conn_t *c;
        pthread_t th;

        if (fd == NET_FD_BAD) {
            if (g_stop)
                break;
            continue; /* interrupted by a signal, or a transient error */
        }

        pthread_mutex_lock(&g_conn_lock);
        if (g_conns >= g_max_conns) {
            static const char busy[] =
                "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            pthread_mutex_unlock(&g_conn_lock);
            log_warn("connection limit reached, rejecting client");
            net_write(fd, busy, sizeof(busy) - 1);
            net_close(fd);
            continue;
        }
        g_conns++;
        pthread_mutex_unlock(&g_conn_lock);

        c = (conn_t *)calloc(1, sizeof(*c));
        if (c == NULL) {
            net_close(fd);
            pthread_mutex_lock(&g_conn_lock);
            g_conns--;
            pthread_mutex_unlock(&g_conn_lock);
            continue;
        }
        c->fd = fd;
        c->store = st;
        snprintf(c->peer, sizeof(c->peer), "%s", peer);

        if (pthread_create(&th, NULL, conn_thread, c) != 0) {
            log_error("cannot create connection thread");
            net_close(fd);
            free(c);
            pthread_mutex_lock(&g_conn_lock);
            g_conns--;
            pthread_mutex_unlock(&g_conn_lock);
            continue;
        }
        pthread_detach(th);
    }

    net_close(g_listen);
    g_listen = NET_FD_BAD;
    log_info("server stopped");
    return 0;
}
