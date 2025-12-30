#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <pthread.h>
#include "store.h"
#include "limiter.h"
#include "metrics.h"

/* ...existing code... (helpers) */

#define API_PORT 8080
#define METRICS_PORT 9090

typedef struct {
    limiter_t *lim;
} server_ctx_t;

static int api_handler(void *cls, struct MHD_Connection *connection, const char *url,
    const char *method, const char *version, const char *upload_data,
    size_t *upload_data_size, void **con_cls) {

    server_ctx_t *sctx = (server_ctx_t*)cls;
    const char *client = MHD_lookup_header(connection, "X-Client-ID");
    if (!client) client = "anonymous";
    char reason[128];
    int ok = limiter_allow(sctx->lim, client, url, reason, sizeof(reason));
    if (!ok) {
        metrics_inc_rejected(reason[0]?reason:"other");
        struct MHD_Response *resp = MHD_create_response_from_buffer(strlen("rate limited"), (void*)"rate limited", MHD_RESPMEM_PERSISTENT);
        int ret = MHD_queue_response(connection, 429, resp);
        MHD_destroy_response(resp);
        return ret;
    }
    metrics_inc_allowed();
    struct MHD_Response *resp = MHD_create_response_from_buffer(strlen("ok"), (void*)"ok", MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, 200, resp);
    MHD_destroy_response(resp);
    return ret;
}

static int metrics_handler(void *cls, struct MHD_Connection *connection, const char *url,
    const char *method, const char *version, const char *upload_data,
    size_t *upload_data_size, void **con_cls) {

    char *body = metrics_get();
    struct MHD_Response *resp = MHD_create_response_from_buffer(strlen(body), body, MHD_RESPMEM_MUST_FREE);
    int ret = MHD_queue_response(connection, 200, resp);
    MHD_destroy_response(resp);
    return ret;
}

int main(int argc, char **argv) {
    const char *redis_addr_env = getenv("REDIS_ADDR"); /* e.g. redis:6379 */
    int use_redis = 0;
    char host[128] = "127.0.0.1";
    int port = 6379;
    if (redis_addr_env) {
        char tmp[128];
        strncpy(tmp, redis_addr_env, sizeof(tmp));
        char *p = strchr(tmp, ':');
        if (p) { *p=0; strncpy(host,tmp,sizeof(host)); port = atoi(p+1); }
        else strncpy(host,tmp,sizeof(host));
        use_redis = 1;
    }

    store_t *store = NULL;
    if (use_redis) {
        store = store_redis_new(host, port);
        if (!store) {
            fprintf(stderr, "failed to connect to redis at %s:%d, falling back to memory store\n", host, port);
            store = store_mem_new(1024);
        }
    } else {
        store = store_mem_new(1024);
    }

    limiter_cfg_t cfg = { .store = store, .default_rate = 100.0, .default_burst = 200.0, .store_latency_threshold_ms = 50, .mem_pressure_mb = 500 };
    limiter_t *lim = limiter_new(&cfg);

    server_ctx_t sctx = { .lim = lim };

    struct MHD_Daemon *metrics_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, METRICS_PORT, NULL, NULL, metrics_handler, NULL, MHD_OPTION_END);
    if (!metrics_daemon) { fprintf(stderr, "failed to start metrics server\n"); return 1; }

    struct MHD_Daemon *daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, API_PORT, NULL, NULL, api_handler, &sctx, MHD_OPTION_END);
    if (!daemon) { fprintf(stderr, "failed to start api server\n"); return 1; }

    printf("servers running: api=%d metrics=%d\n", API_PORT, METRICS_PORT);
    getchar();

    MHD_stop_daemon(daemon);
    MHD_stop_daemon(metrics_daemon);
    limiter_free(lim);
    if (store) store->close(store);
    return 0;
}