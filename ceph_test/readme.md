 sudo cephadm bootstrap \
  --mon-ip 192.168.184.128
Verifying podman|docker is present...
Verifying lvm2 is present...
Verifying time synchronization is in place...
Unit systemd-timesyncd.service is enabled and running
Repeating the final host check...
docker (/usr/bin/docker) is present
systemctl is present
lvcreate is present
Unit systemd-timesyncd.service is enabled and running
Host looks OK
Cluster fsid: 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Verifying IP 192.168.184.128 port 3300 ...
Verifying IP 192.168.184.128 port 6789 ...
Mon IP `192.168.184.128` is in CIDR network `192.168.184.0/24`
Mon IP `192.168.184.128` is in CIDR network `192.168.184.0/24`
Internal network (--cluster-network) has not been provided, OSD replication will default to the public_network
Pulling container image quay.io/ceph/ceph:v20...



Ceph version: ceph version 20.2.4 (7f793731f1b39eb4f465e960113d2363c311b964) tentacle (stable)
Extracting ceph user uid/gid from container image...
Creating initial keys...
Creating initial monmap...
Creating mon...
Waiting for mon to start...
Waiting for mon...
mon is available
Assimilating anything we can from ceph.conf...
Generating new minimal ceph.conf...
Restarting the monitor...
Setting public_network to 192.168.184.0/24 in global config section
Wrote config to /etc/ceph/ceph.conf
Wrote keyring to /etc/ceph/ceph.client.admin.keyring
Creating mgr...
Verifying port 0.0.0.0:9283 ...
Verifying port 0.0.0.0:8765 ...
Verifying port 0.0.0.0:8443 ...
Waiting for mgr to start...
Waiting for mgr...
mgr not available, waiting (1/15)...
mgr not available, waiting (2/15)...
mgr is available
Enabling cephadm module...
Waiting for the mgr to restart...
Waiting for mgr epoch 4...
mgr epoch 4 is available
Verifying orchestrator module is enabled...
Setting orchestrator backend to cephadm...
Generating ssh key...
Wrote public SSH key to /etc/ceph/ceph.pub
Adding key to root@localhost authorized_keys...
Adding host dev-virtual-machine...
Deploying mon service with default placement...
Deploying mgr service with default placement...
Deploying crash service with default placement...
Deploying ceph-exporter service with default placement...
Deploying prometheus service with default placement...
Deploying grafana service with default placement...
Deploying node-exporter service with default placement...
Deploying alertmanager service with default placement...
Enabling the dashboard module...
Waiting for the mgr to restart...
Waiting for mgr epoch 8...
mgr epoch 8 is available
Using certmgr to generate dashboard self-signed certificate...
Creating initial admin user...
Fetching dashboard port number...
Ceph Dashboard is now available at:

             URL: https://dev-virtual-machine:8443/
            User: admin
        Password: nydbdqrexa

Enabling client.admin keyring and conf on hosts with "admin" label
Saving cluster configuration to /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/config directory
You can access the Ceph CLI as following in case of multi-cluster or non-default config:

        sudo /usr/sbin/cephadm shell --fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d -c /etc/ceph/ceph.conf -k /etc/ceph/ceph.client.admin.keyring

Or, if you are only running a single cluster on this host:

        sudo /usr/sbin/cephadm shell

Please consider enabling telemetry to help improve Ceph:

        ceph telemetry on

For more information see:

        https://docs.ceph.com/en/latest/mgr/telemetry/

Bootstrap complete.
Enabling the logrotate.timer service to perform daily log rotation.
root@dev-virtual-machine:/tmp#



 sudo cephadm shell -- ceph -s
Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label
  cluster:
    id:     8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
    health: HEALTH_WARN
            OSD count 0 < osd_pool_default_size 3

  services:
    mon: 1 daemons, quorum dev-virtual-machine (age 2m) [leader: dev-virtual-machine]
    mgr: dev-virtual-machine.nacwiu(active, since 79s)
    osd: 0 osds: 0 up, 0 in

  data:
    pools:   0 pools, 0 pgs
    objects: 0 objects, 0 B
    usage:   0 B used, 0 B / 0 B avail
    pgs:

  progress:
    Updating grafana deployment (+1 -> 1) (0s)
      [............................]

