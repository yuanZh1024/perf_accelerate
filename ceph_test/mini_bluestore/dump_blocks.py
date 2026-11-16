#!/usr/bin/env python3
"""查看裸盘前 N 块，每块前 96 字节，hex + ascii 对照。

用法: sudo python3 dump_blocks.py [设备] [块数]
例:   sudo python3 dump_blocks.py /dev/sde 16

实现要点：O_DIRECT 绕过页缓存，看到的是盘上真实字节；
读整块后切片取前 96 字节，缓冲区用 mmap 保证页对齐（O_DIRECT 硬性要求）。
"""
import os
import sys
import mmap

BLOCK = 4096          # 块大小
DUMP_BYTES = 96       # 每块显示的字节数
DEFAULT_N = 16        # 默认显示块数


def hexdump(data, base=0):
    """把字节串格式化成 hex + ascii 对照，每行 16 字节。

    @param data  待格式化的字节串（bytes）。
    @param base  显示的首行地址偏移量，默认 0；传入块内偏移可让地址列连续。
    @return      多行字符串，每行形如 "00000000:  41 41 ..  AAAA...."。
    """
    lines = []
    for off in range(0, len(data), 16):
        chunk = data[off:off + 16]
        hx = " ".join("%02x" % b for b in chunk)
        hx = hx.ljust(16 * 3 - 1)   # 补齐到固定列宽，ascii 列对齐
        asc = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append("%08x:  %s  %s" % (base + off, hx, asc))
    return "\n".join(lines)


def main():
    """遍历裸盘前 N 块并逐块打印：无入参，从命令行读取设备与块数。

    命令行参数（均以 1 开始计）：
      argv[1]  块设备路径，默认 "/dev/sde"；
      argv[2]  要查看的块数，默认 DEFAULT_N(16)。
    @return  无；结果直接打印到 stdout。
    """
    dev = sys.argv[1] if len(sys.argv) > 1 else "/dev/sde"
    n = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_N

    fd = os.open(dev, os.O_RDONLY | os.O_DIRECT)
    buf = mmap.mmap(-1, BLOCK)   # 页对齐缓冲区，满足 O_DIRECT
    try:
        for i in range(n):
            os.preadv(fd, [buf], i * BLOCK)
            print("===== 块 %d =====" % i)
            print(hexdump(bytes(buf[:DUMP_BYTES])))
            print()
    finally:
        buf.close()
        os.close(fd)


if __name__ == "__main__":
    main()
