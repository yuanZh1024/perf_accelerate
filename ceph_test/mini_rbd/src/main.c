#include "rbd_metadata.h"
#include "rbd_object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DEFAULT_POOL "mini.rbd"

static int create_image(
    rados_client_t *client,
    const char *name,
    uint64_t size)
{
    rbd_image_t image = {0};

    if (strlen(name) >= sizeof(image.name)) {
        return -ENAMETOOLONG;
    }

    strcpy(image.name, name);

    image.size = size;
    image.order = RBD_DEFAULT_ORDER;

    image.features = 0;
    image.stripe_unit = 0;
    image.stripe_count = 1;

    return rbd_metadata_create(client, &image);
}

static int demo(rados_client_t *client, const char *name)
{
    rbd_image_t image = {0};

    strcpy(image.name, name);

    image.size = 8ULL * 1024 * 1024;
    image.order = RBD_DEFAULT_ORDER;
    image.object_size = 1ULL << image.order;
    image.stripe_unit = image.object_size;
    image.stripe_count = 1;

    /*
     * ID generation, object_prefix and derived geometry are filled in
     * by rbd_metadata_create() and kept in this struct on success.
     */
    int ret = rbd_metadata_create(client, &image);

    if (ret < 0) {
        return ret;
    }

    /*
     * Write 3 MiB starting at offset 3 MiB.
     *
     * Object 0: 1 MiB
     * Object 1: 2 MiB
     */
    const size_t len = 3 * 1024 * 1024;
    const uint64_t offset = 3 * 1024 * 1024;

    char *write_buf = malloc(len);
    char *read_buf = malloc(len);

    if (!write_buf || !read_buf) {
        free(write_buf);
        free(read_buf);
        return -ENOMEM;
    }

    memset(write_buf, 'A', len);
    memset(read_buf, 0, len);

    ret = rbd_object_write(
        client,
        &image,
        offset,
        write_buf,
        len
    );

    if (ret < 0) {
        goto out;
    }

    ret = rbd_object_read(
        client,
        &image,
        offset,
        read_buf,
        len
    );

    if (ret < 0) {
        goto out;
    }

    if (memcmp(write_buf, read_buf, len) != 0) {
        ret = -EIO;
        goto out;
    }

    printf("\nMiniRBD cross-object test PASS\n");

out:
    free(write_buf);
    free(read_buf);

    return ret;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s create <image> <size_mb>\n", argv[0]);
        printf("  %s demo <image>\n", argv[0]);
        return 1;
    }

    rados_client_t client = {0};

    int ret = rados_client_init(
        &client,
        DEFAULT_POOL
    );

    if (ret < 0) {
        fprintf(stderr, "RADOS init failed: %s\n",
                strerror(-ret));
        return 1;
    }

    if (strcmp(argv[1], "create") == 0 && argc == 4) {
        uint64_t size_mb = strtoull(argv[3], NULL, 10);

        if (size_mb == 0 ||
            size_mb > UINT64_MAX / (1024 * 1024)) {
            ret = -EINVAL;
        } else {
            ret = create_image(
                &client,
                argv[2],
                size_mb * 1024 * 1024
            );
        }

    } else if (strcmp(argv[1], "demo") == 0 && argc == 3) {
        ret = demo(&client, argv[2]);

    } else {
        ret = -EINVAL;
    }

    if (ret < 0) {
        fprintf(stderr, "MiniRBD failed: %s (%d)\n",
                strerror(-ret), ret);
    }

    rados_client_destroy(&client);

    return ret < 0 ? 1 : 0;
}
