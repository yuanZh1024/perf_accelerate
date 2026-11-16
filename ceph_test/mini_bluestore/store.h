// store.h
#ifndef STORE_H
#define STORE_H
#include <stddef.h>
#include "disk.h"
#include "alloc.h"
#include "meta.h"

typedef struct { Disk disk; Allocator alloc; Meta meta; } Store;

int  store_open(Store *s, const char *dev, const char *meta_path);
int  store_write(Store *s, const char *name, const void *data, size_t len);
int  store_read(Store *s, const char *name, void *out, size_t cap, size_t *outlen);
int  store_delete(Store *s, const char *name);
void store_close(Store *s);
#endif
