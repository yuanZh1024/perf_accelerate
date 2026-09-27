
# 并发修改同一个document
100 个线程
    ↓
UPDATE 同一个 document
    ↓
matched = 1
modified = 1

100 个请求都成功。

最终结果可能是任何一个结果

## 测试结果
Thread-71  matched=1  modified=1
Thread-39  matched=1  modified=1
Thread-44  matched=1  modified=1
Thread-24  matched=1  modified=1
Thread-56  matched=1  modified=1

-----------------------------------------
最终 document
-----------------------------------------
{"_id": {"$oid": "6aa15907d6a71e7eb4a6b674"}, "bucket": "my-bucket", "key": "a.txt", "etag": "etag-99", "writer": "thread-99"}



-----------------------------------------
最终 document
-----------------------------------------
{"_id": {"$oid": "6aa159933b0e7d86c5d075a9"}, "bucket": "my-bucket", "key": "a.txt", "etag": "etag-99", "writer": "thread-99"}



Thread-93  matched=1  modified=1
Thread-87  matched=1  modified=1
Thread-84  matched=1  modified=1
Thread-89  matched=1  modified=1
Thread-81  matched=1  modified=1
Thread-77  matched=1  modified=1
Thread-82  matched=1  modified=1
Thread-71  matched=1  modified=1
Thread-80  matched=1  modified=1
Thread-59  matched=1  modified=1
Thread-70  matched=1  modified=1
Thread-73  matched=1  modified=1
Thread-68  matched=1  modified=1
Thread-61  matched=1  modified=1
Thread-64  matched=1  modified=1

-----------------------------------------
最终 document
-----------------------------------------
{"_id": {"$oid": "6aa159a9e0329f0253fff27c"}, "bucket": "my-bucket", "key": "a.txt", "etag": "etag-97", "writer": "thread-97"}


# 加入并发控制CV
只有一个成功：
Thread-2 WRITE SUCCESS expectedCv=0 newCv=1


## 测试结果

========================================
所有线程准备完成，开始并发 READ
========================================
Thread-2 READ cv=0
Thread-8 READ cv=0
Thread-0 READ cv=0
Thread-6 READ cv=0
Thread-4 READ cv=0
Thread-3 READ cv=0
Thread-7 READ cv=0
Thread-9 READ cv=0
Thread-5 READ cv=0
Thread-1 READ cv=0
Thread-2 WRITE SUCCESS expectedCv=0 newCv=1
Thread-6 CONFLICT: Version conflict! thread=6, expectedCv=0
Thread-3 CONFLICT: Version conflict! thread=3, expectedCv=0
Thread-4 CONFLICT: Version conflict! thread=4, expectedCv=0
Thread-9 CONFLICT: Version conflict! thread=9, expectedCv=0
Thread-8 CONFLICT: Version conflict! thread=8, expectedCv=0
Thread-0 CONFLICT: Version conflict! thread=0, expectedCv=0
Thread-7 CONFLICT: Version conflict! thread=7, expectedCv=0
Thread-5 CONFLICT: Version conflict! thread=5, expectedCv=0
Thread-1 CONFLICT: Version conflict! thread=1, expectedCv=0

========================================
最终 document
========================================
{"_id": {"$oid": "6aa15d1ce3400eba8471f00d"}, "bucket": "my-bucket", "key": "a.txt", "cv": 1, "etag": "etag-thread-2", "writer": "thread-2"}


