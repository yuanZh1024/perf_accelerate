package com.example.upload;

import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.lang.management.ManagementFactory;

/** 健康检查：GET /api/health -> 纯文本 ok */
public class HealthServlet extends HttpServlet {

    private static final long serialVersionUID = 1L;
    private static final long START = System.currentTimeMillis();

    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        resp.setStatus(HttpServletResponse.SC_OK);
        resp.setContentType("text/plain;charset=UTF-8");
        resp.getWriter().write("ok\nuptime=" + (System.currentTimeMillis() - START) / 1000 + "s"
                + "\njvm=" + System.getProperty("java.version")
                + "\npid=" + ManagementFactory.getRuntimeMXBean().getName());
        resp.getWriter().flush();
    }
}
