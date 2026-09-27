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

        MongoClient client =
                MongoClients.create("mongodb://localhost:27017");

        MongoDatabase db =
                client.getDatabase("object_meta");

        MongoCollection<Document> collection =
                db.getCollection("objects");

        // 初始化
        collection.deleteMany(new Document());

        collection.insertOne(
                new Document("bucket", "my-bucket")
                        .append("key", "a.txt")
                        .append("cv", 0)
                        .append("etag", "initial")
                        .append("writer", "initial")
        );

        System.out.println("初始 document：");
        System.out.println(
                collection.find(
                        new Document("key", "a.txt")
                ).first().toJson()
        );

        final int threadCount = 10;

        ExecutorService pool =
                Executors.newFixedThreadPool(threadCount);

        CountDownLatch ready =
                new CountDownLatch(threadCount);

        CountDownLatch start =
                new CountDownLatch(1);

        CountDownLatch finish =
                new CountDownLatch(threadCount);

        for (int i = 0; i < threadCount; i++) {

            final int threadId = i;

            pool.submit(new Runnable() {

                @Override
                public void run() {

                    try {

                        ready.countDown();

                        // 等待所有线程
                        start.await();

                        // =====================================
                        // 1. READ
                        // =====================================

                        Document document =
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

                        int oldCv =
                                document.getInteger("cv");

                        System.out.println(
                                "Thread-" + threadId
                                        + " READ cv="
                                        + oldCv
                        );

                        // 故意制造并发
                        Thread.sleep(100);

                        // =====================================
                        // 2. MODIFY
                        // =====================================

                        int newCv = oldCv + 1;

                        String newEtag =
                                "etag-thread-" + threadId;

                        // =====================================
                        // 3. CAS WRITE
                        // =====================================

                        UpdateResult result =
                                collection.updateOne(

                                        new Document()
                                                .append(
                                                        "bucket",
                                                        "my-bucket"
                                                )
                                                .append(
                                                        "key",
                                                        "a.txt"
                                                )
                                                .append(
                                                        "cv",
                                                        oldCv
                                                ),

                                        new Document(
                                                "$set",

                                                new Document()
                                                        .append(
                                                                "cv",
                                                                newCv
                                                        )
                                                        .append(
                                                                "etag",
                                                                newEtag
                                                        )
                                                        .append(
                                                                "writer",
                                                                "thread-" + threadId
                                                        )
                                        )
                                );

                        // =====================================
                        // 4. 判断版本冲突
                        // =====================================

                        if (result.getMatchedCount() == 0) {

                            throw new ConflictVersionException(
                                    "Version conflict! "
                                            + "thread="
                                            + threadId
                                            + ", expectedCv="
                                            + oldCv
                            );
                        }

                        System.out.println(
                                "Thread-" + threadId
                                        + " WRITE SUCCESS "
                                        + "expectedCv="
                                        + oldCv
                                        + " newCv="
                                        + newCv
                        );

                    } catch (ConflictVersionException e) {

                        System.out.println(
                                "Thread-" + threadId
                                        + " CONFLICT: "
                                        + e.getMessage()
                        );

                    } catch (Exception e) {

                        System.out.println(
                                "Thread-" + threadId
                                        + " ERROR: "
                                        + e.getMessage()
                        );

                    } finally {

                        finish.countDown();
                    }
                }
            });
        }

        // 等待所有线程准备完成
        ready.await();

        System.out.println();
        System.out.println(
                "========================================"
        );

        System.out.println(
                "所有线程准备完成，开始并发 READ"
        );

        System.out.println(
                "========================================"
        );

        // 同时开始
        start.countDown();

        // 等待所有线程结束
        finish.await();

        pool.shutdown();

        // =====================================
        // 最终结果
        // =====================================

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
        System.out.println(
                "========================================"
        );

        System.out.println("最终 document");

        System.out.println(
                "========================================"
        );

        System.out.println(
                finalDocument.toJson()
        );

        client.close();
    }
}