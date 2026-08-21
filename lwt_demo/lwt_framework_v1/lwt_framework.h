#ifndef COROUTINE_H
#define COROUTINE_H

#include <stddef.h>

typedef struct coroutine coroutine_t;

typedef void (*co_func_t)(void *arg);

// 创建协程
/*
func 协程要做的事
arg 参数列表
stack_size 栈大小（内存）
*/
coroutine_t *co_create(
    co_func_t func,
    void *arg,
    size_t stack_size
);

void co_resume(coroutine_t *co);

// 协程自动放弃对“CPU”的占用
void co_yield(void);

int co_finished(coroutine_t *co);

void co_destroy(coroutine_t *co);

#endif