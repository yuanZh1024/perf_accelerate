CC = gcc

CFLAGS = -Wall -Wextra -g -O0
CFLAGS += -Iinclude

LDFLAGS = -lrados

TARGET = mini-rbd

SRCS = \
    src/main.c \
    src/rados_client.c \
    src/rbd_metadata.c \
    src/rbd_object.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
