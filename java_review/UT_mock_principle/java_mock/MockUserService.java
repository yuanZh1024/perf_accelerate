package perf_accelerate.java_review.UT_mock_principle.java_mock;

public class MockUserService implements UserService {

    @Override
    public int getUserId() {
        System.out.println("[MOCK]");
        return 999;
    }
}
