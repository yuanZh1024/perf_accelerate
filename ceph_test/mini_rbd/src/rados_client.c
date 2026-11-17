#include "rados_client.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

/**
 * 初始化 RADOS 客户端：创建句柄 -> 读默认配置 -> 连接 -> 创建 pool ioctx。
 *
 * 任何一步失败都会沿 goto 标签释放已经创建的资源，避免句柄泄漏。
 *
 * @param client 输出参数；函数开头清零，调用方无需预先初始化。
 * @param pool   要连接的 pool 名称，例如 "mini.rbd"。
 * @return 0 成功；负数为 librados 错误码。
 */
int rados_client_init(
    rados_client_t *client,
    const char *pool)
{
    int ret;

    /* 清零是 destroy() 可重复调用、错误路径可安全回退的前提 */
    memset(client, 0, sizeof(*client));

    /* NULL identity：使用配置文件中的 ceph.client.admin.keyring 身份 */
    ret = rados_create(&client->cluster, NULL);
    if (ret < 0) {
        return ret;
    }

    /* NULL 表示读取默认配置文件，通常是 /etc/ceph/ceph.conf */
    ret = rados_conf_read_file(client->cluster, NULL);
    if (ret < 0) {
        goto err_shutdown;
    }

    /* 与 MON / OSD 建立连接 */
    ret = rados_connect(client->cluster);
    if (ret < 0) {
        goto err_shutdown;
    }

    /*
     * ioctx 代表一个 pool 的 I/O 上下文；后面所有 Header/Data Object
     * 的读写都通过这个 ioctx，而不是直接通过 cluster。
     */
    ret = rados_ioctx_create(
        client->cluster,
        pool,
        &client->ioctx
    );

    if (ret < 0) {
        goto err_disconnect;
    }

    return 0;

/* pool 上下文创建失败：集群已连接，只需断开 */
err_disconnect:
    rados_shutdown(client->cluster);
    client->cluster = NULL;
    return ret;

/* 配置读取或连接失败：直接关闭集群句柄 */
err_shutdown:
    rados_shutdown(client->cluster);
    client->cluster = NULL;
    return ret;
}

/**
 * 释放客户端资源：ioctx -> cluster，顺序不能颠倒。
 * 释放后指针置 NULL，因此重复调用是安全的。
 *
 * @param client 待销毁的客户端
 */
void rados_client_destroy(rados_client_t *client)
{
    if (client->ioctx) {
        rados_ioctx_destroy(client->ioctx);
        client->ioctx = NULL;
    }

    if (client->cluster) {
        rados_shutdown(client->cluster);
        client->cluster = NULL;
    }
}

/**
 * 封装 rados_write()：从 offset 开始写入一段数据，对象不存在会自动创建。
 * 注意 librados 约定：成功返回 0，而不是写入字节数。
 *
 * @param client 已初始化的客户端
 * @param oid    目标对象 ID
 * @param buf    源数据缓冲区
 * @param len    写入字节数
 * @param offset 对象内偏移
 * @return 0 成功；负数为错误码
 */
int rados_client_write(
    rados_client_t *client,
    const char *oid,
    const void *buf,
    size_t len,
    uint64_t offset)
{
    return rados_write(
        client->ioctx,
        oid,
        buf,
        len,
        offset
    );
}

/**
 * 封装 rados_read()：返回实际读到的字节数，可能小于 len（读到 EOF）。
 *
 * @param client 已初始化的客户端
 * @param oid    目标对象 ID
 * @param buf    输出缓冲区
 * @param len    期望读取的最长字节数
 * @param offset 对象内偏移
 * @return >=0 实际字节数；-ENOENT 对象不存在；其他负数为错误码
 */
int rados_client_read(
    rados_client_t *client,
    const char *oid,
    void *buf,
    size_t len,
    uint64_t offset)
{
    return rados_read(
        client->ioctx,
        oid,
        buf,
        len,
        offset
    );
}

/**
 * 删除一个对象。rbd_metadata_remove() 用它清理 Header 和名称映射。
 *
 * @param client 已初始化的客户端
 * @param oid    对象 ID
 * @return 0 成功；-ENOENT 不存在；其他负数为错误码
 */
int rados_client_remove(
    rados_client_t *client,
    const char *oid)
{
    return rados_remove(client->ioctx, oid);
}

/**
 * 用 rados_stat() 判断对象是否存在。
 * 只关心返回码，size / mtime 是 stat 必须的输出参数，这里不使用。
 *
 * @param client 已初始化的客户端
 * @param oid    对象 ID
 * @return 0 存在；-ENOENT 不存在；其他负数为错误码
 */
int rados_client_exists(
    rados_client_t *client,
    const char *oid)
{
    uint64_t size;
    time_t mtime;

    return rados_stat(
        client->ioctx,
        oid,
        &size,
        &mtime
    );
}

/**
 * 独占创建对象并一次性写入完整内容。
 *
 * create 与 write_full 被打包进同一个 write op，在 OSD 侧执行：
 * 若对象已存在（例如两个进程并发创建同名 Image），操作返回 -EEXIST，
 * 不会覆盖已有内容。这正是 rbd_id.<name> 映射需要的保护。
 *
 * @param client 已初始化的客户端
 * @param oid    对象 ID
 * @param buf    对象内容
 * @param len    内容长度
 * @return 0 成功；-EEXIST 已存在；其他负数为错误码
 */
int rados_client_create_full(
    rados_client_t *client,
    const char *oid,
    const void *buf,
    size_t len)
{
    rados_write_op_t op = rados_create_write_op();

    if (!op) {
        return -ENOMEM;
    }

    /* 独占创建：绝不覆盖并发创建时已写入的名称 -> ID 映射 */
    rados_write_op_create(op, LIBRADOS_CREATE_EXCLUSIVE, NULL);

    /* 一次写满整个对象；本函数使用场景是小的元数据对象 */
    rados_write_op_write_full(
        op,
        (const char *)buf,
        len
    );

    /* 把整个 op 提交到 OSD；NULL 表示不指定 flags */
    int ret = rados_write_op_operate(
        op,
        client->ioctx,
        oid,
        NULL,
        0
    );

    rados_release_write_op(op);

    return ret;
}
