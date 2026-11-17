#ifndef MINI_RBD_METADATA_H
#define MINI_RBD_METADATA_H

#include "rbd_types.h"
#include "rados_client.h"

int rbd_metadata_create(
    rados_client_t *client,
    rbd_image_t *image);

int rbd_metadata_remove(
    rados_client_t *client,
    rbd_image_t *image);

int rbd_generate_image_id(char *id, size_t len);

#endif
