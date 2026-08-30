package perf_accelerate.java_review.reflect_review;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class ReflectActionDemo {

    public static void main(String[] args) throws Exception {
        // ------------------------------------------------------------
        // 第一步：查（元数据仓库）- 扫描类的全部家底
        // ------------------------------------------------------------
        System.out.println("========== 1. 查（元数据仓库） ==========");

        // 获取Class对象（JVM管理的那个）
        Class<?> clazz = Class.forName("com.helloworld.UserService");

        // 查注解：看看这个类需不需要被框架管理（模拟Spring扫描）
        boolean hasAnnotation = clazz.isAnnotationPresent(MyComponent.class);
        System.out.println("是否带有 @MyComponent ? " + hasAnnotation);
        if (!hasAnnotation) {
            System.out.println("不是组件，跳过创建");
            return;
        }

        // 查构造器：看看怎么创建它
        Constructor<?>[] constructors = clazz.getDeclaredConstructors();
        System.out.println("构造器列表: " + constructors.length + " 个");

        // 查字段：哪怕是私有的，也能扒出来
        Field[] fields = clazz.getDeclaredFields();
        for (Field f : fields) {
            System.out.println("发现的字段: " + f.getName() + " 类型: " + f.getType().getSimpleName());
        }

        // 查方法：包括私有方法
        Method[] methods = clazz.getDeclaredMethods();
        System.out.println("发现的方法数量: " + methods.length);
        for (Method m : methods) {
            System.out.println("  方法名: " + m.getName());
        }

        // ------------------------------------------------------------
        // 第二步：建（动态工厂）- 不用 new，凭空造物
        // ------------------------------------------------------------
        System.out.println("\n========== 2. 建（动态工厂） ==========");

        // 获取无参构造器并暴力创建（如果构造器是 private 也可以 setAccessible）
        Constructor<?> constructor = clazz.getDeclaredConstructor();
        // 这一行就是 Spring 的 `newInstance` 底层逻辑
        UserService service = (UserService) constructor.newInstance();
        System.out.println("创建出的对象哈希: " + service);

        // ------------------------------------------------------------
        // 第三步：调（动态桥梁）- 编译时不知道方法名，运行时动态调
        // ------------------------------------------------------------
        System.out.println("\n========== 3. 调（动态桥梁） ==========");

        // 场景：方法名是从配置文件（application.properties）中读取的，假设配置值为 "login"
        String methodNameFromConfig = "login";

        // 获取 public 方法
        Method publicMethod = clazz.getMethod(methodNameFromConfig, String.class, String.class);
        // 动态调用！参数 "admin", "123" 也是运行时传入的
        publicMethod.invoke(service, "admin", "123");

        // --- 进阶：调用私有的 init 方法（模拟Bean初始化回调） ---
        System.out.println("\n--- 额外：暴力调用私有方法 ---");
        Method privateMethod = clazz.getDeclaredMethod("init");
        // 破坏封装（反射的经典权力）
        privateMethod.setAccessible(true);
        privateMethod.invoke(service);

        // --- 进阶：修改私有字段（模拟依赖注入 @Autowired） ---
        System.out.println("\n--- 额外：暴力修改私有字段（依赖注入） ---");
        Field dbField = clazz.getDeclaredField("databaseUrl");
        dbField.setAccessible(true);
        // 运行时把数据库连接地址改掉（比如从测试环境切到生产环境）
        dbField.set(service, "jdbc:mysql://prod:3306/prod_db");

        // 再次调用业务方法，看看数据库地址是否变了
        publicMethod.invoke(service, "admin", "123");
    }
}
