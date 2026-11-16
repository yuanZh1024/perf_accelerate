/*
 * server.h - HTTP front end of obs-ec-store.
 */
#ifndef OBS_SERVER_H
#define OBS_SERVER_H

#include "store.h"

/* Runs the accept loop until server_request_stop() is called.
 * Returns 0 on clean shutdown, -1 on a fatal error. */
int server_run(store_t *st, const char *host, int port, int max_conns);

/* Async-signal-safe. */
void server_request_stop(void);

#endif /* OBS_SERVER_H */
