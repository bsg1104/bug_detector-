#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "limiter.h"

struct limiter_t {
    limiter_cfg_t cfg;
};

static long long now_ms() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec*1000 + tv.tv_usec/1000;
}

limiter_t *limiter_new(limiter_cfg_t *cfg) {
    limiter_t *l = malloc(sizeof(*l));
    memcpy(&l->cfg, cfg, sizeof(*cfg));
    return l;
}

void limiter_free(limiter_t *l) { free(l); }

int limiter_allow(limiter_t *l, const char *client_id, const char *endpoint, char *reason, int reason_len) {
    long long now = now_ms();
    if (l->cfg.store->latency_ms(l->cfg.store) > l->cfg.store_latency_threshold_ms) {
        snprintf(reason, reason_len, "store_latency");
        return 0;
    }
    /* memory pressure: simple check using RSS via /proc/self/statm (bytes) - keep conservative */
    if (l->cfg.mem_pressure_mb>0) {
        FILE *f = fopen("/proc/self/statm","r");
        if (f) {
            long pages=0; if (fscanf(f,"%ld",&pages)==1) {
                long page_size = sysconf(_SC_PAGESIZE);
                long rss_bytes = pages * page_size;
                if ((size_t)(rss_bytes/1024/1024) > l->cfg.mem_pressure_mb) {
                    fclose(f);
                    snprintf(reason, reason_len, "memory_pressure");
                    return 0;
                }
            }
            fclose(f);
        }
    }
    char key[256];
    store_result_t r1;
    snprintf(key, sizeof(key), "client:%s", client_id);
    if (l->cfg.store->take(l->cfg.store, key, l->cfg.default_burst, l->cfg.default_rate, now, 1.0, &r1) != 0) {
        snprintf(reason, reason_len, "store_error");
        return 0;
    }
    if (!r1.allowed) {
        snprintf(reason, reason_len, "client_rate_limited");
        return 0;
    }
    store_result_t r2;
    snprintf(key, sizeof(key), "endpoint:%s", endpoint);
    if (l->cfg.store->take(l->cfg.store, key, l->cfg.default_burst, l->cfg.default_rate, now, 1.0, &r2) != 0) {
        snprintf(reason, reason_len, "store_error");
        return 0;
    }
    if (!r2.allowed) {
        snprintf(reason, reason_len, "endpoint_rate_limited");
        return 0;
    }
    reason[0]=0;
    return 1;
}
