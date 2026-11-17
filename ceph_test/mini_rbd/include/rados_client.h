#ifndef MINI_RBD_RADOS_CLIENT_H
#define MINI_RBD_RADOS_CLIENT_H

#include <rados/librados.h>

typedef struct {
    rados_t cluster;
    rados_ioctx_t ioctx;
} rados_client_t;

int rados_client_init(
    rados_client_t *client,
    const char *pool);

void rados_client_destroy(rados_client_t *client);

int rados_client_write(
    rados_client_t *client,
    const char *oid,
    const void *buf,
    size_t len,
    uint64_t offset);

int rados_client_read(
    rados_client_t *client,
    const char *oid,
    void *buf,
    size_t len,
    uint64_t offset);

int rados_client_remove(
    rados_client_t *client,
    const char *oid);

int rados_client_exists(
    rados_client_t *client,
    const char *oid);

int rados_client_create_full(
    rados_client_t *client,
    const char *oid,
    const void *buf,
    size_t len);

#endif
