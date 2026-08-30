// main.c
#include <stdio.h>
#include "business_c.h"


int process(int a, int b)
{
    if (a > 10)
    {
        return -1;
    }
    int ret = calc(10, 20);
    printf("calc return: %d\n", ret);
    return ret;
}
