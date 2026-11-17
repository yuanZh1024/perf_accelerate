/*不再使用之前的 test-bucket/hello.txt 这种随意命名方式，而是尽量向 Ceph RGW 的真实数据布局靠拢，包括：
- Bucket 使用 bucket_id、marker、placement_rule。
- Data Pool 使用 Head Object 和 Tail Object。
- Head 保存 Manifest。
- Tail 保存实际对象数据。
- Bucket Index 使用 .dir.<marker> 形式的 RADOS Object。
- Index 使用 OMAP 保存对象条目。
- 增加 list，从 Bucket Index 查询对象，而不是扫描 Data Pool。
先明确一个边界：我们做的是参考 Ceph RGW 命名和核心数据组织方式的教学版，不是直接复刻 Ceph RGW 的内部序列化协议。真实 RGW 的 Manifest、Bucket Index Entry 和 Bucket 元数据都有专门的数据结构及编码格式。
*/
#include <rados/librados.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#define META_POOL  "mini.rgw.meta"
#define DATA_POOL  "mini.rgw.data"
#define INDEX_POOL "mini.rgw.index"
#define CHUNK_SIZE (4U * 1024U * 1024U)
#define MAX_STRIPES 256
#define MAX_OID 2048
#define MANIFEST_XATTR "user.mini_rgw.manifest"

typedef struct {
    rados_t cluster;
    rados_ioctx_t meta_io, data_io, shadow_io, index_io;
} mini_rgw_t;

typedef struct {
    uint64_t offset;
    uint64_t size;
    char ns[32];
    char oid[MAX_OID];
} stripe_t;

typedef struct {
    uint64_t total_size;
    unsigned int count;
    stripe_t stripes[MAX_STRIPES];
} manifest_t;

static int rgw_init(mini_rgw_t *r)
{
    int ret;
    memset(r, 0, sizeof(*r));
    ret = rados_create(&r->cluster, "client.mini-rgw");
    if (ret < 0) goto fail;
    ret = rados_conf_read_file(r->cluster, "/etc/ceph/ceph.conf");
    if (ret < 0) goto fail;
    ret = rados_conf_set(r->cluster, "keyring",
                         "/etc/ceph/ceph.client.mini-rgw.keyring");
    if (ret < 0) goto fail;
    ret = rados_connect(r->cluster);
    if (ret < 0) goto fail;
    ret = rados_ioctx_create(r->cluster, META_POOL, &r->meta_io);
    if (ret < 0) goto fail;
    ret = rados_ioctx_create(r->cluster, DATA_POOL, &r->data_io);
    if (ret < 0) goto fail;
    ret = rados_ioctx_create(r->cluster, DATA_POOL, &r->shadow_io);
    if (ret < 0) goto fail;
    ret = rados_ioctx_create(r->cluster, INDEX_POOL, &r->index_io);
    if (ret < 0) goto fail;
    rados_ioctx_set_namespace(r->meta_io, "root");
    rados_ioctx_set_namespace(r->shadow_io, "shadow");
    return 0;
fail:
    fprintf(stderr, "RGW init failed: %s\n", strerror(-ret));
    return ret;
}

static void rgw_destroy(mini_rgw_t *r)
{
    if (r->meta_io) rados_ioctx_destroy(r->meta_io);
    if (r->data_io) rados_ioctx_destroy(r->data_io);
    if (r->shadow_io) rados_ioctx_destroy(r->shadow_io);
    if (r->index_io) rados_ioctx_destroy(r->index_io);
    if (r->cluster) rados_shutdown(r->cluster);
}

static int get_marker(mini_rgw_t *r, const char *bucket, char *marker, size_t cap)
{
    char buf[512] = {0};
    int n = rados_read(r->meta_io, bucket, buf, sizeof(buf)-1, 0);
    if (n < 0) return n;
    buf[n] = '\0';
    char *p = strstr(buf, "marker=");
    if (!p || sscanf(p, "marker=%255s", marker) != 1) return -EINVAL;
    if (strlen(marker) >= cap) return -ENAMETOOLONG;
    return 0;
}

