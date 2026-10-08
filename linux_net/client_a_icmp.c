#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>   // htons htonl
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h> // struct ethhdr
#include "checksum.h"

#define TAP_NAME "tap0"

int tap_open()
{
    int fd = open("/dev/net/tun", O_RDWR);
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, TAP_NAME, IFNAMSIZ);
    ioctl(fd, TUNSETIFF, &ifr);
    return fd;
}

struct iphdr {
    uint8_t  ihl:4, version:4;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};

struct icmphdr {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
};

int main()
{
    int tap_fd = tap_open();

    // ========== 你的tap0 MAC ==========
    uint8_t tap_mac[6] = {0x5e,0x64,0x7a,0xa2,0x84,0x7f};
    uint8_t src_mac[6] = {0x11,0x22,0x33,0x44,0x55,0x66};

    struct ethhdr  eth;
    struct iphdr   ip;
    struct icmphdr icmp;

    // 二层以太网头
    memcpy(eth.h_dest, tap_mac, 6);
    memcpy(eth.h_source, src_mac, 6);
    eth.h_proto = htons(0x0800); // IPv4

    // IP头
    ip.version = 4;
    ip.ihl     = 5;
    ip.tos     = 0;
    ip.tot_len = htons(20 + 8); // iphdr(20)+icmp(8)
    ip.id      = htons(0x1234);
    ip.frag_off= 0;
    ip.ttl     = 64;
    ip.protocol= 1; // ICMP
    ip.saddr   = htonl(0x0a000002); // 源IP：10.0.0.2
    ip.daddr   = htonl(0x0a000001); // 目的IP：10.0.0.1 tap0地址
    ip.check   = 0;
    ip.check   = ip_checksum(&ip, 20);

    // ICMP Echo Request
    icmp.type = 8;
    icmp.code = 0;
    icmp.checksum = 0;
    icmp.id = htons(0xabcd);
    icmp.seq = htons(1);
    icmp.checksum = ip_checksum(&icmp, 8);

    // 拼接完整帧
    char frame[2048];
    int pos = 0;
    memcpy(frame+pos, &eth, sizeof(eth)); pos += sizeof(eth);
    memcpy(frame+pos, &ip, sizeof(ip)); pos += sizeof(ip);
    memcpy(frame+pos, &icmp, sizeof(icmp)); pos += sizeof(icmp);

    // write 注入TAP
    ssize_t wlen = write(tap_fd, frame, pos);
    printf("进程A ICMP: write %zd bytes frame into tap0\n", wlen);

    close(tap_fd);
    return 0;
}
