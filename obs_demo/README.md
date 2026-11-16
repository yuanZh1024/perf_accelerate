# obs-ec-store

一个用 C 写的极简对象存储：通过 HTTP 读写对象，底层把每个对象切成 **4 个数据分片 + 2 个校验分片**（Reed-Solomon 4+2），任意 2 个分片丢失或损坏都能无损恢复。零第三方依赖，只用到 libc 和 pthread，可直接在 **Ubuntu 22.04.5 LTS** 上编译部署。

```
$ curl -X PUT --data-binary @photo.jpg http://127.0.0.1:9000/v1/objects/demo/photo.jpg
{"key":"demo/photo.jpg","size":4194304,"etag":"a1b2c3d4"}

$ curl -o photo.jpg http://127.0.0.1:9000/v1/objects/demo/photo.jpg
  # 此时即使 6 个分片文件里已经删掉了 2 个，拿到的仍是原始字节
```

---

## 1. 它做了什么

| 能力 | 说明 |
| --- | --- |
| HTTP 接口 | `PUT / GET / HEAD / DELETE /v1/objects/<key>`，另有 `list`、`stats`、`health` |
| 纠删码 | 4+2（k、m 可配，`k+m ≤ 16`），Cauchy 矩阵构造，MDS 性质：任意 4 片即可重建 |
| 流式处理 | 内存占用固定为一个 stripe（默认 1 MiB），与对象大小无关 |
| 完整性 | 每个 stripe 的每个分片都有独立 CRC32，读取时逐片校验，坏片自动剔除并用剩余分片重建 |
| 崩溃一致性 | 分片先落盘，元数据最后原子写入（提交点）；重写对象不会出现"半个新对象" |
| 并发 | 每连接一线程，每 key 一把读写锁 |
| 依赖 | 无。`libc` + `pthread` 而已 |

**它不是**：分布式系统、不是多副本存储、没有认证和 TLS、没有后台 scrub/修复、没有版本管理。生产环境请用 MinIO / Ceph；这个项目的价值是把 EC 的读写路径用几百行 C 讲清楚，并且能跑起来。

---

## 2. 架构

### 2.1 写入路径

```
                 object body (stream)
                        │
        ┌───────────────┴───────────────┐
        │  切成 1 MiB 的 stripe（k×shard_size）
        └───────────────┬───────────────┘
                        │  每个 stripe 独立编码
          ┌─────────────┼─────────────┬─────────────┬─────────────┐
          ▼             ▼             ▼             ▼             ▼
       D0 (256K)     D1 (256K)     D2 (256K)     D3 (256K)     P0 / P1 (256K)
       数据分片        数据分片        数据分片        数据分片        校验分片
          │             │             │             │             │
          ▼             ▼             ▼             ▼             ▼
       data/d0/      data/d1/      data/d2/      data/d3/      data/d4, d5/
     shard_0.ec    shard_1.ec    shard_2.ec    shard_3.ec    shard_4/5.ec
          └─────────────┴──────┬──────┴─────────────┴─────────────┘
                               ▼
                    meta/<key>.meta   ← 最后写，原子 rename，提交点
```

- `P0 = Σ C[0][j]·Dj`，`P1 = Σ C[1][j]·Dj`，系数取自 Cauchy 矩阵 `C[i][j] = 1/(x_i ⊕ y_j)`（GF(2⁸) 上运算，本原多项式 `0x11d`）。
- Cauchy 矩阵天然满足 MDS：任意 k 行构成的子矩阵可逆，所以任意 k 片都能解出原始数据。
- 重建时对 k×k 子矩阵做 GF(2⁸) 高斯消元求逆，再 `D = A⁻¹·R`。

### 2.2 读取路径

```
meta/<key>.meta  ──► 校验元数据 CRC、几何参数
      │
      ├──► 打开 6 个分片文件，校验分片头（magic / shard_idx / 几何 / 对象大小）
      │        可用分片 < 4 ？ ──► 直接 503，不吐脏数据
      │
      └──► 逐 stripe：
              读 6 片 → 逐片 CRC32 比对 meta 里记录的值
              ├─ 6 片全好：直接输出前 4 片
              └─ 有坏片/缺片但 ≥4 片好：高斯消元重建，再输出
```