static int bucket_create(mini_rgw_t *r, const char *bucket)
{
    char marker[256], index_oid[512], meta[512];
    /* Teaching marker: zone.instance_id.bucket_id. Not a production allocator. */
    struct timeval tv; gettimeofday(&tv, NULL);
    snprintf(marker, sizeof(marker), "default.%ld.%06ld", (long)tv.tv_sec, (long)tv.tv_usec);
    snprintf(meta, sizeof(meta), "version=1\nbucket=%s\nmarker=%s\nowner=mini-user\nplacement=default-placement\nindex_shards=1\n",
             bucket, marker);
    int ret = rados_write_full(r->meta_io, bucket, meta, strlen(meta));
    if (ret < 0) return ret;
    snprintf(index_oid, sizeof(index_oid), ".dir.%s", marker);
    ret = rados_write_full(r->index_io, index_oid, "", 0);
    if (ret < 0) return ret;
    printf("Bucket created: %s marker=%s index=%s\n", bucket, marker, index_oid);
    return 0;
}

static uint64_t fnv1a64(const unsigned char *p, size_t n)
{
    uint64_t h = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= UINT64_C(1099511628211); }
    return h;
}

static int append_manifest(char *buf, size_t cap, size_t *used, const manifest_t *m)
{
    int n = snprintf(buf + *used, cap - *used,
                     "MINI-RGW-MANIFEST-V1\ntotal_size=%" PRIu64 "\nstripe_count=%u\n",
                     m->total_size, m->count);
    if (n < 0 || (size_t)n >= cap - *used) return -ENOSPC;
    *used += (size_t)n;
    for (unsigned int i = 0; i < m->count; ++i) {
        const stripe_t *s = &m->stripes[i];
        n = snprintf(buf + *used, cap - *used,
                     "stripe=%" PRIu64 ",%" PRIu64 ",%s,%s\n",
                     s->offset, s->size, s->ns, s->oid);
        if (n < 0 || (size_t)n >= cap - *used) return -ENOSPC;
        *used += (size_t)n;
    }
    return 0;
}

static int parse_manifest(const char *buf, manifest_t *m)
{
    char *copy = strdup(buf), *save = NULL, *line;
    if (!copy) return -ENOMEM;
    memset(m, 0, sizeof(*m));
    line = strtok_r(copy, "\n", &save);
    if (!line || strcmp(line, "MINI-RGW-MANIFEST-V1") != 0) { free(copy); return -EINVAL; }
    while ((line = strtok_r(NULL, "\n", &save))) {
        if (sscanf(line, "total_size=%" SCNu64, &m->total_size) == 1) continue;
        if (strncmp(line, "stripe=", 7) == 0) {
            if (m->count >= MAX_STRIPES) { free(copy); return -E2BIG; }
            stripe_t *s = &m->stripes[m->count];
            char ns[32], oid[MAX_OID];
            unsigned long long off, sz;
            if (sscanf(line + 7, "%llu,%llu,%31[^,],%2047s", &off, &sz, ns, oid) != 4) {
                free(copy); return -EINVAL;
            }
            s->offset = (uint64_t)off; s->size = (uint64_t)sz;
            snprintf(s->ns, sizeof(s->ns), "%s", ns);
            snprintf(s->oid, sizeof(s->oid), "%s", oid);
            m->count++;
        }
    }
    free(copy);
    return m->count ? 0 : -EINVAL;
}

static int write_head(mini_rgw_t *r, const char *oid, const char *data,
                      size_t len, const char *manifest, const char *etag)
{
    rados_write_op_t op = rados_create_write_op();
    if (!op) return -ENOMEM;
    const char *acl = "private";
    const char *ctype = "application/octet-stream";
    rados_write_op_write_full(op, data, len);
    rados_write_op_setxattr(op, "user.rgw.acl", acl, strlen(acl));
    rados_write_op_setxattr(op, "user.rgw.etag", etag, strlen(etag));
    rados_write_op_setxattr(op, "user.rgw.content_type", ctype, strlen(ctype));
    rados_write_op_setxattr(op, MANIFEST_XATTR, manifest, strlen(manifest));
    int ret = rados_write_op_operate(op, r->data_io, oid, NULL, LIBRADOS_OPERATION_NOFLAG);
    rados_release_write_op(op);
    return ret;
}

