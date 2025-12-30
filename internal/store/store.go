package store

import "time"

type Result struct {
	Allowed   bool
	Remaining float64
}

type Store interface {
	Take(key string, capacity, refillPerSec, burst float64, now time.Time, tokens float64) (Result, error)
	Latency() int64 // last observed latency in ms
	Close() error
}

// Provide in-memory store for tests/dev