root@dev-virtual-machine:/tmp#



 sudo cephadm shell -- ceph orch daemon add osd dev-virtual-machine:/dev/sdc
Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label
Created osd(s) 0 on host 'dev-virtual-machine'
root@dev-virtual-machine:/tmp#
root@dev-virtual-machine:/tmp# sudo cephadm shell -- ceph -s
sudo cephadm shell -- ceph orch ps
sudo cephadm shell -- ceph osd tree
Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label
  cluster:
    id:     8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
    health: HEALTH_WARN
            OSD count 1 < osd_pool_default_size 3

  services:
    mon: 1 daemons, quorum dev-virtual-machine (age 8m) [leader: dev-virtual-machine]
    mgr: dev-virtual-machine.nacwiu(active, since 36s)
    osd: 1 osds: 1 up (since 14s), 1 in (since 37s)

  data:
    pools:   0 pools, 0 pgs
    objects: 0 objects, 0 B
    usage:   26 MiB used, 10 GiB / 10 GiB avail
    pgs:

Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label
NAME                               HOST                 PORTS             STATUS         REFRESHED  AGE  MEM USE  MEM LIM  VERSION    IMAGE ID      CONTAINER ID
alertmanager.dev-virtual-machine   dev-virtual-machine  *:9093,9094       starting               -    -        -        -  <unknown>  <unknown>     <unknown>
ceph-exporter.dev-virtual-machine  dev-virtual-machine  *:9926            running (7m)     27s ago   7m    7400k        -  20.2.4     6bb1c8a42fbc  ca0d011cca56
crash.dev-virtual-machine          dev-virtual-machine                    running (7m)     27s ago   7m    7312k        -  20.2.4     6bb1c8a42fbc  f3cb0535cafa
grafana.dev-virtual-machine        dev-virtual-machine  *:3000            starting               -    -        -        -  <unknown>  <unknown>     <unknown>
mgr.dev-virtual-machine.nacwiu     dev-virtual-machine  *:9283,8765,8443  running (8m)     27s ago   8m     169M        -  20.2.4     6bb1c8a42fbc  b010c0b64bd4
mon.dev-virtual-machine            dev-virtual-machine                    running (8m)     27s ago   8m    37.7M    2048M  20.2.4     6bb1c8a42fbc  c9b546e493fa
node-exporter.dev-virtual-machine  dev-virtual-machine  *:9100            running (6m)     27s ago   7m    15.9M        -  1.9.1      d00a542e409e  e1d8be65f1dc
osd.0                              dev-virtual-machine                    starting               -    -        -    4096M  <unknown>  <unknown>     <unknown>
prometheus.dev-virtual-machine     dev-virtual-machine  *:9095            running (43s)    27s ago  43s    26.7M        -  3.6.0      76947e7ef22f  0fc28e271d40
Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label
ID  CLASS  WEIGHT   TYPE NAME                     STATUS  REWEIGHT  PRI-AFF
-1         0.00980  root default
-3         0.00980      host dev-virtual-machine
 0    hdd  0.00980          osd.0                     up   1.00000  1.00000
root@dev-virtual-machine:/tmp#
root@dev-virtual-machine:/tmp#
root@dev-virtual-machine:/tmp#
root@dev-virtual-machine:/tmp#

# 创建存储池

sudo cephadm shell -- ceph osd pool create labpool
Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label
pool 'labpool' created
root@dev-virtual-machine:/tmp#
root@dev-virtual-machine:/tmp#
root@dev-virtual-machine:/tmp# sudo cephadm shell -- ceph osd pool set labpool size 2
sudo cephadm shell -- ceph osd pool set labpool min_size 1
Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label
set pool 1 size to 2
Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label
set pool 1 min_size to 1
root@dev-virtual-machine:/tmp#


# CURSH映射规则
sudo cephadm shell -- ceph osd pool get labpool crush_rule
Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label

crush_rule: replicated_rule
root@dev-virtual-machine:/tmp# sudo cephadm shell -- ceph osd crush rule dump
Inferring fsid 8d8d78b8-cb09-11f1-8d25-85614a5f2a8d
Inferring config /var/lib/ceph/8d8d78b8-cb09-11f1-8d25-85614a5f2a8d/mon.dev-virtual-machine/config
Not using image 'sha256:6bb1c8a42fbc0bf87938946990b65174466997bc11c31eb5a323225a779fd8f9' of ceph daemon: not in the list of non-dangling images with ceph=True label

[
    {
        "rule_id": 0,
        "rule_name": "replicated_rule",
        "type": 1,
        "steps": [
            {
                "op": "take",
                "item": -1,
                "item_name": "default"
            },
            {
                "op": "chooseleaf_firstn",
                "num": 0,
                "type": "host"
            },
            {
                "op": "emit"
            }
        ]
    }
]


