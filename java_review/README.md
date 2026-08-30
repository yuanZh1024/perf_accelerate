
## 一个java程序的内存地址空间布局
可以看到有大量的 [anon]

https://www.doubao.com/chat/38438165011302658

root@dev-virtual-machine:/home/dev# pmap 12822
12822:   java -Xms64M -Xmx128M TestMaps
00000000f8000000  44032K rw---   [ anon ]
00000000fab00000  43520K -----   [ anon ]
00000000fd580000  21504K rw---   [ anon ]
00000000fea80000  22016K -----   [ anon ]
0000000100000000    512K rw---   [ anon ]
0000000100080000 1048064K -----   [ anon ]
00005934ffbfe000      4K r---- java
00005934ffbff000      4K r-x-- java
00005934ffc00000      4K r---- java
00005934ffc01000      4K r---- java
00005934ffc02000      4K rw--- java
00005935342d8000    132K rw---   [ anon ]
00007a1ff0000000    132K rw---   [ anon ]
00007a1ff0021000  65404K -----   [ anon ]
00007a1ff8000000    132K rw---   [ anon ]
00007a1ff8021000  65404K -----   [ anon ]
00007a1ffc000000    132K rw---   [ anon ]
00007a1ffc021000  65404K -----   [ anon ]
00007a2000000000    132K rw---   [ anon ]
00007a2000021000  65404K -----   [ anon ]
00007a2004000000   1536K rw---   [ anon ]
00007a2004180000  64000K -----   [ anon ]
00007a2008000000    132K rw---   [ anon ]
00007a2008021000  65404K -----   [ anon ]
00007a200c000000   1164K rw---   [ anon ]
00007a200c123000  64372K -----   [ anon ]
00007a2010000000    132K rw---   [ anon ]
00007a2010021000  65404K -----   [ anon ]
00007a2014000000    132K rw---   [ anon ]
00007a2014021000  65404K -----   [ anon ]
00007a2018000000    132K rw---   [ anon ]
00007a2018021000  65404K -----   [ anon ]
00007a201c000000    132K rw---   [ anon ]
00007a201c021000  65404K -----   [ anon ]
00007a2020000000    132K rw---   [ anon ]
00007a2020021000  65404K -----   [ anon ]
00007a2028000000    132K rw---   [ anon ]
00007a2028021000  65404K -----   [ anon ]
00007a2030000000    132K rw---   [ anon ]
00007a2030021000  65404K -----   [ anon ]
00007a20342fc000     12K -----   [ anon ]
00007a20342ff000   1012K rw---   [ anon ]
00007a20343fc000      4K -----   [ anon ]
00007a20343fd000   1024K rw---   [ anon ]
00007a20344fd000     12K -----   [ anon ]
00007a2034500000   1012K rw---   [ anon ]
00007a20345fd000      4K -----   [ anon ]
00007a20345fe000     12K -----   [ anon ]
00007a2034601000   1012K rw---   [ anon ]
00007a20346fe000      4K -----   [ anon ]
00007a20346ff000     12K -----   [ anon ]
00007a2034702000   1012K rw---   [ anon ]
00007a20347ff000      4K -----   [ anon ]
00007a2034800000     12K -----   [ anon ]
00007a2034803000   1012K rw---   [ anon ]
00007a2034900000     12K -----   [ anon ]
00007a2034903000   1012K rw---   [ anon ]
00007a2034a00000   5580K r---- locale-archive
00007a2035000000   2496K rwx--   [ anon ]
00007a2035270000 243264K -----   [ anon ]
00007a2044000000   2160K rw---   [ anon ]
00007a204421c000  63376K -----   [ anon ]
00007a2048039000    196K rw---   [ anon ]
00007a204806a000     20K r---- libnet.so
00007a204806f000     68K r-x-- libnet.so
00007a2048080000     16K r---- libnet.so
00007a2048084000      4K r---- libnet.so
00007a2048085000      4K rw--- libnet.so
00007a2048086000     28K r---- libnio.so
00007a204808d000     36K r-x-- libnio.so
00007a2048096000     12K r---- libnio.so
00007a2048099000      4K r---- libnio.so
00007a204809a000      4K rw--- libnio.so
00007a204809b000     40K r--s- localedata.jar
00007a20480a5000     12K r--s- icedtea-sound.jar
00007a20480a8000      8K r--s- sunec.jar
00007a20480aa000     12K r--s- sunpkcs11.jar
00007a20480ad000    112K r--s- cldrdata.jar
00007a20480c9000     12K -----   [ anon ]
00007a20480cc000   1012K rw---   [ anon ]
00007a20481c9000     12K -----   [ anon ]
00007a20481cc000   1012K rw---   [ anon ]
00007a20482c9000      4K -----   [ anon ]
00007a20482ca000   1024K rw---   [ anon ]
00007a20483ca000  15948K rw---   [ anon ]
00007a204935d000   1864K r--s- rt.jar
00007a204952f000   5188K rw---   [ anon ]
00007a2049a40000   3840K -----   [ anon ]
00007a2049e00000   4096K rw---   [ anon ]
00007a204a200000    108K r--s- nashorn.jar
00007a204a21b000    132K rw---   [ anon ]
00007a204a23c000      4K -----   [ anon ]
00007a204a23d000   1024K rw---   [ anon ]
00007a204a33d000      4K -----   [ anon ]
00007a204a33e000   1024K rw---   [ anon ]
00007a204a43e000      4K -----   [ anon ]
00007a204a43f000   1024K rw---   [ anon ]
00007a204a53f000      4K -----   [ anon ]
00007a204a540000   1024K rw---   [ anon ]
00007a204a640000     40K rw---   [ anon ]
00007a204a64a000   3800K -----   [ anon ]
00007a204aa00000    616K r---- libstdc++.so.6.0.30
00007a204aa9a000   1092K r-x-- libstdc++.so.6.0.30
00007a204abab000    444K r---- libstdc++.so.6.0.30
00007a204ac1a000      4K ----- libstdc++.so.6.0.30
00007a204ac1b000     44K r---- libstdc++.so.6.0.30
00007a204ac26000     12K rw--- libstdc++.so.6.0.30
00007a204ac29000     12K rw---   [ anon ]
00007a204ac2c000      8K r--s- dnsns.jar
00007a204ac2e000      8K r--s- zipfs.jar
00007a204ac30000    656K rw---   [ anon ]
00007a204acd4000     80K -----   [ anon ]
00007a204ace8000     48K rw---   [ anon ]
00007a204acf4000     40K -----   [ anon ]
00007a204acfe000      4K rw---   [ anon ]
00007a204acff000      4K -----   [ anon ]
00007a204ad00000     12K -----   [ anon ]
00007a204ad03000   1012K rw---   [ anon ]
00007a204ae00000   2008K r---- libjvm.so
00007a204aff6000   9460K r-x-- libjvm.so
00007a204b933000   1732K r---- libjvm.so
00007a204bae4000      4K ----- libjvm.so
00007a204bae5000    600K r---- libjvm.so
00007a204bb7b000    164K rw--- libjvm.so
00007a204bba4000    204K rw---   [ anon ]
00007a204bbd8000    160K rw---   [ anon ]
00007a204bc00000    160K r---- libc.so.6
00007a204bc28000   1620K r-x-- libc.so.6
00007a204bdbd000    352K r---- libc.so.6
00007a204be15000      4K ----- libc.so.6
00007a204be16000     16K r---- libc.so.6
00007a204be1a000      8K rw--- libc.so.6
00007a204be1c000     52K rw---   [ anon ]
00007a204be29000     24K r--s- sunjce_provider.jar
00007a204be2f000     28K r--s- jfr.jar
00007a204be36000     88K rw---   [ anon ]
00007a204be4c000     84K -----   [ anon ]
00007a204be61000     12K r---- libzip.so
00007a204be64000     20K r-x-- libzip.so
00007a204be69000      8K r---- libzip.so
00007a204be6b000      4K r---- libzip.so
00007a204be6c000      4K rw--- libzip.so
00007a204be6d000     52K r---- libjava.so
00007a204be7a000     96K r-x-- libjava.so
00007a204be92000     28K r---- libjava.so
00007a204be99000      4K r---- libjava.so
00007a204be9a000      8K rw--- libjava.so
00007a204be9c000     20K r---- libverify.so
00007a204bea1000     36K r-x-- libverify.so
00007a204beaa000      8K r---- libverify.so
00007a204beac000      8K r---- libverify.so
00007a204beae000      4K rw--- libverify.so
00007a204beaf000      4K r---- librt.so.1
00007a204beb0000      4K r-x-- librt.so.1
00007a204beb1000      4K r---- librt.so.1
00007a204beb2000      4K r---- librt.so.1
00007a204beb3000      4K rw--- librt.so.1
00007a204beb4000     12K r---- libgcc_s.so.1
00007a204beb7000     92K r-x-- libgcc_s.so.1
00007a204bece000     16K r---- libgcc_s.so.1
00007a204bed2000      4K r---- libgcc_s.so.1
00007a204bed3000      4K rw--- libgcc_s.so.1
00007a204bed4000     56K r---- libm.so.6
00007a204bee2000    496K r-x-- libm.so.6
00007a204bf5e000    364K r---- libm.so.6
00007a204bfb9000      4K r---- libm.so.6
00007a204bfba000      4K rw--- libm.so.6
00007a204bfbb000      8K rw---   [ anon ]
00007a204bfbd000     12K r---- libjli.so
00007a204bfc0000     36K r-x-- libjli.so
00007a204bfc9000     12K r---- libjli.so
00007a204bfcc000      4K r---- libjli.so
00007a204bfcd000      4K rw--- libjli.so
00007a204bfce000      8K r---- libz.so.1.2.11
00007a204bfd0000     68K r-x-- libz.so.1.2.11
00007a204bfe1000     24K r---- libz.so.1.2.11
00007a204bfe7000      4K ----- libz.so.1.2.11
00007a204bfe8000      4K r---- libz.so.1.2.11
00007a204bfe9000      4K rw--- libz.so.1.2.11
00007a204bfea000      4K r--s- jaccess.jar
00007a204bfeb000      8K r--s- java-atk-wrapper.jar
00007a204bfed000     12K rw---   [ anon ]
00007a204bff0000     32K rw-s- 12822
00007a204bff8000     12K rw---   [ anon ]
00007a204bffb000      8K r---- ld-linux-x86-64.so.2
00007a204bffd000    168K r-x-- ld-linux-x86-64.so.2
00007a204c027000     44K r---- ld-linux-x86-64.so.2
00007a204c032000      4K r----   [ anon ]
00007a204c033000      8K r---- ld-linux-x86-64.so.2
00007a204c035000      8K rw--- ld-linux-x86-64.so.2
00007ffc56c10000    132K rw---   [ stack ]
00007ffc56cb8000     16K r----   [ anon ]
00007ffc56cbc000      8K r-x--   [ anon ]
ffffffffff600000      4K --x--   [ anon ]
 total          2487048K

