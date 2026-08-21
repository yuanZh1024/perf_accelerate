#ifndef DEBUG_TRACE_H
#define DEBUG_TRACE_H

#ifdef DEBUG_TRACE
#include <stdio.h>

#define TRACE_ENTER(...)  do{ printf("[ENTER] %s\n", __func__); }while(0)
#else
#define TRACE_ENTER(...) ((void)0)
#endif

#endif