关键点：**先确认可用分片数再决定是否发 200**。如果中途才失败（说明损坏已经超过冗余度），服务端直接断开连接，让客户端看到短读，而不是悄悄返回被截断的数据。

### 2.3 磁盘布局

```
<data-dir>/
├── data/
│   ├── d0/demo/photo.jpg/shard_0.ec     # 数据分片 0
│   ├── d1/demo/photo.jpg/shard_1.ec
│   ├── d2/...  d3/...                   # 数据分片
│   ├── d4/demo/photo.jpg/shard_4.ec     # 校验分片
│   └── d5/demo/photo.jpg/shard_5.ec
└── meta/
    └── demo/photo.jpg.meta              # 元数据（含每 stripe 每分片的 CRC32）
```

`d0..d5` 扮演"6 个存储节点"的角色。单机部署时它们是同一块盘上的 6 个目录——**能扛文件损坏和误删，扛不住整块盘掉线**。想让它们真的落在 6 块盘上，见 §8。

每个分片文件 = 512 字节头部 + 该对象所有 stripe 的该分片拼接：

| 偏移 | 字段 |
| --- | --- |
| 0 | magic `OBSEC001` |
| 8 | version / shard_idx / k / m / shard_size / num_stripes |
| 32 | payload_len / payload_crc32 / object_size |
| 52 | header_crc32 |

元数据文件（`*.meta`）保存对象大小、几何、整体 etag，以及 `num_stripes × 6` 个 CRC32，末尾带整文件 CRC。它既是索引也是"哪个分片坏了"的判据。

---

## 3. 编译与运行

```bash
sudo apt update && sudo apt install -y build-essential curl   # gcc 11.2 / Ubuntu 22.04
make                 # 生成 ./obsd
make test            # 单元测试（RS 全组合 + 存储层 + HTTP 解析）

mkdir -p /tmp/obsd
./obsd -d /tmp/obsd -l 127.0.0.1 -p 9000 -k 4 -m 2 -s 256K
```

启动输出：

```
obs-ec-store starting: dir=/tmp/obsd ec=4+2 shard=262144 stripe=1048576
listening on 127.0.0.1:9000
```

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `-d` | `/var/lib/obsd` | 数据目录 |
| `-l` | `0.0.0.0` | 监听地址 |
| `-p` | `9000` | 监听端口 |
| `-k` | `4` | 数据分片数 |
| `-m` | `2` | 校验分片数 |
| `-s` | `256K` | 每个数据分片的字节数（stripe = k × 该值） |
| `-c` | `128` | 最大并发连接数 |
| `-v` | 关 | debug 日志 |

---

## 4. HTTP API

| 方法与路径 | 说明 | 成功码 |
| --- | --- | --- |
| `PUT /v1/objects/<key>` | 写入对象，**必须带 `Content-Length`** | 200 |
| `GET /v1/objects/<key>` | 读取对象 | 200 |
| `HEAD /v1/objects/<key>` | 对象元信息（`ETag`、`X-EC-Scheme`、`X-Stripes`） | 200 |
| `DELETE /v1/objects/<key>` | 删除对象及其全部分片 | 204 |
| `GET /v1/objects?prefix=<p>` | 列举对象（JSON） | 200 |
| `GET /v1/stats` | 对象数、分片数、各分片目录占用 | 200 |
| `GET /v1/health` | 存活探针 | 200 |

`<key>` 是相对路径：允许 `[A-Za-z0-9._/-]`，禁止 `..`、`//`、首尾 `/`，最长 512 字节。

```bash
# 写入
curl -X PUT --data-binary @photo.jpg http://127.0.0.1:9000/v1/objects/demo/photo.jpg

# 读取
curl -o out.jpg http://127.0.0.1:9000/v1/objects/demo/photo.jpg

# 元信息
curl -I http://127.0.0.1:9000/v1/objects/demo/photo.jpg
# HTTP/1.1 200 OK
# ETag: "a1b2c3d4"
# X-EC-Scheme: 4+2
# X-Object-Size: 4194304
# Content-Length: 4194304

# 列举 / 统计 / 删除
curl 'http://127.0.0.1:9000/v1/objects?prefix=demo/'
curl http://127.0.0.1:9000/v1/stats
curl -X DELETE http://127.0.0.1:9000/v1/objects/demo/photo.jpg
```