## 一个C简单程序的内存地址
几乎没有anon

5aa307a49000-5aa307a4a000 r--p 00000000 08:03 790758                     /home/dev/code/test_malloc
5aa307a4a000-5aa307a4b000 r-xp 00001000 08:03 790758                     /home/dev/code/test_malloc
5aa307a4b000-5aa307a4c000 r--p 00002000 08:03 790758                     /home/dev/code/test_malloc
5aa307a4c000-5aa307a4d000 r--p 00002000 08:03 790758                     /home/dev/code/test_malloc
5aa307a4d000-5aa307a4e000 rw-p 00003000 08:03 790758                     /home/dev/code/test_malloc
5aa31f5a2000-5aa31f5c3000 rw-p 00000000 00:00 0                          [heap]
72aae6200000-72aae6228000 r--p 00000000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6
72aae6228000-72aae63bd000 r-xp 00028000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6
72aae63bd000-72aae6415000 r--p 001bd000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6
72aae6415000-72aae6416000 ---p 00215000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6
72aae6416000-72aae641a000 r--p 00215000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6
72aae641a000-72aae641c000 rw-p 00219000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6
72aae641c000-72aae6429000 rw-p 00000000 00:00 0
72aae65b1000-72aae65b4000 rw-p 00000000 00:00 0
72aae65c3000-72aae65c5000 rw-p 00000000 00:00 0
72aae65c5000-72aae65c7000 r--p 00000000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
72aae65c7000-72aae65f1000 r-xp 00002000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
72aae65f1000-72aae65fc000 r--p 0002c000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
72aae65fd000-72aae65ff000 r--p 00037000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
72aae65ff000-72aae6601000 rw-p 00039000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7ffd76dae000-7ffd76dcf000 rw-p 00000000 00:00 0                          [stack]
7ffd76dd3000-7ffd76dd7000 r--p 00000000 00:00 0                          [vvar]
7ffd76dd7000-7ffd76dd9000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]


