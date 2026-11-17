
#include <rados/librados.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#define META_POOL  "mini.rgw.meta"
#define DATA_POOL  "mini.rgw.data"
#define INDEX_POOL "mini.rgw.index"

typedef struct {
    rados_t cluster;

    rados_ioctx_t meta_io;
    rados_ioctx_t data_io;
    rados_ioctx_t index_io;
} mini_rgw_t;


/* =====================================================
 * 1. 初始化 RGW：连接 Ceph，打开三个 Pool
 * ===================================================== */

static int rgw_init(mini_rgw_t *rgw)
{
    int ret;

    memset(rgw, 0, sizeof(*rgw));

    // 创建集群句柄, 第二个参数是用户身份名称，也就是以xxx用户去访问rados
    ret = rados_create(&rgw->cluster, "mini-rgw");
    if (ret < 0) goto error;

    // 读取 Ceph 配置
    ret = rados_conf_read_file(
        rgw->cluster, "/etc/ceph/ceph.conf");
    if (ret < 0) goto error;

    // 连接 Ceph
    ret = rados_connect(rgw->cluster);
    if (ret < 0) goto error;

    // 打开 Metadata Pool
    ret = rados_ioctx_create(
        rgw->cluster, META_POOL, &rgw->meta_io);
    if (ret < 0) goto error;

    // 打开 Data Pool
    ret = rados_ioctx_create(
        rgw->cluster, DATA_POOL, &rgw->data_io);
    if (ret < 0) goto error;

    // 打开 Index Pool
    ret = rados_ioctx_create(
        rgw->cluster, INDEX_POOL, &rgw->index_io);
    if (ret < 0) goto error;

    printf("Connected to Ceph successfully\n");
    return 0;

error:
    fprintf(stderr, "RGW init failed: %s\n", strerror(-ret));
    return ret;
}


static void rgw_destroy(mini_rgw_t *rgw)
{
    rados_ioctx_destroy(rgw->meta_io);
    rados_ioctx_destroy(rgw->data_io);
    rados_ioctx_destroy(rgw->index_io);

    rados_shutdown(rgw->cluster);
}


/* =====================================================
 * 2. 创建 Bucket
 * ===================================================== */

static int bucket_create(mini_rgw_t *rgw, const char *bucket)
{
    char bucket_oid[512];

    snprintf(bucket_oid, sizeof(bucket_oid),
             "bucket.%s", bucket);

    // 1. 创建 Bucket 元数据对象
    const char *metadata = "bucket_created";

    int ret = rados_write_full(
        rgw->meta_io,
        bucket_oid,
        metadata,
        strlen(metadata));

    if (ret < 0) {
        fprintf(stderr, "create bucket metadata failed\n");
        return ret;
    }

    // 2. 创建 Bucket Index 对象
    // 此时 OMAP 为空，后续 PUT 时再添加索引
    ret = rados_write_full(
        rgw->index_io,
        bucket_oid,
        "",
        0);

    if (ret < 0) {
        fprintf(stderr, "create bucket index failed\n");
        return ret;
    }

    printf("Bucket created: %s\n", bucket);

    return 0;
}


/* =====================================================
 * 3. 上传 Object
 * ===================================================== */

