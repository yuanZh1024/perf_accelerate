import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

import java.util.*;

public class MiniMapReduce {

    /*
     * Map:
     *
     * input:
     *     "hello hadoop"
     *
     * output:
     *     ("hello", 1)
     *     ("hadoop", 1)
     */
    static List<KeyValue> map(String line) {

        List<KeyValue> output = new ArrayList<>();

        String[] words = line.split("\\s+");

        for (String word : words) {

            if (!word.isEmpty()) {
                output.add(
                    new KeyValue(word, 1)
                );
            }
        }

        return output;
    }


    /*
     * Shuffle:
     *
     * ("hello", 1)
     * ("hadoop", 1)
     * ("hello", 1)
     *
     *       ↓
     *
     * hello  -> [1, 1]
     * hadoop -> [1]
     */
    static Map<String, List<Integer>> shuffle(
            List<KeyValue> mapOutput) {

        Map<String, List<Integer>> grouped =
                new HashMap<>();

        for (KeyValue kv : mapOutput) {

            grouped
                .computeIfAbsent(
                    kv.key,
                    k -> new ArrayList<>()
                )
                .add(kv.value);
        }

        return grouped;
    }


    /*
     * Reduce:
     *
     * hello -> [1,1,1]
     *
     *       ↓
     *
     * hello -> 3
     */
    static Map<String, Integer> reduce(
            Map<String, List<Integer>> grouped) {

        Map<String, Integer> result =
                new TreeMap<>();

        for (Map.Entry<String, List<Integer>> entry
                : grouped.entrySet()) {

            int sum = 0;

            for (Integer value : entry.getValue()) {
                sum += value;
            }

            result.put(
                entry.getKey(),
                sum
            );
        }

        return result;
    }


    /*
     * 从 HDFS 读取文件
     */
    static List<String> readFromHDFS(
            FileSystem fs,
            Path input)
            throws Exception {

        List<String> lines =
                new ArrayList<>();

        FSDataInputStream inputStream =
                fs.open(input);

        BufferedReader reader =
                new BufferedReader(
                    new InputStreamReader(
                        inputStream,
                        StandardCharsets.UTF_8
                    )
                );

        String line;

        while ((line = reader.readLine()) != null) {
            lines.add(line);
        }

        reader.close();

        return lines;
    }


    /*
     * 把 Reduce 结果写回 HDFS
     */
    static void writeToHDFS(
            FileSystem fs,
            Path output,
            Map<String, Integer> result)
            throws Exception {

        FSDataOutputStream outputStream =
                fs.create(output, true);

        for (Map.Entry<String, Integer> entry
                : result.entrySet()) {

            String line =
                    entry.getKey()
                    + "\t"
                    + entry.getValue()
                    + "\n";

            outputStream.write(
                line.getBytes(StandardCharsets.UTF_8)
            );
        }

        outputStream.close();
    }


    public static void main(String[] args)
            throws Exception {

        if (args.length != 2) {

            System.out.println(
                "Usage: MiniMapReduce <input> <output>"
            );

            return;
        }


        Path input =
                new Path(args[0]);

        Path output =
                new Path(args[1]);


        /*
         * 创建 Hadoop Configuration
         */
        Configuration conf =
                new Configuration();


        /*
         * 获取 HDFS FileSystem
         */
        FileSystem fs =
                FileSystem.get(conf);


        System.out.println(
            "=== Read From HDFS ==="
        );


        /*
         * 1. 从 HDFS 读取
         */
        List<String> lines =
                readFromHDFS(fs, input);


        /*
         * 2. Map
         */
        System.out.println(
            "=== Map ==="
        );

        List<KeyValue> mapOutput =
                new ArrayList<>();

        for (String line : lines) {

            mapOutput.addAll(
                map(line)
            );
        }


        for (KeyValue kv : mapOutput) {

            System.out.println(
                kv.key + " -> " + kv.value
            );
        }


        /*
         * 3. Shuffle
         */
        System.out.println(
            "=== Shuffle ==="
        );

        Map<String, List<Integer>> grouped =
                shuffle(mapOutput);


        for (Map.Entry<String, List<Integer>> entry
                : grouped.entrySet()) {

            System.out.println(
                entry.getKey()
                + " -> "
                + entry.getValue()
            );
        }


        /*
         * 4. Reduce
         */
        System.out.println(
            "=== Reduce ==="
        );

        Map<String, Integer> result =
                reduce(grouped);


        for (Map.Entry<String, Integer> entry
                : result.entrySet()) {

            System.out.println(
                entry.getKey()
                + " -> "
                + entry.getValue()
            );
        }


        /*
         * 5. 写回 HDFS
         */
        writeToHDFS(
            fs,
            output,
            result
        );


        fs.close();


        System.out.println(
            "=== Done ==="
        );
    }


    /*
     * Map 的输出：
     *
     * key/value
     */
    static class KeyValue {

        String key;

        int value;

        KeyValue(
                String key,
                int value) {

            this.key = key;
            this.value = value;
        }
    }
}