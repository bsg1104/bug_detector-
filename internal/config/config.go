package config

import "time"

type Config struct {
	ListenAddr       string
	MetricsAddr      string
	RedisAddr        string
	RedisPassword    string
	DefaultRate      float64 // tokens per second
	DefaultBurst     float64
	StoreLatencyMs   int64
	MemoryPressureMB uint64
}

func LoadDefault() Config {
	return Config{
		ListenAddr:       ":8080",
		MetricsAddr:      ":9090",
		RedisAddr:        "redis:6379",
		RedisPassword:    "",
		DefaultRate:      100.0, // 100 r/s
		DefaultBurst:     200.0,
		StoreLatencyMs:   50,
		MemoryPressureMB: 500,
	}
}
