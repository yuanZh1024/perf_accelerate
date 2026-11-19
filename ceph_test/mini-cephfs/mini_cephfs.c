#include "mini_cephfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>

#define SUPER_OID "super"

static uint64_t mfs_now(void)
{
    return (uint64_t)time(NULL);
}

/* =========================================================
 * 1. RADOS Object naming
 * ========================================================= */

/*
 * inode object:
 * inode.1
 * inode.2
 */
static void inode_oid(char *oid, size_t size, uint64_t ino)
{
    snprintf(oid, size, "inode.%" PRIu64, ino);
}

/*
 * directory object:
 * dir.1
 * dir.2
 */
static void dir_oid(char *oid, size_t size, uint64_t ino)
{
    snprintf(oid, size, "dir.%" PRIu64, ino);
}

/*
 * file data object:
 * 3.00000000
 * 3.00000001
 */
static void data_oid(char *oid, size_t size,
                     uint64_t ino, uint64_t objno)
{
    snprintf(oid, size, "%" PRIu64 ".%08" PRIx64,
             ino, objno);
}

/* =========================================================
 * 2. RADOS connection
 * ========================================================= */

int mfs_open(mfs_t *fs)
{
    int ret;

    memset(fs, 0, sizeof(*fs));

    ret = rados_create(&fs->cluster, NULL);
    if (ret < 0)
        return ret;

    ret = rados_conf_read_file(fs->cluster, NULL);
    if (ret < 0)
        goto fail;

    ret = rados_connect(fs->cluster);
    if (ret < 0)
        goto fail;

    ret = rados_ioctx_create(
        fs->cluster, MFS_META_POOL, &fs->meta_ioctx);

    if (ret < 0)
        goto fail;

    ret = rados_ioctx_create(
        fs->cluster, MFS_DATA_POOL, &fs->data_ioctx);

    if (ret < 0)
        goto fail;

    return 0;

fail:
    mfs_close(fs);
    return ret;
}

void mfs_close(mfs_t *fs)
{
    if (fs->data_ioctx)
        rados_ioctx_destroy(fs->data_ioctx);

    if (fs->meta_ioctx)
        rados_ioctx_destroy(fs->meta_ioctx);

    if (fs->cluster)
        rados_shutdown(fs->cluster);

    memset(fs, 0, sizeof(*fs));
}

/* =========================================================
 * 3. Inode operations
 * ========================================================= */

int mfs_inode_get(mfs_t *fs, uint64_t ino,
                  mfs_inode_t *inode)
{
    char oid[64];

    inode_oid(oid, sizeof(oid), ino);

    int ret = rados_read(
        fs->meta_ioctx,
        oid,
        (char *)inode,
        sizeof(*inode),
        0);

    if (ret < 0)
        return ret;

    if ((size_t)ret != sizeof(*inode))
        return -EIO;

    if (inode->magic != MFS_MAGIC ||
        inode->version != MFS_VERSION ||
        inode->ino != ino)
        return -EIO;

    return 0;
}

int mfs_inode_put(mfs_t *fs, const mfs_inode_t *inode)
{
    char oid[64];

    inode_oid(oid, sizeof(oid), inode->ino);

    return rados_write_full(
        fs->meta_ioctx,
        oid,
        (const char *)inode,
        sizeof(*inode));
}

/* =========================================================
 * 4. Directory operations
 * ========================================================= */

static int dir_get(mfs_t *fs, uint64_t ino, mfs_dir_t **out)
{
    char oid[64];

    dir_oid(oid, sizeof(oid), ino);

    mfs_dir_t *dir = calloc(1, sizeof(*dir));
    if (!dir)
        return -ENOMEM;

    int ret = rados_read(
        fs->meta_ioctx,
        oid,
        (char *)dir,
        sizeof(*dir),
        0);

    if (ret < 0) {
        free(dir);
        return ret;
    }

    if (ret < (int)(sizeof(uint32_t) * 2 + sizeof(uint64_t))) {
        free(dir);
        return -EIO;
    }

    if (dir->magic != MFS_MAGIC ||
        dir->version != MFS_VERSION ||
        dir->count > MFS_DIR_MAX) {
        free(dir);
        return -EIO;
    }

    size_t expected =
        sizeof(uint32_t) * 2 +
        sizeof(uint64_t) +
        dir->count * sizeof(mfs_dentry_t);

    if ((size_t)ret != expected) {
        free(dir);
        return -EIO;
    }

    *out = dir;
    return 0;
}

