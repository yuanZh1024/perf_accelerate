#include "spdk/stdinc.h"

#include "spdk/bdev.h"
#include "spdk/env.h"

#include "storage_engine.h"

struct storage_engine {
    struct spdk_bdev *bdev;
    struct spdk_bdev_desc *desc;
    struct spdk_io_channel *ch;
};

static struct storage_engine g_storage;

int
storage_init(const char *bdev_name)
{
    int rc;

    printf("[storage] initializing...\n");

    /*
     * 找到 bdev
     */
    g_storage.bdev = spdk_bdev_get_by_name(bdev_name);

    if (g_storage.bdev == NULL) {
        printf("[storage] bdev not found: %s\n", bdev_name);
        return -1;
    }

    /*
     * 打开 bdev
     */
    rc = spdk_bdev_open_ext(
        bdev_name,
        true,
        NULL,
        NULL,
        &g_storage.desc
    );

    if (rc != 0) {
        printf("[storage] failed to open bdev: %d\n", rc);
        return rc;
    }

    /*
     * 获取 I/O channel
     */
    g_storage.ch =
        spdk_bdev_get_io_channel(g_storage.desc);

    if (g_storage.ch == NULL) {
        printf("[storage] failed to get io channel\n");

        spdk_bdev_close(g_storage.desc);
        g_storage.desc = NULL;

        return -1;
    }

    printf("[storage] initialized: %s\n", bdev_name);

    return 0;
}

static void
write_complete(
    struct spdk_bdev_io *bdev_io,
    bool success,
    void *cb_arg)
{
    bool *result = cb_arg;

    *result = success;

    spdk_bdev_free_io(bdev_io);
}

int
storage_write(
    uint64_t offset,
    void *buf,
    uint64_t size)
{
    bool success = false;

    printf(
        "[storage] write offset=%lu size=%lu\n",
        offset,
        size
    );

    int rc = spdk_bdev_write(
        g_storage.desc,
        g_storage.ch,
        buf,
        offset,
        size,
        write_complete,
        &success
    );

    if (rc != 0) {
        printf(
            "[storage] write submit failed: %d\n",
            rc
        );

        return rc;
    }

    /*
     * 注意：
     *
     * 这里先不处理异步问题。
     *
     * 下一步我们会专门解决：
     *
     * submit
     *    ↓
     * completion
     */
    return 0;
}

static void
read_complete(
    struct spdk_bdev_io *bdev_io,
    bool success,
    void *cb_arg)
{
    bool *result = cb_arg;

    *result = success;

    spdk_bdev_free_io(bdev_io);
}

int
storage_read(
    uint64_t offset,
    void *buf,
    uint64_t size)
{
    bool success = false;

    printf(
        "[storage] read offset=%lu size=%lu\n",
        offset,
        size
    );

    int rc = spdk_bdev_read(
        g_storage.desc,
        g_storage.ch,
        buf,
        offset,
        size,
        read_complete,
        &success
    );

    if (rc != 0) {
        printf(
            "[storage] read submit failed: %d\n",
            rc
        );

        return rc;
    }

    return 0;
}

void
storage_fini(void)
{
    printf("[storage] shutting down...\n");

    if (g_storage.ch != NULL) {
        spdk_put_io_channel(g_storage.ch);
        g_storage.ch = NULL;
    }

    if (g_storage.desc != NULL) {
        spdk_bdev_close(g_storage.desc);
        g_storage.desc = NULL;
    }

    g_storage.bdev = NULL;
}
