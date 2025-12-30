package store

import (
	"sync"
	"time"
)

type bucket struct {
	tokens float64
	last   time.Time
}

type InMemoryStore struct {
	shards []sync.Map
	latMs  int64
}

func NewInMemoryStore() *InMemoryStore {
	s := &InMemoryStore{shards: make([]sync.Map, 8)}
	return s
}

func (m *InMemoryStore) shard(key string) *sync.Map {
	return &m.shards[uint64(len(key))%uint64(len(m.shards))]
}

func (m *InMemoryStore) Take(key string, capacity, refillPerSec, burst float64, now time.Time, tokens float64) (Result, error) {
	sm := m.shard(key)
	v, _ := sm.LoadOrStore(key, &bucket{tokens: capacity, last: now})
	b := v.(*bucket)

	// simple locking with temporary replacement
	var mu sync.Mutex
	mu.Lock()
	defer mu.Unlock()

	elapsed := now.Sub(b.last).Seconds()
	refill := elapsed * refillPerSec
	b.tokens = min(capacity, b.tokens+refill)
	b.last = now

	allowed := false
	if b.tokens+1e-9 >= tokens {
		b.tokens -= tokens
		allowed = true
	}

	return Result{Allowed: allowed, Remaining: b.tokens}, nil
}

func (m *InMemoryStore) Latency() int64 { return m.latMs }
func (m *InMemoryStore) Close() error  { return nil }

func min(a, b float64) float64 {
	if a < b {
		return a
	}
	return b
}
