package perf_accelerate.java_review.UT_mock_principle.java_mock;

import java.lang.reflect.Proxy;
/*
你可以把整个 Mockito 压缩成：

             Mockito
                │
       ┌────────┴────────┐
       ↓                 ↓
   创建Mock对象        管理Mock规则
       │                 │
       ↓                 ↓
   动态生成类          getUserId → 999
       │                 │
       └────────┬────────┘
                ↓
             方法调用
                ↓
              拦截
                ↓
             查规则
                ↓
             返回结果
              */

public class SimpleMock {

    public static UserService create() {

        return (UserService) Proxy.newProxyInstance(
                UserService.class.getClassLoader(),
                new Class[]{UserService.class},
                (proxy, method, args) -> {

                    System.out.println(
                            "[MOCK] " + method.getName()
                    );

                    if (method.getName().equals("getUserId")) {
                        return 999;
                    }

                    return null;
                }
        );
    }
}