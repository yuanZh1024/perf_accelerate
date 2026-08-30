package obsa_demo;

import java.io.*;

/**
 * 模拟 LocalFileSystem，实现上面FileSystem接口
 */
public class OBSFileSystem implements FileSystem {

    @Override
    public boolean mkdirs(String path) {
        File file = new File(path);
        return file.mkdirs();
    }

    @Override
    public boolean delete(String path, boolean recursive) {
        File f = new File(path);
        if(recursive && f.isDirectory()){
            // 简化：省略递归删除逻辑
        }
        return f.delete();
    }

    @Override
    public boolean rename(String src, String dst) {
        return new File(src).renameTo(new File(dst));
    }

    @Override
    public boolean exists(String path) {
        return new File(path).exists();
    }

    @Override
    public boolean isDirectory(String path) {
        return new File(path).isDirectory();
    }

    @Override
    public boolean isFile(String path) {
        return new File(path).isFile();
    }

    @Override
    public FSDataInputStream open(String path) throws IOException {
        FileInputStream fis = new FileInputStream(path);
        return new FSDataInputStream() {
            @Override
            public int read() throws IOException {
                return fis.read();
            }
            @Override
            public void seek(long pos) throws IOException {
                fis.getChannel().position(pos);
            }
            @Override
            public void close() throws IOException {
                fis.close();
            }
        };
    }

    @Override
    public FSDataOutputStream create(String path, boolean overwrite) throws IOException {
        System.out.println("mock obs put...");
        return null;
        // FileOutputStream fos = new FileOutputStream(path, !overwrite);
        // return new FSDataOutputStream() {
        //     @Override
        //     public void write(byte[] b) throws IOException {
        //         fos.write(b);
        //     }
        //     @Override
        //     public void close() throws IOException {
        //         fos.close();
        //     }
        // };
    }

    @Override
    public FSDataOutputStream append(String path) throws IOException {
        FileOutputStream fos = new FileOutputStream(path, true);
        // 省略实现
        return null;
    }

    @Override
    public FileStatus[] listStatus(String path) {
        return new FileStatus[0]; // 简化
    }

    @Override
    public FileStatus getFileStatus(String path) {
        return new FileStatus(); // 简化
    }

    @Override
    public void close() throws IOException {
        // 关闭文件系统资源
    }
}

