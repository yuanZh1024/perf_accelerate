
测试结果
GMock 对普通 C 函数的 mock 本质就是：**链接期符号抢占**。
同一个函数符号，如果你自己实现一份，链接的时候，会优先用你写的版本，原函数可以选择是否保留。

## 链接期符号抢占
使用mock函数测试
PS D:\Code\code_new_0826\perf_accelerate\java_review\UT_mock_principle> gcc .\Test_business.c .\business_c.c .\mock_func.c  -o mocktest

PS D:\Code\code_new_0826\perf_accelerate\java_review\UT_mock_principle> .\mocktest.exe
run test_case_01
PASS
run test_case_01
[mock calc] intercepted! a=10 b=20
calc return: 999
PASS


使用真实函数测试
gcc .\Test_business.c .\business_c.c .\func_module.c  -o mocktest            
PS D:\Code\code_new_0826\perf_accelerate\java_review\UT_mock_principle> 
PS D:\Code\code_new_0826\perf_accelerate\java_review\UT_mock_principle> .\mocktest.exe
run test_case_01
PASS
run test_case_01
[real calc] run, a=10 b=20
calc return: 30
FAIL: 30 != 999
PS D:\Code\code_new_0826\perf_accelerate\java_review\UT_mock_principle> 


## 动态库，LD_PRELOAD改变动态库加载优先级

gcc -shared -fPIC -o libmock.so mock_func.c

 gcc -shared -fPIC -o libbusiness.so business_c.c func_module.c




 - 场景1： 使用libmock.so
 LD_LIBRARY_PATH=. LD_PRELOAD=./libmock.so ./app
run test_case_01
PASS
run test_case_01
[mock calc] intercepted! a=10 b=20
calc return: 999
PASS
root@dev-virtual-machine:/home/dev/code/UT_mock_principle#

 - 场景2： 不使用libmock.so
root@dev-virtual-machine:/home/dev/code/UT_mock_principle# LD_LIBRARY_PATH=.  ./app
run test_case_01
PASS
run test_case_01
[real calc] run, a=10 b=20
calc return: 30
FAIL: 30 != 999