static int object_put(
    mini_rgw_t *rgw,
    const char *bucket,
    const char *key,
    const char *filename)
{
    char object_oid[1024];
    char bucket_index_oid[512];

    // 构造 RADOS Object 名称
    snprintf(object_oid, sizeof(object_oid),
             "%s/%s", bucket, key);

    snprintf(bucket_index_oid, sizeof(bucket_index_oid),
             "bucket.%s", bucket);

    // -------------------------------------------------
    // Step 1: 从本地文件读取数据
    // -------------------------------------------------

    FILE *fp = fopen(filename, "rb");

    if (!fp) {
        perror("fopen");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    if (file_size < 0) {
        fclose(fp);
        return -1;
    }

    size_t size = (size_t)file_size;

    char *buffer = malloc(size ? size : 1);

    if (!buffer) {
        fclose(fp);
        return -1;
    }

    if (fread(buffer, 1, size, fp) != size) {
        fclose(fp);
        free(buffer);
        return -1;
    }

    fclose(fp);

    // -------------------------------------------------
    // Step 2: 写入 RADOS Object 的 Data
    // -------------------------------------------------

    printf("PUT data: %s, size=%zu\n",
           object_oid, size);

    int ret = rados_write_full(
        rgw->data_io,
        object_oid,
        buffer,
        size);

    if (ret < 0) {
        fprintf(stderr, "write data failed: %s\n",
                strerror(-ret));
        free(buffer);
        return ret;
    }

    free(buffer);

    // -------------------------------------------------
    // Step 3: 写入 Object XATTR
    // -------------------------------------------------

    const char *content_type = "application/octet-stream";
    const char *acl = "private";

    ret = rados_setxattr(
        rgw->data_io,
        object_oid,
        "user.rgw.content_type",
        content_type,
        strlen(content_type));

    if (ret < 0) return ret;

    ret = rados_setxattr(
        rgw->data_io,
        object_oid,
        "user.rgw.acl",
        acl,
        strlen(acl));

    if (ret < 0) return ret;

    // -------------------------------------------------
    // Step 4: 更新 Bucket Index 的 OMAP
    // -------------------------------------------------

    char size_str[64];

    snprintf(size_str, sizeof(size_str),
             "%" PRIu64, (uint64_t)size);

    const char *keys[] = { key };
    const char *vals[] = { size_str };
    size_t lens[] = { strlen(size_str) };

    rados_write_op_t op = rados_create_write_op();

    if (!op) return -1;
    
    // 这里是直接写omap
    rados_write_op_omap_set(
        op,
        keys,
        vals,
        lens,
        1);

    ret = rados_write_op_operate(
        op,
        rgw->index_io,
        bucket_index_oid,
        NULL,
        LIBRADOS_OPERATION_NOFLAG);

    rados_release_write_op(op);

    if (ret < 0) {
        fprintf(stderr, "update bucket index failed: %s\n",
                strerror(-ret));
        return ret;
    }

    printf("PUT success: %s/%s\n", bucket, key);

    return 0;
}


/* =====================================================
 * 4. 下载 Object
 * ===================================================== */

static int object_get(
    mini_rgw_t *rgw,
    const char *bucket,
    const char *key,
    const char *filename)
{
    char object_oid[1024];

    snprintf(object_oid, sizeof(object_oid),
             "%s/%s", bucket, key);

    // -------------------------------------------------
    // Step 1: 获取 RADOS Object 的大小
    // -------------------------------------------------

    uint64_t object_size = 0;

    int ret = rados_stat(
        rgw->data_io,
        object_oid,
        &object_size,
        NULL);

    if (ret < 0) {
        fprintf(stderr, "object not found: %s\n",
                object_oid);
        return ret;
    }

    // -------------------------------------------------
    // Step 2: 从 RADOS Object 读取数据
    // -------------------------------------------------

    char *buffer = malloc(object_size ? object_size : 1);

    if (!buffer) return -1;

    ret = rados_read(
        rgw->data_io,
        object_oid,
        buffer,
        object_size,
        0);

    if (ret < 0) {
        fprintf(stderr, "read object failed\n");
        free(buffer);
        return ret;
    }

    // -------------------------------------------------
    // Step 3: 写入本地文件
    // -------------------------------------------------

    FILE *fp = fopen(filename, "wb");

    if (!fp) {
        perror("fopen");
        free(buffer);
        return -1;
    }

    if (fwrite(buffer, 1, ret, fp) != (size_t)ret) {
        fclose(fp);
        free(buffer);
        return -1;
    }

    fclose(fp);
    free(buffer);

    printf("GET success: %s/%s, size=%d\n",
           bucket, key, ret);

    return 0;
}


/* =====================================================
 * 5. 命令行入口
 * ===================================================== */

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage:\n");
        printf("  %s bucket-create <bucket>\n", argv[0]);
        printf("  %s put <bucket> <key> <file>\n", argv[0]);
        printf("  %s get <bucket> <key> <file>\n", argv[0]);
        return 1;
    }

    mini_rgw_t rgw;

    if (rgw_init(&rgw) < 0) {
        return 1;
    }

    int ret = -1;

    if (strcmp(argv[1], "bucket-create") == 0) {

        ret = bucket_create(&rgw, argv[2]);

    } else if (strcmp(argv[1], "put") == 0 && argc == 5) {

        ret = object_put(
            &rgw, argv[2], argv[3], argv[4]);

    } else if (strcmp(argv[1], "get") == 0 && argc == 5) {

        ret = object_get(
            &rgw, argv[2], argv[3], argv[4]);

    } else {

        printf("Invalid command\n");
    }

    rgw_destroy(&rgw);

    return ret < 0 ? 1 : 0;
}
