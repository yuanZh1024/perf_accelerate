CC      ?= cc
AR      ?= ar

OPT     ?= -O2 -g
WARN     = -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
           -Wpointer-arith -Wcast-align
override CFLAGS  += $(OPT) -std=gnu11 $(WARN) -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
override LDLIBS  += -pthread

PREFIX   ?= /usr/local
BINDIR    = $(PREFIX)/bin
UNITDIR  ?= /lib/systemd/system
STATEDIR ?= /var/lib/obsd

LIB_SRC  = src/gf.c src/rs.c src/util.c src/os.c src/meta.c src/store.c \
           src/http_parse.c src/net.c src/server.c
LIB_OBJ  = $(LIB_SRC:.c=.o)

TEST_BINS = test/test_rs test/test_store test/test_http_parse

.PHONY: all clean test install uninstall

all: obsd

libobs.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

obsd: src/obsd.o libobs.a
	$(CC) $(CFLAGS) -o $@ src/obsd.o libobs.a $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test/test_rs: test/test_rs.c libobs.a
	$(CC) $(CFLAGS) -o $@ $< libobs.a $(LDLIBS)

test/test_store: test/test_store.c libobs.a
	$(CC) $(CFLAGS) -o $@ $< libobs.a $(LDLIBS)

test/test_http_parse: test/test_http_parse.c libobs.a
	$(CC) $(CFLAGS) -o $@ $< libobs.a $(LDLIBS)

test: $(TEST_BINS)
	@echo "== test_rs =="
	@./test/test_rs
	@echo "== test_store =="
	@./test/test_store
	@echo "== test_http_parse =="
	@./test/test_http_parse

install: obsd
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 obsd $(DESTDIR)$(BINDIR)/obsd
	install -d $(DESTDIR)$(UNITDIR)
	install -m 0644 deploy/obsd.service $(DESTDIR)$(UNITDIR)/obsd.service
	install -d $(DESTDIR)$(STATEDIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/obsd $(DESTDIR)$(UNITDIR)/obsd.service

clean:
	rm -f $(LIB_OBJ) src/obsd.o obsd libobs.a $(TEST_BINS)
	rm -rf test/tmp
