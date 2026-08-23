 

 int value = 100;

    int ret = __sync_bool_compare_and_swap(
        &value,
        100,
        200
    );


 
 subq    $16, %rsp
        movl    $100, -8(%rbp)
        movl    $100, %eax
        movl    $200, %edx
        lock cmpxchgl   %edx, -8(%rbp)
        sete    %al
        movzbl  %al, %eax


1、subq    $16, %rsp
RSP = RSP - 16
给当前函数的栈帧预留 16 字节空间。

2、movl    $100, -8(%rbp)
局部参数入栈
100 放到 内存地址 = RBP - 8

3、movl    $100, %eax
        movl    $200, %edx

EAX = 100 
EDX = 200
  因为 x86 的：
cmpxchg规定：
比较值放在 EAX 中。

4、lock cmpxchgl %edx, -8(%rbp)
%edx 现在是200
-8(%rbp) 是 value

if (value == EAX)
    value = EDX;
else
    EAX = value;
也就是

if (value == 100)
    value = 200;
else
    EAX = value;