## 2026-11-14 20:22:13

 radosgw-admin user create --uid=test --display-name="test"
{
    "user_id": "test",
    "display_name": "test",
    "email": "",
    "suspended": 0,
    "max_buckets": 1000,
    "subusers": [],
    "keys": [
        {
            "user": "test",
            "access_key": "KF34U8RJHMDNA6ZO4LEJ",
            "secret_key": "CKXmXo2Nc9xIQqQjkzN0u0whEGRl8lAGSs84WHXN",
            "active": true,
            "create_date": "2026-11-14T17:21:55.798597Z"
        }
    ],
    "swift_keys": [],
    "caps": [],
    "op_mask": "read, write, delete",
    "default_placement": "",
    "default_storage_class": "",
    "placement_tags": [],
    "bucket_quota": {
        "enabled": false,
        "check_on_raw": false,
        "max_size": -1,
        "max_size_kb": 0,
        "max_objects": -1
    },
    "user_quota": {
        "enabled": false,
        "check_on_raw": false,
        "max_size": -1,
        "max_size_kb": 0,
        "max_objects": -1
    },
    "temp_url_keys": [],
    "type": "rgw",
    "mfa_ids": [],
    "account_id": "",
    "path": "/",
    "create_date": "2026-11-14T17:21:55.797812Z",
    "tags": [],
    "group_ids": []
}


# ceph命令常用
# 1. 列出集群所有 Pool
ceph osd pool ls

# 2. 列出指定 Pool 中的所有 RADOS Object
rados -p default.rgw.buckets.index ls

# 3. 查看 Pool 的统计信息
ceph df detail

# 4. 查看某个 RADOS Object 属于哪个 PG
ceph osd map default.rgw.buckets.index <object_name>


#  5.查询所有shard内的omapkey：
for i in $(seq 0 10); do
  echo "== shard $i"
  rados -p default.rgw.buckets.index listomapkeys .dir.94d11251-e46a-4958-ae3c-0deddd5b81cb.24160.1.$i
done

某个omap key对应的value
rados -p default.rgw.buckets.index   listomapkeys .dir.94d11251-e46a-4958-ae3c-0deddd5b81cb.24160.1.3
photo.jpg
small.bin

rados -p default.rgw.buckets.index \
  getomapval .dir.94d11251-e46a-4958-ae3c-0deddd5b81cb.24160.1.3  small.bin
value (245 bytes) :
00000000  08 03 ef 00 00 00 09 00  00 00 73 6d 61 6c 6c 2e  |..........small.|
00000010  62 69 6e 04 00 00 00 00  00 00 00 01 07 03 72 00  |bin...........r.|
00000020  00 00 01 00 00 20 00 00  00 00 00 1e cd fa 6a 31  |..... ........j1|
00000030  6a f3 14 20 00 00 00 30  66 37 35 66 63 66 64 31  |j.. ...0f75fcfd1|
00000040  36 33 63 39 66 63 32 30  37 64 62 31 63 36 66 36  |63c9fc207db1c6f6|
00000050  37 31 61 64 36 65 33 04  00 00 00 74 65 73 74 04  |71ad6e3....test.|
00000060  00 00 00 74 65 73 74 18  00 00 00 61 70 70 6c 69  |...test....appli|
00000070  63 61 74 69 6f 6e 2f 6f  63 74 65 74 2d 73 74 72  |cation/octet-str|
00000080  65 61 6d 00 00 20 00 00  00 00 00 00 00 00 00 00  |eam.. ..........|
00000090  00 00 00 00 00 00 00 00  00 00 00 00 01 01 02 00  |................|
000000a0  00 00 08 04 03 3e 00 00  00 39 34 64 31 31 32 35  |.....>...94d1125|
000000b0  31 2d 65 34 36 61 2d 34  39 35 38 2d 61 65 33 63  |1-e46a-4958-ae3c|
000000c0  2d 30 64 65 64 64 64 35  62 38 31 63 62 2e 33 34  |-0deddd5b81cb.34|
000000d0  31 31 35 2e 36 34 36 36  38 34 37 31 34 36 38 32  |115.646684714682|
000000e0  36 36 38 38 37 32 37 00  00 00 00 00 00 00 00 00  |6688727.........|
000000f0  00 00 00 00 00                                    |.....|
000000f5



在你的 Ceph 环境里，上传一个大文件，然后观察三件事：
# 1. 查看 RGW 数据池中的 RADOS Objects
rados -p default.rgw.buckets.data ls

# 2. 查看 Head 的 XATTR
rados -p default.rgw.buckets.data listxattr <HEAD_OBJECT>

# 3. 查看 Bucket Index 的 OMAP
rados -p default.rgw.buckets.index listomapvals <INDEX_OBJECT>


