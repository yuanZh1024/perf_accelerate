package perf_accelerate.java_review.UT_mock_principle.java_mock;

public class TestMain {

    public static void main(String[] args) {

        UserService userService =
                new RealUserService();

        /*
            UserService userService =
                SimpleMock.create();
         */

        OrderService orderService =
                new OrderService(userService);

        System.out.println(
                "process() = " +
                orderService.process()
        );
    }
}