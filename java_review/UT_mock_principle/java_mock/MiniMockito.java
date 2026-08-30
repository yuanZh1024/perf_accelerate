package perf_accelerate.java_review.UT_mock_principle.java_mock;

import java.lang.reflect.*;
import java.util.*;

public class MiniMockito {

    static Map<Method, Object> rules = new HashMap<>();
    static Method lastMethod;

    // 创建 Mock 对象
    static <T> T mock(Class<T> clazz) {
        return (T) Proxy.newProxyInstance(
                clazz.getClassLoader(),
                new Class<?>[] { clazz },
                (obj, method, args) -> {
                    lastMethod = method;

                    Object value = rules.get(method);

                    if (value != null) {
                        return value;
                    }

                    if (method.getReturnType() == int.class) {
                        return 0;
                    }

                    return null;
                   
                });
    }

    // 记录刚才调用的方法
    static <T> Stub<T> when(T ignored) {
        return new Stub<>(lastMethod);
    }

    // 配置行为
    static class Stub<T> {
        Method method;

        Stub(Method method) {
            this.method = method;
        }

        void thenReturn(T value) {
            rules.put(method, value);
        }
    }

    // 测试
    interface UserService {
        int getUserId();

        String getName();
    }

    public static void main(String[] args) {
        UserService user = mock(UserService.class);

       
        when(user.getUserId()).thenReturn(999);
        when(user.getName()).thenReturn("Tom");

        System.out.println(user.getUserId());
        System.out.println(user.getName());
    }
}
