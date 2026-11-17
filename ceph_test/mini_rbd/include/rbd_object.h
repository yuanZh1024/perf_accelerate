#ifndef MINI_RBD_OBJECT_H
#define MINI_RBD_OBJECT_H

#include "rbd_types.h"
#include "rados_client.h"

int rbd_object_write(
    rados_client_t *client,
    rbd_image_t *image,
    uint64_t offset,
    const void *buf,
    size_t len);

int rbd_object_read(
    rados_client_t *client,
    rbd_image_t *image,
    uint64_t offset,
    void *buf,
    size_t len);

int rbd_object_get_oid(
    rbd_image_t *image,
    uint64_t object_no,
    char *oid,
    size_t oid_len);

#endif
