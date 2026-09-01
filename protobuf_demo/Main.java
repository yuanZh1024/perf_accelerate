package demo;

public class Main {

    public static void main(String[] args) {

        UserOuterClass.User user =
                UserOuterClass.User.newBuilder()
                        .setId(100)
                        .setName("zhangsan")
                        .setAge(30)
                        .build();

        System.out.println(user);
    }
}
