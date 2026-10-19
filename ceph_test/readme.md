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
