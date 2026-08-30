package perf_accelerate.java_review.UT_mock_principle.java_mock;

public class MockMain {

    public static void main(String[] args) {

            UserService userService =
                SimpleMock.create();
        
        OrderService orderService =
                new OrderService(userService);

        System.out.println(
                "process() = " +
                orderService.process()
        );
    }
}