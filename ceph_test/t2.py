import rados

cluster = rados.Rados(conffile='/etc/ceph/ceph.conf')
cluster.connect()

# 1. 创建 pool（对象的逻辑命名空间，PG 数量等策略在 pool 上）
if not cluster.pool_exists('demo'):
    cluster.create_pool('demo')
ioctx = cluster.open_ioctx('demo')

# 2. 基本对象读写：data 部分
ioctx.write_full('obj2', b'hello zzy')
print(ioctx.read('obj2'))



ioctx.close()
cluster.shutdown()