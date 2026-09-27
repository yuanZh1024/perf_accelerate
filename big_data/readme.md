
# 测试Catalog
理解Catalog：  用rest api来理解 catalog有哪些能力  ---表在哪里？怎么管理？

 REST Catalog = 用 REST API 暴露 Catalog 能力的 Catalog。
 Catalog 本质是一个“表目录管理抽象”；REST Catalog 是把这个抽象做成 Server + API。


docker run -d \
  --name iceberg-rest \
  -p 8181:8181 \
  apache/iceberg-rest-fixture:latest
84f6bf0940a9582546d19df16fa55076c04887b55362a57afbb750dccaf06b0f
root@dev-virtual-machine:~# docker ps
CONTAINER ID   IMAGE                                COMMAND                  CREATED         STATUS                   PORTS                                         NAMES
84f6bf0940a9   apache/iceberg-rest-fixture:latest   "java -jar iceberg-r…"   5 seconds ago   Up 4 seconds (healthy)   0.0.0.0:8181->8181/tcp, [::]:8181->8181/tcp   iceberg-rest




curl http://localhost:8181/v1/config|jq


{
  "defaults": {},
  "overrides": {
    "namespace-separator": "%2E"
  },
  "endpoints": [
    "POST v1/oauth/tokens",
    "POST https://auth-server.com/token",
    "GET v1/config",
    "GET /v1/{prefix}/namespaces",
    "POST /v1/{prefix}/namespaces",
    "HEAD /v1/{prefix}/namespaces/{namespace}",
    "GET /v1/{prefix}/namespaces/{namespace}",
    "DELETE /v1/{prefix}/namespaces/{namespace}",
    "POST /v1/{prefix}/namespaces/{namespace}/properties",
    "GET /v1/{prefix}/namespaces/{namespace}/tables",
    "POST /v1/{prefix}/namespaces/{namespace}/tables",
    "HEAD /v1/{prefix}/namespaces/{namespace}/tables/{table}",
    "GET /v1/{prefix}/namespaces/{namespace}/tables/{table}",
    "POST /v1/{prefix}/namespaces/{namespace}/register",
    "POST /v1/{prefix}/namespaces/{namespace}/tables/{table}",
    "DELETE /v1/{prefix}/namespaces/{namespace}/tables/{table}",
    "POST /v1/{prefix}/tables/rename",
    "POST /v1/{prefix}/namespaces/{namespace}/tables/{table}/metrics",
    "POST /v1/{prefix}/transactions/commit",
    "GET /v1/{prefix}/namespaces/{namespace}/views",
    "HEAD /v1/{prefix}/namespaces/{namespace}/views/{view}",
    "GET /v1/{prefix}/namespaces/{namespace}/views/{view}",
    "POST /v1/{prefix}/namespaces/{namespace}/views",
    "POST /v1/{prefix}/namespaces/{namespace}/views/{view}",
    "POST /v1/{prefix}/views/rename",
    "DELETE /v1/{prefix}/namespaces/{namespace}/views/{view}",
    "POST /v1/{prefix}/namespaces/{namespace}/register-view",
    "POST /v1/{prefix}/namespaces/{namespace}/tables/{table}/plan",
    "GET /v1/{prefix}/namespaces/{namespace}/tables/{table}/plan/{plan-id}",
    "POST /v1/{prefix}/namespaces/{namespace}/tables/{table}/tasks",
    "DELETE /v1/{prefix}/namespaces/{namespace}/tables/{table}/plan/{plan-id}"
  ]
}
API解释：https://www.doubao.com/chat/38440421619067138


~# curl -X POST \
  http://localhost:8181/v1/namespaces \
  -H 'Content-Type: application/json' \
  -d '{"namespace":["demo"]}'
{"namespace":["demo"],"properties":{"location":"/tmp/iceberg_warehouse13324867713853113448/iceberg_data/demo"}}root@dev-virtual-machine:~#
root@dev-virtual-machine:~#

实际上是在对 Catalog Server 说：
“帮我注册一个叫 demo 的命名空间。”


表格

| 模块 | 接口 |
| --- | --- |
| Auth | 获取 OAuth token |
| Catalog 全局 | 获取 catalog config |
| Namespace(DB) | 增删改查、属性更新、探测存在 |
| Table (Iceberg 表) | 建表、删表、查表元数据、更新快照、重命名、注册存量表、metrics 上报 |
| View (Iceberg 视图) | 增删改查、重命名、注册存量视图 |
| Transaction | 原子元数据事务 commit（Iceberg 核心） |
| Plan/Task | 查询扫描计划、后台维护任务 |

> 补充：这套就是 **Apache Iceberg REST Catalog Spec v1** 标准 API，Spark/Flink/Presto/Trino 都可以作为客户端对接这套接口，替代 Hive Metastore。