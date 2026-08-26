import java.io.IOException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.IntWritable;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.FileInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;

public class ComplexUserPurchase {

    public static class PurchaseMapper
            extends Mapper<LongWritable, Text, Text, Text> {

        private final Text outKey = new Text();
        private final Text outValue = new Text();

        @Override
        protected void map(
                LongWritable key,
                Text value,
                Context context)
                throws IOException, InterruptedException {

            String[] fields = value.toString().split(",");

            String user = fields[0];
            String product = fields[1];
            String amount = fields[2];

            outKey.set(user);
            outValue.set(product + "," + amount);

            context.write(outKey, outValue);
        }
    }

    public static class PurchaseReducer
            extends Reducer<Text, Text, Text, Text> {

        @Override
        protected void reduce(
                Text key,
                Iterable<Text> values,
                Context context)
                throws IOException, InterruptedException {

            int total = 0;
            int count = 0;
            int max = Integer.MIN_VALUE;

            List<String[]> orders = new ArrayList<>();

            for (Text value : values) {

                String[] fields = value.toString().split(",");

                String product = fields[0];
                int amount = Integer.parseInt(fields[1]);

                total += amount;
                count++;

                max = Math.max(max, amount);

                orders.add(new String[]{
                        product,
                        String.valueOf(amount)
                });
            }

            double avg = (double) total / count;

            orders.sort(
                    Comparator.comparingInt(
                            x -> -Integer.parseInt(x[1])
                    )
            );

            StringBuilder top2 = new StringBuilder();

            for (int i = 0; i < Math.min(2, orders.size()); i++) {

                if (i > 0) {
                    top2.append(", ");
                }

                top2.append(
                        orders.get(i)[0]
                ).append(":")
                 .append(
                        orders.get(i)[1]
                 );
            }

            String result =
                    "total=" + total +
                    ", avg=" + avg +
                    ", max=" + max +
                    ", top2=[" + top2 + "]";

            context.write(key, new Text(result));
        }
    }

    public static void main(String[] args) throws Exception {

        Configuration conf = new Configuration();

        Job job = Job.getInstance(
                conf,
                "User Purchase Analysis"
        );

        job.setJarByClass(UserPurchase.class);

        job.setMapperClass(PurchaseMapper.class);
        job.setReducerClass(PurchaseReducer.class);

        job.setMapOutputKeyClass(Text.class);
        job.setMapOutputValueClass(Text.class);

        job.setOutputKeyClass(Text.class);
        job.setOutputValueClass(Text.class);

        FileInputFormat.addInputPath(
                job,
                new Path(args[0])
        );

        FileOutputFormat.setOutputPath(
                job,
                new Path(args[1])
        );

        System.exit(
                job.waitForCompletion(true)
                ? 0 : 1
        );
    }
}