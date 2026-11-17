#ifndef MINI_RBD_OBJECT_H
#define MINI_RBD_OBJECT_H

#include "rbd_types.h"
#include "rados_client.h"

/**
 * 把 Image 的逻辑地址区间写入底层 Data Object。
 *
 * 逻辑地址映射（当前仅支持普通条带）：
 *   object_no      = offset / object_size
 *   object_offset  = offset % object_size
 * 跨对象边界时会被拆成多次 rados_write()，逐段写入。
 *
 * 限制：stripe_count 必须为 1，且 stripe_unit 必须等于 object_size；
 * Fancy Striping（任意条带参数）尚未实现。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param image  Image 描述结构（需含有效的 object_prefix / object_size）
 * @param offset Image 内逻辑起始偏移（字节）
 * @param buf    待写入数据
 * @param len    写入长度（字节）
 * @return 0 成功；-EINVAL 越界；-ENOTSUP 条带参数不支持；其他负数为 RADOS 错误码
 */
int rbd_object_write(
    rados_client_t *client,
    rbd_image_t *image,
    uint64_t offset,
    const void *buf,
    size_t len);

/**
 * 从 Image 的逻辑地址区间读取数据。
 *
 * 稀疏语义：未分配的 Data Object（对象不存在）或读到对象 EOF 之后的
 * 字节，在逻辑上都视为全 0，不返回错误。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param image  Image 描述结构（需含有效的 object_prefix / object_size）
 * @param offset Image 内逻辑起始偏移（字节）
 * @param buf    输出缓冲区
 * @param len    读取长度（字节）
 * @return 0 成功（缓冲区已填满，缺失部分补 0）；负数为错误码
 */
int rbd_object_read(
    rados_client_t *client,
    rbd_image_t *image,
    uint64_t offset,
    void *buf,
    size_t len);

/**
 * 根据对象编号生成 Data Object 的 OID。
 * 格式："<object_prefix>.%016llx"，例如
 *        rbd_data.d39bceda1e8fb8ca.0000000000000000
 *
 * @param image    Image 描述结构（使用其中的 object_prefix）
 * @param object_no 对象编号
 * @param oid      输出缓冲区
 * @param oid_len  输出缓冲区大小
 * @return 0 成功；-ENAMETOOLONG 缓冲区不足导致截断
 */
int rbd_object_get_oid(
    rbd_image_t *image,
    uint64_t object_no,
    char *oid,
    size_t oid_len);

#endif
