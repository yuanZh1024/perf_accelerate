package perf_accelerate.java_review.reflect_review;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// 模拟Spring的 @Component 注解
@Retention(RetentionPolicy.RUNTIME)
@interface MyComponent {
}

// 这是一个普通的业务类，我们打上模拟注解，并故意包含私有字段
@MyComponent
class UserService {
    // 私有字段，模拟数据库连接池或配置
    private String databaseUrl = "jdbc:mysql://localhost:3306/test";

    // 无参构造器（反射创建必须要有，或者通过构造器参数）
    public UserService() {
        System.out.println("[构造器] UserService 被反射创建了！");
    }

    // 一个业务方法
    public void login(String username, String password) {
        System.out.println("用户 " + username + " 正在使用数据库: " + databaseUrl + " 登录...");
    }

    // 一个私有方法，模拟只有框架底层才能调用的初始化
    private void init() {
        System.out.println("[私有方法] 初始化数据库连接池...");
    }
}