static int dir_put(mfs_t *fs, uint64_t ino, mfs_dir_t *dir)
{
    char oid[64];

    dir_oid(oid, sizeof(oid), ino);

    size_t len =
        sizeof(uint32_t) * 2 +
        sizeof(uint64_t) +
        dir->count * sizeof(mfs_dentry_t);

    return rados_write_full(
        fs->meta_ioctx,
        oid,
        (const char *)dir,
        len);
}

static int dir_lookup(mfs_t *fs,
                      uint64_t parent_ino,
                      const char *name,
                      uint64_t *ino,
                      uint32_t *type)
{
    mfs_dir_t *dir;

    int ret = dir_get(fs, parent_ino, &dir);
    if (ret < 0)
        return ret;

    ret = -ENOENT;

    for (uint64_t i = 0; i < dir->count; i++) {
        if (strcmp(dir->entries[i].name, name) == 0) {
            *ino = dir->entries[i].ino;

            if (type)
                *type = dir->entries[i].type;

            ret = 0;
            break;
        }
    }

    free(dir);
    return ret;
}

static int dir_add(mfs_t *fs,
                   uint64_t parent_ino,
                   const char *name,
                   uint64_t ino,
                   uint32_t type)
{
    mfs_dir_t *dir;

    int ret = dir_get(fs, parent_ino, &dir);
    if (ret < 0)
        return ret;

    for (uint64_t i = 0; i < dir->count; i++) {
        if (strcmp(dir->entries[i].name, name) == 0) {
            free(dir);
            return -EEXIST;
        }
    }

    if (dir->count >= MFS_DIR_MAX) {
        free(dir);
        return -ENOSPC;
    }

    mfs_dentry_t *entry = &dir->entries[dir->count++];

    memset(entry, 0, sizeof(*entry));

    snprintf(entry->name, sizeof(entry->name), "%s", name);

    entry->ino = ino;
    entry->type = type;

    ret = dir_put(fs, parent_ino, dir);

    free(dir);

    return ret;
}

static int dir_remove(mfs_t *fs,
                      uint64_t parent_ino,
                      const char *name,
                      uint64_t *removed_ino,
                      uint32_t *removed_type)
{
    mfs_dir_t *dir;

    int ret = dir_get(fs, parent_ino, &dir);
    if (ret < 0)
        return ret;

    uint64_t index = dir->count;

    for (uint64_t i = 0; i < dir->count; i++) {
        if (strcmp(dir->entries[i].name, name) == 0) {
            index = i;
            break;
        }
    }

    if (index == dir->count) {
        free(dir);
        return -ENOENT;
    }

    if (removed_ino)
        *removed_ino = dir->entries[index].ino;

    if (removed_type)
        *removed_type = dir->entries[index].type;

    for (uint64_t i = index + 1; i < dir->count; i++)
        dir->entries[i - 1] = dir->entries[i];

    dir->count--;

    ret = dir_put(fs, parent_ino, dir);

    free(dir);

    return ret;
}

/* =========================================================
 * 5. Superblock and inode allocation
 * ========================================================= */

static int alloc_ino(mfs_t *fs, uint64_t *ino)
{
    mfs_super_t super;

    int ret = rados_read(
        fs->meta_ioctx,
        SUPER_OID,
        (char *)&super,
        sizeof(super),
        0);

    if (ret < 0)
        return ret;

    if ((size_t)ret != sizeof(super) ||
        super.magic != MFS_MAGIC ||
        super.version != MFS_VERSION)
        return -EIO;

    *ino = super.next_ino++;

    return rados_write_full(
        fs->meta_ioctx,
        SUPER_OID,
        (const char *)&super,
        sizeof(super));
}

/* =========================================================
 * 6. Filesystem initialization
 * ========================================================= */

int mfs_init(mfs_t *fs)
{
    mfs_super_t super;

    int ret = rados_read(
        fs->meta_ioctx,
        SUPER_OID,
        (char *)&super,
        sizeof(super),
        0);

    if (ret >= 0)
        return -EEXIST;

    if (ret != -ENOENT)
        return ret;

    memset(&super, 0, sizeof(super));

    super.magic = MFS_MAGIC;
    super.version = MFS_VERSION;
    super.next_ino = 2;

    ret = rados_write_full(
        fs->meta_ioctx,
        SUPER_OID,
        (const char *)&super,
        sizeof(super));

    if (ret < 0)
        return ret;

    mfs_inode_t root;

    memset(&root, 0, sizeof(root));

    root.magic = MFS_MAGIC;
    root.version = MFS_VERSION;

    root.ino = MFS_ROOT_INO;
    root.parent_ino = MFS_ROOT_INO;

    root.mode = 0755;
    root.uid = getuid();
    root.gid = getgid();

    root.nlink = 2;
    root.type = MFS_DIR;

    root.atime = root.mtime = root.ctime = mfs_now();

    ret = mfs_inode_put(fs, &root);
    if (ret < 0)
        return ret;

    mfs_dir_t *dir = calloc(1, sizeof(*dir));
    if (!dir)
        return -ENOMEM;

    dir->magic = MFS_MAGIC;
    dir->version = MFS_VERSION;
    dir->count = 0;

    ret = dir_put(fs, MFS_ROOT_INO, dir);

    free(dir);

    return ret;
}

