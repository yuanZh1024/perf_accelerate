#include "rbd_metadata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/**
 * 拼接前缀与名称，构造 RADOS 对象 ID，并检查是否被截断。
 *
 * @param out    输出缓冲区
 * @param len    out 的容量
 * @param prefix 前缀，例如 "rbd_id." 或 "rbd_header."
 * @param name   后缀，例如 Image 名称或 Image ID
 * @return 0 成功；-ENAMETOOLONG 缓冲区不足
 */
static int build_oid(
    char *out,
    size_t len,
    const char *prefix,
    const char *name)
{
    int n = snprintf(out, len, "%s%s", prefix, name);

    /* snprintf 返回“本来需要写多少字节”，>= len 即发生截断 */
    if (n < 0 || (size_t)n >= len) {
        return -ENAMETOOLONG;
    }

    return 0;
}

/**
 * 生成 16 位十六进制 Image ID（8 字节随机数据，每个字节转成 2 个 hex 字符）。
 * 长度与字符集与官方 RBD 的 image id 风格一致，但这里是本地随机生成，
 * 不经过任何 OSD 类（cls_rbd）协商。
 *
 * @param id  输出缓冲区，至少 17 字节
 * @param len id 的容量
 * @return 0 成功；-EINVAL 容量不足；-errno / -EIO 读取 /dev/urandom 失败
 */
int rbd_generate_image_id(char *id, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char random_bytes[8];

    if (len < 17) {
        return -EINVAL;
    }

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -errno;
    }

    ssize_t n = read(fd, random_bytes, sizeof(random_bytes));
    close(fd);

    if (n != sizeof(random_bytes)) {
        return -EIO;
    }

    /* 高 4 位与低 4 位分别映射成一个十六进制字符 */
    for (int i = 0; i < 8; i++) {
        id[i * 2] = hex[random_bytes[i] >> 4];
        id[i * 2 + 1] = hex[random_bytes[i] & 0x0f];
    }

    id[16] = '\0';

    return 0;
}

/**
 * 创建 Header 对象，并在同一个写操作中填充它的 OMAP。
 *
 * 这里是我们自定义的简化 Header schema，不是官方 RBD Format 2 的
 * 完整 on-disk 编码；官方实现由 cls_rbd 在 OSD 侧维护一致性。
 *
 * OMAP 键值含义：
 *   size           Image 逻辑大小（字节，十进制字符串）
 *   order          对象大小指数
 *   object_prefix  Data Object 的公共前缀
 *   features       特性位图（当前为 0）
 *   stripe_unit    条带单元（默认 = object_size）
 *   stripe_count   条带数量（默认 = 1）
 *
 * @param client     已初始化的 RADOS 客户端
 * @param header_oid Header 对象 ID，形如 "rbd_header.<id>"
 * @param image      Image 描述结构（只读）
 * @return 0 成功；-ENOMEM 创建 op 失败；其他负数为 RADOS 错误码
 */
static int create_header(
    rados_client_t *client,
    const char *header_oid,
    rbd_image_t *image)
{
    /* OMAP 的 value 统一以 ASCII 十进制字符串保存，便于 rados CLI 直接查看 */
    char size[32];
    char order[16];
    char features[32];
    char stripe_unit[32];
    char stripe_count[32];

    snprintf(size, sizeof(size), "%llu",
             (unsigned long long)image->size);

    snprintf(order, sizeof(order), "%u", image->order);

    snprintf(features, sizeof(features), "%llu",
             (unsigned long long)image->features);

    snprintf(stripe_unit, sizeof(stripe_unit), "%llu",
             (unsigned long long)image->stripe_unit);

    snprintf(stripe_count, sizeof(stripe_count), "%llu",
             (unsigned long long)image->stripe_count);

    /* 三个数组按位置一一对应，最后一项是键值数量 */
    const char *keys[] = {
        "size",
        "order",
        "object_prefix",
        "features",
        "stripe_unit",
        "stripe_count"
    };

    const char *values[] = {
        size,
        order,
        image->object_prefix,
        features,
        stripe_unit,
        stripe_count
    };

    size_t lengths[] = {
        strlen(size),
        strlen(order),
        strlen(image->object_prefix),
        strlen(features),
        strlen(stripe_unit),
        strlen(stripe_count)
    };

    rados_write_op_t op = rados_create_write_op();

    if (!op) {
        return -ENOMEM;
    }

    /* 对象创建与 OMAP 设置合并为一个 op，在 OSD 侧一次性完成 */
    rados_write_op_create(op, LIBRADOS_CREATE_EXCLUSIVE, NULL);

    rados_write_op_omap_set(
        op,
        keys,
        values,
        lengths,
        sizeof(keys) / sizeof(keys[0])
    );

    int ret = rados_write_op_operate(
        op,
        client->ioctx,
        header_oid,
        NULL,
        0
    );

    rados_release_write_op(op);

    return ret;
}

