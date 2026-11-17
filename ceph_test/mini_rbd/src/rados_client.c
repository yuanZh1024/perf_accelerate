#include "rados_client.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

int rados_client_init(
    rados_client_t *client,
    const char *pool)
{
    int ret;

    memset(client, 0, sizeof(*client));

    ret = rados_create(&client->cluster, NULL);
    if (ret < 0) {
        return ret;
    }

    ret = rados_conf_read_file(client->cluster, NULL);
    if (ret < 0) {
        goto err_shutdown;
    }

    ret = rados_connect(client->cluster);
    if (ret < 0) {
        goto err_shutdown;
    }

    ret = rados_ioctx_create(
        client->cluster,
        pool,
        &client->ioctx
    );

    if (ret < 0) {
        goto err_disconnect;
    }

    return 0;

err_disconnect:
    rados_shutdown(client->cluster);
    client->cluster = NULL;
    return ret;

err_shutdown:
    rados_shutdown(client->cluster);
    client->cluster = NULL;
    return ret;
}

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

int rados_client_remove(
    rados_client_t *client,
    const char *oid)
{
    return rados_remove(client->ioctx, oid);
}

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

    /* Exclusive create: never overwrite a concurrent image-id mapping. */
    rados_write_op_create(op, LIBRADOS_CREATE_EXCLUSIVE, NULL);

    rados_write_op_write_full(
        op,
        (const char *)buf,
        len
    );

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
