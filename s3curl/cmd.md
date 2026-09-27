


s3curl --debug --id=minio_root --createBucket -- http://127.0.0.1:9000/bucket02 -v
s3curl --debug --id=minio_root --put=.s3curl -- http://127.0.0.1:9000/bucket02/obj01 -v

s3curl --debug --id=minio_root --put=version.xml -- http://127.0.0.1:9000/bucket02?versioning -v


s3curl --debug --id=minio_root --put=.s3curl -- http://127.0.0.1:9000/bucket02/obj01 -H 'If-Match: "abc1234567890etag"'  -v


## MinIO 必须开启 SigV2 兼容才能跑通（测试环境）

```
export MINIO_API_SIGNATURE_V2_ENABLE=on
minio server /data --console-address ":9001"
```

> 
> ⚠️生产环境不要开启 SigV2，SigV2 已经淘汰；这个配置只用于学习 S3‑SigV2 协议。


## 配置aws api
curl -fsSL https://awscli.amazonaws.com/v2/install.sh | bash

 echo 'export PATH="$PATH:/root/.local/bin"' >> /root/.bashrc
source /root/.bashrc
aws --version

aws configure set aws_access_key_id "minioadmin"
aws configure set aws_secret_access_key "minioadmin"
aws configure set default.region us-east-1
aws configure set default.s3.signature_version s3v4


aws s3api put-object \
--debug \
--endpoint-url http://127.0.0.1:9000 \
--bucket bucket02 \
--key obj01 \
--body .s3curl \
--if-match "d4d03c8b47f49aec29ad0d464096b4cf"