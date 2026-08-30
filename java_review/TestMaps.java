public class TestMaps {
    public static void main(String[] args) throws InterruptedException {
        // JDK8没有ProcessHandle，从系统环境变量拿pid(Linux)
        String pid = System.getenv("PID");
        System.out.println("Java Process PID (环境变量): " + pid);

        // 启动5个后台线程，保持存活，观察maps里的栈VMA
        for (int i = 0; i < 1; i++) {
            new Thread(() -> {
                try {
                    Thread.sleep(300_000);
                } catch (InterruptedException e) {
                }
            }, "demo‑thread‑" + i).start();
        }

        System.out.println("===== 请新开终端执行 =====");
        System.out.println("cat /proc/" + pid + "/maps");
        System.out.println("==========================");

        Thread.sleep(300_000);
    }
}
