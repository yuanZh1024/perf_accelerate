import com.mongodb.client.MongoClient;
import com.mongodb.client.MongoClients;
import com.mongodb.client.MongoCollection;
import com.mongodb.client.MongoDatabase;
import com.mongodb.client.result.UpdateResult;

import org.bson.Document;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MongoConcurrentTest {

    public static void main(String[] args) throws Exception {

        // 连接 MongoDB
        MongoClient client =
                MongoClients.create("mongodb://localhost:27017");

        MongoDatabase db =
                client.getDatabase("object_meta");

        MongoCollection<Document> collection =
                db.getCollection("objects");


        // =====================================================
        // 1. 初始化数据
        // =====================================================

        collection.deleteMany(new Document());

        collection.insertOne(
                new Document("bucket", "my-bucket")
                        .append("key", "a.txt")
                        .append("etag", "initial")
                        .append("writer", "initial")
        );

        System.out.println("初始数据：");

        System.out.println(
                collection.find(
                        new Document("key", "a.txt")
                ).first().toJson()
        );


        // =====================================================
        // 2. 创建 100 个线程
        // =====================================================

        final int threadCount = 100;

        ExecutorService pool =
                Executors.newFixedThreadPool(threadCount);


        /*
         * ready
         *
         * 等待 100 个线程全部准备完成
         */
        CountDownLatch ready =
                new CountDownLatch(threadCount);


        /*
         * start
         *
         * 所有线程先等待这里。
         *
         * 主线程调用 start.countDown()
         * 后，所有线程开始 UPDATE。
         */
        CountDownLatch start =
                new CountDownLatch(1);


        /*
         * finish
         *
         * 等待所有线程执行结束
         */
        CountDownLatch finish =
                new CountDownLatch(threadCount);


        // =====================================================
        // 3. 创建 100 个并发任务
        // =====================================================

        for (int i = 0; i < threadCount; i++) {

            final int threadId = i;

            pool.submit(new Runnable() {

                @Override
                public void run() {

                    try {

                        // 告诉主线程：
                        // 我已经准备好了
                        ready.countDown();


                        // 等待其他线程
                        start.await();


                        // =================================================
                        // 真正执行 UPDATE
                        // =================================================

                        UpdateResult result =
                                collection.updateOne(

                                        /*
                                         * 查询条件
                                         *
                                         * 只有 bucket + key
                                         *
                                         * 没有 version
                                         * 没有 etag
                                         */
                                        new Document()
                                                .append(
                                                        "bucket",
                                                        "my-bucket"
                                                )
                                                .append(
                                                        "key",
                                                        "a.txt"
                                                ),

                                        /*
                                         * 修改内容
                                         */
                                        new Document(
                                                "$set",

                                                new Document()
                                                        .append(
                                                                "etag",
                                                                "etag-" + threadId
                                                        )
                                                        .append(
                                                                "writer",
                                                                "thread-" + threadId
                                                        )
                                        )
                                );


                        // =================================================
                        // 打印 MongoDB 返回结果
                        // =================================================

                        System.out.println(
                                "Thread-" + threadId
                                        + "  matched="
                                        + result.getMatchedCount()
                                        + "  modified="
                                        + result.getModifiedCount()
                        );


                    } catch (Exception e) {

                        System.out.println(
                                "Thread-" + threadId
                                        + " ERROR: "
                                        + e.getMessage()
                        );

                    } finally {

                        // 告诉主线程：
                        // 我执行完了
                        finish.countDown();
                    }
                }
            });
        }


        // =====================================================
        // 4. 等待所有线程准备完成
        // =====================================================

        ready.await();

        System.out.println();
        System.out.println("-----------------------------------------");
        System.out.println("100 个线程已经全部准备完成");
        System.out.println("-----------------------------------------");


        // =====================================================
        // 5. 同时释放所有线程
        // =====================================================

        System.out.println("开始并发 UPDATE...");

        start.countDown();


        // =====================================================
        // 6. 等待所有 UPDATE 完成
        // =====================================================

        finish.await();

        pool.shutdown();


        // =====================================================
        // 7. 查询最终结果
        // =====================================================

        Document finalDocument =
                collection.find(
                        new Document()
                                .append(
                                        "bucket",
                                        "my-bucket"
                                )
                                .append(
                                        "key",
                                        "a.txt"
                                )
                ).first();


        System.out.println();
        System.out.println("-----------------------------------------");
        System.out.println("最终 document");
        System.out.println("-----------------------------------------");

        System.out.println(
                finalDocument.toJson()
        );


        client.close();
    }
}