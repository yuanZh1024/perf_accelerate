// real_func.c
#include <stdio.h>

// 原始真实函数
int calc(int a, int b)
{
    printf("[real calc] run, a=%d b=%d\n", a, b);
    return a + b;
}
//如果 `main()` 和 `calc()` 在同一个 `.c` 源码文件里，链接期符号抢占直接失效！

// int main(void)
// {
//     int ret = calc(10,20);
//     printf("calc return: %d\n", ret);
//     return 0;
// }