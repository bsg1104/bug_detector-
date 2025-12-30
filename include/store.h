#ifndef STORE_H
#define STORE_H

#include <time.h>

typedef struct {
    int allowed; /* 0 or 1 */
    double remaining;
} store_result_t;

typedef struct store_t store_t;

struct store_t {
    void *ctx;
    int (*take)(store_t *s, const char *key, double capacity, double refill_per_sec, long long now_ms, double tokens, store_result_t *out);
    int (*latency_ms)(store_t *s);
    void (*close)(store_t *s);
};

#endif
