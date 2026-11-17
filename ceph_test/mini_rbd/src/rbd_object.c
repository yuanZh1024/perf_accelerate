#include "rbd_object.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

/**
 * 生成 Data Object 的对象 ID。
 *
 * 格式：<object_prefix>.<16 位十六进制对象编号>，例如
 *   rbd_data.d39bceda1e8fb8ca.0000000000000000
 * 用固定宽度补零，保证对象名排序与编号顺序一致（和官方 RBD 风格相同）。
 *
 * @param image      Image 描述结构，使用其中的 object_prefix
 * @param object_no  对象编号（offset / object_size）
 * @param oid        输出缓冲区
 * @param oid_len    输出缓冲区大小
 * @return 0 成功；-ENAMETOOLONG 缓冲区不足
 */
int rbd_object_get_oid(
    rbd_image_t *image,
    uint64_t object_no,
    char *oid,
    size_t oid_len)
{
    int n = snprintf(
        oid,
        oid_len,
        "%s.%016" PRIx64,
        image->object_prefix,
        object_no
    );

    if (n < 0 || (size_t)n >= oid_len) {
        return -ENAMETOOLONG;
    }

    return 0;
}

/**
 * 写 Image 的一段逻辑地址：按 object_size 切块，逐块写入 Data Object。
 *
 * 地址映射：object_no = offset / object_size，
 *           object_offset = offset % object_size。
 * 每次循环只写到当前对象的尾部；若还有剩余数据，则推进到下一个对象继续写。
 *
 * 当前只支持普通条带：stripe_count == 1 且 stripe_unit == object_size。
 * 官方 Fancy Striping 的映射算法尚未实现。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param image  Image 描述结构（需含有效 object_prefix / object_size）
 * @param offset Image 内逻辑起始偏移（字节）
 * @param buf    源数据缓冲区
 * @param len    写入长度（字节）
 * @return 0 成功；-EINVAL 越界；-ENOTSUP 条带参数不支持；其他负数为 RADOS 错误码
 */
int rbd_object_write(
    rados_client_t *client,
    rbd_image_t *image,
    uint64_t offset,
    const void *buf,
    size_t len)
{
    /* 先做整体越界检查：避免 offset + len 直接相加造成 uint64_t 回绕 */
    if (offset > image->size ||
        len > image->size - offset) {
        return -EINVAL;
    }

    /* 只支持普通条带，其他组合留给后续阶段实现 */
    if (image->stripe_count != 1 ||
        image->stripe_unit != image->object_size) {
        return -ENOTSUP;
    }

    const char *data = (const char *)buf;

    while (len > 0) {
        /* 逻辑地址 -> 对象编号 + 对象内偏移 */
        uint64_t object_no =
            offset / image->object_size;

        uint64_t object_offset =
            offset % image->object_size;

        /* 当前对象还能写多少字节 */
        uint64_t remaining =
            image->object_size - object_offset;

        /* 本次实际写入量：不超过对象尾部，也不超过用户剩余数据 */
        size_t chunk_len =
            len < remaining ? len : (size_t)remaining;

        char oid[RBD_MAX_OBJECT_NAME];

        int ret = rbd_object_get_oid(
            image,
            object_no,
            oid,
            sizeof(oid)
        );

        if (ret < 0) {
            return ret;
        }

        ret = rados_client_write(
            client,
            oid,
            data,
            chunk_len,
            object_offset
        );

        /*
         * rados_write() 成功返回 0，失败返回负数；
         * 不能拿返回值和 chunk_len 比较（那是 rados_read() 的约定）。
         */
        if (ret < 0) {
            return ret;
        }

        printf(
            "WRITE oid=%s offset=%llu len=%zu\n",
            oid,
            (unsigned long long)object_offset,
            chunk_len
        );

        /* 三个游标同步推进：用户缓冲区、逻辑偏移、剩余长度 */
        data += chunk_len;
        offset += chunk_len;
        len -= chunk_len;
    }

    return 0;
}

/**
 * 读 Image 的一段逻辑地址：按 object_size 切块，逐块从 Data Object 读取。
 *
 * 稀疏块设备语义：
 *   - 对象根本不存在（-ENOENT）-> 整块补 0；
 *   - 读到对象 EOF 之后（返回值 < 请求长度）-> 尾部补 0。
 * 两种情况都不视为错误，这是“未写入的扇区读出为 0”的实现方式。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param image  Image 描述结构（需含有效 object_prefix / object_size）
 * @param offset Image 内逻辑起始偏移（字节）
 * @param buf    输出缓冲区
 * @param len    读取长度（字节）
 * @return 0 成功（缺失部分已补 0）；-EINVAL 越界；-ENOTSUP 条带不支持；
 *         其他负数为 RADOS 错误码
 */
int rbd_object_read(
    rados_client_t *client,
    rbd_image_t *image,
    uint64_t offset,
    void *buf,
    size_t len)
{
    if (offset > image->size ||
        len > image->size - offset) {
        return -EINVAL;
    }

    if (image->stripe_count != 1 ||
        image->stripe_unit != image->object_size) {
        return -ENOTSUP;
    }

    char *data = (char *)buf;

    while (len > 0) {
        /* 与 write 相同的地址映射公式 */
        uint64_t object_no =
            offset / image->object_size;

        uint64_t object_offset =
            offset % image->object_size;

        uint64_t remaining =
            image->object_size - object_offset;

        size_t chunk_len =
            len < remaining ? len : (size_t)remaining;

        char oid[RBD_MAX_OBJECT_NAME];

        int ret = rbd_object_get_oid(
            image,
            object_no,
            oid,
            sizeof(oid)
        );

        if (ret < 0) {
            return ret;
        }

        ret = rados_client_read(
            client,
            oid,
            data,
            chunk_len,
            object_offset
        );

        if (ret == -ENOENT) {
            /*
             * 对象不存在 = 从未写入过的稀疏区域，
             * 逻辑上应当读出全 0，而不是报错。
             */
            memset(data, 0, chunk_len);
        } else if (ret < 0) {
            return ret;
        } else if ((size_t)ret < chunk_len) {
            /*
             * 短读：请求范围超过了对象的物理末尾，
             * 剩余部分同样按全 0 处理。
             */
            memset(data + ret, 0, chunk_len - ret);
        }

        printf(
            "READ oid=%s offset=%llu len=%zu\n",
            oid,
            (unsigned long long)object_offset,
            chunk_len
        );

        data += chunk_len;
        offset += chunk_len;
        len -= chunk_len;
    }

    return 0;
}
