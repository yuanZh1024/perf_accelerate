#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <rocksdb/c.h>

#define DB_DIR  "./data"
#define DB_PATH "./data/rocksdb"

typedef struct {
    rocksdb_t *db;
} kv_store_t;


/* 检查 RocksDB 操作是否成功 */
static int check_error(char *err) {
    if (err == NULL) {
        return 0;
    }

    fprintf(stderr, "RocksDB Error: %s\n", err);
    rocksdb_free(err);

    return -1;
}


/* RocksDB 只创建 DB_PATH 的最后一级目录，父目录需要自己准备 */
static void ensure_data_dir(void) {
    if (mkdir(DB_DIR, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdir %s failed: %s\n", DB_DIR, strerror(errno));
    }
}


/* 打开数据库 */
static int kv_store_open(kv_store_t *store) {
    rocksdb_options_t *options = rocksdb_options_create();

    rocksdb_options_set_create_if_missing(options, 1);

    char *err = NULL;

    ensure_data_dir();

    store->db = rocksdb_open(options, DB_PATH, &err);

    rocksdb_options_destroy(options);

    if (check_error(err) != 0) {
        return -1;
    }

    printf("RocksDB opened successfully\n");

    return 0;
}


/* 关闭数据库 */
static void kv_store_close(kv_store_t *store) {
    if (store->db != NULL) {
        rocksdb_close(store->db);
        store->db = NULL;
    }

    printf("RocksDB closed\n");
}


/* 写入 KV */
static int kv_store_put(
    kv_store_t *store,
    const char *key,
    const char *value
) {
    rocksdb_writeoptions_t *write_options =
        rocksdb_writeoptions_create();

    char *err = NULL;

    rocksdb_put(
        store->db,
        write_options,
        key,
        strlen(key),
        value,
        strlen(value),
        &err
    );

    rocksdb_writeoptions_destroy(write_options);

    if (check_error(err) != 0) {
        return -1;
    }

    printf("Put OK: %s\n", key);

    return 0;
}


/* 查询 KV */
static int kv_store_get(
    kv_store_t *store,
    const char *key
) {
    rocksdb_readoptions_t *read_options =
        rocksdb_readoptions_create();

    size_t value_len = 0;
    char *err = NULL;

    char *value = rocksdb_get(
        store->db,
        read_options,
        key,
        strlen(key),
        &value_len,
        &err
    );

    rocksdb_readoptions_destroy(read_options);

    if (check_error(err) != 0) {
        return -1;
    }

    if (value == NULL) {
        printf("Key not found: %s\n", key);
        return -1;
    }

    printf("%s => %.*s\n",
           key,
           (int)value_len,
           value);

    rocksdb_free(value);

    return 0;
}


/* 删除 KV */
static int kv_store_delete(
    kv_store_t *store,
    const char *key
) {
    rocksdb_writeoptions_t *write_options =
        rocksdb_writeoptions_create();

    char *err = NULL;

    rocksdb_delete(
        store->db,
        write_options,
        key,
        strlen(key),
        &err
    );

    rocksdb_writeoptions_destroy(write_options);

    if (check_error(err) != 0) {
        return -1;
    }

    printf("Delete OK: %s\n", key);

    return 0;
}


/* 遍历所有 KV */
static void kv_store_scan(kv_store_t *store) {
    rocksdb_readoptions_t *read_options =
        rocksdb_readoptions_create();

    rocksdb_iterator_t *iter =
        rocksdb_create_iterator(store->db, read_options);

    rocksdb_iter_seek_to_first(iter);

    while (rocksdb_iter_valid(iter)) {
        size_t key_len = 0;
        size_t value_len = 0;

        const char *key =
            rocksdb_iter_key(iter, &key_len);

        const char *value =
            rocksdb_iter_value(iter, &value_len);

        printf("%.*s => %.*s\n",
               (int)key_len, key,
               (int)value_len, value);

        rocksdb_iter_next(iter);
    }

    char *err = NULL;

    rocksdb_iter_get_error(iter, &err);

    check_error(err);

    rocksdb_iter_destroy(iter);
    rocksdb_readoptions_destroy(read_options);
}


/* MemTable Flush */
static int kv_store_flush(kv_store_t *store) {
    rocksdb_flushoptions_t *flush_options =
        rocksdb_flushoptions_create();

    rocksdb_flushoptions_set_wait(flush_options, 1);

    char *err = NULL;

    rocksdb_flush(
        store->db,
        flush_options,
        &err
    );

    rocksdb_flushoptions_destroy(flush_options);

    return check_error(err);
}


int main(void) {
    kv_store_t store = {0};

    if (kv_store_open(&store) != 0) {
        return 1;
    }

    char command[32];
    char key[256];
    char value[1024];

    while (1) {
        printf("\nrocksdb> ");
        fflush(stdout);

        if (scanf("%31s", command) != 1) {
            break;
        }

        if (strcmp(command, "exit") == 0) {
            break;
        }

        if (strcmp(command, "put") == 0) {
            if (scanf("%255s %1023s", key, value) == 2) {
                kv_store_put(&store, key, value);
            }
        }
        else if (strcmp(command, "get") == 0) {
            if (scanf("%255s", key) == 1) {
                kv_store_get(&store, key);
            }
        }
        else if (strcmp(command, "delete") == 0) {
            if (scanf("%255s", key) == 1) {
                kv_store_delete(&store, key);
            }
        }
        else if (strcmp(command, "scan") == 0) {
            kv_store_scan(&store);
        }
        else if (strcmp(command, "flush") == 0) {
            if (kv_store_flush(&store) == 0) {
                printf("Flush completed\n");
            }
        }
        else {
            printf("Unknown command\n");
        }
    }

    kv_store_close(&store);

    return 0;
}
