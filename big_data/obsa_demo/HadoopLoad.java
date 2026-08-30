package obsa_demo;

import java.util.Properties;

public class HadoopLoad {

    private Properties conf;

    public void initConf() {
        // 加载本地文件 classConf
        /*
         * <property>
         * <name>fs.AbstractFileSystem.obs.impl</name>
         * <value>org.apache.hadoop.fs.obs.OBS</value>
         * </property>
         */
        conf = new Properties();
        // 模拟 core‑site.xml 里的配置项
        // conf.setProperty("fs.AbstractFileSystem.obs.impl",
        // "org.apache.hadoop.fs.obs.OBS");
        conf.setProperty("fs.obs.impl", "obsa_demo.OBSFileSystem");

        // OBS鉴权模拟配置
        conf.setProperty("fs.obs.access.key", "FAKE_AK");
        conf.setProperty("fs.obs.secret.key", "FAKE_SK");
        conf.setProperty("fs.obs.endpoint", "obs.cn-east-2.myhuaweicloud.com");
    }

    private void checkSecurity(String className) {
        System.out.println("check...");
    }

    public static void main(String[] args) {
        HadoopLoad hLoad = new HadoopLoad();
        hLoad.initConf();
        Properties conf = hLoad.conf;
        String className = conf.getProperty("fs.obs.impl");

        hLoad.checkSecurity(className);

        try {
            hLoad.checkSecurity(className);

            // 反射：尝试加载类（这里类不存在，会抛ClassNotFoundException，符合真实场景）
            Class<?> clazz = Class.forName(className);
            FileSystem obsFsObj = (FileSystem) clazz.getDeclaredConstructor().newInstance();

            System.out.println("反射实例化成功, 对象=" + obsFsObj);

            obsFsObj.create("/test/1", false);

        } catch (SecurityException e) {
            System.err.println("安全校验失败：" + e.getMessage());
        } catch (ClassNotFoundException e) {

            System.err.println("类找不到，请提供对应jar包：" + e.getMessage());
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

}
