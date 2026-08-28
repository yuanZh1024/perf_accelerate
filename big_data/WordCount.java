import java.io.IOException;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;

import org.apache.hadoop.io.IntWritable;
import org.apache.hadoop.io.Text;

import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;

import org.apache.hadoop.mapreduce.lib.input.FileInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;

/*
 使用mapreduce库
 调用标准的Mapper<Object, Text, Text, IntWritable> 
Reducer<Text, IntWritable, Text, IntWritable>

目的：理解如何使用mapper、reducer，里面分别填写什么逻辑
用本地文件系统：
hadoop jar \
/root/mr-demo/wordcount.jar \
WordCount \
file:///root/mr-demo/input \
file:///root/mr-demo/output

执行结果
root@dev-virtual-machine:~/mr-demo# ll output/
total 20
drwxr-xr-x 2 root root 4096 Aug 27 06:04 ./
drwxr-xr-x 6 root root 4096 Aug 27 06:04 ../
-rw-r--r-- 1 root root   36 Aug 27 06:04 part-r-00000
-rw-r--r-- 1 root root   12 Aug 27 06:04 .part-r-00000.crc
-rw-r--r-- 1 root root    0 Aug 27 06:04 _SUCCESS
-rw-r--r-- 1 root root    8 Aug 27 06:04 ._SUCCESS.crc
root@dev-virtual-machine:~/mr-demo#
root@dev-virtual-machine:~/mr-demo# cat output/part-r-00000
hadoop  3
hdfs    2
hello   3
mapreduce       2
root@dev-virtual-machine:~/mr-demo#
root@dev-virtual-machine:~/mr-demo# pwd
/root/mr-demo


用hdfs：
# 删除旧输出
hdfs dfs -rm -r /mr-demo/output

# 提交MR作业
hadoop jar wordcount.jar WordCount /mr-demo/input /mr-demo/output

# 查看运行结果
hdfs dfs -cat /mr-demo/output/part-r-00000

*/

public class WordCount {

    public static class TokenizerMapper
            extends Mapper<Object, Text, Text, IntWritable> {

        private final static IntWritable one =
                new IntWritable(1);

        private Text word = new Text();

        @Override
        public void map(
                Object key,
                Text value,
                Context context)
                throws IOException, InterruptedException {

            String[] words = value.toString().split("\\s+");

            for (String w : words) {
                word.set(w);
                context.write(word, one);
            }
        }
    }

    public static class IntSumReducer
            extends Reducer<Text, IntWritable, Text, IntWritable> {

        private IntWritable result =
                new IntWritable();

        @Override
        public void reduce(
                Text key,
                Iterable<IntWritable> values,
                Context context)
                throws IOException, InterruptedException {

            int sum = 0;

            for (IntWritable value : values) {
                sum += value.get();
            }

            result.set(sum);

            context.write(key, result);
        }
    }

    public static void main(String[] args)
            throws Exception {

        Configuration conf =
                new Configuration();

        Job job =
                Job.getInstance(conf, "word count");

        job.setJarByClass(WordCount.class);

        job.setMapperClass(TokenizerMapper.class);

        job.setCombinerClass(IntSumReducer.class);

        job.setReducerClass(IntSumReducer.class);

        job.setOutputKeyClass(Text.class);

        job.setOutputValueClass(IntWritable.class);

        FileInputFormat.addInputPath(
                job,
                new Path(args[0]));

        FileOutputFormat.setOutputPath(
                job,
                new Path(args[1]));

        System.exit(
                job.waitForCompletion(true)
                ? 0 : 1);
    }
}