#!/bin/bash
# Simple loadtest: parallel curl
CLIENT=${1:-123}
HOST=${2:-localhost:8080}
for i in {1..1000}; do
  (curl -s -o /dev/null -w "%{http_code}\n" -H "X-Client-ID: $CLIENT" http://$HOST/api/resource) &
done
wait
