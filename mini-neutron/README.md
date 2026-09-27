
## 执行结果
 curl -X POST http://localhost:9696/networks \
  -H 'Content-Type: application/json' \
  -d '{"name":"vpc-prod"}'
{"id":"net-001","name":"vpc-prod"}
root@dev-virtual-machine:~#
root@dev-virtual-machine:~# curl -X GET  http://localhost:9696/networks
[{"id":"net-001","name":"vpc-prod"}]
root@dev-virtual-machine:~#
root@dev-virtual-machine:~#
root@dev-virtual-machine:~# curl -X POST http://localhost:9696/subnets \
  -H 'Content-Type: application/json' \
  -d '{
    "network_id":"net-001",
    "cidr":"10.0.1.0/24"
  }'
{"id":"subnet-001","network_id":"net-001","cidr":"10.0.1.0/24"}
root@dev-virtual-machine:~#
root@dev-virtual-machine:~#
root@dev-virtual-machine:~#
root@dev-virtual-machine:~#
root@dev-virtual-machine:~# curl -X POST http://localhost:9696/ports \
  -H 'Content-Type: application/json' \
  -d '{
    "network_id":"net-001"
  }'
{"id":"port-001","network_id":"net-001","mac":"fa:16:3e:00:00:01","fixed_ip":"10.0.1.10"}
root@dev-virtual-machine:~#
root@dev-virtual-machine:~#
root@dev-virtual-machine:~# curl -X POST http://localhost:9696/ports \
  -H 'Content-Type: application/json' \
  -d '{
    "network_id":"net-002"
  }'
{"id":"port-002","network_id":"net-002","mac":"fa:16:3e:00:00:02","fixed_ip":"10.0.1.11"}
root@dev-virtual-machine:~#


