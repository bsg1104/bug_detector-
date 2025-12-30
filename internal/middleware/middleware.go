package middleware

import (
	"net/http"
	"strings"

	"github.com/example/bug_detector/internal/limiter"
	"github.com/example/bug_detector/internal/metrics"
	"github.com/sirupsen/logrus"
)

type Middleware struct {
	lim *limiter.Limiter
}

func New(l *limiter.Limiter) *Middleware { return &Middleware{lim: l} }

func (m *Middleware) Handler(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		client := r.Header.Get("X-Client-ID")
		if client == "" {
			client = "anonymous"
		}
		ep := r.URL.Path
		allowed, reason := m.lim.Allow(client, ep)
		if !allowed {
			metrics.IncRejected(reason)
			logrus.WithFields(logrus.Fields{"client": client, "path": ep, "reason": reason}).Warn("request rejected")
			w.Header().Set("Content-Type", "text/plain")
			w.WriteHeader(429)
			w.Write([]byte("rate limited"))
			return
		}
		metrics.IncAllowed()
		next.ServeHTTP(w, r)
	})
}
