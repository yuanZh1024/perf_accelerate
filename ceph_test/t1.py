import rados

cluster = rados.Rados(conffile='/etc/ceph/ceph.conf')
cluster.connect()

# 1. 创建 pool（对象的逻辑命名空间，PG 数量等策略在 pool 上）
if not cluster.pool_exists('demo'):
    cluster.create_pool('demo')
ioctx = cluster.open_ioctx('demo')

# 2. 基本对象读写：data 部分
ioctx.write_full('obj1', b'hello rados')
print(ioctx.read('obj1'))

# 3. 按偏移写：体会"可随机读写"，这是 S3 做不到的
ioctx.write('obj1', b'RADOS', offset=6)
print(ioctx.read('obj1'))            # b'hello RADOS'

# 4. xattr
ioctx.set_xattr('obj1', 'owner', b'alice')
print(ioctx.get_xattr('obj1', 'owner'))

# 5. omap：对象上的有序 KV
with rados.WriteOpCtx() as op:
    ioctx.set_omap(op, ('k1', 'k2', 'k3'), (b'v1', b'v2', b'v3'))
    ioctx.operate_write_op(op, 'obj1')

with rados.ReadOpCtx() as op:
    it, _ = ioctx.get_omap_vals(op, "", "", 10)
    ioctx.operate_read_op(op, 'obj1')
    print(list(it))

# 6. 原子复合操作：data + xattr + omap 在同一事务里
with rados.WriteOpCtx() as op:
    op.new()                          # 对象不存在则创建
    op.write_full(b'atomic')
    op.set_omap(('a',), (b'1',))
    ioctx.operate_write_op(op, 'obj2')

# 7. 列出 pool 里的对象
for o in ioctx.list_objects():
    print(o.key)

ioctx.close()
cluster.shutdown()