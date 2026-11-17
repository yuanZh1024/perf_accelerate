#include "rbd_metadata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static int build_oid(
    char *out,
    size_t len,
    const char *prefix,
    const char *name)
{
    int n = snprintf(out, len, "%s%s", prefix, name);

    if (n < 0 || (size_t)n >= len) {
        return -ENAMETOOLONG;
    }

    return 0;
}

int rbd_generate_image_id(char *id, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char random_bytes[8];

    if (len < 17) {
        return -EINVAL;
    }

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -errno;
    }

    ssize_t n = read(fd, random_bytes, sizeof(random_bytes));
    close(fd);

    if (n != sizeof(random_bytes)) {
        return -EIO;
    }

    for (int i = 0; i < 8; i++) {
        id[i * 2] = hex[random_bytes[i] >> 4];
        id[i * 2 + 1] = hex[random_bytes[i] & 0x0f];
    }

    id[16] = '\0';

    return 0;
}

/*
 * Create an object and populate its OMAP atomically.
 *
 * This is our simplified Header schema, NOT the complete
 * official Ceph RBD Format 2 on-disk schema.
 */
static int create_header(
    rados_client_t *client,
    const char *header_oid,
    rbd_image_t *image)
{
    char size[32];
    char order[16];
    char features[32];
    char stripe_unit[32];
    char stripe_count[32];

    snprintf(size, sizeof(size), "%llu",
             (unsigned long long)image->size);

    snprintf(order, sizeof(order), "%u", image->order);

    snprintf(features, sizeof(features), "%llu",
             (unsigned long long)image->features);

    snprintf(stripe_unit, sizeof(stripe_unit), "%llu",
             (unsigned long long)image->stripe_unit);

    snprintf(stripe_count, sizeof(stripe_count), "%llu",
             (unsigned long long)image->stripe_count);

    const char *keys[] = {
        "size",
        "order",
        "object_prefix",
        "features",
        "stripe_unit",
        "stripe_count"
    };

    const char *values[] = {
        size,
        order,
        image->object_prefix,
        features,
        stripe_unit,
        stripe_count
    };

    size_t lengths[] = {
        strlen(size),
        strlen(order),
        strlen(image->object_prefix),
        strlen(features),
        strlen(stripe_unit),
        strlen(stripe_count)
    };

    rados_write_op_t op = rados_create_write_op();

    if (!op) {
        return -ENOMEM;
    }

    rados_write_op_create(op, LIBRADOS_CREATE_EXCLUSIVE, NULL);

    rados_write_op_omap_set(
        op,
        keys,
        values,
        lengths,
        sizeof(keys) / sizeof(keys[0])
    );

    int ret = rados_write_op_operate(
        op,
        client->ioctx,
        header_oid,
        NULL,
        0
    );

    rados_release_write_op(op);

    return ret;
}

int rbd_metadata_create(
    rados_client_t *client,
    rbd_image_t *image)
{
    char id_oid[256];
    char header_oid[256];

    int ret;

    ret = rbd_generate_image_id(
        image->id,
        sizeof(image->id)
    );

    if (ret < 0) {
        return ret;
    }

    snprintf(
        image->object_prefix,
        sizeof(image->object_prefix),
        "rbd_data.%s",
        image->id
    );

    image->object_size = 1ULL << image->order;

    if (image->stripe_unit == 0) {
        image->stripe_unit = image->object_size;
    }

    if (image->stripe_count == 0) {
        image->stripe_count = 1;
    }

    ret = build_oid(
        id_oid,
        sizeof(id_oid),
        "rbd_id.",
        image->name
    );

    if (ret < 0) {
        return ret;
    }

    ret = build_oid(
        header_oid,
        sizeof(header_oid),
        "rbd_header.",
        image->id
    );

    if (ret < 0) {
        return ret;
    }

    /*
     * Do not overwrite an existing image.
     */
    ret = rados_client_exists(client, id_oid);

    if (ret == 0) {
        return -EEXIST;
    }

    if (ret != -ENOENT) {
        return ret;
    }

    /*
     * Create Header first.
     */
    ret = create_header(client, header_oid, image);

    if (ret < 0) {
        return ret;
    }

    /*
     * Create name -> image_id mapping.
     */
    ret = rados_client_create_full(
        client,
        id_oid,
        image->id,
        strlen(image->id)
    );

    if (ret < 0) {
        rados_client_remove(client, header_oid);
        return ret;
    }

    printf("Image created:\n");
    printf("  name          : %s\n", image->name);
    printf("  id            : %s\n", image->id);
    printf("  header        : %s\n", header_oid);
    printf("  object_prefix : %s\n", image->object_prefix);
    printf("  size          : %llu\n",
           (unsigned long long)image->size);
    printf("  order         : %u\n", image->order);
    printf("  object_size   : %llu\n",
           (unsigned long long)image->object_size);

    return 0;
}

int rbd_metadata_remove(
    rados_client_t *client,
    rbd_image_t *image)
{
    char id_oid[256];
    char header_oid[256];

    snprintf(id_oid, sizeof(id_oid),
             "rbd_id.%s", image->name);

    snprintf(header_oid, sizeof(header_oid),
             "rbd_header.%s", image->id);

    int ret = rados_client_remove(client, header_oid);

    if (ret < 0 && ret != -ENOENT) {
        return ret;
    }

    return rados_client_remove(client, id_oid);
}