/**
 * 创建一个 Image：生成 ID -> 推导几何参数 -> 写 Header -> 写名称映射。
 *
 * 注意两个细节：
 *   1) ID 只在这里生成，并回填到 image->id / object_prefix，供调用方复用；
 *   2) Header 与 rbd_id 是两个不同对象，不是跨对象原子事务；
 *      映射写失败时尽力删除刚建的 Header（best-effort 回滚）。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param image  输入/输出参数：输入 name/size/order/features/stripe_*，
 *               输出 id/object_prefix/object_size 及补齐的 stripe 参数
 * @return 0 成功；-EEXIST 同名 Image 已存在；其他负数为错误码
 */
int rbd_metadata_create(
    rados_client_t *client,
    rbd_image_t *image)
{
    char id_oid[256];
    char header_oid[256];

    int ret;

    /* 生成 ID 是元数据模块职责，调用方不应提前生成 */
    ret = rbd_generate_image_id(
        image->id,
        sizeof(image->id)
    );

    if (ret < 0) {
        return ret;
    }

    /* Data Object 的公共前缀，后续由 rbd_object_get_oid() 补对象编号 */
    snprintf(
        image->object_prefix,
        sizeof(image->object_prefix),
        "rbd_data.%s",
        image->id
    );

    /* order -> 对象大小；这里用移位而不是 rbd_get_object_size()，语义相同 */
    image->object_size = 1ULL << image->order;

    /* stripe_unit == 0 表示调用方未指定，取默认值 = object_size */
    if (image->stripe_unit == 0) {
        image->stripe_unit = image->object_size;
    }

    /* stripe_count == 0 同样表示未指定，退化为普通（非 fancy）条带 */
    if (image->stripe_count == 0) {
        image->stripe_count = 1;
    }

    /* 名称 -> ID 映射对象的 OID */
    ret = build_oid(
        id_oid,
        sizeof(id_oid),
        "rbd_id.",
        image->name
    );

    if (ret < 0) {
        return ret;
    }

    /* Header 对象的 OID */
    ret = build_oid(
        header_oid,
        sizeof(header_oid),
        "rbd_header.",
        image->id
    );

    if (ret < 0) {
        return ret;
    }

    /*
     * 同名 Image 不允许覆盖：rados_stat 返回 0 表示对象已存在。
     * 这里只是提前检查，真正的并发保护依赖下面 write op 的 exclusive create。
     */
    ret = rados_client_exists(client, id_oid);

    if (ret == 0) {
        return -EEXIST;
    }

    if (ret != -ENOENT) {
        return ret;
    }

    /* 先建 Header：即使映射写失败，也不会留下“有名称无元数据”的半状态 */
    ret = create_header(client, header_oid, image);

    if (ret < 0) {
        return ret;
    }

    /* 再建 name -> image_id 映射，供第二阶段 open 使用 */
    ret = rados_client_create_full(
        client,
        id_oid,
        image->id,
        strlen(image->id)
    );

    if (ret < 0) {
        rados_client_remove(client, header_oid);
        return ret;
    }

    printf("Image created:\n");
    printf("  name          : %s\n", image->name);
    printf("  id            : %s\n", image->id);
    printf("  header        : %s\n", header_oid);
    printf("  object_prefix : %s\n", image->object_prefix);
    printf("  size          : %llu\n",
           (unsigned long long)image->size);
    printf("  order         : %u\n", image->order);
    printf("  object_size   : %llu\n",
           (unsigned long long)image->object_size);

    return 0;
}

/**
 * 删除 Image 的两个元数据对象：先 Header，再 name -> ID 映射。
 *
 * 限制：不会删除 Data Object（稀疏空间回收 / discard 尚未实现），
 * image->id 必须由调用方提供正确值，否则 Header 可能删不掉。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param image  含 name 与 id 的 Image 描述结构
 * @return 0 成功；负数为错误码（对象不存在 -ENOENT 视为成功）
 */
int rbd_metadata_remove(
    rados_client_t *client,
    rbd_image_t *image)
{
    char id_oid[256];
    char header_oid[256];

    snprintf(id_oid, sizeof(id_oid),
             "rbd_id.%s", image->name);

    snprintf(header_oid, sizeof(header_oid),
             "rbd_header.%s", image->id);

    int ret = rados_client_remove(client, header_oid);

    if (ret < 0 && ret != -ENOENT) {
        return ret;
    }

    return rados_client_remove(client, id_oid);
}
