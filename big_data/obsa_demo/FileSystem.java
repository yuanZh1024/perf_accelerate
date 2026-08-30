package obsa_demo;

import java.io.Closeable;
import java.io.IOException;

/**
 * 模拟 Hadoop FileSystem 契约接口（简化版，对应Hadoop3 FileSystemInterface）
 * 原Hadoop2是抽象类，Hadoop3抽接口定义行为规范
 */
public interface FileSystem extends Closeable {
    // hadoop FileSystem接口的定义
    // 定义 Hadoop 统一文件系统访问契约，HDFS、LocalFileSystem、S3A、ViewFileSystem 都实现它。

    /**
     * 创建目录，支持多级目录
     * 
     * @param path 文件路径
     * @return true 创建成功
     */
    boolean mkdirs(String path) throws IOException;

    /**
     * 删除文件/目录
     * 
     * @param path      路径
     * @param recursive 是否递归删除目录
     * @return true 删除成功
     */
    boolean delete(String path, boolean recursive) throws IOException;

    /**
     * 重命名 / 移动文件
     */
    boolean rename(String src, String dst) throws IOException;

    /**
     * 判断路径是否存在
     */
    boolean exists(String path) throws IOException;

    /**
     * 判断是否为目录
     */
    boolean isDirectory(String path) throws IOException;

    /**
     * 判断是否为普通文件
     */
    boolean isFile(String path) throws IOException;

    /**
     * 打开文件，获取输入流（读文件）
     */
    FSDataInputStream open(String path) throws IOException;

    /**
     * 创建文件，获取输出流（写文件）
     * 
     * @param overwrite 是否覆盖已有文件
     */
    FSDataOutputStream create(String path, boolean overwrite) throws IOException;

    /**
     * 追加写入文件
     */
    FSDataOutputStream append(String path) throws IOException;

    /**
     * 列出目录下的文件
     */
    FileStatus[] listStatus(String path) throws IOException;

    /**
     * 获取文件元信息
     */
    FileStatus getFileStatus(String path) throws IOException;

    /**
     * 关闭文件系统资源，来自 Closeable
     */
    @Override
    void close() throws IOException;
}

// ---------------- 配套简单模型类 ----------------

/**
 * 文件元数据：大小、路径、是否目录等
 */
class FileStatus {
    private String path;
    private long length;
    private boolean isDir;

    // getter setter ...
    public String getPath() {
        return path;
    }

    public long getLen() {
        return length;
    }

    public boolean isDirectory() {
        return isDir;
    }
}

/**
 * 模拟Hadoop输入流
 */
abstract class FSDataInputStream {
    public abstract int read() throws IOException;

    public abstract void seek(long pos) throws IOException; // 随机跳转读，hadoop特色

    public abstract void close() throws IOException;
}

/**
 * 模拟Hadoop输出流
 */
abstract class FSDataOutputStream {
    public abstract void write(byte[] b) throws IOException;

    public abstract void close() throws IOException;
}
