#include "spdk/stdinc.h"

#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/string.h"

struct app_context {
    const char *bdev_name;

    struct spdk_bdev_desc *bdev_desc;
    struct spdk_io_channel *bdev_ch;

    void *buf;
};

static void
write_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
    struct app_context *ctx = cb_arg;

    printf("\n=============================\n");
    printf("WRITE CALLBACK\n");
    printf("=============================\n");

    printf("write result: %s\n", success ? "SUCCESS" : "FAILED");

    spdk_bdev_free_io(bdev_io);

    spdk_app_stop(success ? 0 : -1);
}

static void
start_write(struct app_context *ctx)
{
    uint64_t offset = 0;
    uint64_t nbytes = 4096;

    printf("\n=============================\n");
    printf("START WRITE\n");
    printf("=============================\n");

    printf("bdev   : %s\n", ctx->bdev_name);
    printf("offset : %lu\n", offset);
    printf("size   : %lu\n", nbytes);

    int rc = spdk_bdev_write(
        ctx->bdev_desc,
        ctx->bdev_ch,
        ctx->buf,
        offset,
        nbytes,
        write_complete,
        ctx
    );

    if (rc != 0) {
        printf("spdk_bdev_write() failed: %d\n", rc);

        spdk_app_stop(-1);
        return;
    }

    printf("spdk_bdev_write() submitted successfully\n");
}

static void
storage_bdev_event_cb(
    enum spdk_bdev_event_type type,
    struct spdk_bdev *bdev,
    void *event_ctx)
{
    printf(
        "[storage] bdev event: type=%d bdev=%s\n",
        type,
        spdk_bdev_get_name(bdev)
    );
}

static void
app_start(void *arg)
{
    struct app_context *ctx = arg;
    struct spdk_bdev *bdev;
    int rc;

    printf("\n=============================\n");
    printf("MY STORAGE APP START\n");
    printf("=============================\n");

    /*
     * 1. 找到 bdev
     */
    bdev = spdk_bdev_get_by_name(ctx->bdev_name);

    if (bdev == NULL) {
        printf("Cannot find bdev: %s\n", ctx->bdev_name);
        spdk_app_stop(-1);
        return;
    }

    printf("Found bdev: %s\n", ctx->bdev_name);

    /*
     * 2. 打开 bdev
     */
    rc = spdk_bdev_open_ext(
        ctx->bdev_name,
        true,
        storage_bdev_event_cb,
        NULL,
        &ctx->bdev_desc
    );

    if (rc != 0) {
        printf("spdk_bdev_open_ext() failed: %d\n", rc);
        spdk_app_stop(-1);
        return;
    }

    printf("Bdev opened\n");

    /*
     * 3. 获取 I/O channel
     */
    ctx->bdev_ch = spdk_bdev_get_io_channel(ctx->bdev_desc);

    if (ctx->bdev_ch == NULL) {
        printf("spdk_bdev_get_io_channel() failed\n");

        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }

    printf("I/O channel acquired\n");

    /*
     * 4. 分配 DMA buffer
     */
    ctx->buf = spdk_dma_zmalloc(
        4096,
        4096,
        NULL
    );

    if (ctx->buf == NULL) {
        printf("spdk_dma_zmalloc() failed\n");

        spdk_put_io_channel(ctx->bdev_ch);
        spdk_bdev_close(ctx->bdev_desc);

        spdk_app_stop(-1);
        return;
    }

    /*
     * 5. 准备数据
     */
    snprintf(
        ctx->buf,
        4096,
        "Hello SPDK from my_storage_app!"
    );

    printf("Buffer prepared: %s\n", (char *)ctx->buf);

    /*
     * 6. 发起 write
     */
    start_write(ctx);
}
static void
usage(void)
{
    printf("Usage: my_storage_app [options]\n");
    printf("  -b <bdev>    bdev name, default Malloc0\n");
}

static const char *g_bdev_name = "Malloc0";



static int
my_parse_arg(int ch, char *arg)
{
    switch (ch) {
    case 'b':
        g_bdev_name = arg;
        return 0;

    default:
        return -EINVAL;
    }
}

int
main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    struct app_context ctx = {};
    int rc;

    spdk_app_opts_init(&opts, sizeof(opts));

    opts.name = "my_storage_app";
    opts.rpc_addr = NULL;

    rc = spdk_app_parse_args(
        argc,
        argv,
        &opts,
        "b:",
        NULL,
        my_parse_arg,
        usage
    );

    if (rc != SPDK_APP_PARSE_ARGS_SUCCESS) {
        return rc;
    }

    ctx.bdev_name = g_bdev_name;

    rc = spdk_app_start(
        &opts,
        app_start,
        &ctx
    );

    if (rc != 0) {
        printf("SPDK application failed: %d\n", rc);
    }

    if (ctx.buf != NULL) {
        spdk_dma_free(ctx.buf);
    }

    if (ctx.bdev_ch != NULL) {
        spdk_put_io_channel(ctx.bdev_ch);
    }

    if (ctx.bdev_desc != NULL) {
        spdk_bdev_close(ctx.bdev_desc);
    }

    spdk_app_fini();

    return rc;
}