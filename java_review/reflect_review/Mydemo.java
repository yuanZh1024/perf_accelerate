package perf_accelerate.java_review.reflect_review;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
/*
`Class` 对象是通往 Java 类结构信息的唯一入口。它的作用可以概括为“查、建、调”三个方面：

- **查（元数据仓库）**：提供 API 获取类的全方位结构信息，包括修饰符（`getModifiers`）、包路径、父类（`getSuperclass`）、接口（`getInterfaces`）、构造器、方法、字段、注解（`getAnnotations`）等。
- **建（动态工厂）**：不必使用 `new` 关键字，通过 `newInstance()` 或配合 `Constructor` 就可以在运行时动态创建对象（依赖注入框架的核心原理）。
- **调（动态桥梁）**：通过 `getMethod` 获取 `Method` 对象，然后调用 `invoke()`，即使编译时不知道方法名，运行时也能动态调用（如动态代理）。
 */

public class Mydemo {
    public static void main(String[] args) throws ClassNotFoundException, NoSuchMethodException, InvocationTargetException, IllegalAccessException, InstantiationException {
        Class<?> clazz = Class.forName("perf_accelerate.java_review.reflect_review.UserService");

        Object userService = clazz.newInstance();

        Method login = clazz.getMethod("login",String.class, String.class);
        // 必须先有个对象才能调用方法
        login.invoke(userService,"123","hds");
    }
}