# mmap不同映射观察maps
## 1、匿名 + PRIVATE
实验 1：匿名 + PRIVATE  mmap succeeded, returned address: 0x79f642d2c000

========================================
1. MAP_ANONYMOUS | MAP_PRIVATE
PID = 4437
Press ENTER to continue...
========================================
pmap 4437
**000079f642d2c000     24K rw---   [ anon ]**

cat /proc/4437/maps

5a4c72763000-5a4c72764000 r--p 00000000 08:03 790756                     /home/dev/code/a.out

5a4c72764000-5a4c72765000 r-xp 00001000 08:03 790756                     /home/dev/code/a.out

5a4c72765000-5a4c72766000 r--p 00002000 08:03 790756                     /home/dev/code/a.out

5a4c72766000-5a4c72767000 r--p 00002000 08:03 790756                     /home/dev/code/a.out

5a4c72767000-5a4c72768000 rw-p 00003000 08:03 790756                     /home/dev/code/a.out

5a4caada4000-5a4caadc5000 rw-p 00000000 00:00 0                          [heap]

79f642a00000-79f642a28000 r--p 00000000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6

79f642a28000-79f642bbd000 r-xp 00028000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6

79f642bbd000-79f642c15000 r--p 001bd000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6

79f642c15000-79f642c16000 ---p 00215000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6

