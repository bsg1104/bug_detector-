package limiter

import (
	"testing"
	"time"

	"github.com/example/bug_detector/internal/store"
)

func TestAllowBurstAndRefill(t *testing.T) {
	mem := store.NewInMemoryStore()
	lim := New(Config{Store: mem, DefaultRate: 1, DefaultBurst: 5, StoreLatencyMs: 1000, MemoryPressureMB: 1000})
	client := "c1"
	ep := "/x"
	// consume burst
	for i := 0; i < 5; i++ {
		ok, _ := lim.Allow(client, ep)
		if !ok { t.Fatalf("expected allowed burst") }
	}
	// next should be rejected
	ok, _ := lim.Allow(client, ep)
	if ok { t.Fatalf("expected rejected after burst") }
	// wait one second -> refill 1 token
	time.Sleep(time.Second)
	ok, _ = lim.Allow(client, ep)
	if !ok { t.Fatalf("expected allowed after refill") }
}
