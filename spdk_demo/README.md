## 1、初始化SPDK，模拟读写 --- my_app_for_spdk


我使用的是单核机器

sudo ./my_storage_app \
    -m 0x1 \
    -c /tmp/my_bdev.json \
    -b Malloc0

cat /tmp/my_bdev.json
{
  "subsystems": [
    {
      "subsystem": "bdev",
      "config": [
        {
          "method": "bdev_malloc_create",
          "params": {
            "name": "Malloc0",
            "block_size": 512,
            "num_blocks": 131072
          }
        }
      ]
    }
  ]
}


输出：
root@ecs-72fb:~/spdk/my_app# sudo ./my_storage_app \
    -m 0x1 \
    -c /tmp/my_bdev.json \
    -b Malloc0
[2026-08-18 23:16:35.676309] Starting SPDK v26.09-pre git sha1 da06c45b7 / DPDK 26.03.0 initialization...
[2026-08-18 23:16:35.676739] [ DPDK EAL parameters: my_storage_app --no-shconf -l 0 --huge-unlink --no-telemetry --log-level=lib.eal:6 --log-level=lib.cryptodev:5 --log-level=lib.power:5 --log-level=user1:6 --iova-mode=pa --base-virtaddr=0x200000000000 --match-allocations --file-prefix=spdk_pid26063 ]
[2026-08-18 23:16:35.789447] app.c: 993:spdk_app_start: *NOTICE*: Total cores available: 1
[2026-08-18 23:16:35.807501] reactor.c: 996:reactor_run: *NOTICE*: Reactor started on core 0

=============================
MY STORAGE APP START
=============================
Found bdev: Malloc0
[2026-08-18 23:16:35.850215] bdev.c:9093:spdk_bdev_open_ext_v2: *ERROR*: Missing event callback function
spdk_bdev_open_ext() failed: -22
[2026-08-18 23:16:35.850539] app.c:1138:spdk_app_stop: *WARNING*: spdk_app_stop'd on non-zero
SPDK application failed: -1
root@ecs-72fb:~/spdk/my_app# 

## 2、写一个简单的“真实存储引擎”
