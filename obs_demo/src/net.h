/*
 * net.h - blocking TCP helpers (POSIX sockets on the target platform).
 */
#ifndef OBS_NET_H
#define OBS_NET_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
typedef intptr_t netfd_t;
#else
typedef int netfd_t;
#endif

#define NET_FD_BAD ((netfd_t)-1)

int net_init(void);
netfd_t net_listen(const char *host, int port, int backlog);
netfd_t net_accept(netfd_t lfd, char *peer, size_t peer_cap);
long net_read(netfd_t fd, void *buf, size_t n);
long net_write(netfd_t fd, const void *buf, size_t n);
void net_close(netfd_t fd);
void net_set_timeout(netfd_t fd, int seconds);
void net_set_nodelay(netfd_t fd);

#endif /* OBS_NET_H */
