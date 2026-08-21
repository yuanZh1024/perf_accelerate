#ifndef MOCK_DB_H
#define MOCK_DB_H

/*
 * 模拟异步数据库查询
 *
 * 返回一个 fd：
 *
 * fd 可读
 *     ↓
 * 数据库查询完成
 */
int mock_db_query_async(
    const char *sql,
    int request_id
);

#endif