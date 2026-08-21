#ifndef COROUTINE_H
#define COROUTINE_H

#include <stddef.h>

typedef struct coroutine coroutine_t;

typedef void (*coroutine_func)(void *arg);

coroutine_t *co_create(
    coroutine_func func,
    void *arg,
    size_t stack_size
);

void co_resume(coroutine_t *co);

void co_yield(void);

/*
 * 当前 coroutine 等待 fd 可读
 */
void co_wait_read(int fd);

int co_finished(coroutine_t *co);

void co_destroy(coroutine_t *co);

/*
 * 启动 scheduler
 */
void co_scheduler_run(void);

#endif