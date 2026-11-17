#include "rbd_object.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

int rbd_object_get_oid(
    rbd_image_t *image,
    uint64_t object_no,
    char *oid,
    size_t oid_len)
{
    int n = snprintf(
        oid,
        oid_len,
        "%s.%016" PRIx64,
        image->object_prefix,
        object_no
    );

    if (n < 0 || (size_t)n >= oid_len) {
        return -ENAMETOOLONG;
    }

    return 0;
}

int rbd_object_write(
    rados_client_t *client,
    rbd_image_t *image,
    uint64_t offset,
    const void *buf,
    size_t len)
{
    if (offset > image->size ||
        len > image->size - offset) {
        return -EINVAL;
    }

    if (image->stripe_count != 1 ||
        image->stripe_unit != image->object_size) {
        return -ENOTSUP;
    }

    const char *data = (const char *)buf;

    while (len > 0) {
        uint64_t object_no =
            offset / image->object_size;

        uint64_t object_offset =
            offset % image->object_size;

        uint64_t remaining =
            image->object_size - object_offset;

        size_t chunk_len =
            len < remaining ? len : (size_t)remaining;

        char oid[RBD_MAX_OBJECT_NAME];

        int ret = rbd_object_get_oid(
            image,
            object_no,
            oid,
            sizeof(oid)
        );

        if (ret < 0) {
            return ret;
        }

        ret = rados_client_write(
            client,
            oid,
            data,
            chunk_len,
            object_offset
        );

        /* rados_write() returns 0 on success, negative on failure. */
        if (ret < 0) {
            return ret;
        }

        printf(
            "WRITE oid=%s offset=%llu len=%zu\n",
            oid,
            (unsigned long long)object_offset,
            chunk_len
        );

        data += chunk_len;
        offset += chunk_len;
        len -= chunk_len;
    }

    return 0;
}

int rbd_object_read(
    rados_client_t *client,
    rbd_image_t *image,
    uint64_t offset,
    void *buf,
    size_t len)
{
    if (offset > image->size ||
        len > image->size - offset) {
        return -EINVAL;
    }

    if (image->stripe_count != 1 ||
        image->stripe_unit != image->object_size) {
        return -ENOTSUP;
    }

    char *data = (char *)buf;

    while (len > 0) {
        uint64_t object_no =
            offset / image->object_size;

        uint64_t object_offset =
            offset % image->object_size;

        uint64_t remaining =
            image->object_size - object_offset;

        size_t chunk_len =
            len < remaining ? len : (size_t)remaining;

        char oid[RBD_MAX_OBJECT_NAME];

        int ret = rbd_object_get_oid(
            image,
            object_no,
            oid,
            sizeof(oid)
        );

        if (ret < 0) {
            return ret;
        }

        ret = rados_client_read(
            client,
            oid,
            data,
            chunk_len,
            object_offset
        );

        if (ret == -ENOENT) {
            /*
             * A missing RADOS object represents
             * a logical zero-filled sparse extent.
             */
            memset(data, 0, chunk_len);
        } else if (ret < 0) {
            return ret;
        } else if ((size_t)ret < chunk_len) {
            /*
             * Read beyond the physical object EOF:
             * remaining bytes are logically zero.
             */
            memset(data + ret, 0, chunk_len - ret);
        }

        printf(
            "READ oid=%s offset=%llu len=%zu\n",
            oid,
            (unsigned long long)object_offset,
            chunk_len
        );

        data += chunk_len;
        offset += chunk_len;
        len -= chunk_len;
    }

    return 0;
}