static int update_index(mini_rgw_t *r, const char *index_oid, const char *key,
                        uint64_t size, const char *etag)
{
    char value[256];
    snprintf(value, sizeof(value), "size=%" PRIu64 ";etag=%s", size, etag);
    const char *keys[] = { key };
    const char *vals[] = { value };
    size_t lens[] = { strlen(value) };
    rados_write_op_t op = rados_create_write_op();
    if (!op) return -ENOMEM;
    rados_write_op_omap_set(op, keys, vals, lens, 1);
    int ret = rados_write_op_operate(op, r->index_io, index_oid, NULL,
                                     LIBRADOS_OPERATION_NOFLAG);
    rados_release_write_op(op);
    return ret;
}

static int object_put(mini_rgw_t *r, const char *bucket, const char *key, const char *path)
{
    char marker[256], head_oid[MAX_OID], index_oid[512];
    int ret = get_marker(r, bucket, marker, sizeof(marker));
    if (ret < 0) { fprintf(stderr, "bucket not found; create it first\n"); return ret; }
    snprintf(head_oid, sizeof(head_oid), "%s_%s", marker, key);
    snprintf(index_oid, sizeof(index_oid), ".dir.%s", marker);

    FILE *fp = fopen(path, "rb");
    if (!fp) { perror("fopen"); return -errno; }
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -EIO; }
    long end = ftell(fp);
    if (end < 0) { fclose(fp); return -EIO; }
    rewind(fp);
    uint64_t total = (uint64_t)end;
    unsigned char *all = malloc(total ? (size_t)total : 1);
    if (!all) { fclose(fp); return -ENOMEM; }
    if (total && fread(all, 1, (size_t)total, fp) != total) { fclose(fp); free(all); return -EIO; }
    fclose(fp);

    manifest_t m; memset(&m, 0, sizeof(m)); m.total_size = total;
    uint64_t left = total, offset = 0;
    unsigned int stripe_no = 0;
    char upload_id[64];
    struct timeval upload_tv; gettimeofday(&upload_tv, NULL);
    snprintf(upload_id, sizeof(upload_id), "%lx_%lx_%x", (unsigned long)upload_tv.tv_sec, (unsigned long)upload_tv.tv_usec, (unsigned)getpid());
    do {
        if (m.count >= MAX_STRIPES) { free(all); return -E2BIG; }
        stripe_t *s = &m.stripes[m.count++];
        s->offset = offset;
        s->size = left > CHUNK_SIZE ? CHUNK_SIZE : left;
        if (stripe_no == 0) {
            snprintf(s->ns, sizeof(s->ns), "default");
            snprintf(s->oid, sizeof(s->oid), "%s", head_oid);
        } else {
            snprintf(s->ns, sizeof(s->ns), "shadow");
            snprintf(s->oid, sizeof(s->oid), "%s__shadow_%s_%u", head_oid, upload_id, stripe_no);
            ret = rados_write_full(r->shadow_io, s->oid, (const char *)all + offset, s->size);
            if (ret < 0) { free(all); return ret; }
        }
        offset += s->size; left -= s->size; stripe_no++;
    } while (left > 0);

    char manifest_buf[65536]; size_t used = 0;
    ret = append_manifest(manifest_buf, sizeof(manifest_buf), &used, &m);
    if (ret < 0) { free(all); return ret; }
    uint64_t hash = fnv1a64(all, (size_t)total);
    char etag[32]; snprintf(etag, sizeof(etag), "%016" PRIx64, hash);

    /* Head is written after all tail objects: it is the visibility/commit point in this lab. */
    ret = write_head(r, head_oid, (const char *)all, (size_t)m.stripes[0].size,
                     manifest_buf, etag);
    free(all);
    if (ret < 0) return ret;
    ret = update_index(r, index_oid, key, total, etag);
    if (ret < 0) return ret;
    printf("PUT success: %s/%s size=%" PRIu64 " stripes=%u head=%s\n",
           bucket, key, total, m.count, head_oid);
    return 0;
}

