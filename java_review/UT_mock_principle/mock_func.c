// mock_func.c
#include <stdio.h>

// 和原函数完全一样的符号
int calc(int a, int b)
{
    printf("[mock calc] intercepted! a=%d b=%d\n", a, b);
    // mock可以直接返回伪造结果
    return 999;
}
