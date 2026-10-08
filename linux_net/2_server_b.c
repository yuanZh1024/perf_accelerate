#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUF_LEN 1024

int main() {
    int sock_fd, conn_fd;
    struct sockaddr_in addr;
    char buf[BUF_LEN];

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    // 绑定tap0的IP
    addr.sin_addr.s_addr = htonl(0x0a000001); 

    bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock_fd, 1);
    printf("进程B TCP服务启动: 10.0.0.1:8080\n");

    while (1) {
        conn_fd = accept(sock_fd, NULL, NULL);
        ssize_t n = read(conn_fd, buf, BUF_LEN);
        if (n > 0) {
            printf("【进程B 收到业务数据】: %.*s\n", (int)n, buf);
        }
        close(conn_fd);
    }
    return 0;
}