`/v1/stats` 返回：

```json
{"objects":24,"shard_files":144,"shard_bytes":9510912,
 "ec":"4+2","shard_size":65536,"stripe_size":262144,
 "shards":{"d0":24,"d1":24,"d2":24,"d3":24,"d4":24,"d5":24}}
```

错误码：`400` 非法 key / body 短于 Content-Length，`404` 对象不存在，`405` 方法不支持，`411` 缺少 Content-Length，`500` 损坏超出修复能力，`503` 可用分片不足 4 片。

---

## 5. 看看 EC 到底做了什么

```bash
./scripts/demo.sh          # 逐步演示：写入 4 MiB → 看 6 个分片 → 删 2 个 → 仍能读回
./scripts/smoke_test.sh    # 自动化检查：多种尺寸、故障注入、错误码
```

手工验证故障恢复：

```bash
# 删掉 2 个分片（模拟 2 块盘挂掉）
rm -f /tmp/obsd/data/d0/demo/photo.jpg/shard_0.ec
rm -f /tmp/obsd/data/d1/demo/photo.jpg/shard_1.ec

# 仍然能读，且字节一致
curl -o back.jpg http://127.0.0.1:9000/v1/objects/demo/photo.jpg
cmp photo.jpg back.jpg && echo "identical"

# 日志会显示每个 stripe 的重建
# [INFO] object 'demo/photo.jpg' stripe 0: rebuilding 2 shard(s)

# 再删一个（只剩 3 片），服务端拒绝而不是返回脏数据
rm -f /tmp/obsd/data/d2/demo/photo.jpg/shard_2.ec
curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:9000/v1/objects/demo/photo.jpg
# 503
```

静默损坏同样能发现：往某个分片文件中间写几个字节，CRC32 会判定该分片失效，读取时用其余 4 片重建。

---

## 6. 部署到 Ubuntu 22.04 VM

### 方式一：一键脚本

```bash
# 把整个目录拷到 VM 上，然后
sudo ./deploy/install.sh

# 换端口 / 数据目录
sudo PORT=9100 DATA=/data/obsd ./deploy/install.sh
```

脚本做的事：`make` → 安装 `/usr/local/bin/obsd` → 创建系统用户 `obsd` → 建数据目录 → 安装并启用 systemd 服务 → 等健康检查通过后打印 stats。重复执行即为升级（数据不动）。

### 方式二：手工

```bash
sudo apt install -y build-essential
make && sudo make install                 # 装到 /usr/local/bin/obsd

sudo useradd --system --home-dir /var/lib/obsd --shell /usr/sbin/nologin obsd
sudo mkdir -p /var/lib/obsd && sudo chown obsd:obsd /var/lib/obsd

sudo cp deploy/obsd.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now obsd
```

运维命令：

```bash
systemctl status obsd
journalctl -u obsd -f
curl -s http://127.0.0.1:9000/v1/stats
```

`deploy/obsd.service` 已开启 `ProtectSystem=strict`、`StateDirectory=obsd`、`NoNewPrivileges` 等加固项，只有 `/var/lib/obsd` 可写。

### 开放端口

```bash
sudo ufw allow 9000/tcp
```

---

## 7. 容量与开销

对象大小 `S`、分片大小 `s`、`k=4, m=2`：

- stripe 大小 = `4s`（默认 1 MiB）
- stripe 数 = `⌈S / 4s⌉`
- 每个分片文件 ≈ `512 + ⌈S/4s⌉ × s`
- **磁盘占用 ≈ 1.5 × S**（6/4 = 50% 冗余，可容忍任意 2 片故障）

小对象放大明显：1 字节的对象也会产生 6 个文件，每个约 `512 + s` 字节（默认约 1.5 MiB）。如果主要存小文件，把 `-s` 调小（例如 `-s 4K`）会好很多——代价是同样大小的对象 stripe 数变多，元数据里的 CRC 数组变大。

内存：每个并发请求占用 `(k+m) × s` 的 stripe 缓冲区（默认 1.5 MiB），所以 `-c 128` 在最坏情况下约 192 MiB。VM 内存紧张时同时调小 `-s` 和 `-c`。

---

## 8. 想真的扛住"节点"故障

