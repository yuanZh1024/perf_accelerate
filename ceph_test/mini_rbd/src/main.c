#include "rbd_metadata.h"
#include "rbd_object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* 默认操作的 pool；需要提前用 ceph osd pool create 创建并启用 application */
#define DEFAULT_POOL "mini.rbd"

/**
 * 实现 `mini-rbd create <image> <size_mb>` 子命令。
 *
 * 构造一个初始化的 rbd_image_t：
 *   - order 使用默认值 22（4 MiB 对象）；
 *   - features 为 0，不声明任何 RBD 特性；
 *   - stripe_unit = 0 交给元数据层填默认值（= object_size）；
 *   - stripe_count = 1 使用普通条带。
 * 然后交给 rbd_metadata_create() 完成 ID 生成和落盘。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param name   Image 名称
 * @param size   Image 大小（字节）
 * @return 0 成功；-ENAMETOOLONG 名称过长；其他负数为元数据层错误码
 */
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

/**
 * 实现 `mini-rbd demo <image>` 子命令：创建一个 8 MiB Image 并做跨对象读写测试。
 *
 * 测试布局（object_size = 4 MiB）：
 *   逻辑偏移 3 MiB 开始写 3 MiB：
 *     - object 0：从对象内 3 MiB 处写 1 MiB（到对象末尾）
 *     - object 1：从对象内 0 处写 2 MiB（跨越边界后的剩余部分）
 * 写完后按相同区间读回并逐字节比对，验证地址拆分与 RADOS 读写正确。
 *
 * 注意：ID 与 object_prefix 由 rbd_metadata_create() 生成并回填到 image，
 * 这里不再提前生成，避免创建后被覆盖导致数据写到错误的对象前缀。
 *
 * @param client 已初始化的 RADOS 客户端
 * @param name   本次 demo 创建的 Image 名称
 * @return 0 成功并通过比对；负数表示创建/读写失败或数据不一致（-EIO）
 */
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
     * ID 生成、object_prefix 与派生几何参数均由 rbd_metadata_create()
     * 填入，成功后保留在这个结构体里供下面的读写使用。
     */
    int ret = rbd_metadata_create(client, &image);

    if (ret < 0) {
        return ret;
    }

    /* 3 MiB 长度、3 MiB 起始偏移：正好横跨 object 0 / object 1 边界 */
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

    /* 逐字节比对：不一致说明跨对象地址映射或补零逻辑有问题 */
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

/**
 * 程序入口：解析两个子命令，连接 RADOS 后分派执行。
 *
 * 用法：
 *   ./mini-rbd create <image> <size_mb>   创建指定大小的 Image
 *   ./mini-rbd demo <image>               创建 8 MiB Image 并测试跨对象读写
 *
 * @param argc 参数个数
 * @param argv 参数数组；argv[1] 是子命令
 * @return 0 成功；1 参数错误 / 初始化失败 / 操作失败
 */
int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s create <image> <size_mb>\n", argv[0]);
        printf("  %s demo <image>\n", argv[0]);
        return 1;
    }

    rados_client_t client = {0};

    /* 所有命令都针对同一个 pool 操作 */
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

        /* 拒绝 0 大小以及乘以 1 MiB 后会溢出的值 */
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

    /* 无论命令成功失败，都要释放 cluster / ioctx */
    rados_client_destroy(&client);

    return ret < 0 ? 1 : 0;
}
