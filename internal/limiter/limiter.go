package limiter

import (
	"time"
	"runtime"

	"github.com/example/bug_detector/internal/store"
	"github.com/sirupsen/logrus"
)

type Config struct {
	Store            store.Store
	DefaultRate      float64
	DefaultBurst     float64
	StoreLatencyMs   int64
	MemoryPressureMB uint64
}

type Limiter struct {
	cfg Config
}

func New(cfg Config) *Limiter { return &Limiter{cfg: cfg} }

func (l *Limiter) Allow(clientID, endpoint string) (bool, string) {
	now := time.Now()

	// load-shedding based on store latency
	if l.cfg.Store.Latency() > l.cfg.StoreLatencyMs {
		logrus.Warn("store latency high, shedding")
		return false, "store_latency"
	}

	// memory pressure
	var ms runtime.MemStats
	runtime.ReadMemStats(&ms)
	if ms.Alloc/1024/1024 > l.cfg.MemoryPressureMB {
		logrus.Warn("memory pressure, shedding")
		return false, "memory_pressure"
	}

	// check client bucket
	clientKey := "client:" + clientID
	resC, err := l.cfg.Store.Take(clientKey, l.cfg.DefaultBurst, l.cfg.DefaultRate, l.cfg.DefaultBurst, now, 1.0)
	if err != nil {
		logrus.Errorf("store error: %v", err)
		return false, "store_error"
	}
	if !resC.Allowed {
		return false, "client_rate_limited"
	}

	// check endpoint bucket
	epKey := "endpoint:" + endpoint
	resE, err := l.cfg.Store.Take(epKey, l.cfg.DefaultBurst, l.cfg.DefaultRate, l.cfg.DefaultBurst, now, 1.0)
	if err != nil {
		logrus.Errorf("store error: %v", err)
		return false, "store_error"
	}
	if !resE.Allowed {
		// revert client token is optional; for simplicity we don't return token
		return false, "endpoint_rate_limited"
	}

	return true, ""
}
