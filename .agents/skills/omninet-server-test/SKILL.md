---
name: omninet-server-test
description: "Tests OmniNet HTTP/HTTPS server demos safely without hanging or resource leaks"
version: 1.0.0
license: MIT
---

# OmniNet Server Testing Skill

This skill provides safe patterns for testing OmniNet HTTP and HTTPS server demos without getting stuck in loops or leaving zombie processes.

## Core Principle

**Always wrap servers with `timeout` and kill existing processes before starting.**

## HTTP Server Testing Pattern

```bash
# 1. Kill any existing server processes FIRST
pkill -f http_server_demo 2>/dev/null
sleep 1

# 2. Start server with timeout wrapper (prevents hanging)
timeout 10 ./build/demos/http_server_demo > /tmp/server.log 2>&1 &
SERVER_PID=$!

# 3. Wait for server to initialize
sleep 3

# 4. Test endpoints
curl -s http://127.0.0.1:8080/api/hello
curl -s http://127.0.0.1:8080/api/health

# 5. Cleanup (timeout kills automatically, but be safe)
kill $SERVER_PID 2>/dev/null
pkill -f http_server_demo 2>/dev/null
```

## HTTPS Server Testing Pattern

```bash
# 1. Kill any existing server processes FIRST
pkill -f https_server_demo 2>/dev/null
sleep 1

# 2. Start server with timeout wrapper
timeout 10 ./build/demos/https_server_demo > /tmp/server.log 2>&1 &
SERVER_PID=$!

# 3. Wait for server to initialize
sleep 3

# 4. Test endpoints (use -k for self-signed certs, -s for silent)
curl -sk https://localhost:8443/api/hello
curl -sk https://localhost:8443/api/health

# 5. Cleanup
kill $SERVER_PID 2>/dev/null
pkill -f https_server_demo 2>/dev/null
```

## Critical Rules

1. **Never run demos without `timeout`** - demos can hang on errors
2. **Always kill before starting** - prevents port conflicts
3. **Use `sleep` before testing** - servers need time to bind to ports
4. **Separate curl commands** - easier to debug failures
5. **Use `>/tmp/server.log 2>&1`** - capture output for debugging

## One-Line Testing

For quick verification:

```bash
# HTTP
pkill -f http_server 2>/dev/null; timeout 8 ./build/demos/http_server_demo &
sleep 3 && curl -s http://127.0.0.1:8080/api/health && pkill -f http_server

# HTTPS
pkill -f https_server 2>/dev/null; timeout 8 ./build/demos/https_server_demo &
sleep 3 && curl -sk https://localhost:8443/api/health && pkill -f https_server
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Connection refused | Increase sleep time to 4-5 seconds |
| Port already in use | `pkill -9 -f server_demo` before starting |
| Timeout not working | Check if PID is still running with `ps aux` |
| No output from curl | Add `-v` flag for verbose output |

## Test All Endpoints Pattern

```bash
#!/bin/bash
# Safe multi-endpoint test script

SERVER=$1
PORT=$2
PROTO=${3:-http}

pkill -f "${SERVER}" 2>/dev/null
sleep 1

timeout 15 "./build/demos/${SERVER}" > /tmp/${SERVER}.log 2>&1 &
sleep 4

ENDPOINTS=("/api/hello" "/api/health" "/api/time")
for ep in "${ENDPOINTS[@]}"; do
    echo "Testing: ${PROTO}://127.0.0.1:${PORT}${ep}"
    curl -s "${PROTO}://127.0.0.1:${PORT}${ep}" || echo "FAILED"
done

pkill -f "${SERVER}" 2>/dev/null
```

Usage:
```bash
./safe_test.sh http_server_demo 8080 http
./safe_test.sh https_server_demo 8443 https
```

## What NOT To Do

- ❌ `while true; do ./server; done` - infinite loop
- ❌ `./server &` without timeout - will hang forever on error
- ❌ `sleep 1000` - never wait this long
- ❌ Forgetting to kill before restart - port conflicts
