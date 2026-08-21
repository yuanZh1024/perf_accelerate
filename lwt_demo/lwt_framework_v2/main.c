#include "coroutine.h"
#include "mock_db.h"
#include "debug_trace.h"

#include <stdio.h>
#include <unistd.h>




typedef struct {

    int request_id;

} request_t;


void handle_request(void *arg)
{
    TRACE_ENTER();
    request_t *req = arg;

    printf(
        "\n[request %d] start\n",
        req->request_id
    );


    /*
     * ============================
     * 1. 发起数据库查询
     * ============================
     */

    printf(
        "[request %d] "
        "send SQL to database\n",
        req->request_id
    );


    int db_fd =
        mock_db_query_async(
            "SELECT * FROM user",
            req->request_id
        );


    printf(
        "[request %d] "
        "db query submitted, fd=%d\n",
        req->request_id,
        db_fd
    );


    /*
     * ============================
     * 2. 等待数据库
     * ============================
     *
     * 注意：
     *
     * 这里不是 sleep()
     *
     * 而是：
     *
     *      epoll_wait()
     *
     * 等待 fd。
     */

    printf(
        "[request %d] "
        "wait database...\n",
        req->request_id
    );


    co_wait_read(db_fd);


    /*
     * ============================
     * 3. 被 scheduler resume
     * ============================
     */

    printf(
        "[request %d] "
        "database response arrived\n",
        req->request_id
    );


    /*
     * 读取数据库结果
     */
    char result[256];

    ssize_t n =
        read(
            db_fd,
            result,
            sizeof(result)
        );


    if (n > 0) {

        printf(
            "[request %d] "
            "result = %s\n",
            req->request_id,
            result
        );
    }


    close(db_fd);


    /*
     * ============================
     * 4. 数据库结果处理
     * ============================
     */

    printf(
        "[request %d] "
        "process database result\n",
        req->request_id
    );


    /*
     * 模拟业务处理
     */
    sleep(1);


    printf(
        "[request %d] "
        "send response to client\n",
        req->request_id
    );


    printf(
        "[request %d] finish\n",
        req->request_id
    );
}


int main()
{
    request_t req1 = {
        .request_id = 1
    };

    request_t req2 = {
        .request_id = 2
    };

    request_t req3 = {
        .request_id = 3
    };


    /*
     * 创建三个请求协程
     */
    coroutine_t *co1 =
        co_create(
            handle_request,
            &req1,
            64 * 1024
        );

    coroutine_t *co2 =
        co_create(
            handle_request,
            &req2,
            64 * 1024
        );

    coroutine_t *co3 =
        co_create(
            handle_request,
            &req3,
            64 * 1024
        );


    /*
     * 先启动三个 coroutine
     *
     * 它们都会：
     *
     * submit DB
     *     ↓
     * co_wait_read()
     *     ↓
     * yield
     */
    co_resume(co1);

    co_resume(co2);

    co_resume(co3);


    /*
     * ============================
     * Scheduler
     * ============================
     *
     * 从这里开始：
     *
     * epoll_wait()
     */
    co_scheduler_run();


    /*
     * 当前示例中 scheduler 是无限循环，
     * 所以正常情况下不会执行到这里。
     */

    co_destroy(co1);
    co_destroy(co2);
    co_destroy(co3);

    return 0;
}