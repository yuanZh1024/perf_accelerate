#include "os.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#define os_open _open
#define os_read _read
#define os_write _write
#define os_lseek _lseeki64
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static int is_sep(char c)
{
    return c == '/' || c == '\\';
}

int os_mkdir_p(const char *dir)
{
    char tmp[2048];
    size_t len, i;
    struct stat st;

    if (dir == NULL || *dir == '\0')
        return -1;

    len = strlen(dir);
    if (len >= sizeof(tmp))
        return -1;
    memcpy(tmp, dir, len + 1);

    /* strip trailing separators */
    while (len > 1 && is_sep(tmp[len - 1]))
        tmp[--len] = '\0';

    for (i = 1; i < len; i++) {
        if (!is_sep(tmp[i]))
            continue;
        tmp[i] = '\0';
#ifdef _WIN32
        _mkdir(tmp);
#else
        mkdir(tmp, 0755);
#endif
        tmp[i] = '/';
    }

#ifdef _WIN32
    if (_mkdir(tmp) != 0 && errno != EEXIST)
        return -1;
#else
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
#endif

    if (stat(tmp, &st) != 0 || !(st.st_mode & S_IFDIR))
        return -1;
    return 0;
}

int os_mkdir_parent(const char *file)
{
    char tmp[2048];
    char *slash;

    if (file == NULL)
        return -1;
    if (strlen(file) >= sizeof(tmp))
        return -1;
    strcpy(tmp, file);

    slash = strrchr(tmp, '/');
    if (slash == NULL)
        return 0;
    *slash = '\0';
    if (tmp[0] == '\0')
        return 0;
    return os_mkdir_p(tmp);
}

int os_rmdir_if_empty(const char *dir)
{
#ifdef _WIN32
    return _rmdir(dir);
#else
    return rmdir(dir);
#endif
}

int os_open_read(const char *path)
{
#ifdef _WIN32
    return os_open(path, _O_RDONLY | _O_BINARY);
#else
    int fd;
    do {
        fd = open(path, O_RDONLY | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    return fd;
#endif
}

int os_open_rw_trunc(const char *path)
{
#ifdef _WIN32
    return os_open(path, _O_RDWR | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    int fd;
    do {
        fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    } while (fd < 0 && errno == EINTR);
    return fd;
#endif
}

int os_open_rw(const char *path)
{
#ifdef _WIN32
    return os_open(path, _O_RDWR | _O_BINARY);
#else
    int fd;
    do {
        fd = open(path, O_RDWR | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    return fd;
#endif
}

int os_close(int fd)
{
#ifdef _WIN32
    return _close(fd);
#else
    return close(fd);
#endif
}

long long os_pread(int fd, void *buf, size_t len, uint64_t off)
{
#ifdef _WIN32
    if (os_lseek(fd, (long long)off, SEEK_SET) < 0)
        return -1;
    return (long long)os_read(fd, buf, (unsigned)len);
#else
    size_t done = 0;
    while (done < len) {
        ssize_t n = pread(fd, (char *)buf + done, len - done, (off_t)(off + done));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            break;
        done += (size_t)n;
    }
    return (long long)done;
#endif
}

long long os_pwrite(int fd, const void *buf, size_t len, uint64_t off)
{
#ifdef _WIN32
    if (os_lseek(fd, (long long)off, SEEK_SET) < 0)
        return -1;
    return (long long)os_write(fd, buf, (unsigned)len);
#else
    size_t done = 0;
    while (done < len) {
        ssize_t n = pwrite(fd, (const char *)buf + done, len - done, (off_t)(off + done));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        done += (size_t)n;
    }
    return (long long)done;
#endif
}

int os_fsync(int fd)
{
#ifdef _WIN32
    return _commit(fd);
#else
    return fsync(fd);
#endif
}

int os_rename(const char *from, const char *to)
{
#ifdef _WIN32
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
#else
    return rename(from, to);
#endif
}

int os_unlink(const char *path)
{
#ifdef _WIN32
    return _unlink(path);
#else
    return unlink(path);
#endif
}

int os_file_size(const char *path, uint64_t *size)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;
    if (size)
        *size = (uint64_t)st.st_size;
    return 0;
}

int os_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

int os_is_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    return (st.st_mode & S_IFDIR) ? 1 : 0;
}
