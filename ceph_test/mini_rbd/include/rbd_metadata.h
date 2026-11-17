#ifndef MINI_RBD_METADATA_H
#define MINI_RBD_METADATA_H

#include "rbd_types.h"
#include "rados_client.h"

/**
 * 创建一个新的 RBD Image 元数据。
 *
 * 输入：image->name / size / order / features / stripe_unit / stripe_count
 *       （stripe_unit == 0 时默认取 object_size，stripe_count == 0 时默认取 1）。
 * 输出：成功时 image->id、image->object_prefix、image->object_size
 *       以及补齐后的 stripe_unit / stripe_count 会被回填。
 *
 * 写入两个对象：
 *   1) rbd_header.<id>  ：用 OMAP 保存 size/order/object_prefix/... 等键；
 *   2) rbd_id.<name>    ：保存 ID 字符串，供后续 open 通过名称查找。
 *
 * 这两个对象不在同一个 RADOS 事务里；失败时会尽量回滚 Header。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param image  输入/输出参数，见上
 * @return 0 成功；-EEXIST 同名 Image 已存在；其他负数为错误码
 */
int rbd_metadata_create(
    rados_client_t *client,
    rbd_image_t *image);

/**
 * 删除 Image 的两个元数据对象（Header 与名称映射）。
 * 不删除 Data Object，也不做跨对象原子保证。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param image  需提供正确的 name 与 id 以定位两个对象
 * @return 0 成功；负数为错误码（-ENOENT 按成功处理）
 */
int rbd_metadata_remove(
    rados_client_t *client,
    rbd_image_t *image);

/**
 * 生成 16 个字符的十六进制 Image ID（8 字节随机数，格式与官方 RBD 类似）。
 *
 * @param id  输出缓冲区，至少容纳 17 字节（16 字符 + '\0'）
 * @param len id 缓冲区大小
 * @return 0 成功；-EINVAL 缓冲区太小；-errno / -EIO 读取随机源失败
 */
int rbd_generate_image_id(char *id, size_t len);

#endif
