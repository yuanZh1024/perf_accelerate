package perf_accelerate.java_review.UT_mock_principle.java_mock;

public class OrderService {

    private final UserService userService;

    public OrderService(UserService userService) {
        this.userService = userService;
    }

    public int process() {
        int id = userService.getUserId();

        System.out.println("getUserId() = " + id);

        if (id == 100) {
            return 1;
        }

        return 0;
    }
}