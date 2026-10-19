package com.example.upload;

import com.example.rs.HelloResource;
import com.fasterxml.jackson.jaxrs.json.JacksonJsonProvider;
import org.apache.cxf.Bus;
import org.apache.cxf.BusFactory;
import org.apache.cxf.jaxrs.JAXRSServerFactoryBean;
import org.apache.cxf.transport.servlet.CXFNonSpringServlet;

import javax.servlet.ServletConfig;

/**
 * 把 CXF 挂到已有的 Jetty 容器里（不用 CXF 自带的 Jetty，避免起第二个端口）。
 *
 * 原理：CXFNonSpringServlet 是 CXF 提供的「宿主容器适配器」
 *   - init() 时回调 loadBus()，我们在这里拿到 CXF 的 Bus（总线）
 *   - 再用 JAXRSServerFactoryBean 往 Bus 上发布资源，地址写相对路径
 *   - 实际访问路径 = servlet 映射路径 + address，例如 /services/* + /rest = /services/rest
 */
public class CxfServlet extends CXFNonSpringServlet {

    private static final long serialVersionUID = 1L;

    @Override
    protected void loadBus(ServletConfig sc) {
        super.loadBus(sc);

        Bus bus = getBus();
        BusFactory.setDefaultBus(bus);

        // 发布 JAX-RS 资源；要加新资源照这个再调一次 publish()
        publish(bus, "/rest", new HelloResource());
    }

    private void publish(Bus bus, String address, Object resource) {
        JAXRSServerFactoryBean factory = new JAXRSServerFactoryBean();
        factory.setBus(bus);
        factory.setAddress(address);
        factory.setServiceBean(resource);              // 单例资源
        factory.setProvider(new JacksonJsonProvider()); // JSON 序列化
        factory.create();
    }
}
