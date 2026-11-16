#!/usr/bin/env python3
"""第 1 步：把整块 /dev/sde 当成一个"块的数组"来读写。

盘上没有"文件"的概念，只有"第几块"。
后面要自己回答"这个对象存在哪几块" —— 这就是元数据的由来。

运行：sudo python3 disk.py
"""
import os
import mmap

BLOCK = 4096


class Disk:
    def __init__(self, path):
        # O_DIRECT：绕过页缓存，直接和磁盘交互
        self.fd = os.open(path, os.O_RDWR | os.O_DIRECT | os.O_SYNC)
        # O_DIRECT 要求内存缓冲区也按页对齐，mmap 申请的内存天然满足
        self.buf = mmap.mmap(-1, BLOCK)

    def write_block(self, n, data):
        assert len(data) <= BLOCK
        self.buf.seek(0)
        self.buf.write(data.ljust(BLOCK, b"\0"))
        os.pwritev(self.fd, [self.buf], n * BLOCK)

    def read_block(self, n):
        os.preadv(self.fd, [self.buf], n * BLOCK)
        return bytes(self.buf)


if __name__ == "__main__":
    d = Disk("/dev/sde")
    d.write_block(10, b"hello bluestore")
    print(d.read_block(10)[:15])
