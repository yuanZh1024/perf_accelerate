


s3curl --debug --id=minio_root --createBucket -- http://127.0.0.1:9000/bucket02 -v
s3curl --debug --id=minio_root --put=.s3curl -- http://127.0.0.1:9000/bucket02/obj01 -v

s3curl --debug --id=minio_root --put=version.xml -- http://127.0.0.1:9000/bucket02?versioning -v


## MinIO 必须开启 SigV2 兼容才能跑通（测试环境）

```
export MINIO_API_SIGNATURE_V2_ENABLE=on
minio server /data --console-address ":9001"
```

> 
> ⚠️生产环境不要开启 SigV2，SigV2 已经淘汰；这个配置只用于学习 S3‑SigV2 协议。