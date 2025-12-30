# bug_detector-

Distributed Rate Limiter (C)

Overview
- Token Bucket per-client and per-endpoint.
- Redis backend (hiredis + Lua EVAL) for atomic refill+take.
- In-memory store for local testing.
- HTTP REST via libmicrohttpd, metrics endpoint exposing Prometheus text.

Build & Run
- make
- docker-compose up --build
- curl -H "X-Client-ID: 123" http://localhost:8080/api/resource

Notes
- This is a compact, pragmatic reimplementation in C focusing on core behavior.
- Token bucket chosen for smooth burst handling; Redis Lua script provides atomic operations across nodes.
- Load-shedding: store latency and memory pressure checks cause fast rejections.