static int object_get(mini_rgw_t *r, const char *bucket, const char *key, const char *path)
{
    char marker[256], head_oid[MAX_OID];
    int ret = get_marker(r, bucket, marker, sizeof(marker));
    if (ret < 0) return ret;
    snprintf(head_oid, sizeof(head_oid), "%s_%s", marker, key);
    char manifest_buf[65536];
    ret = rados_getxattr(r->data_io, head_oid, MANIFEST_XATTR,
                         manifest_buf, sizeof(manifest_buf)-1);
    if (ret < 0) { fprintf(stderr, "read manifest failed: %s\n", strerror(-ret)); return ret; }
    manifest_buf[ret] = '\0';
    manifest_t m;
    ret = parse_manifest(manifest_buf, &m);
    if (ret < 0) return ret;
    FILE *fp = fopen(path, "wb");
    if (!fp) return -errno;
    unsigned char *buf = malloc(CHUNK_SIZE);
    if (!buf) { fclose(fp); return -ENOMEM; }
    for (unsigned int i = 0; i < m.count; ++i) {
        stripe_t *s = &m.stripes[i];
        rados_ioctx_t io = (strcmp(s->ns, "shadow") == 0) ? r->shadow_io : r->data_io;
        uint64_t pos = 0;
        while (pos < s->size) {
            size_t want = (s->size - pos > CHUNK_SIZE) ? CHUNK_SIZE : (size_t)(s->size - pos);
            int n = rados_read(io, s->oid, (char *)buf, want, pos);
            if (n < 0) { ret = n; goto out; }
            if (n == 0) { ret = -EIO; goto out; }
            if (fwrite(buf, 1, (size_t)n, fp) != (size_t)n) { ret = -EIO; goto out; }
            pos += (uint64_t)n;
        }
    }
    ret = 0;
    printf("GET success: %s/%s size=%" PRIu64 " stripes=%u\n", bucket, key, m.total_size, m.count);
out:
    free(buf); fclose(fp); return ret;
}

static int object_list(mini_rgw_t *r, const char *bucket)
{
    char marker[256], index_oid[512];
    int ret = get_marker(r, bucket, marker, sizeof(marker));
    if (ret < 0) return ret;
    snprintf(index_oid, sizeof(index_oid), ".dir.%s", marker);
    rados_read_op_t op = rados_create_read_op();
    if (!op) return -ENOMEM;
    rados_omap_iter_t iter = NULL;
    unsigned char more = 0;
    int prval = 0;
    rados_read_op_omap_get_vals2(op, "", "", 10000, &iter, &more, &prval);
    ret = rados_read_op_operate(op, r->index_io, index_oid, LIBRADOS_OPERATION_NOFLAG);
    rados_release_read_op(op);
    if (ret < 0) return ret;
    if (prval < 0) { if (iter) rados_omap_get_end(iter); return prval; }
    printf("OBJECT\tSIZE\tETAG\n");
    char *k = NULL, *v = NULL; size_t kl = 0, vl = 0;
    while (iter && rados_omap_get_next2(iter, &k, &v, &kl, &vl) == 0 && k) {
        uint64_t size = 0; char etag[80] = "-";
        if (v) {
            char value[512]; size_t cp = vl < sizeof(value)-1 ? vl : sizeof(value)-1;
            memcpy(value, v, cp); value[cp] = '\0';
            char *p = strstr(value, "size="); if (p) size = strtoull(p+5, NULL, 10);
            p = strstr(value, "etag="); if (p) snprintf(etag, sizeof(etag), "%s", p+5);
        }
        printf("%.*s\t%" PRIu64 "\t%s\n", (int)kl, k, size, etag);
    }
    if (iter) rados_omap_get_end(iter);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage:\n  %s bucket-create <bucket>\n  %s put <bucket> <key> <file>\n  %s get <bucket> <key> <file>\n  %s list <bucket>\n", argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }
    mini_rgw_t rgw;
    int ret = rgw_init(&rgw);
    if (ret < 0) return 1;
    if (!strcmp(argv[1], "bucket-create") && argc == 3)
        ret = bucket_create(&rgw, argv[2]);
    else if (!strcmp(argv[1], "put") && argc == 5)
        ret = object_put(&rgw, argv[2], argv[3], argv[4]);
    else if (!strcmp(argv[1], "get") && argc == 5)
        ret = object_get(&rgw, argv[2], argv[3], argv[4]);
    else if (!strcmp(argv[1], "list") && argc == 3)
        ret = object_list(&rgw, argv[2]);
    else { fprintf(stderr, "Invalid command\n"); ret = -EINVAL; }
    rgw_destroy(&rgw);
    if (ret < 0) fprintf(stderr, "Operation failed: %s (%d)\n", strerror(-ret), ret);
    return ret < 0 ? 1 : 0;
}

