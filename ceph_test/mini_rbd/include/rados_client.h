#ifndef MINI_RBD_RADOS_CLIENT_H
#define MINI_RBD_RADOS_CLIENT_H

#include <rados/librados.h>

/**
 * librados 连接的轻量封装。
 *
 * cluster 是到集群的连接句柄；ioctx 是某个 pool 的 I/O 上下文。
 * 后续 Header Object、Data Object 的所有读写都通过 ioctx 进行。
 */
typedef struct {
    rados_t cluster;        /**< rados 集群连接句柄 */
    rados_ioctx_t ioctx;    /**< pool I/O 上下文，绑定到一个具体 pool */
} rados_client_t;

/**
 * 初始化 RADOS 客户端：创建句柄 -> 读默认配置 -> 连接集群 -> 创建 ioctx。
 *
 * @param client 输出参数；函数内部先清零，失败时会回滚已创建的资源。
 * @param pool   目标 pool 名，例如 "mini.rbd"（需提前创建）。
 * @return 0 成功；负数为 librados 错误码，例如 -ENOENT（pool 不存在）。
 */
int rados_client_init(
    rados_client_t *client,
    const char *pool);

/**
 * 释放客户端资源：先销毁 ioctx，再关闭集群句柄。
 * 可重复调用（内部指针会置 NULL）。
 *
 * @param client 已初始化（或已销毁）的客户端指针
 */
void rados_client_destroy(rados_client_t *client);

/**
 * 向指定对象写入数据（可指定偏移，对象不存在时自动创建）。
 * 封装 rados_write()，成功返回 0 而不是写入字节数。
 *
 * @param client 已初始化的客户端
 * @param oid    对象 ID，例如 "rbd_data.<id>.0000000000000000"
 * @param buf    待写入数据缓冲区
 * @param len    写入字节数
 * @param offset 对象内起始偏移
 * @return 0 成功；负数为 librados 错误码
 */
int rados_client_write(
    rados_client_t *client,
    const char *oid,
    const void *buf,
    size_t len,
    uint64_t offset);

/**
 * 从指定对象读取数据。
 * 封装 rados_read()，返回实际读到的字节数，可能小于 len（读到对象末尾）。
 *
 * @param client 已初始化的客户端
 * @param oid    对象 ID
 * @param buf    输出缓冲区
 * @param len    期望读取的最长字节数
 * @param offset 对象内起始偏移
 * @return >=0 实际读取字节数；负数为错误码，-ENOENT 表示对象不存在
 */
int rados_client_read(
    rados_client_t *client,
    const char *oid,
    void *buf,
    size_t len,
    uint64_t offset);

/**
 * 删除一个对象。
 *
 * @param client 已初始化的客户端
 * @param oid    要删除的对象 ID
 * @return 0 成功；-ENOENT 表示对象本来就不存在；其他负数为错误码
 */
int rados_client_remove(
    rados_client_t *client,
    const char *oid);

/**
 * 判断对象是否存在（通过 rados_stat 探测）。
 *
 * @param client 已初始化的客户端
 * @param oid    要探测的对象 ID
 * @return 0 存在；-ENOENT 不存在；其他负数为错误码
 */
int rados_client_exists(
    rados_client_t *client,
    const char *oid);

/**
 * 以“独占创建 + 整体覆盖写”的方式创建对象。
 *
 * 使用单个 write op 把 create(exclusive) 和 write_full 组合起来，
 * 避免并发创建同名 Image 时覆盖已有的名称 -> ID 映射。
 *
 * @param client 已初始化的客户端
 * @param oid    要创建的对象 ID
 * @param buf    对象内容
 * @param len    内容长度
 * @return 0 成功；-EEXIST 对象已存在；其他负数为错误码
 */
int rados_client_create_full(
    rados_client_t *client,
    const char *oid,
    const void *buf,
    size_t len);

#endif
