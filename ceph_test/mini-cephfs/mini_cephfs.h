#ifndef MINI_CEPHFS_H
#define MINI_CEPHFS_H

#include <rados/librados.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define MFS_MAGIC       0x4D434653
#define MFS_VERSION     1
#define MFS_ROOT_INO    1ULL
#define MFS_OBJECT_SIZE (4ULL * 1024 * 1024)
#define MFS_NAME_MAX    255
#define MFS_DIR_MAX     4096
#define MFS_PATH_MAX    4096

#define MFS_META_POOL "cephfs_meta"
#define MFS_DATA_POOL "cephfs_data"

enum {
    MFS_FILE = 1,
    MFS_DIR  = 2
};

/* inode metadata */
typedef struct {
    uint32_t magic;
    uint32_t version;

    uint64_t ino;
    uint64_t parent_ino;
    uint64_t size;

    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;

    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t nlink;
    uint32_t type;
} mfs_inode_t;

/* directory entry */
typedef struct {
    char name[MFS_NAME_MAX + 1];
    uint64_t ino;
    uint32_t type;
} mfs_dentry_t;

/* directory metadata */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t count;

    mfs_dentry_t entries[MFS_DIR_MAX];
} mfs_dir_t;

/* filesystem superblock */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t next_ino;
} mfs_super_t;

/* filesystem context */
typedef struct {
    rados_t cluster;
    rados_ioctx_t meta_ioctx;
    rados_ioctx_t data_ioctx;
} mfs_t;

/* lifecycle */
int mfs_open(mfs_t *fs);
void mfs_close(mfs_t *fs);
int mfs_init(mfs_t *fs);

/* inode */
int mfs_inode_get(mfs_t *fs, uint64_t ino,
                  mfs_inode_t *inode);
int mfs_inode_put(mfs_t *fs,
                  const mfs_inode_t *inode);

/* directory */
int mfs_mkdir(mfs_t *fs, const char *path, mode_t mode);
int mfs_ls(mfs_t *fs, const char *path);

/* file */
int mfs_create(mfs_t *fs, const char *path, mode_t mode);
int mfs_write(mfs_t *fs, const char *path,
              const void *buf, size_t len, uint64_t offset);
int mfs_read(mfs_t *fs, const char *path,
             void *buf, size_t len, uint64_t offset);
int mfs_cat(mfs_t *fs, const char *path);

/* metadata operations */
int mfs_stat(mfs_t *fs, const char *path);
int mfs_unlink(mfs_t *fs, const char *path);

#endif
