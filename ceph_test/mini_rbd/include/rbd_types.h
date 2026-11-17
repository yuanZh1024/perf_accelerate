#ifndef MINI_RBD_TYPES_H
#define MINI_RBD_TYPES_H

#include <stdint.h>
#include <stddef.h>

/*
 * MiniRBD 基础类型定义。
 *
 * 命名和概念尽量贴近 Ceph RBD Image Format 2：
 *   - rbd_id.<name>      名称 -> Image ID 的映射对象（小对象）
 *   - rbd_header.<id>    Header 对象，元数据放在 OMAP 中
 *   - rbd_data.<id>.<no> 真正存放块数据的 Data Object
 *
 * 注意：这是教学用的简化模型，不实现官方 cls_rbd 与完整 Format 2 协议。
 */

/* RBD 风格的对象名前缀，拼接规则见 rbd_metadata.c / rbd_object.c */
#define RBD_ID_PREFIX       "rbd_id."
#define RBD_HEADER_PREFIX   "rbd_header."
#define RBD_DATA_PREFIX     "rbd_data."

/*
 * 默认 object order：object_size = 1 << order。
 * 22 表示每个 Data Object 最大 4 MiB，也是官方 RBD 的常用默认值。
 */
#define RBD_DEFAULT_ORDER   22

/* 各类名称的缓冲区上限，避免无限长名字截断 RADOS OID */
#define RBD_MAX_IMAGE_NAME  128
#define RBD_MAX_IMAGE_ID    64
#define RBD_MAX_OBJECT_NAME 256

/**
 * 一个 Image 在进程内的描述结构。
 *
 * 前半部分（name/size/order/features/stripe_*）由调用方填入，作为创建参数；
 * 后半部分（id/object_prefix/object_size 及 stripe 默认值）由
 * rbd_metadata_create() 生成并回填。
 * 当前版本尚未实现 open，所以这些字段只存在于当前进程内存中。
 */
typedef struct {
    char name[RBD_MAX_IMAGE_NAME];       /**< Image 名称，例如 "disk01" */

    char id[RBD_MAX_IMAGE_ID];           /**< 16 位十六进制 Image ID，由 /dev/urandom 生成 */

    char object_prefix[RBD_MAX_OBJECT_NAME]; /**< Data Object 前缀："rbd_data.<id>" */

    uint64_t size;                       /**< Image 逻辑大小，字节为单位 */

    uint64_t features;                   /**< 特性位图；当前简化版恒为 0 */

    uint8_t order;                       /**< 对象大小指数：object_size = 1 << order */

    uint64_t object_size;                /**< 由 order 计算出的对象大小（字节） */

    uint64_t stripe_unit;                /**< 条带单元；当前仅支持等于 object_size */

    uint64_t stripe_count;               /**< 条带数量；当前仅支持 1（普通条带） */

} rbd_image_t;

/**
 * 描述一段逻辑地址落在哪个 Data Object 的哪个偏移上。
 * 目前 rbd_object_*.c 内部直接计算这些值，结构体保留供后续重构 / 教学使用。
 */
typedef struct {
    uint64_t object_no;      /**< 对象编号 = offset / object_size */

    uint64_t object_offset;  /**< 对象内偏移 = offset % object_size */

    uint64_t length;         /**< 本次读写在这一段内的长度 */

} rbd_object_extent_t;

/**
 * 由 order 计算对象大小。
 *
 * @param order 对象大小指数（RBD_DEFAULT_ORDER = 22 -> 4 MiB）
 * @return 对象大小，字节为单位；1ULL 保证 64 位左移不溢出
 */
static inline uint64_t rbd_get_object_size(uint8_t order)
{
    return 1ULL << order;
}

#endif
