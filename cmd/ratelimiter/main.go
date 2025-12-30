package main

import (
	"context"
	"net/http"
	"os"
	"time"

	"github.com/example/bug_detector/internal/config"
	"github.com/example/bug_detector/internal/limiter"
	"github.com/example/bug_detector/internal/middleware"
	"github.com/example/bug_detector/internal/metrics"
	"github.com/example/bug_detector/internal/store"
	"github.com/example/bug_detector/internal/store/redisstore"
	"github.com/sirupsen/logrus"
)

func main() {
	ctx := context.Background()
	cfg := config.LoadDefault()

	logrus.SetLevel(logrus.InfoLevel)

	var s store.Store
	if cfg.RedisAddr != "" {
		s = redisstore.New(cfg.RedisAddr, cfg.RedisPassword)
	} else {
		s = store.NewInMemoryStore()
	}

	lim := limiter.New(limiter.Config{
		Store:            s,
		DefaultRate:      cfg.DefaultRate,
		DefaultBurst:     cfg.DefaultBurst,
		StoreLatencyMs:   cfg.StoreLatencyMs,
		MemoryPressureMB: cfg.MemoryPressureMB,
	})

	metrics.Start(cfg.MetricsAddr)
	mw := middleware.New(lim)

	mux := http.NewServeMux()
	mux.Handle("/metrics", metrics.Handler())
	mux.Handle("/api/resource", mw.Handler(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(200)
		w.Write([]byte("ok"))
	})))

	srv := &http.Server{
		Addr:         cfg.ListenAddr,
		Handler:      mux,
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 5 * time.Second,
	}

	logrus.Infof("starting server %s", cfg.ListenAddr)
	if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		logrus.Fatalf("server failed: %v", err)
	}
}