/* =========================================================
 * 7. Path resolution
 * ========================================================= */

static int resolve_path(mfs_t *fs,
                        const char *path,
                        uint64_t *ino,
                        mfs_inode_t *result)
{
    if (!path || path[0] != '/')
        return -EINVAL;

    if (strlen(path) >= MFS_PATH_MAX)
        return -ENAMETOOLONG;

    uint64_t current = MFS_ROOT_INO;

    mfs_inode_t node;

    int ret = mfs_inode_get(fs, current, &node);
    if (ret < 0)
        return ret;

    char tmp[MFS_PATH_MAX];

    strcpy(tmp, path);

    char *saveptr = NULL;

    char *part = strtok_r(tmp, "/", &saveptr);

    while (part) {
        if (strcmp(part, ".") == 0) {
            part = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        if (strcmp(part, "..") == 0) {
            current = node.parent_ino;

            ret = mfs_inode_get(fs, current, &node);
            if (ret < 0)
                return ret;

            part = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        if (strlen(part) > MFS_NAME_MAX)
            return -ENAMETOOLONG;

        if (node.type != MFS_DIR)
            return -ENOTDIR;

        uint64_t child;

        ret = dir_lookup(fs, current, part, &child, NULL);
        if (ret < 0)
            return ret;

        current = child;

        ret = mfs_inode_get(fs, current, &node);
        if (ret < 0)
            return ret;

        part = strtok_r(NULL, "/", &saveptr);
    }

    if (ino)
        *ino = current;

    if (result)
        *result = node;

    return 0;
}

static int resolve_parent(mfs_t *fs,
                          const char *path,
                          uint64_t *parent_ino,
                          char *name)
{
    if (!path || path[0] != '/')
        return -EINVAL;

    if (strlen(path) >= MFS_PATH_MAX)
        return -ENAMETOOLONG;

    char tmp[MFS_PATH_MAX];

    strcpy(tmp, path);

    size_t len = strlen(tmp);

    while (len > 1 && tmp[len - 1] == '/')
        tmp[--len] = '\0';

    char *slash = strrchr(tmp, '/');

    if (!slash || slash[1] == '\0')
        return -EINVAL;

    if (strlen(slash + 1) > MFS_NAME_MAX)
        return -ENAMETOOLONG;

    strcpy(name, slash + 1);

    if (!strcmp(name, ".") || !strcmp(name, ".."))
        return -EINVAL;

    if (slash == tmp) {
        *parent_ino = MFS_ROOT_INO;
    } else {
        *slash = '\0';

        int ret = resolve_path(fs, tmp, parent_ino, NULL);
        if (ret < 0)
            return ret;
    }

    mfs_inode_t parent;

    int ret = mfs_inode_get(fs, *parent_ino, &parent);
    if (ret < 0)
        return ret;

    if (parent.type != MFS_DIR)
        return -ENOTDIR;

    return 0;
}

/* =========================================================
 * 8. Create inode / mkdir / create
 * ========================================================= */

static int create_node(mfs_t *fs,
                       const char *path,
                       mode_t mode,
                       uint32_t type)
{
    uint64_t parent_ino;

    char name[MFS_NAME_MAX + 1];

    int ret = resolve_parent(
        fs, path, &parent_ino, name);

    if (ret < 0)
        return ret;

    uint64_t existing;

    ret = dir_lookup(
        fs, parent_ino, name, &existing, NULL);

    if (ret == 0)
        return -EEXIST;

    if (ret != -ENOENT)
        return ret;

    uint64_t ino;

    ret = alloc_ino(fs, &ino);
    if (ret < 0)
        return ret;

    mfs_inode_t inode;

    memset(&inode, 0, sizeof(inode));

    inode.magic = MFS_MAGIC;
    inode.version = MFS_VERSION;

    inode.ino = ino;
    inode.parent_ino = parent_ino;

    inode.mode = mode;
    inode.uid = getuid();
    inode.gid = getgid();

    inode.nlink = type == MFS_DIR ? 2 : 1;
    inode.type = type;

    inode.atime = inode.mtime = inode.ctime = mfs_now();

    ret = mfs_inode_put(fs, &inode);
    if (ret < 0)
        return ret;

    if (type == MFS_DIR) {
        mfs_dir_t *dir = calloc(1, sizeof(*dir));
        if (!dir)
            return -ENOMEM;

        dir->magic = MFS_MAGIC;
        dir->version = MFS_VERSION;

        ret = dir_put(fs, ino, dir);

        free(dir);

        if (ret < 0)
            return ret;
    }

    ret = dir_add(fs, parent_ino, name, ino, type);
    if (ret < 0)
        return ret;

    mfs_inode_t parent;

    if (mfs_inode_get(fs, parent_ino, &parent) == 0) {
        parent.mtime = parent.ctime = mfs_now();

        if (type == MFS_DIR)
            parent.nlink++;

        mfs_inode_put(fs, &parent);
    }

    return 0;
}

int mfs_mkdir(mfs_t *fs, const char *path, mode_t mode)
{
    return create_node(fs, path, mode, MFS_DIR);
}

int mfs_create(mfs_t *fs, const char *path, mode_t mode)
{
    return create_node(fs, path, mode, MFS_FILE);
}

/* =========================================================
 * 9. Directory listing
 * ========================================================= */

int mfs_ls(mfs_t *fs, const char *path)
{
    uint64_t ino;

    mfs_inode_t inode;

    int ret = resolve_path(fs, path, &ino, &inode);
    if (ret < 0)
        return ret;

    if (inode.type != MFS_DIR)
        return -ENOTDIR;

    mfs_dir_t *dir;

    ret = dir_get(fs, ino, &dir);
    if (ret < 0)
        return ret;

    printf("%-32s %-12s %s\n", "NAME", "INODE", "TYPE");

    for (uint64_t i = 0; i < dir->count; i++) {
        printf("%-32s %-12" PRIu64 " %s\n",
               dir->entries[i].name,
               dir->entries[i].ino,
               dir->entries[i].type == MFS_DIR
                   ? "directory" : "file");
    }

    free(dir);

    return 0;
}

/* =========================================================
 * 10. Stat
 * ========================================================= */

int mfs_stat(mfs_t *fs, const char *path)
{
    mfs_inode_t inode;

    int ret = resolve_path(fs, path, NULL, &inode);
    if (ret < 0)
        return ret;

    printf("inode      : %" PRIu64 "\n", inode.ino);
    printf("parent     : %" PRIu64 "\n", inode.parent_ino);
    printf("type       : %s\n",
           inode.type == MFS_DIR ? "directory" : "file");
    printf("mode       : %04o\n", inode.mode);
    printf("uid        : %u\n", inode.uid);
    printf("gid        : %u\n", inode.gid);
    printf("size       : %" PRIu64 "\n", inode.size);
    printf("nlink      : %u\n", inode.nlink);
    printf("atime      : %" PRIu64 "\n", inode.atime);
    printf("mtime      : %" PRIu64 "\n", inode.mtime);
    printf("ctime      : %" PRIu64 "\n", inode.ctime);

    return 0;
}

/* =========================================================
 * 11. File write
 * ========================================================= */

int mfs_write(mfs_t *fs,
              const char *path,
              const void *buf,
              size_t len,
              uint64_t offset)
{
    uint64_t ino;

    mfs_inode_t inode;

    int ret = resolve_path(fs, path, &ino, &inode);
    if (ret < 0)
        return ret;

    if (inode.type != MFS_FILE)
        return -EISDIR;

    if (len > 0 && offset > UINT64_MAX - len)
        return -EFBIG;

    const char *p = buf;

    size_t done = 0;

    while (done < len) {
        uint64_t pos = offset + done;

        uint64_t objno = pos / MFS_OBJECT_SIZE;

        uint64_t objoff = pos % MFS_OBJECT_SIZE;

        size_t chunk = MFS_OBJECT_SIZE - objoff;

        if (chunk > len - done)
            chunk = len - done;

        char oid[96];

        data_oid(oid, sizeof(oid), ino, objno);

        /*
         * rados_write returns 0 on success,
         * negative errno-style value on failure.
         */
        ret = rados_write(
            fs->data_ioctx,
            oid,
            p + done,
            chunk,
            objoff);

        if (ret < 0)
            return ret;

        done += chunk;
    }

    if (offset + len > inode.size)
        inode.size = offset + len;

    inode.mtime = inode.ctime = mfs_now();

    return mfs_inode_put(fs, &inode);
}

/* =========================================================
 * 12. File read  (with sparse-hole support)
 * ========================================================= */

int mfs_read(mfs_t *fs,
             const char *path,
             void *buf,
             size_t len,
             uint64_t offset)
{
    uint64_t ino;

    mfs_inode_t inode;

    int ret = resolve_path(fs, path, &ino, &inode);
    if (ret < 0)
        return ret;

    if (inode.type != MFS_FILE)
        return -EISDIR;

    if (offset >= inode.size)
        return 0;

    if ((uint64_t)len > inode.size - offset)
        len = (size_t)(inode.size - offset);

    char *p = buf;

    size_t done = 0;

    while (done < len) {
        uint64_t pos = offset + done;

        uint64_t objno = pos / MFS_OBJECT_SIZE;

        uint64_t objoff = pos % MFS_OBJECT_SIZE;

        size_t chunk = MFS_OBJECT_SIZE - objoff;

        if (chunk > len - done)
            chunk = len - done;

        char oid[96];

        data_oid(oid, sizeof(oid), ino, objno);

        ret = rados_read(
            fs->data_ioctx,
            oid,
            p + done,
            chunk,
            objoff);

        /* Sparse hole: object does not exist -> return zeroes. */
        if (ret == -ENOENT) {
            memset(p + done, 0, chunk);
            done += chunk;
            continue;
        }

        if (ret < 0)
            return ret;

        /*
         * A short read means the rest of this logical
         * range is a sparse hole. Return zero bytes there.
         */
        if ((size_t)ret < chunk)
            memset(p + done + ret, 0, chunk - ret);

        done += chunk;
    }

    return (int)done;
}

/* =========================================================
 * 13. cat
 * ========================================================= */

int mfs_cat(mfs_t *fs, const char *path)
{
    mfs_inode_t inode;

    int ret = resolve_path(fs, path, NULL, &inode);
    if (ret < 0)
        return ret;

    if (inode.type != MFS_FILE)
        return -EISDIR;

    char *buf = malloc(1024 * 1024);
    if (!buf)
        return -ENOMEM;

    uint64_t offset = 0;

    while (offset < inode.size) {
        size_t len = inode.size - offset > 1024 * 1024
                   ? 1024 * 1024
                   : (size_t)(inode.size - offset);

        ret = mfs_read(fs, path, buf, len, offset);

        if (ret < 0) {
            free(buf);
            return ret;
        }

        if (ret == 0)
            break;

        fwrite(buf, 1, ret, stdout);

        offset += ret;
    }

    free(buf);

    return 0;
}

/* =========================================================
 * 14. Unlink
 * ========================================================= */

int mfs_unlink(mfs_t *fs, const char *path)
{
    uint64_t parent_ino;

    char name[MFS_NAME_MAX + 1];

    int ret = resolve_parent(
        fs, path, &parent_ino, name);

    if (ret < 0)
        return ret;

    uint64_t ino;

    uint32_t type;

    ret = dir_lookup(
        fs, parent_ino, name, &ino, &type);

    if (ret < 0)
        return ret;

    mfs_inode_t inode;

    ret = mfs_inode_get(fs, ino, &inode);
    if (ret < 0)
        return ret;

    if (type == MFS_DIR) {
        mfs_dir_t *dir;

        ret = dir_get(fs, ino, &dir);
        if (ret < 0)
            return ret;

        if (dir->count != 0) {
            free(dir);
            return -ENOTEMPTY;
        }

        free(dir);

        char oid[64];

        dir_oid(oid, sizeof(oid), ino);

        ret = rados_remove(fs->meta_ioctx, oid);
        if (ret < 0 && ret != -ENOENT)
            return ret;
    } else {
        uint64_t objects =
            (inode.size + MFS_OBJECT_SIZE - 1) / MFS_OBJECT_SIZE;

        for (uint64_t i = 0; i < objects; i++) {
            char oid[96];

            data_oid(oid, sizeof(oid), ino, i);

            ret = rados_remove(fs->data_ioctx, oid);

            if (ret < 0 && ret != -ENOENT)
                return ret;
        }
    }

    ret = dir_remove(
        fs, parent_ino, name, NULL, NULL);

    if (ret < 0)
        return ret;

    char oid[64];

    inode_oid(oid, sizeof(oid), ino);

    ret = rados_remove(fs->meta_ioctx, oid);

    if (ret < 0)
        return ret;

    mfs_inode_t parent;

    if (mfs_inode_get(fs, parent_ino, &parent) == 0) {
        parent.mtime = parent.ctime = mfs_now();

        if (type == MFS_DIR && parent.nlink > 2)
            parent.nlink--;

        mfs_inode_put(fs, &parent);
    }

    return 0;
}
