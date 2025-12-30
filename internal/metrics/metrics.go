package metrics

import (
	"net/http"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

var (
	reqTotal = prometheus.NewCounter(prometheus.CounterOpts{Name: "rl_requests_total", Help: "Total requests"})
	allowed = prometheus.NewCounter(prometheus.CounterOpts{Name: "rl_requests_allowed_total", Help: "Allowed requests"})
	rejected = prometheus.NewCounterVec(prometheus.CounterOpts{Name: "rl_requests_rejected_total", Help: "Rejected requests"}, []string{"reason"})
)

func init() {
	prometheus.MustRegister(reqTotal, allowed, rejected)
}

func Start(addr string) {
	go func() {
		srv := &http.Server{Addr: addr, Handler: promhttp.Handler()}
		srv.ListenAndServe()
	}()
}

func Handler() http.Handler { return promhttp.Handler() }

func IncAllowed() { reqTotal.Inc(); allowed.Inc() }
func IncRejected(reason string) { reqTotal.Inc(); rejected.WithLabelValues(reason).Inc() }
