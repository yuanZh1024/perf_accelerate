package com.example.upload;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.Part;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.Enumeration;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;

/**
 * 对象上传 Servlet。
 *
 * POST /api/upload
 *   1) multipart/form-data：取所有带文件名的 part 落盘
 *   2) 其它 Content-Type（如 application/octet-stream）：整体当裸流落盘，
 *      文件名取请求头 X-File-Name，没有则自动生成
 *   请求头全部解析并打日志，同时把解析结果写成 <uploadId>.meta.json
 *   成功统一返回纯文本 "success"
 */
public class UploadServlet extends HttpServlet {

    private static final long serialVersionUID = 1L;
    private static final Logger LOG = LoggerFactory.getLogger(UploadServlet.class);
    private static final int BUF = 64 * 1024;
    private static final int MAX_NAME = 120;

    private final Path uploadDir;

    public UploadServlet(String dir) throws IOException {
        this.uploadDir = Paths.get(dir).toAbsolutePath().normalize();
        Files.createDirectories(this.uploadDir);
    }

    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {

        long t0 = System.currentTimeMillis();
        String uploadId = UUID.randomUUID().toString().replace("-", "").substring(0, 16);

        // ---------- 1. 解析请求头 ----------
        Map<String, String> headers = parseHeaders(req);
        String clientIp = firstIp(headers.get("x-forwarded-for"));
        if (clientIp == null || clientIp.isEmpty()) {
            clientIp = req.getRemoteAddr();
        }

        LOG.info("==== upload {} begin ====", uploadId);
        LOG.info("  {} {} from {}:{}", req.getMethod(), req.getRequestURI(), req.getRemoteAddr(), req.getRemotePort());
        LOG.info("  clientIp={} (via X-Forwarded-For={})", clientIp, headers.get("x-forwarded-for"));
        LOG.info("  userAgent={}", headers.get("user-agent"));
        LOG.info("  contentType={} contentLength={} transferEncoding={}",
                headers.get("content-type"), headers.get("content-length"), headers.get("transfer-encoding"));
        LOG.info("  traceId={}", firstNonNull(headers.get("x-trace-id"), headers.get("x-request-id"), "-"));
        LOG.info("  headers total={}", headers.size());
        for (Map.Entry<String, String> e : headers.entrySet()) {
            LOG.info("    H {}: {}", e.getKey(), mask(e.getKey(), e.getValue()));
        }

        // ---------- 2. 落盘 ----------
        List<SavedFile> saved = new ArrayList<SavedFile>();
        try {
            String ct = firstNonNull(headers.get("content-type"), "").toLowerCase(Locale.ROOT);
            if (ct.startsWith("multipart/")) {
                for (Part part : req.getParts()) {
                    String fn = part.getSubmittedFileName();
                    if (fn == null || fn.isEmpty()) {
                        LOG.info("  skip form field '{}' = {}", part.getName(), peek(part));
                        continue;
                    }
                    LOG.info("  part '{}' filename={} contentType={} size={}",
                            part.getName(), fn, part.getContentType(), part.getSize());
                    saved.add(save(uploadId, fn, part.getInputStream()));
                }
            } else {
                String fn = headers.get("x-file-name");
                if (fn == null || fn.trim().isEmpty()) {
                    fn = "raw-" + uploadId + ".bin";
                }
                saved.add(save(uploadId, fn, req.getInputStream()));
            }
        } catch (Exception ex) {
            LOG.error("upload " + uploadId + " failed", ex);
            writeText(resp, HttpServletResponse.SC_INTERNAL_SERVER_ERROR, "failed: " + ex.getMessage());
            return;
        }

        if (saved.isEmpty()) {
            LOG.warn("upload {}: no file part found", uploadId);
            writeText(resp, HttpServletResponse.SC_BAD_REQUEST, "failed: no file part");
            return;
        }

        // ---------- 3. 元数据（请求头解析结果）落盘 ----------
        Path meta = uploadDir.resolve(uploadId + ".meta.json");
        Files.write(meta, metaJson(uploadId, clientIp, headers, saved, req).getBytes(StandardCharsets.UTF_8));

        long cost = System.currentTimeMillis() - t0;
        LOG.info("==== upload {} done: {} file(s), {} ms ====", uploadId, saved.size(), cost);

        // ---------- 4. 返回 success ----------
        resp.setStatus(HttpServletResponse.SC_OK);
        resp.setContentType("text/plain;charset=UTF-8");
        resp.setHeader("X-Upload-Id", uploadId);
        resp.setHeader("X-File-Count", String.valueOf(saved.size()));
        resp.setHeader("X-File-Sha256", saved.get(0).sha256);
        resp.setHeader("X-Elapsed-Ms", String.valueOf(cost));
        PrintWriter w = resp.getWriter();
        w.write("success");
        w.flush();
    }

    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        writeText(resp, HttpServletResponse.SC_METHOD_NOT_ALLOWED,
                "请用 POST /api/upload 上传，例如:\n"
                        + "curl -F \"file=@/path/a.txt\" http://127.0.0.1:8080/api/upload\n");
    }

    // ==================== 内部实现 ====================

    /** 解析全部请求头，key 统一小写，多值以 ", " 合并 */
    private Map<String, String> parseHeaders(HttpServletRequest req) {
        Map<String, String> map = new LinkedHashMap<String, String>();
        Enumeration<String> names = req.getHeaderNames();
        if (names == null) {
            return map;
        }
        List<String> list = Collections.list(names);
        Collections.sort(list, String.CASE_INSENSITIVE_ORDER);
        for (String name : list) {
            StringBuilder sb = new StringBuilder();
            Enumeration<String> vals = req.getHeaders(name);
            while (vals != null && vals.hasMoreElements()) {
                if (sb.length() > 0) {
                    sb.append(", ");
                }
                sb.append(vals.nextElement());
            }
            map.put(name.toLowerCase(Locale.ROOT), sb.toString());
        }
        return map;
    }

    private SavedFile save(String uploadId, String rawName, InputStream in) throws Exception {
        String name = sanitize(rawName);
        Path target = uploadDir.resolve(uploadId + "_" + name);

        MessageDigest md = MessageDigest.getInstance("SHA-256");
        long size = 0;
        InputStream is = in;
        OutputStream os = Files.newOutputStream(target);
        try {
            byte[] buf = new byte[BUF];
            int n;
            while ((n = is.read(buf)) > 0) {
                os.write(buf, 0, n);
                md.update(buf, 0, n);
                size += n;
            }
            os.flush();
        } finally {
            try {
                os.close();
            } catch (IOException ignore) {
            }
            try {
                is.close();
            } catch (IOException ignore) {
            }
        }

        String sha = hex(md.digest());
        LOG.info("  saved {} -> {} ({} bytes, sha256={})", name, target, size, sha);
        return new SavedFile(name, target.toString(), size, sha);
    }

    private String metaJson(String uploadId, String clientIp, Map<String, String> headers,
                            List<SavedFile> files, HttpServletRequest req) {
        StringBuilder sb = new StringBuilder(1024);
        sb.append("{\n");
        sb.append("  \"uploadId\": \"").append(uploadId).append("\",\n");
        sb.append("  \"time\": \"").append(new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssXXX").format(new Date())).append("\",\n");
        sb.append("  \"method\": \"").append(esc(req.getMethod())).append("\",\n");
        sb.append("  \"uri\": \"").append(esc(req.getRequestURI())).append("\",\n");
        sb.append("  \"query\": \"").append(esc(req.getQueryString())).append("\",\n");
        sb.append("  \"clientIp\": \"").append(esc(clientIp)).append("\",\n");
        sb.append("  \"protocol\": \"").append(esc(req.getProtocol())).append("\",\n");

        sb.append("  \"headers\": {\n");
        int i = 0;
        for (Map.Entry<String, String> e : headers.entrySet()) {
            sb.append("    \"").append(esc(e.getKey())).append("\": \"").append(esc(mask(e.getKey(), e.getValue()))).append("\"");
            sb.append(++i < headers.size() ? ",\n" : "\n");
        }
        sb.append("  },\n");

        sb.append("  \"files\": [\n");
        for (int k = 0; k < files.size(); k++) {
            SavedFile f = files.get(k);
            sb.append("    {\"name\": \"").append(esc(f.name)).append("\", \"size\": ").append(f.size)
                    .append(", \"sha256\": \"").append(f.sha256).append("\", \"path\": \"").append(esc(f.path)).append("\"}");
            sb.append(k + 1 < files.size() ? ",\n" : "\n");
        }
        sb.append("  ]\n");
        sb.append("}\n");
        return sb.toString();
    }

    /** 去掉路径分隔符与控制字符，防止目录穿越；超长截断 */
    private String sanitize(String raw) {
        String name = raw.replace('\\', '/');
        int slash = name.lastIndexOf('/');
        if (slash >= 0) {
            name = name.substring(slash + 1);
        }
        name = name.replaceAll("[\\p{Cntrl}]", "").trim();
        if (name.isEmpty() || ".".equals(name) || "..".equals(name)) {
            name = "unnamed.bin";
        }
        if (name.length() > MAX_NAME) {
            name = name.substring(name.length() - MAX_NAME);
        }
        return name;
    }

    /** 敏感头打码，避免把 token 写进日志 */
    private String mask(String key, String value) {
        if (value == null) {
            return "";
        }
        String k = key.toLowerCase(Locale.ROOT);
        if (k.equals("authorization") || k.equals("cookie") || k.equals("x-api-key") || k.contains("token")) {
            return value.length() <= 8 ? "***" : value.substring(0, 8) + "***(" + value.length() + ")";
        }
        return value;
    }

    private String firstIp(String xff) {
        if (xff == null || xff.isEmpty()) {
            return null;
        }
        String ip = xff.split(",")[0].trim();
        return ip.isEmpty() ? null : ip;
    }

    private String peek(Part part) {
        try {
            InputStream is = part.getInputStream();
            byte[] b = new byte[256];
            int n = is.read(b);
            is.close();
            return n <= 0 ? "" : new String(b, 0, n, StandardCharsets.UTF_8);
        } catch (IOException e) {
            return "<unreadable>";
        }
    }

    private String firstNonNull(String... vals) {
        for (String v : vals) {
            if (v != null && !v.isEmpty()) {
                return v;
            }
        }
        return null;
    }

    private String hex(byte[] bytes) {
        StringBuilder sb = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) {
            sb.append(Character.forDigit((b >> 4) & 0xF, 16)).append(Character.forDigit(b & 0xF, 16));
        }
        return sb.toString();
    }

    private String esc(String s) {
        if (s == null) {
            return "";
        }
        StringBuilder sb = new StringBuilder(s.length() + 8);
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
                case '"': sb.append("\\\""); break;
                case '\\': sb.append("\\\\"); break;
                case '\n': sb.append("\\n"); break;
                case '\r': sb.append("\\r"); break;
                case '\t': sb.append("\\t"); break;
                default:
                    if (c < 0x20) {
                        sb.append(String.format("\\u%04x", (int) c));
                    } else {
                        sb.append(c);
                    }
            }
        }
        return sb.toString();
    }

    private void writeText(HttpServletResponse resp, int status, String body) throws IOException {
        resp.setStatus(status);
        resp.setContentType("text/plain;charset=UTF-8");
        resp.getWriter().write(body);
        resp.getWriter().flush();
    }

    private static class SavedFile {
        final String name;
        final String path;
        final long size;
        final String sha256;

        SavedFile(String name, String path, long size, String sha256) {
            this.name = name;
            this.path = path;
            this.size = size;
            this.sha256 = sha256;
        }
    }
}
