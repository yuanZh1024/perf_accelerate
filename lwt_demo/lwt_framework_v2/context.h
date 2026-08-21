#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdint.h>

typedef struct {
    uint64_t rsp;

    uint64_t rbx;
    uint64_t rbp;

    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
} context_t;

void context_switch(context_t *from, context_t *to);

#endif