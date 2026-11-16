/*
 * obsd.c - entry point of the erasure coded object store daemon.
 */
#include "server.h"
#include "store.h"
#include "util.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <getopt.h>
#endif

static void usage(const char *prog)
{
    printf("obsd - erasure coded object store (k+m)\n"
           "\n"
           "usage: %s [options]\n"
           "  -d DIR    data directory (default /var/lib/obsd)\n"
           "  -l ADDR   listen address (default 0.0.0.0)\n"
           "  -p PORT   listen port (default 9000)\n"
           "  -k N      number of data shards (default 4)\n"
           "  -m N      number of parity shards (default 2)\n"
           "  -s SIZE   bytes per data shard, e.g. 256K (default 256K)\n"
           "  -c N      max concurrent connections (default 128)\n"
           "  -v        verbose (debug) logging\n"
           "  -h        this help\n"
           "\n"
           "example: %s -d /var/lib/obsd -p 9000 -k 4 -m 2 -s 256K\n",
           prog, prog);
}

static void on_signal(int sig)
{
    (void)sig;
    server_request_stop();
}

int main(int argc, char **argv)
{
    const char *dir = "/var/lib/obsd";
    const char *host = "0.0.0.0";
    int port = 9000;
    int k = 4, m = 2;
    int max_conns = 128;
    int verbose = 0;
    uint32_t shard_size = 256 * 1024;
    store_t *store = NULL;
#ifndef _WIN32
    int opt;
#endif

#ifdef _WIN32
    /* keep the same short options, without getopt_long */
    int i;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] != '-')
            continue;
        switch (a[1]) {
        case 'd': if (i + 1 < argc) dir = argv[++i]; break;
        case 'l': if (i + 1 < argc) host = argv[++i]; break;
        case 'p': if (i + 1 < argc) port = atoi(argv[++i]); break;
        case 'k': if (i + 1 < argc) k = atoi(argv[++i]); break;
        case 'm': if (i + 1 < argc) m = atoi(argv[++i]); break;
        case 's': if (i + 1 < argc) parse_size(argv[++i], &shard_size); break;
        case 'c': if (i + 1 < argc) max_conns = atoi(argv[++i]); break;
        case 'v': verbose = 1; break;
        case 'h': usage(argv[0]); return 0;
        default: usage(argv[0]); return 1;
        }
    }
#else
    while ((opt = getopt(argc, argv, "d:l:p:k:m:s:c:vh")) != -1) {
        switch (opt) {
        case 'd': dir = optarg; break;
        case 'l': host = optarg; break;
        case 'p': port = atoi(optarg); break;
        case 'k': k = atoi(optarg); break;
        case 'm': m = atoi(optarg); break;
        case 's':
            if (parse_size(optarg, &shard_size) != 0) {
                fprintf(stderr, "invalid shard size: %s\n", optarg);
                return 1;
            }
            break;
        case 'c': max_conns = atoi(optarg); break;
        case 'v': verbose = 1; break;
        case 'h': usage(argv[0]); return 0;
        default: usage(argv[0]); return 1;
        }
    }
#endif

    log_set_level(verbose ? LOG_DEBUG : LOG_INFO);

    if (k < 1 || m < 1 || k + m > 16) {
        fprintf(stderr, "invalid EC geometry: %d+%d (1 <= k, 1 <= m, k+m <= 16)\n", k, m);
        return 1;
    }
    if (port < 1 || port > 65535) {
        fprintf(stderr, "invalid port: %d\n", port);
        return 1;
    }

    if (store_open(&store, dir, k, m, shard_size) != 0) {
        fprintf(stderr, "cannot open data directory: %s\n", dir);
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
    signal(SIGQUIT, on_signal);
#endif

    log_info("obs-ec-store starting: dir=%s ec=%d+%d shard=%u stripe=%llu", dir, k, m, shard_size,
             (unsigned long long)shard_size * (uint64_t)k);

    if (server_run(store, host, port, max_conns) != 0) {
        store_close(store);
        return 1;
    }

    store_close(store);
    return 0;
}
