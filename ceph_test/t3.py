#!/usr/bin/env python3
"""
迷你 RGW：用 librados 的 omap 模拟 bucket index。

对象布局（都在同一个 pool 里）：
  .dir.<bucket>        索引对象。data 为空，omap 里 key=文件名, value=JSON 元信息
  <bucket>__<key>      数据对象。data 为文件内容

运行：python3 mini_rgw.py
"""
import hashlib
import json
import time

import rados

POOL = 'demo'
BUCKET = 'photos'


class MiniBucket:
    def __init__(self, ioctx, name):
        self.io = ioctx
        self.name = name
        self.index_oid = f'.dir.{name}'
        # 创建索引对象（空对象，只用它的 omap）
        self.io.write_full(self.index_oid, b'')

    def _data_oid(self, key):
        return f'{self.name}__{key}'

    # ---------- PUT ----------
    def put(self, key, data: bytes):
        # 1. 先写数据对象
        self.io.write_full(self._data_oid(key), data)
        # 2. 再写索引（保证索引里有的条目，数据一定已存在）
        meta = {
            'size': len(data),
            'mtime': int(time.time()),
            'etag': hashlib.md5(data).hexdigest(),
        }
        with rados.WriteOpCtx() as op:
            self.io.set_omap(op, (key,), (json.dumps(meta).encode(),))
            self.io.operate_write_op(op, self.index_oid)
        return meta

    # ---------- HEAD：只查索引，不碰数据对象 ----------
    def head(self, key):
        with rados.ReadOpCtx() as op:
            it, _ = self.io.get_omap_vals_by_keys(op, (key,))
            self.io.operate_read_op(op, self.index_oid)
            items = list(it)          # 必须在 operate 之后消费
        if not items:
            return None
        return json.loads(items[0][1])

    # ---------- GET ----------
    def get(self, key):
        meta = self.head(key)
        if meta is None:
            raise KeyError(key)
        data = self.io.read(self._data_oid(key), length=meta['size'])
        return data, meta

    # ---------- LIST：有序、可分页、可按前缀 ----------
    def list(self, prefix='', marker='', max_keys=1000):
        with rados.ReadOpCtx() as op:
            it, _ = self.io.get_omap_vals(op, marker, prefix, max_keys)
            self.io.operate_read_op(op, self.index_oid)
            items = [(k, json.loads(v)) for k, v in it]
        next_marker = items[-1][0] if len(items) == max_keys else None
        return items, next_marker

    # ---------- DELETE ----------
    def delete(self, key):
        # 与 put 相反：先删索引，再删数据
        # 这样即使中途失败，最多留下"孤儿数据对象"，不会出现"索引指向不存在的数据"
        with rados.WriteOpCtx() as op:
            self.io.remove_omap_keys(op, (key,))
            self.io.operate_write_op(op, self.index_oid)
        try:
            self.io.remove_object(self._data_oid(key))
        except rados.ObjectNotFound:
            pass


def main():
    cluster = rados.Rados(conffile='/etc/ceph/ceph.conf')
    cluster.connect()
    if not cluster.pool_exists(POOL):
        cluster.create_pool(POOL)
    io = cluster.open_ioctx(POOL)

    b = MiniBucket(io, BUCKET)

    # PUT
    for name in ['2024/a.jpg', '2024/b.jpg', '2024/c.jpg',
                 '2025/d.jpg', '2025/e.jpg', 'readme.txt']:
        b.put(name, f'content of {name}'.encode())
    print('put done')

    # GET
    data, meta = b.get('2024/a.jpg')
    print('get 2024/a.jpg ->', data, meta)

    # LIST 全部
    items, _ = b.list()
    print('\nlist all:')
    for k, m in items:
        print(f'  {k:15} size={m["size"]}')

    # LIST 按前缀
    items, _ = b.list(prefix='2024/')
    print('\nlist prefix=2024/:', [k for k, _ in items])

    # LIST 分页：每页 2 条
    print('\nlist paged (page size 2):')
    marker = ''
    while True:
        items, marker = b.list(marker=marker, max_keys=2)
        print('  page:', [k for k, _ in items])
        if marker is None:
            break

    # DELETE
    b.delete('2024/b.jpg')
    print('\nafter delete 2024/b.jpg, head ->', b.head('2024/b.jpg'))

    io.close()
    cluster.shutdown()


if __name__ == '__main__':
    main()