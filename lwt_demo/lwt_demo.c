#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define STACK_SIZE (64 * 1024)

struct context {
    uint64_t rsp;

    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
};

extern void context_switch(struct context *old,
                            struct context *new);

struct coroutine {
    struct context ctx;

    void *stack;

    void (*func)(void *);

    void *arg;

    int finished;
};

static struct coroutine *current;

static struct context main_ctx;

static struct coroutine *co_a;
static struct coroutine *co_b;


void yield(void)
{
    struct coroutine *self = current;

    context_switch(&self->ctx,
                   &main_ctx);
}


static void coroutine_entry(void)
{
    current->func(current->arg);

    current->finished = 1;

    yield();

    abort();
}


struct coroutine *
coroutine_create(void (*func)(void *), void *arg)
{
    struct coroutine *co;

    co = malloc(sizeof(*co));

    if (!co)
        return NULL;

    memset(co, 0, sizeof(*co));

    co->stack = malloc(STACK_SIZE);

    if (!co->stack) {
        free(co);
        return NULL;
    }

    co->func = func;
    co->arg = arg;

    uint64_t sp =
        (uint64_t)co->stack + STACK_SIZE;

    sp &= ~0xFULL;

    sp -= sizeof(uint64_t);

    *(uint64_t *)sp =
        (uint64_t)coroutine_entry;

    co->ctx.rsp = sp;

    return co;
}


void run_coroutine(struct coroutine *co)
{
    if (co->finished)
        return;

    current = co;

    context_switch(&main_ctx,
                   &co->ctx);
}


void task_a(void *arg)
{
    for (int i = 1; i <= 3; i++) {

        printf("A: %d\n", i);

        yield();
    }
}


void task_b(void *arg)
{
    for (int i = 1; i <= 3; i++) {

        printf("B: %d\n", i);

        yield();
    }
}


int main(void)
{
    printf("main: start\n");

    co_a = coroutine_create(task_a, NULL);
    co_b = coroutine_create(task_b, NULL);

    while (!co_a->finished ||
           !co_b->finished) {

        if (!co_a->finished)
            run_coroutine(co_a);

        if (!co_b->finished)
            run_coroutine(co_b);
    }

    printf("main: done\n");

    free(co_a->stack);
    free(co_a);

    free(co_b->stack);
    free(co_b);

    return 0;
}