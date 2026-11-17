# MiniRBD 第一版构建脚本
# 依赖：gcc、librados-dev（Ceph 20.2.5）；pool mini.rbd 需提前创建
# 用法：make 编译；make clean 清理产物

CC = gcc

# -Wall -Wextra 打开常用警告；-g -O0 保留调试信息、关闭优化
CFLAGS = -Wall -Wextra -g -O0
# 头文件搜索路径（include/ 下的 rbd_types.h 等）
CFLAGS += -Iinclude

# 链接 librados：librados.so 提供 rados_* API
LDFLAGS = -lrados

# 最终可执行文件
TARGET = mini-rbd

# 四个源文件：入口、RADOS 封装、元数据管理、数据对象读写
SRCS = \
    src/main.c \
    src/rados_client.c \
    src/rbd_metadata.c \
    src/rbd_object.c

# 由源文件列表推导目标文件列表（.c -> .o）
OBJS = $(SRCS:.c=.o)

# 默认目标：生成 mini-rbd
all: $(TARGET)

# 链接所有目标文件；$(CC) 与 $(LDFLAGS) 由上方定义
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# 清理编译产物
clean:
	rm -f $(OBJS) $(TARGET)

# 声明伪目标，避免同名文件干扰
.PHONY: all clean
