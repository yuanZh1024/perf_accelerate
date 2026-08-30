#include <stdio.h>
#include "business_c.h"

//简易UT断言
#define ASSERT_EQ(a,b) do{ \
    if((a)!=(b)){ \
        printf("FAIL: %d != %d\n",a,b); \
    }else{ \
        printf("PASS\n"); \
    } \
}while(0)

//UT测试函数
void test_case_01(void)
{
    printf("run test_case_01\n");
    int res = process(11,20);
    ASSERT_EQ(res,-1);
}

//UT测试函数
void test_case_02(void)
{
    printf("run test_case_01\n");
    int res = process(10,20);
    ASSERT_EQ(res, 999); //mock固定返回999
}

int main(void)
{
    test_case_01();
    test_case_02();
    return 0;
}
