/*
 * os.h - thin file-system layer.
 *
 * The store only needs a handful of operations, and keeping them in one
 * place keeps store.c free of platform #ifdef noise.
 *
 * The primary (and supported) implementation is POSIX, targeting
 * Ubuntu 22.04. A small shim for Windows exists only so that the test
 * suite can also run on a developer workstation.
 */
#ifndef OBS_OS_H
#define OBS_OS_H

#include <stddef.h>
#include <stdint.h>

#define OS_FD_BAD (-1)

int os_mkdir_p(const char *dir);          /* create dir and parents */
int os_mkdir_parent(const char *file);    /* create dirname(file) */
int os_rmdir_if_empty(const char *dir);

int os_open_read(const char *path);
int os_open_rw_trunc(const char *path);
int os_open_rw(const char *path); /* read/write, existing file, no truncate */
int os_close(int fd);

/* Return bytes transferred, or -1 on error. */
long long os_pread(int fd, void *buf, size_t len, uint64_t off);
long long os_pwrite(int fd, const void *buf, size_t len, uint64_t off);

int os_fsync(int fd);
int os_rename(const char *from, const char *to);
int os_unlink(const char *path);
int os_file_size(const char *path, uint64_t *size);
int os_exists(const char *path);
int os_is_dir(const char *path);

#endif /* OBS_OS_H */
