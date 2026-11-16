#include "net.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

int net_init(void)
{
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

netfd_t net_listen(const char *host, int port, int backlog)
{
    netfd_t fd;
    struct sockaddr_in addr;
    int one = 1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (host == NULL || *host == '\0' || strcmp(host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        return NET_FD_BAD;
    }

#ifdef _WIN32
    fd = (netfd_t)socket(AF_INET, SOCK_STREAM, 0);
    if (fd == NET_FD_BAD)
        return NET_FD_BAD;
    setsockopt((SOCKET)fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
#else
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return NET_FD_BAD;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#endif

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        net_close(fd);
        return NET_FD_BAD;
    }
    if (listen(fd, backlog) != 0) {
        net_close(fd);
        return NET_FD_BAD;
    }
    return fd;
}

netfd_t net_accept(netfd_t lfd, char *peer, size_t peer_cap)
{
    struct sockaddr_in addr;
    socklen_t alen = (socklen_t)sizeof(addr);
    netfd_t fd;

    memset(&addr, 0, sizeof(addr));
#ifdef _WIN32
    fd = (netfd_t)accept((SOCKET)lfd, (struct sockaddr *)&addr, &alen);
#else
    fd = accept(lfd, (struct sockaddr *)&addr, &alen);
#endif
    if (fd == NET_FD_BAD)
        return NET_FD_BAD;

    if (peer != NULL && peer_cap > 0) {
        char ip[64] = "";
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        snprintf(peer, peer_cap, "%s:%u", ip, (unsigned)ntohs(addr.sin_port));
    }
    return fd;
}

long net_read(netfd_t fd, void *buf, size_t n)
{
    if (n == 0)
        return 0;
#ifdef _WIN32
    return (long)recv((SOCKET)fd, (char *)buf, (int)(n > 0x7fffffff ? 0x7fffffff : n), 0);
#else
    for (;;) {
        ssize_t r = read(fd, buf, n);
        if (r < 0 && errno == EINTR)
            continue;
        return (long)r;
    }
#endif
}

long net_write(netfd_t fd, const void *buf, size_t n)
{
    if (n == 0)
        return 0;
#ifdef _WIN32
    return (long)send((SOCKET)fd, (const char *)buf, (int)(n > 0x7fffffff ? 0x7fffffff : n), 0);
#else
    for (;;) {
        ssize_t r = send(fd, buf, n, MSG_NOSIGNAL);
        if (r < 0 && errno == EINTR)
            continue;
        return (long)r;
    }
#endif
}

void net_close(netfd_t fd)
{
#ifdef _WIN32
    closesocket((SOCKET)fd);
#else
    close(fd);
#endif
}

void net_set_timeout(netfd_t fd, int seconds)
{
#ifdef _WIN32
    DWORD tv = (DWORD)seconds * 1000;
    setsockopt((SOCKET)fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    setsockopt((SOCKET)fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

void net_set_nodelay(netfd_t fd)
{
    int one = 1;
#ifdef _WIN32
    setsockopt((SOCKET)fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
#else
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#endif
}
