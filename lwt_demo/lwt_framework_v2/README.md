gcc -g -O0 -std=c99 -Wall \
    main.c \
    coroutine.c \
    mock_db.c \
    context.S \
    -lpthread \
    -o demo

gcc -g -finstrument-functions app.c -o app
uftrace ./app

gcc -g -DDEBUG_TRACE   -O0 -std=c99 -Wall  \
    main.c \
    coroutine.c \
    mock_db.c \
    context.S \
    -lpthread \
    -ldl \
    -o demo2

gcc -D_GNU_SOURCE -g -finstrument-functions -O0 -std=c99 -Wall \
>     main.c coroutine.c mock_db.c context.S \
>     -lpthread -ldl -o demo2


会看到：
    [request 1] start
[request 1] send SQL to database
[request 1] db query submitted, fd=4
[request 1] wait database...

[request 2] start
[request 2] send SQL to database
[request 2] db query submitted, fd=6
[request 2] wait database...

[request 3] start
[request 3] send SQL to database
[request 3] db query submitted, fd=8
[request 3] wait database...

注意这里：

三个协程全部进入 WAITING。
此时：

Coroutine 1 ──WAIT──► fd 4
Coroutine 2 ──WAIT──► fd 6
Coroutine 3 ──WAIT──► fd 8
                         │
                         ▼
                      epoll_wait()


与此同时三个 mock DB worker 在后台工作：

DB Worker 1 ── sleep(2)
DB Worker 2 ── sleep(2)
DB Worker 3 ── sleep(2)


这时候你应该能看到一个非常关键的分工：

context_switch
    ↓
解决“我从哪里继续执行？”


epoll
    ↓
解决“什么时候可以继续执行？”


scheduler
    ↓
解决“现在应该让谁继续执行？”


async DB client
    ↓
解决“怎么把数据库操作变成可被 epoll 监听的异步 IO？”


业务代码
    ↓
只负责业务逻辑

## epoll关键函数
int epoll_create(int size); 
作用：创建 epoll 实例，返回 epoll 文件描述符

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
用来向 epoll 内核事件表注册 / 修改 / 删除 fd，唯一修改内核监听集合的 API。
op 操作类型
EPOLL_CTL_ADD：添加 fd 到 epoll 监听
EPOLL_CTL_MOD：修改已注册 fd 的监听事件
EPOLL_CTL_DEL：从 epoll 移除 fd

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
-1：出错


## scheduler函数解释
事件循环 + 协程调度器
while(1) {
    epoll_wait() //等待事件
    process event //处理事件
     哪个 fd 有事件？

    找到这个 fd 对应的 coroutine;

    resume coroutine;

}


 context_switch(
        &scheduler_ctx,
        &co->ctx
    );

把scheduler 看成一个特殊的 coroutine，只不过他只负责 while(1)循环来监听事件 等待事件就绪


可以把整个 co_scheduler_run() 压缩成：

while (1) {


    // 1. 等待 IO
    n = epoll_wait();


    // 2. 找到 IO 对应的 coroutine
    for each event {


        co = event.data.ptr;


        // 3. IO 已经好了
        co->state = READY;


        // 4. 恢复 coroutine
        co_resume(co);
    }
}

所以你可以把它记成一句话：

Scheduler = epoll_wait() + 找到对应 Coroutine + co_resume()。




## 最终输出结果
gcc -g -DDEBUG_TRACE   -O0 -std=c99 -Wall  \
    main.c \
    coroutine.c \
    mock_db.c \
    context.S \
    -lpthread \
    -ldl \
    -o demo2

    [ENTER] co_create
[ENTER] co_create
[ENTER] co_create

[ENTER] co_resume
[ENTER] coroutine_entry
[scheduler] start coroutine 0xdac010
[ENTER] handle_request

[request 1] start
[request 1] send SQL to database
[request 1] db query submitted, fd=4
[request 1] wait database...
[ENTER] co_wait_read

[ENTER] co_resume
[ENTER] coroutine_entry
[scheduler] start coroutine 0xdbc090
[ENTER] handle_request

[request 2] start
[request 2] send SQL to database
[request 2] db query submitted, fd=6
[ENTER] db_worker
[mock-db] request 1: database processing...
[request 2] wait database...
[ENTER] co_wait_read
[ENTER] db_worker
[mock-db] request 2: database processing...
[ENTER] co_resume
[ENTER] coroutine_entry
[scheduler] start coroutine 0xdcc110
[ENTER] handle_request

[request 3] start
[request 3] send SQL to database
[request 3] db query submitted, fd=8
[request 3] wait database...
[ENTER] co_wait_read


[ENTER] co_scheduler_run
[ENTER] db_worker
[mock-db] request 3: database processing...
[mock-db] request 1: database finished
[mock-db] request 2: database finished


[scheduler] fd=4 ready, resume coroutine 0xdac010
[ENTER] co_resume
[request 1] database response arrived
[request 1] result = DB_RESULT(request=1)
[request 1] process database result
[mock-db] request 3: database finished
[request 1] send response to client
[request 1] finish
[scheduler] coroutine 0xdac010 finished


[scheduler] fd=6 ready, resume coroutine 0xdbc090
[ENTER] co_resume
[request 2] database response arrived
[request 2] result = DB_RESULT(request=2)
[request 2] process database result
[request 2] send response to client
[request 2] finish
[scheduler] coroutine 0xdbc090 finished


[scheduler] fd=8 ready, resume coroutine 0xdcc110
[ENTER] co_resume
[request 3] database response arrived
[request 3] result = DB_RESULT(request=3)
[request 3] process database result
[request 3] send response to client
[request 3] finish
[scheduler] coroutine 0xdcc110 finished
