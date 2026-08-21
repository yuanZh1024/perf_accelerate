.text
.globl context_switch
.type context_switch, @function

# void context_switch(struct context *old,
#                     struct context *new);

context_switch:

    # =========================
    # 保存当前 context
    # =========================

    movq %rsp, 0(%rdi)

    movq %rbx, 8(%rdi)
    movq %rbp, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 32(%rdi)
    movq %r14, 40(%rdi)
    movq %r15, 48(%rdi)


    # =========================
    # 恢复 new context
    # =========================

    movq 0(%rsi), %rsp

    movq 8(%rsi), %rbx
    movq 16(%rsi), %rbp
    movq 24(%rsi), %r12
    movq 32(%rsi), %r13
    movq 40(%rsi), %r14
    movq 48(%rsi), %r15


    # =========================
    # 从 new stack 中取返回地址
    # =========================

    ret