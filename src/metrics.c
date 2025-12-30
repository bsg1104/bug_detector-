#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "metrics.h"

static atomic_ulong total = 0;
static atomic_ulong allowed = 0;
/* simple fixed-size reasons map */
#define REASONS 8
static const char *reason_keys[REASONS] = {"store_latency","memory_pressure","store_error","client_rate_limited","endpoint_rate_limited","other","cpu_pressure","load_shed"};
static atomic_ulong rejected[REASONS];

void metrics_inc_allowed() { atomic_fetch_add(&total,1); atomic_fetch_add(&allowed,1); }
void metrics_inc_rejected(const char *reason) {
    atomic_fetch_add(&total,1);
    int i;
    for (i=0;i<REASONS;i++) if (strcmp(reason_keys[i],reason)==0) break;
    if (i==REASONS) i = 5; /* other */
    atomic_fetch_add(&rejected[i],1);
}

char *metrics_get() {
    /* build Prometheus text */
    char *buf = malloc(4096);
    size_t off=0;
    off += snprintf(buf+off,4096-off,"# HELP rl_requests_total Total requests\n# TYPE rl_requests_total counter\nrl_requests_total %lu\n", (unsigned long)atomic_load(&total));
    off += snprintf(buf+off,4096-off,"# HELP rl_requests_allowed_total Allowed requests\n# TYPE rl_requests_allowed_total counter\nrl_requests_allowed_total %lu\n", (unsigned long)atomic_load(&allowed));
    for (int i=0;i<REASONS;i++) {
        off += snprintf(buf+off,4096-off,"rl_requests_rejected_total{reason=\"%s\"} %lu\n", reason_keys[i], (unsigned long)atomic_load(&rejected[i]));
    }
    return buf;
}