79f642c16000-79f642c1a000 r--p 00215000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6

79f642c1a000-79f642c1c000 rw-p 00219000 08:03 1050747                    /usr/lib/x86_64-linux-gnu/libc.so.6

79f642c1c000-79f642c29000 rw-p 00000000 00:00 0

79f642d1e000-79f642d21000 rw-p 00000000 00:00 0

**79f642d2c000-79f642d32000 rw-p 00000000 00:00 0**

79f642d32000-79f642d34000 r--p 00000000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2

79f642d34000-79f642d5e000 r-xp 00002000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2

79f642d5e000-79f642d69000 r--p 0002c000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2

79f642d6a000-79f642d6c000 r--p 00037000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2

79f642d6c000-79f642d6e000 rw-p 00039000 08:03 1050732                    /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2

7ffcc2aca000-7ffcc2aeb000 rw-p 00000000 00:00 0                          [stack]

7ffcc2b3d000-7ffcc2b41000 r--p 00000000 00:00 0                          [vvar]

7ffcc2b41000-7ffcc2b43000 r-xp 00000000 00:00 0                          [vdso]

ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]


实验 2：匿名 + SHARED   
000075890cac9000     16K rw-s- zero (deleted)


实验 3：文件 + PRIVATE

========================================
3. file + MAP_PRIVATE
PID = 7101
Press ENTER to continue...
========================================

000075890cac5000     16K rw--- mmap_test.dat

75890cac5000-75890cac9000 rw-p 00000000 08:03 790757                     /home/dev/code/mmap_test.dat

