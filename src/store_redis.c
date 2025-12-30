#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include <sys/time.h>
#include "store.h"

typedef struct {
    redisContext *ctx;
    int lat;
    char *script_sha;
} redis_ctx_t;

static long long now_ms() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec*1000 + tv.tv_usec/1000;
}

static const char *lua_script =
"local key=KEYS[1]\n"
"local capacity=tonumber(ARGV[1])\n"
"local refill=tonumber(ARGV[2])\n"
"local now=tonumber(ARGV[3])\n"
"local take=tonumber(ARGV[4])\n"
"local v=redis.call('HMGET', key, 'tokens','last')\n"
"local tokens=tonumber(v[1]) or capacity\n"
"local last=tonumber(v[2]) or now\n"
"local elapsed=math.max(0,(now-last)/1000)\n"
"tokens = math.min(capacity, tokens + elapsed*refill)\n"
"local allowed=0\n"
"if tokens + 1e-9 >= take then tokens = tokens - take; allowed = 1 end\n"
"redis.call('HMSET', key, 'tokens', tokens, 'last', now)\n"
"redis.call('PEXPIRE', key, 60000)\n"
"return {allowed, tostring(tokens)}\n";

static int redis_take(store_t *s, const char *key, double capacity, double refill_per_sec, long long now_ms_v, double tokens, store_result_t *out) {
    redis_ctx_t *r = (redis_ctx_t*)s->ctx;
    long long start = now_ms();
    redisReply *reply = redisCommand(r->ctx, "EVAL %s 1 %s %f %f %lld %f", lua_script, key, capacity, refill_per_sec, now_ms_v, tokens);
    if (!reply) { r->lat = (int)(now_ms() - start); return -1; }
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements>=2) {
        int allowed = 0;
        if (reply->element[0]->type==REDIS_REPLY_INTEGER) allowed = reply->element[0]->integer==1;
        double rem = 0.0;
        if (reply->element[1]->type==REDIS_REPLY_STRING) rem = atof(reply->element[1]->str);
        out->allowed = allowed;
        out->remaining = rem;
    } else {
        freeReplyObject(reply);
        r->lat = (int)(now_ms() - start);
        return -1;
    }
    freeReplyObject(reply);
    r->lat = (int)(now_ms() - start);
    return 0;
}

static int redis_latency(store_t *s) {
    redis_ctx_t *r = (redis_ctx_t*)s->ctx; return r->lat;
}

static void redis_close(store_t *s) {
    redis_ctx_t *r = (redis_ctx_t*)s->ctx;
    if (r->ctx) redisFree(r->ctx);
    free(r);
    free(s);
}

store_t *store_redis_new(const char *addr, int port) {
    struct timeval timeout = {1,500000}; // 1.5s
    redisContext *c = redisConnectWithTimeout(addr, port, timeout);
    if (!c || c->err) {
        if (c) redisFree(c);
        return NULL;
    }
    redis_ctx_t *r = calloc(1, sizeof(*r));
    r->ctx = c;
    store_t *s = calloc(1, sizeof(*s));
    s->ctx = r;
    s->take = redis_take;
    s->latency_ms = redis_latency;
    s->close = redis_close;
    return s;
}
