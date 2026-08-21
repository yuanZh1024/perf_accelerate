
本质就是save load，通过改变rsp rip寄存器的值来实现“协程”的切换




执行过程命令回显：
(gdb) b context_switch
Breakpoint 1 at 0x400932: file context.S, line 14.
(gdb) r
Starting program: /lwt/lwt 
main: start

Breakpoint 1, context_switch () at context.S:14
14          movq %rsp, 0(%rdi)
Missing separate debuginfos, use: debuginfo-install glibc-2.17-111.h35.x86_64
(gdb) info registers rsp rip rdi rsi
rsp            0x7fffffffd6f8   0x7fffffffd6f8
rip            0x400932 0x400932 <context_switch>
rdi            0x6010c0 6295744
rsi            0x602010 6299664
(gdb) l
9
10          # =========================
11          # 保存当前 context
12          # =========================
13
14          movq %rsp, 0(%rdi)
15
16          movq %rbx, 8(%rdi)
17          movq %rbp, 16(%rdi)
18          movq %r12, 24(%rdi)
(gdb) n
16          movq %rbx, 8(%rdi)
(gdb) n
17          movq %rbp, 16(%rdi)
(gdb) n
18          movq %r12, 24(%rdi)
(gdb) n
19          movq %r13, 32(%rdi)
(gdb) n
20          movq %r14, 40(%rdi)
(gdb) n
21          movq %r15, 48(%rdi)
(gdb) n
28          movq 0(%rsi), %rsp
(gdb) n
context_switch () at context.S:30
30          movq 8(%rsi), %rbx
(gdb) n
31          movq 16(%rsi), %rbp
(gdb) n
32          movq 24(%rsi), %r12
(gdb) n
33          movq 32(%rsi), %r13
(gdb) n
34          movq 40(%rsi), %r14
(gdb) n
35          movq 48(%rsi), %r15
(gdb) n
context_switch () at context.S:42
42          ret
gdb) info registers rsp rip rdi rsi
rsp            0x612068 0x612068
rip            0x400968 0x400968 <context_switch+54>
rdi            0x6010c0 6295744
rsi            0x602010 6299664
(gdb) x/1gx 0x612068
0x612068:       0x0000000000400699
(gdb) p coroutine_entry 
$1 = {void (void)} 0x400699 <coroutine_entry>
(gdb) l
37
38          # =========================
39          # 从 new stack 中取返回地址
40          # =========================
41
42          ret
(gdb) n
coroutine_entry () at main.c:52
52      {
(gdb) bt
#0  coroutine_entry () at main.c:52
#1  0x0000000000000000 in ?? ()
(gdb) l
47                         &main_ctx);
48      }
49
50
51      static void coroutine_entry(void)
52      {
53          current->func(current->arg);
54
55          current->finished = 1;
56
(gdb) b task_a
Breakpoint 2 at 0x4007df: file main.c, line 115.
(gdb) bt
#0  coroutine_entry () at main.c:52
#1  0x0000000000000000 in ?? ()
(gdb) c
Continuing.

Breakpoint 2, task_a (arg=0x0) at main.c:115
115         for (int i = 1; i <= 3; i++) {
(gdb) bt
#0  task_a (arg=0x0) at main.c:115
#1  0x00000000004006b8 in coroutine_entry () at main.c:53
#2  0x0000000000000000 in ?? ()
(gdb) l
10     }
111
112
113     void task_a(void *arg)
114     {
115         for (int i = 1; i <= 3; i++) {
116
117             printf("A: %d\n", i);
118
119             yield();
(gdb) n
117             printf("A: %d\n", i);
(gdb) info registers rsp rip rdi rsi
rsp            0x612038 0x612038
rip            0x4007e8 0x4007e8 <task_a+21>
rdi            0x0      0
rsi            0x602010 6299664
(gdb) x/20gx 0x612038 
0x612038:       0x0000000000000000      0x0000000000000000
0x612048:       0x0000000000000000      0x0000000100000000
0x612058:       0x0000000000612068      0x00000000004006b8
0x612068:       0x0000000000000000      0x0000000000000000
0x612078:       0x0000000000000061      0x00000000006220d8
0x612088:       0x0000000000000000      0x0000000000000000
0x612098:       0x0000000000000000      0x0000000000000000
0x6120a8:       0x0000000000000000      0x0000000000000000
0x6120b8:       0x00000000006120e0      0x000000000040080f
0x6120c8:       0x0000000000000000      0x0000000000000000
(gdb) n
A: 1
119             yield();
(gdb) s
yield () at main.c:44
44          struct coroutine *self = current;
(gdb) bt
#0  yield () at main.c:44
#1  0x0000000000400801 in task_a (arg=0x0) at main.c:119
#2  0x00000000004006b8 in coroutine_entry () at main.c:53
#3  0x0000000000000000 in ?? ()
(gdb) l
39      static struct coroutine *co_b;
40
41
42      void yield(void)
43      {
44          struct coroutine *self = current;
45
46          context_switch(&self->ctx,
47                         &main_ctx);
48      }
(gdb) n
46          context_switch(&self->ctx,
(gdb) n

Breakpoint 1, context_switch () at context.S:14
14          movq %rsp, 0(%rdi)
(gdb) bt
#0  context_switch () at context.S:14
#1  0x0000000000400696 in yield () at main.c:46
#2  0x0000000000400801 in task_a (arg=0x0) at main.c:119
#3  0x00000000004006b8 in coroutine_entry () at main.c:53
#4  0x0000000000000000 in ?? ()
(gdb) l
9
10          # =========================
11          # 保存当前 context
12          # =========================
13
14          movq %rsp, 0(%rdi)
15
16          movq %rbx, 8(%rdi)
17          movq %rbp, 16(%rdi)
18          movq %r12, 24(%rdi)
(gdb) c
Continuing.

Breakpoint 1, context_switch () at context.S:14
14          movq %rsp, 0(%rdi)
(gdb) bt
#0  context_switch () at context.S:14
#1  0x00000000004007ce in run_coroutine (co=0x612080) at main.c:108
#2  0x00000000004008c1 in main () at main.c:149

reakpoint 1, context_switch () at context.S:14
14          movq %rsp, 0(%rdi)
(gdb) info registers rsp rip rdi rsi
rsp            0x622080 0x622080
rip            0x400932 0x400932 <context_switch>
rdi            0x612080 6365312
rsi            0x6010c0 6295744
(gdb) x/20gx 0x622080 
0x622080:       0x0000000000400696      0x0000000000000000
0x622090:       0x0000000000612080      0x00000000006220c8
0x6220a0:       0x000000000040083d      0x0000000000000000
0x6220b0:       0x0000000000000000      0x0000000000000000
0x6220c0:       0x0000000300000000      0x00000000006220d8
0x6220d0:       0x00000000004006b8      0x0000000000000000
0x6220e0:       0x0000000000000000      0x0000000000000f21
0x6220f0:       0x0000000000000000      0x0000000000000000
0x622100:       0x0000000000000000      0x0000000000000000
0x622110:       0x0000000000000000      0x0000000000000000
(gdb) 