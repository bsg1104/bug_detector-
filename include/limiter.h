#ifndef LIMITER_H
#define LIMITER_H

#include "store.h"

typedef struct {
    store_t *store;
    double default_rate;
    double default_burst;
    int store_latency_threshold_ms;
    size_t mem_pressure_mb;
} limiter_cfg_t;

typedef struct limiter_t limiter_t;

limiter_t *limiter_new(limiter_cfg_t *cfg);
void limiter_free(limiter_t *l);
int limiter_allow(limiter_t *l, const char *client_id, const char *endpoint, char *reason, int reason_len);

#endif
