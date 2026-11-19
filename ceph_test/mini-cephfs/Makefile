CC = gcc

CFLAGS = -Wall -Wextra -g -O2
LDFLAGS = -lrados

TARGET = mini-cephfs

OBJS = main.o mini_cephfs.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

main.o: main.c mini_cephfs.h
	$(CC) $(CFLAGS) -c main.c

mini_cephfs.o: mini_cephfs.c mini_cephfs.h
	$(CC) $(CFLAGS) -c mini_cephfs.c

clean:
	rm -f *.o $(TARGET)

.PHONY: all clean
