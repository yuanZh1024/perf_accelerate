#include "coroutine.h"
#include "context.h"
#include "debug_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>


typedef enum {
    CO_READY,
    CO_RUNNING,
    CO_WAITING,
    CO_DEAD
} coroutine_state_t;


struct coroutine {

    context_t ctx;

    void *stack;
    size_t stack_size;

    coroutine_func func;
    void *arg;

    coroutine_state_t state;

    /*
     * 如果 coroutine 正在等待 IO，
     * 保存它等待的 fd
     */
    int wait_fd;
};


/*
 * Scheduler 自己的 context
 *
 * 注意：
 *
 * Scheduler 本身也是一个执行上下文。
 *
 * Coroutine yield 后，
 * 会切换回 scheduler_ctx。
 */
static context_t scheduler_ctx;


/*
 * 当前正在运行的 coroutine
 */
static coroutine_t *current_co = NULL;


/*
 * epoll fd
 */
static int epoll_fd = -1;


/*
 * coroutine entry
 */
static void coroutine_entry(void)
{
    TRACE_ENTER();
    coroutine_t *co = current_co;

    co->state = CO_RUNNING;

    printf(
        "[scheduler] start coroutine %p\n",
        (void *)co
    );

    /*
     * 执行业务代码
     */
    co->func(co->arg);

    /*
     * 业务执行结束
     */
    co->state = CO_DEAD;

    printf(
        "[scheduler] coroutine %p finished\n",
        (void *)co
    );

    current_co = NULL;

    /*
     * 回到 scheduler
     */
    context_switch(
        &co->ctx,
        &scheduler_ctx
    );

    abort();
}


/*
 * 创建 coroutine
 */
coroutine_t *co_create(
    coroutine_func func,
    void *arg,
    size_t stack_size
)
{
    TRACE_ENTER();
    coroutine_t *co;

    co = calloc(1, sizeof(*co));

    if (co == NULL) {
        return NULL;
    }

    co->stack = malloc(stack_size);

    if (co->stack == NULL) {
        free(co);
        return NULL;
    }

    co->stack_size = stack_size;
    co->func = func;
    co->arg = arg;

    co->state = CO_READY;

    co->wait_fd = -1;


    /*
     * x86-64 stack 向低地址增长
     */
    uintptr_t stack_top =
        (uintptr_t)co->stack + stack_size;

    /*
     * 16 byte alignment
     */
    stack_top &= ~((uintptr_t)0xF);

    /*
     * fake return address
     */
    stack_top -= sizeof(uint64_t);

         *(uint64_t *)stack_top =
        (uint64_t)coroutine_entry;


    /*
     * 第一次 resume 时：
     *
     * rsp -> coroutine stack
     *
     * rip -> coroutine_entry
     */
    co->ctx.rsp = stack_top;



    return co;
}




/*
 * 当前 coroutine 主动 yield
 */
void co_yield(void)
{
    TRACE_ENTER();
    coroutine_t *co = current_co;

    if (co == NULL) {
        return;
    }

    /*
     * 保存当前 coroutine
     */
    context_switch(
        &co->ctx,
        &scheduler_ctx
    );
}


/*
 * 当前 coroutine 等待 fd 可读
 */
void co_wait_read(int fd)
{
    TRACE_ENTER();
    coroutine_t *co = current_co;

    if (co == NULL) {
        return;
    }

    struct epoll_event ev;

    memset(&ev, 0, sizeof(ev));

    ev.events = EPOLLIN;

    /*
     * 把 coroutine 指针直接放进 epoll event
     */
    ev.data.ptr = co;


    /*
     * 告诉 epoll：
     *
     * fd 可读的时候通知我
     */
    if (epoll_ctl(
            epoll_fd,
            EPOLL_CTL_ADD,
            fd,
            &ev) < 0) {

        perror("epoll_ctl ADD");
        exit(1);
    }


    co->wait_fd = fd;

    co->state = CO_WAITING;

    /*
     * 当前 coroutine 暂停
     *
     * CPU 控制权交给 scheduler
     */
    context_switch(
        &co->ctx,
        &scheduler_ctx
    );
}


/*
 * resume coroutine
 */
void co_resume(coroutine_t *co)
{
    TRACE_ENTER();
    if (co == NULL) {
        return;
    }

    if (co->state == CO_DEAD) {
        return;
    }

    /*
     * Scheduler -> Coroutine
     */
    current_co = co;

    co->state = CO_RUNNING;

    context_switch(
        &scheduler_ctx,
        &co->ctx
    );
}


/*
 * 判断结束
 */
int co_finished(coroutine_t *co)
{
    TRACE_ENTER();
    if (co == NULL) {
        return 1;
    }

    return co->state == CO_DEAD;
}


/*
 * 销毁
 */
void co_destroy(coroutine_t *co)
{
    TRACE_ENTER();
    if (co == NULL) {
        return;
    }

    free(co->stack);
    free(co);
}


/*
 * Scheduler
 */
void co_scheduler_run(void)
{
    TRACE_ENTER();
    struct epoll_event events[16];

    while (1) {

        /*
         * 等待 IO
         */
        int n = epoll_wait(
            epoll_fd,
            events,
            16,
            -1
        );
/*

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
阻塞等待 IO 事件就绪。
epfd：epoll 实例 fd
events：输出参数，内核把就绪事件拷贝到这个数组
maxevents：数组最大容量，不能为 0
timeout：超时毫秒
-1：永久阻塞
0：非阻塞，立刻返回
0：等待毫秒数
返回值：
0：就绪事件数量
0：超时没有事件
-1：出错*/
        if (n < 0) {

            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait");
            exit(1);
        }


        for (int i = 0; i < n; i++) {

            coroutine_t *co =
                events[i].data.ptr;

            int fd = co->wait_fd;

            printf(
                "[scheduler] fd=%d ready, "
                "resume coroutine %p\n",
                fd,
                (void *)co
            );


            /*
             * 从 epoll 删除
             */
            epoll_ctl(
                epoll_fd,
                EPOLL_CTL_DEL,
                fd,
                NULL
            );


            co->wait_fd = -1;

            co->state = CO_READY;


            /*
             * Scheduler
             *      ↓
             * Coroutine
             */
            co_resume(co);


            /*
             * Coroutine 可能又 yield：
             *
             * 1. co_wait_read()
             * 2. co_yield()
             * 3. 或者直接结束
             *
             * 回到这里继续 scheduler
             */
        }
    }
}


/*
 * 初始化 scheduler
 */
__attribute__((constructor))
static void scheduler_init(void)
{
    epoll_fd = epoll_create1(0);

    if (epoll_fd < 0) {
        perror("epoll_create1");
        exit(1);
    }
}