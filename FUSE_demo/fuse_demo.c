#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static int my_getattr(const char *path,
                      struct stat *st,
                      struct fuse_file_info *fi)
{
    memset(st, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    if (strcmp(path, "/hello.txt") == 0) {
        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = 12;
        return 0;
    }

    return -ENOENT;
}


static int my_readdir(const char *path,
                      void *buf,
                      fuse_fill_dir_t filler,
                      off_t offset,
                      struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, "hello.txt", NULL, 0, 0);

    return 0;
}


static int my_read(const char *path,
                   char *buf,
                   size_t size,
                   off_t offset,
                   struct fuse_file_info *fi)
{
    const char *content = "Hello FUSE!\n";

    if (strcmp(path, "/hello.txt") != 0)
        return -ENOENT;

    size_t len = strlen(content);

    if (offset >= len)
        return 0;

    if (offset + size > len)
        size = len - offset;

    memcpy(buf, content + offset, size);

    return size;
}

static struct fuse_operations ops = {
    .getattr = my_getattr,
    .readdir = my_readdir,
    .open    = my_open,
    .read    = my_read,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &ops, NULL);
}