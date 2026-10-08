#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#define BUF_SIZE 2048
/*
open("/dev/net/tun") 打开 tun/tun 字符设备；
ioctl(TUNSETIFF) 向内核发送请求：帮我注册一个 struct net_device（tap0），并且把这个 net_device 绑定到当前 fd；
一旦成功：
read(fd)：内核要从 tap 网卡发送报文 → 报文给到用户态；
write(fd, buf)：用户态提交二层帧 → 内核 tap 驱动调用netif_rx，假装这个帧是 tap 网卡硬件收到，送入内核协议栈；
当程序退出，fd 关闭 → 内核自动销毁这个 tap net_device，tap0 消失。

*/

// 创建tap设备，返回fd，ifname输出tap名字（如tap0）
int tap_create(char *ifname)
{
    struct ifreq ifr;
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        perror("open /dev/net/tun");
        exit(1);
    }

    memset(&ifr, 0, sizeof(ifr));
    // IFF_TAP：二层tap；IFF_NO_PI：不附带packet info头，只返回原始以太网帧
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (*ifname) {
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    }

    // ioctl TUNSETIFF命令--> 在内核分配 TAP 虚拟网络设备，绑定到这个 file 对象
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        perror("ioctl TUNSETIFF");
        close(fd);
        exit(1);
    }
    strcpy(ifname, ifr.ifr_name);
    return fd;
}

int main()
{
    char tap_name[IFNAMSIZ] = {0};
    int tap_fd = tap_create(tap_name);
    printf("Created tap device: %s\n", tap_name);
    printf("tap fd = %d\n", tap_fd);
    printf("Now you can configure it in another shell:\n");
    printf("sudo ip link set %s up\n", tap_name);
    printf("sudo ip addr add 10.0.0.1/24 dev %s\n", tap_name);
    printf("\nWaiting for ethernet frames...\n");

    unsigned char buf[BUF_SIZE];
    ssize_t n;
    while (1) {
        // read：读取内核发给tap的二层帧（内核从tap网卡发包 → 用户态读到）
        n = read(tap_fd, buf, BUF_SIZE);
        if (n < 0) {
            perror("read tap");
            break;
        }
        printf("==== Got frame, len=%zd ====\n", n);
        // 简单打印前6字节目的MAC
        printf("Dst MAC: %02x:%02x:%02x:%02x:%02x\n",
               buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);
    }
    /*
      // ======================
    // 构造极简二层帧示例（简化演示，不是合法IP包！）
    // 以太网头：DST_MAC(6), SRC_MAC(6), TYPE(2)
    unsigned char frame[] = {
        0x00,0x00,0x00,0x00,0x00,0x01, // DST MAC
        0x00,0x00,0x00,0x00,0x00,0x02, // SRC MAC
        0x08,0x00,                     // EtherType: IPv4 (0x0800)
        // 后面可以继续填充IP头+payload，这里省略
    };
    ssize_t ret = write(tap_fd, frame, sizeof(frame));
    if(ret <0) perror("write tap");
    else printf("write frame ok, len=%zd\n", ret);
    
    */
    close(tap_fd);
    return 0;
}