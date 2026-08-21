#include "mock_db.h"
#include "debug_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>


typedef struct {

    int write_fd;

    int request_id;

    char sql[256];

} db_job_t;


static void *db_worker(void *arg)
{
    TRACE_ENTER();
    db_job_t *job = arg;

    printf(
        "[mock-db] request %d: "
        "database processing...\n",
        job->request_id
    );


    /*
     * 模拟数据库处理 SQL
     *
     * 比如真正数据库可能需要：
     *
     * 2 秒
     */
    sleep(2);


    char result[256];

    snprintf(
        result,
        sizeof(result),
        "DB_RESULT(request=%d)",
        job->request_id
    );


    printf(
        "[mock-db] request %d: "
        "database finished\n",
        job->request_id
    );


    /*
     * 模拟数据库返回数据
     *
     * 真正场景这里相当于：
     *
     * DB
     *  ↓
     * TCP
     *  ↓
     * socket
     */
    write(
        job->write_fd,
        result,
        strlen(result) + 1
    );


    close(job->write_fd);

    free(job);

    return NULL;
}


int mock_db_query_async(
    const char *sql,
    int request_id
)
{
    int pipefd[2];

    /*
     * pipefd[0] = read
     * pipefd[1] = write
     */
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return -1;
    }


    db_job_t *job =
        calloc(1, sizeof(*job));

    if (job == NULL) {
        close(pipefd[0]);
        close(pipefd[1]);

        return -1;
    }


    job->write_fd = pipefd[1];

    job->request_id = request_id;

    strncpy(
        job->sql,
        sql,
        sizeof(job->sql) - 1
    );


    pthread_t tid;

    if (pthread_create(
            &tid,
            NULL,
            db_worker,
            job) != 0) {

        perror("pthread_create");

        free(job);

        close(pipefd[0]);
        close(pipefd[1]);

        return -1;
    }


    /*
     * DB worker 不需要 join
     */
    pthread_detach(tid);


    /*
     * Application 只拿到 read fd
     *
     * 后续由 epoll 监听
     */
    return pipefd[0];
}