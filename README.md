# perf_accelerate

## 文件名术语
- principle working principle 工作原理 ✅（技术文档高频）
- review  复习


## 每个文件夹的内容说明/导航
- linux_net
  - 理解tap  用户态进程转发包给另一个用户态进程
  https://www.doubao.com/chat/38445910822564354
  


## 常用命令
myself


```
启动hdfs
start-dfs.sh

hdfs dfs -ls /tmp/hive-demo/data
```


CREATE TABLE users (
    id INT,
    name STRING,
    age INT
)
ROW FORMAT DELIMITED
FIELDS TERMINATED BY ',';

LOAD DATA LOCAL INPATH '/tmp/hive-demo/data/user1.csv'
INTO TABLE users;

再：

LOAD DATA LOCAL INPATH '/tmp/hive-demo/data/user2.csv'
INTO TABLE users;

LOAD DATA LOCAL INPATH '/tmp/hive-demo/data/user3.csv'
INTO TABLE users;

进入hive命令行
hive --service hiveserver2
 beeline -u 'jdbc:hive2://localhost:10000/default'

set mapreduce.task.io.sort.mb=20;