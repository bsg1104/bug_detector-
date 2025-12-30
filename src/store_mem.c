#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "store.h"

typedef struct bucket_s {
    char *key;
    double tokens;
    long long last_ms;
    struct bucket_s *next;
} bucket_t;

typedef struct {
    bucket_t **table;
    int size;
    pthread_mutex_t lock;
    int lat;
} mem_ctx_t;

static long long now_ms() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec*1000 + tv.tv_usec/1000;
}

static unsigned hash_str(const char *s) {
    unsigned h=5381; int c;
    while ((c=*s++)) h = ((h<<5)+h) + c;
    return h;
}

static bucket_t *find_bucket(mem_ctx_t *m, const char *key, int create, long long now) {
    unsigned h = hash_str(key) % m->size;
    bucket_t *b = m->table[h];
    while (b) {
        if (strcmp(b->key, key)==0) return b;
        b = b->next;
    }
    if (!create) return NULL;
    b = malloc(sizeof(*b));
    b->key = strdup(key);
    b->tokens = 0;
    b->last_ms = now;
    b->next = m->table[h];
    m->table[h] = b;
    return b;
}

static int mem_take(store_t *s, const char *key, double capacity, double refill_per_sec, long long now_ms_v, double tokens, store_result_t *out) {
    mem_ctx_t *m = (mem_ctx_t*)s->ctx;
    long long start = now_ms();
    pthread_mutex_lock(&m->lock);
    bucket_t *b = find_bucket(m, key, 1, now_ms_v);
    double elapsed = (now_ms_v - b->last_ms) / 1000.0;
    double refill = elapsed * refill_per_sec;
    b->tokens += refill;
    if (b->tokens > capacity) b->tokens = capacity;
    b->last_ms = now_ms_v;
    int allowed = 0;
    if (b->tokens + 1e-9 >= tokens) {
        b->tokens -= tokens;
        allowed = 1;
    }
    out->allowed = allowed;
    out->remaining = b->tokens;
    pthread_mutex_unlock(&m->lock);
    m->lat = (int)(now_ms() - start);
    return 0;
}

static int mem_latency(store_t *s) {
    mem_ctx_t *m = (mem_ctx_t*)s->ctx;
    return m->lat;
}

static void mem_close(store_t *s) {
    mem_ctx_t *m = (mem_ctx_t*)s->ctx;
    for (int i=0;i<m->size;i++){
        bucket_t *b = m->table[i];
        while (b) { bucket_t *n=b->next; free(b->key); free(b); b=n; }
    }
    free(m->table);
    pthread_mutex_destroy(&m->lock);
    free(m);
    free(s);
}

store_t *store_mem_new(int table_size) {
    mem_ctx_t *m = calloc(1, sizeof(*m));
    m->size = table_size>0?table_size:1024;
    m->table = calloc(m->size, sizeof(bucket_t*));
    pthread_mutex_init(&m->lock, NULL);
    store_t *s = calloc(1, sizeof(*s));
    s->ctx = m;
    s->take = mem_take;
    s->latency_ms = mem_latency;
    s->close = mem_close;
    return s;
}
