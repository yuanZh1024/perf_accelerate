# CAS
CAS是原子指令，才保证了读、比较、写过程中不会有其他cpu核去修改
```
int __sync_bool_compare_and_swap(volatile int *addr, int expected, int new_value)
比较addr的值是否 == expected
如果等于，就更新为new_value，返回0
否则就返回非0值

```
**硬件指令级原子性**
CAS 是 CPU 单条汇编指令，不是好几条普通 C 代码。普通代码：读、if 判断、写，是多条指令，线程中间会被切换；
CAS 指令执行期间，CPU 不会发生线程切换，整个【读‑比较‑写】作为一个不可分割整体。
**多核场景：缓存一致性协议 (MESI)**
多核 CPU 每个核有自己 L1/L2 缓存。多个 CPU 同时对同一个内存地址做 CAS：
CPU 通过 MESI 缓存一致性协议，保证同一时刻只有一个 CPU 核拥有该内存行的修改权限。
其它 CPU 的缓存副本失效，必须等待，不能同时写。
也就是说：就算多核并发抢同一个变量，硬件层面天然串行化 CAS 操作，不会出现两个 CPU 同时改写同一个地址。

**业务层的自旋重试逻辑**
硬件只保证单条 CAS 原子；上层代码一般写自旋循环：
运行
```
//一定要用volatile来修饰val，保证val的读取是直接从内存中读取 不会从寄存器读取缓存值
volatile int val = 0;
while(!CAS(&val, old, new)){
    old = val; // 拿到最新值，重试
}
```
CAS 失败≠出错，代表别的线程抢先修改了变量，本地更新预期值，再重试。
这就是无锁编程的核心：不阻塞线程，循环抢硬件原子指令。


## cas_mock.c
输出： 每次计数都不一样
```
./mock_cas.out
counter = 720702
[root@debug-env test]# ./mock_cas.out
counter = 462763
[root@debug-env test]# ./mock_cas.out
counter = 609662
```


## 关于volatile，从汇编指令的区别去看

volatile 的作用之一，是要求编译器不要把对该变量的访问优化掉、合并掉或缓存成寄存器值；每一次 C 代码中对 volatile 对象的访问，都必须产生对应的实际内存访问。

在 x86 上，这通常表现为生成一个 Load（通常就是 mov）。

代码1： 不加volatile
```
#include <stdio.h>
#include <stdint.h>
uint64_t val;

uint64_t foo()
{
    uint64_t a = val;
    uint64_t b = val;
    return a + b;
}
int main(){};

```
汇编如下
```
foo:
.LFB3:
        .cfi_startproc
        movq    val(%rip), %rax
        addq    %rax, %rax
        ret
        .cfi_endproc
.LFE3:
        .size   foo, .-foo
        .section        .text.startup,"ax",@progbits
        .p2align 4,,15
        .globl  main
        .type   main, @function
main:
.LFB4:
        .cfi_startproc
        xorl    %eax, %eax
        ret
        .cfi_endproc
```
注意只生成了一条  movq    val(%rip), %rax

代码2：
```
#include <stdio.h>
#include <stdint.h>
volatile uint64_t val;

uint64_t foo()
{
    uint64_t a = val;
    uint64_t b = val;
    return a + b;
}
int main(){};

```
汇编：

```
foo:
.LFB3:
        .cfi_startproc
        movq    val(%rip), %rax
        movq    val(%rip), %rdx
        addq    %rdx, %rax
        ret
        .cfi_endproc
.LFE3:
        .size   foo, .-foo
        .section        .text.startup,"ax",@progbits
        .p2align 4,,15
        .globl  main
        .type   main, @function
main:
.LFB4:
        .cfi_startproc
        xorl    %eax, %eax
        ret
        .cfi_endproc

```

注意只生成了2条  movq    val(%rip)

这两条指令实际上访问的是同一个内存

注意：

movq val(%rip), %rax
movq val(%rip), %rdx


             val
              │
        ┌─────┴─────┐
        │           │
      Load        Load
        │           │
        ▼           ▼
       RAX         RDX

也就是两次读取同一个 val。

movq 源 目的
movq A，B  = A赋值给B
movq  内存, 寄存器    → Load
movq  寄存器, 内存    → Store