当前实现里 `d0..d5` 只是 6 个目录。要变成 6 台机器，把 `store.c` 里的两处文件操作换掉即可：

1. `shard_path()` 生成路径 → 换成"分片号 → 节点地址"的映射；
2. `os_pread/os_pwrite/os_open_*` → 换成对该节点的 RPC（HTTP 或 gRPC）。

编码/解码逻辑（`rs.c`）、stripe 流水线（`store_put` / `store_get`）都不用改。再往前一步需要补的东西：元数据多副本（3 份 + 一致性哈希）、后台 scrub 与跨节点修复、心跳与降级写。

---

## 9. 测试

```bash
make test
```

- `test/test_rs.c`：GF(2⁸) 运算自检；4+2 的 **全部 22 种**"≥4 片在场"组合逐一重建并逐字节比对；6+3、2+2 同样跑全组合。
- `test/test_store.c`：0/1/100/4095/4096/4097/16383/16384/16385/100000 字节对象读写；分片布局；删 2 片仍可读；1 片缺失 + 1 片被改字节仍可读；删 3 片必须失败；删元数据后对象消失；list/stats/delete/非法 key。
- `test/test_http_parse.c`：请求行、百分号解码、query、Content-Length、Expect、残缺/畸形请求、纯 LF 换行。

```bash
./scripts/smoke_test.sh    # 起真实进程跑 HTTP 端到端，含故障注入
```

---

## 10. 已知限制（按重要程度）

1. **单机**。6 个目录在同一块盘上，磁盘整体故障会丢全部数据。这是演示 EC 算法的设计取舍，不是多副本。
2. **元数据是单点**。`meta/<key>.meta` 只有一份；丢了对象就"消失"（分片还在但不可达）。生产上应复制 3 份。
3. **无认证、无 TLS**。只适合内网/实验环境，公网暴露前必须加反向代理做鉴权。
4. **无后台修复**。重建只在读取时发生，冷数据坏了不会被发现。可以加一个定时 scrub（顺序读每个对象、校验 CRC、重写坏片）。
5. **流式中途失败会断连**。对象若在传输途中损坏超过冗余度，客户端看到短读而非错误码。
6. **每连接一线程**，无 keep-alive、无 HTTP/2。压测到几千并发需要改成 epoll 事件循环。
7. **无对象版本、无生命周期**。`DELETE` 直接 unlink，不回收目录。

---

## 11. 目录结构

```
.
├── Makefile
├── README.md
├── src/
│   ├── gf.c / gf.h            GF(2^8) 算术（log/exp 表，本原多项式 0x11d）
│   ├── rs.c / rs.h            Cauchy 矩阵、编码、高斯消元重建
│   ├── meta.c / meta.h        对象元数据序列化（含每 stripe 每分片 CRC32）
│   ├── store.c / store.h      EC 对象存储核心：stripe 流水线、分片落盘、读时修复
│   ├── os.c / os.h            文件系统薄封装（POSIX，Windows 仅测试用垫片）
│   ├── net.c / net.h          TCP 辅助
│   ├── http_parse.c / .h      请求头解析（可独立单测）
│   ├── server.c / server.h    HTTP 路由与连接处理
│   ├── util.c / util.h        crc32、日志、动态字符串、key 校验
│   └── obsd.c                 入口与参数解析
├── test/                      单元测试
├── scripts/
│   ├── demo.sh                人类可读的 EC 演示
│   └── smoke_test.sh          HTTP 端到端 + 故障注入
└── deploy/
    ├── obsd.service           systemd 单元
    └── install.sh             一键安装
```

---

## 12. 代码阅读顺序

想快速看懂实现，按这个顺序读：

1. `src/gf.c`（60 行）：GF(2⁸) 的加法和乘法。
2. `src/rs.c`：`rs_encode` 就是 6 次矩阵乘；`rs_reconstruct` 是求逆后重算。
3. `src/store.c` 的 `store_put`：一个 for 循环覆盖所有 stripe，看清楚"先写分片、后写元数据"的提交顺序。
4. `src/store.c` 的 `store_get`：CRC 校验 → 数一数有几片好的 → 不够就重建 → 输出前 4 片。
5. `src/server.c`：HTTP 只是薄薄一层胶水。
