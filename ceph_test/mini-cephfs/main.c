#include "mini_cephfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

static void usage(const char *program)
{
    printf(
        "Usage: %s COMMAND [ARGS]\n"
        "\n"
        "Commands:\n"
        "  init\n"
        "  mkdir PATH [MODE]\n"
        "  create PATH [MODE]\n"
        "  ls PATH\n"
        "  stat PATH\n"
        "  write PATH TEXT [OFFSET]\n"
        "  read PATH LENGTH [OFFSET]\n"
        "  cat PATH\n"
        "  unlink PATH\n",
        program);
}

static void report_error(int ret)
{
    if (ret < 0)
        fprintf(stderr, "Error: %s (%d)\n",
                strerror(-ret), ret);
}

static int parse_mode(const char *s, mode_t *mode)
{
    char *end;

    unsigned long value = strtoul(s, &end, 8);

    if (*s == '\0' || *end != '\0' || value > 07777)
        return -EINVAL;

    *mode = (mode_t)value;

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    mfs_t fs;

    int ret = mfs_open(&fs);

    if (ret < 0) {
        report_error(ret);
        return 1;
    }

    if (strcmp(cmd, "init") == 0) {
        ret = mfs_init(&fs);
    }

    else if (strcmp(cmd, "mkdir") == 0) {
        if (argc < 3) {
            usage(argv[0]);
            ret = -EINVAL;
        } else {
            mode_t mode = 0755;

            if (argc >= 4)
                ret = parse_mode(argv[3], &mode);

            if (ret == 0)
                ret = mfs_mkdir(&fs, argv[2], mode);
        }
    }

    else if (strcmp(cmd, "create") == 0) {
        if (argc < 3) {
            usage(argv[0]);
            ret = -EINVAL;
        } else {
            mode_t mode = 0644;

            if (argc >= 4)
                ret = parse_mode(argv[3], &mode);

            if (ret == 0)
                ret = mfs_create(&fs, argv[2], mode);
        }
    }

    else if (strcmp(cmd, "ls") == 0) {
        if (argc < 3)
            ret = -EINVAL;
        else
            ret = mfs_ls(&fs, argv[2]);
    }

    else if (strcmp(cmd, "stat") == 0) {
        if (argc < 3)
            ret = -EINVAL;
        else
            ret = mfs_stat(&fs, argv[2]);
    }

    else if (strcmp(cmd, "write") == 0) {
        if (argc < 4) {
            ret = -EINVAL;
        } else {
            uint64_t offset = 0;

            if (argc >= 5) {
                char *end;

                offset = strtoull(argv[4], &end, 10);

                if (*end != '\0')
                    ret = -EINVAL;
            }

            if (ret == 0) {
                ret = mfs_write(
                    &fs,
                    argv[2],
                    argv[3],
                    strlen(argv[3]),
                    offset);
            }
        }
    }

    else if (strcmp(cmd, "read") == 0) {
        if (argc < 4) {
            ret = -EINVAL;
        } else {
            char *end;

            unsigned long long length =
                strtoull(argv[3], &end, 10);

            if (*end != '\0' || length > 64 * 1024 * 1024) {
                ret = -EINVAL;
            } else {
                uint64_t offset = 0;

                if (argc >= 5) {
                    offset = strtoull(argv[4], &end, 10);

                    if (*end != '\0')
                        ret = -EINVAL;
                }

                if (ret == 0) {
                    char *buf = malloc((size_t)length + 1);

                    if (!buf) {
                        ret = -ENOMEM;
                    } else {
                        ret = mfs_read(
                            &fs,
                            argv[2],
                            buf,
                            (size_t)length,
                            offset);

                        if (ret >= 0) {
                            fwrite(buf, 1, (size_t)ret, stdout);
                            putchar('\n');
                        }

                        free(buf);
                    }
                }
            }
        }
    }

    else if (strcmp(cmd, "cat") == 0) {
        if (argc < 3)
            ret = -EINVAL;
        else
            ret = mfs_cat(&fs, argv[2]);
    }

    else if (strcmp(cmd, "unlink") == 0) {
        if (argc < 3)
            ret = -EINVAL;
        else
            ret = mfs_unlink(&fs, argv[2]);
    }

    else {
        usage(argv[0]);
        ret = -EINVAL;
    }

    if (ret < 0)
        report_error(ret);

    mfs_close(&fs);

    return ret < 0 ? 1 : 0;
}
