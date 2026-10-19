package com.example.rs;

import javax.ws.rs.Consumes;
import javax.ws.rs.DefaultValue;
import javax.ws.rs.GET;
import javax.ws.rs.HeaderParam;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.Produces;
import javax.ws.rs.QueryParam;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.HttpHeaders;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.MultivaluedMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.TreeMap;

/**
 * JAX-RS 资源示例（CXF 实现）。
 * 挂载点：/services/rest ，类上的 @Path("/hello") 再拼一层
 *   GET  /services/rest/hello/{name}     路径参数 + 查询参数 + 单个请求头
 *   GET  /services/rest/hello/headers    解析并返回全部请求头
 *   POST /services/rest/hello/echo       JSON 进 JSON 出
 */
@Path("/hello")
@Produces(MediaType.APPLICATION_JSON)
public class HelloResource {

    /** 路径参数 @PathParam + 查询参数 @QueryParam + 请求头 @HeaderParam 三种取值方式 */
    @GET
    @Path("/{name}")
    public Map<String, Object> hello(@PathParam("name") String name,
                                     @QueryParam("lang") @DefaultValue("zh") String lang,
                                     @HeaderParam("X-Trace-Id") String traceId) {
        Map<String, Object> r = new LinkedHashMap<String, Object>();
        r.put("greeting", "zh".equalsIgnoreCase(lang) ? "你好，" + name : "Hello, " + name);
        r.put("lang", lang);
        r.put("traceId", traceId);
        return r;
    }

    /** 用 @Context 注入 HttpHeaders，拿到完整请求头列表 */
    @GET
    @Path("/headers")
    public Map<String, Object> headers(@Context HttpHeaders httpHeaders) {
        MultivaluedMap<String, String> raw = httpHeaders.getRequestHeaders();
        Map<String, String> flat = new TreeMap<String, String>();
        for (Map.Entry<String, java.util.List<String>> e : raw.entrySet()) {
            flat.put(e.getKey().toLowerCase(), String.join(", ", e.getValue()));
        }

        Map<String, Object> r = new LinkedHashMap<String, Object>();
        r.put("count", flat.size());
        r.put("headers", flat);
        r.put("userAgent", httpHeaders.getHeaderString("User-Agent"));
        return r;
    }

    /** JSON 请求体直接映射成 Map，再原样回显 + 补充服务端信息 */
    @POST
    @Path("/echo")
    @Consumes(MediaType.APPLICATION_JSON)
    public Map<String, Object> echo(Map<String, Object> body) {
        Map<String, Object> r = new LinkedHashMap<String, Object>();
        r.put("received", body);
        r.put("serverTime", System.currentTimeMillis());
        return r;
    }
}
