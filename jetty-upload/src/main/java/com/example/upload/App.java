package com.example.upload;

import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.ServerConnector;
import org.eclipse.jetty.servlet.ServletContextHandler;
import org.eclipse.jetty.servlet.ServletHolder;
import org.eclipse.jetty.util.thread.QueuedThreadPool;

import javax.servlet.MultipartConfigElement;
import java.io.File;

/**
 * 内嵌 Jetty 启动入口。
 *
 * 启动参数（-D 或环境变量均可）：
 *   port       监听端口，默认 8080        环境变量 PORT
 *   uploadDir  文件落盘目录，默认 ./uploads 环境变量 UPLOAD_DIR
 *   maxSize    单个文件上限字节，默认 1GB   环境变量 MAX_SIZE
 */
public class App {

    public static void main(String[] args) throws Exception {
        int port = intOpt("port", "PORT", 8080);
        String uploadDir = strOpt("uploadDir", "UPLOAD_DIR", new File("uploads").getAbsolutePath());
        long maxSize = longOpt("maxSize", "MAX_SIZE", 1024L * 1024 * 1024);

        new File(uploadDir).mkdirs();

        QueuedThreadPool pool = new QueuedThreadPool(50, 4);
        pool.setName("jetty");

        Server server = new Server(pool);
        ServerConnector connector = new ServerConnector(server);
        connector.setPort(port);
        connector.setIdleTimeout(300_000);
        server.addConnector(connector);

        ServletContextHandler ctx = new ServletContextHandler(ServletContextHandler.NO_SESSIONS);
        ctx.setContextPath("/");

        // 上传接口：multipart 解析上限在这里配置
        ServletHolder upload = new ServletHolder("upload", new UploadServlet(uploadDir));
        upload.getRegistration().setMultipartConfig(
                new MultipartConfigElement(uploadDir, maxSize, maxSize + 8L * 1024 * 1024, 1024 * 1024));
        ctx.addServlet(upload, "/api/upload");

        ctx.addServlet(new ServletHolder("health", new HealthServlet()), "/api/health");

        ctx.addServlet(new ServletHolder("hi", new MyServlet()), "/hi");

        server.setHandler(ctx);
        server.setStopAtShutdown(true);
        server.start();

        System.out.println("[jetty-upload] started");
        System.out.println("[jetty-upload]   POST http://0.0.0.0:" + port + "/api/upload");
        System.out.println("[jetty-upload]   GET  http://0.0.0.0:" + port + "/api/health");
        System.out.println("[jetty-upload]   uploadDir = " + uploadDir);
        System.out.println("[jetty-upload]   maxSize   = " + maxSize + " bytes");
        server.join();
    }

    private static String strOpt(String prop, String env, String def) {
        String v = System.getProperty(prop);
        if (v == null || v.isEmpty()) {
            v = System.getenv(env);
        }
        return (v == null || v.isEmpty()) ? def : v;
    }

    private static int intOpt(String prop, String env, int def) {
        return Integer.parseInt(strOpt(prop, env, String.valueOf(def)));
    }

    private static long longOpt(String prop, String env, long def) {
        return Long.parseLong(strOpt(prop, env, String.valueOf(def)));
    }
}