实验 4：文件 + SHARED
========================================
4. file + MAP_SHARED
PID = 7101
Press ENTER to continue...
========================================
75890cabb000-75890cabf000 rw-s 00000000 08:03 790757                     /home/dev/code/mmap_test.dat



注意：
00004000  是offset   偏移量是不同的
790757 是inode
75890cab7000-75890cabb000 rw-s 00004000 08:03 790757                     /home/dev/code/mmap_test.dat
75890cabb000-75890cabf000 rw-s 00000000 08:03 790757                     /home/dev/code/mmap_test.dat
75890cabf000-75890cac2000 rw-p 00000000 00:00 0
75890cac5000-75890cac9000 rw-p 00000000 08:03 790757                     /home/dev/code/mmap_test.dat


ls -i /home/dev/code/mmap_test.dat
790757 /home/dev/code/mmap_test.dat


最终输出

(echo -e "起始-结束\t权限\t偏移\t设备\tinode\t文件"; cat /proc/7101/maps) | column -t


起始-结束                          权限  偏移      设备   inode    文件
57c1f9cf3000-57c1f9cf4000          r--p  00000000  08:03  790756   /home/dev/code/a.out
57c1f9cf4000-57c1f9cf5000          r-xp  00001000  08:03  790756   /home/dev/code/a.out
57c1f9cf5000-57c1f9cf6000          r--p  00002000  08:03  790756   /home/dev/code/a.out
57c1f9cf6000-57c1f9cf7000          r--p  00002000  08:03  790756   /home/dev/code/a.out
57c1f9cf7000-57c1f9cf8000          rw-p  00003000  08:03  790756   /home/dev/code/a.out
57c20624a000-57c20626b000          rw-p  00000000  00:00  0        [heap]
75890c800000-75890c828000          r--p  00000000  08:03  1050747  /usr/lib/x86_64-linux-gnu/libc.so.6
75890c828000-75890c9bd000          r-xp  00028000  08:03  1050747  /usr/lib/x86_64-linux-gnu/libc.so.6
75890c9bd000-75890ca15000          r--p  001bd000  08:03  1050747  /usr/lib/x86_64-linux-gnu/libc.so.6
75890ca15000-75890ca16000          ---p  00215000  08:03  1050747  /usr/lib/x86_64-linux-gnu/libc.so.6
75890ca16000-75890ca1a000          r--p  00215000  08:03  1050747  /usr/lib/x86_64-linux-gnu/libc.so.6
75890ca1a000-75890ca1c000          rw-p  00219000  08:03  1050747  /usr/lib/x86_64-linux-gnu/libc.so.6
75890ca1c000-75890ca29000          rw-p  00000000  00:00  0
75890cab7000-75890cabb000          rw-s  00004000  08:03  790757   /home/dev/code/mmap_test.dat
75890cabb000-75890cabf000          rw-s  00000000  08:03  790757   /home/dev/code/mmap_test.dat
75890cabf000-75890cac2000          rw-p  00000000  00:00  0
75890cac5000-75890cac9000          rw-p  00000000  08:03  790757   /home/dev/code/mmap_test.dat
75890cac9000-75890cacd000          rw-s  00000000  00:01  1034     /dev/zero                                       (deleted)
75890cacd000-75890cad3000          rw-p  00000000  00:00  0
75890cad3000-75890cad5000          r--p  00000000  08:03  1050732  /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
75890cad5000-75890caff000          r-xp  00002000  08:03  1050732  /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
75890caff000-75890cb0a000          r--p  0002c000  08:03  1050732  /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
75890cb0b000-75890cb0d000          r--p  00037000  08:03  1050732  /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
75890cb0d000-75890cb0f000          rw-p  00039000  08:03  1050732  /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7ffd92112000-7ffd92133000          rw-p  00000000  00:00  0        [stack]
7ffd92133000-7ffd92137000          r--p  00000000  00:00  0        [vvar]
7ffd92137000-7ffd92139000          r-xp  00000000  00:00  0        [vdso]
ffffffffff600000-ffffffffff601000  --xp  00000000  00:00  0        [vsyscall]