再用：
radosgw-admin object stat \
  --bucket mybucket \
  --object big.bin


# 观察上传流程
观察 RGW 的 Manifest 和对象布局。

这样你就能把 S3 PUT、Head/Tail、XATTR、OMAP、librados、Onode、ExtentMap、Blob、RocksDB 和磁盘块真正串起来。



export ENDPOINT="http://127.0.0.1:8080"
root@dev-virtual-machine:~#
root@dev-virtual-machine:~# dd if=/dev/urandom of=/tmp/big.bin bs=1M count=20
20+0 records in
20+0 records out
20971520 bytes (21 MB, 20 MiB) copied, 0.146522 s, 143 MB/s
root@dev-virtual-machine:~#
root@dev-virtual-machine:~# aws --endpoint-url "$ENDPOINT" s3api create-bucket \
  --bucket ceph-lab-obj-20261117
root@dev-virtual-machine:~#
root@dev-virtual-machine:~# aws --endpoint-url "$ENDPOINT" s3api put-object \
  --bucket ceph-lab-obj-20261117 \
  --key big.bin \
  --body /tmp/big.bin
{
    "ETag": "\"6403fdd9aeae0154dbf47fcfd0090e23\"",
    "ChecksumCRC64NVME": "BhfTI3awXFk=",
    "ChecksumType": "FULL_OBJECT"
}
root@dev-virtual-machine:~#
root@dev-virtual-machine:~#
root@dev-virtual-machine:~# radosgw-admin bucket radoslist \
  --bucket=ceph-lab-obj-20261117
94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1__shadow_.jUNc6UycRlHZbz-Qx1ABJDdaXrtaI-T_1
94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1__shadow_.jUNc6UycRlHZbz-Qx1ABJDdaXrtaI-T_2
94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1__shadow_.jUNc6UycRlHZbz-Qx1ABJDdaXrtaI-T_3
94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1__shadow_.jUNc6UycRlHZbz-Qx1ABJDdaXrtaI-T_4
94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1_big.bin


~#
root@dev-virtual-machine:~# radosgw-admin object manifest \
  --bucket=ceph-lab-obj-20261117 \
  --object=big.bin
{
    "size": 20971520,
    "objects": [
        {
            "index": 0,
            "part_id": 0,
            "stripe_id": 0,
            "offset": 0,
            "size": 4194304,
            "raw_obj": {
                "pool": "default.rgw.buckets.data",
                "oid": "94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1_big.bin",
                "loc": ""
            }
        },
        {
            "index": 1,
            "part_id": 0,
            "stripe_id": 1,
            "offset": 4194304,
            "size": 4194304,
            "raw_obj": {
                "pool": "default.rgw.buckets.data",
                "oid": "94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1__shadow_.jUNc6UycRlHZbz-Qx1ABJDdaXrtaI-T_1",
                "loc": ""
            }
        },
        {
            "index": 2,
            "part_id": 0,
            "stripe_id": 2,
            "offset": 8388608,
            "size": 4194304,
            "raw_obj": {
                "pool": "default.rgw.buckets.data",
                "oid": "94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1__shadow_.jUNc6UycRlHZbz-Qx1ABJDdaXrtaI-T_2",
                "loc": ""
            }
        },
        {
            "index": 3,
            "part_id": 0,
            "stripe_id": 3,
            "offset": 12582912,
            "size": 4194304,
            "raw_obj": {
                "pool": "default.rgw.buckets.data",
                "oid": "94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1__shadow_.jUNc6UycRlHZbz-Qx1ABJDdaXrtaI-T_3",
                "loc": ""
            }
        },
        {
            "index": 4,
            "part_id": 0,
            "stripe_id": 4,
            "offset": 16777216,
            "size": 4194304,
            "raw_obj": {
                "pool": "default.rgw.buckets.data",
                "oid": "94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1__shadow_.jUNc6UycRlHZbz-Qx1ABJDdaXrtaI-T_4",
                "loc": ""
            }
        }
    ]
}
root@dev-virtual-machine:~#


rados -p default.rgw.buckets.data listxattr 94d11251-e46a-4958-ae3c-0deddd5b81cb.34115.1_big.bin
user.rgw.acl
user.rgw.cksum
user.rgw.etag
user.rgw.idtag
user.rgw.manifest
user.rgw.pg_ver
user.rgw.source_zone
user.rgw.tail_tag
user.rgw.x-amz-checksum-crc64nvme
user.rgw.x-amz-sdk-checksum-algorithm
root@dev-virtual-machine:~#



