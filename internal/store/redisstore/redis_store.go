package redisstore

import (
	"context"
	"time"

	"github.com/example/bug_detector/internal/store"
	"github.com/go-redis/redis/v8"
)

type RedisStore struct {
	client *redis.Client
	script *redis.Script
	latMs  int64
}

func New(addr, password string) store.Store {
	r := &RedisStore{
		client: redis.NewClient(&redis.Options{Addr: addr, Password: password}),
	}
	// Lua script: args: key, capacity, refill_per_sec, now_ms, tokens
	// returns allowed(1/0), remaining
	lua := `
local key = KEYS[1]
local capacity = tonumber(ARGV[1])
local refill = tonumber(ARGV[2])
local now = tonumber(ARGV[3])
local take = tonumber(ARGV[4])
local v = redis.call("HMGET", key, "tokens", "last")
local tokens = tonumber(v[1]) or capacity
local last = tonumber(v[2]) or now
local elapsed = math.max(0, (now - last)/1000)
tokens = math.min(capacity, tokens + elapsed * refill)
local allowed = 0
if tokens + 1e-9 >= take then
  tokens = tokens - take
  allowed = 1
end
redis.call("HMSET", key, "tokens", tokens, "last", now)
redis.call("PEXPIRE", key, 60000)
return {allowed, tokens}
`
	r.script = redis.NewScript(lua)
	return r
}

func (r *RedisStore) Take(key string, capacity, refillPerSec, burst float64, now time.Time, tokens float64) (store.Result, error) {
	ctx := context.Background()
	start := time.Now()
	res, err := r.script.Run(ctx, r.client, []string{key},
		capacity, refillPerSec, now.UnixMilli(), tokens).Result()
	r.latMs = time.Since(start).Milliseconds()
	if err != nil {
		return store.Result{}, err
	}
	arr := res.([]interface{})
	allowed := arr[0].(int64) == 1
	remaining, _ := strconv.ParseFloat(string(arr[1].(string)), 64)
	return store.Result{Allowed: allowed, Remaining: remaining}, nil
}

func (r *RedisStore) Latency() int64 { return r.latMs }
func (r *RedisStore) Close() error  { return r.client.Close() }
