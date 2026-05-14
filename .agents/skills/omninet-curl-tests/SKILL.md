---
name: omninet-curl-tests
description: "Comprehensive curl-based integration testing for OmniNet HTTP/HTTPS server demos. Use when testing API endpoints, admin routes, static file serving, WebSocket, and error handling."
version: 1.0.0
license: MIT
---

# OmniNet Curl Tests Skill

Comprehensive integration testing for OmniNet HTTP/HTTPS server demos using curl.

## Prerequisites

1. **Build the project first:**
```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

2. **Verify demo binaries exist:**
```bash
ls -la build/demos/http_server_demo
ls -la build/demos/https_server_demo
ls -la build/demos/websocket_echo_demo
ls -la build/demos/static_files_demo
```

## Testing Workflow

### Step 1: Start Server in Background

Use the `background_process` tool to start the server:

```
action: start
command: ./build/demos/http_server_demo
cwd: /home/lee/projects/Websites/OmniNet
```

### Step 2: Wait for Server Startup

Poll the process to check for startup messages, then wait 3-5 seconds:
```
action: poll
process_id: <server_pid>
```

### Step 3: Run Tests with curl

Use `background_process` for each curl command:
```
action: start
command: curl -s http://localhost:8080/api/hello
cwd: /home/lee/projects/Websites/OmniNet
```

### Step 4: Stop Server

```
action: stop
process_id: <server_pid>
```

## Test Categories

### Category 1: Basic API Endpoints

| Test | Command | Expected Response |
|------|---------|-------------------|
| Hello | `curl -s http://localhost:8080/api/hello` | `{"message": "Hello, World!"}` |
| Health | `curl -s http://localhost:8080/api/health` | `{"status": "healthy"}` |
| Time | `curl -s http://localhost:8080/api/time` | `{"timestamp": <unix_timestamp>}` |
| Echo POST | `curl -s -X POST http://localhost:8080/api/echo -H "Content-Type: application/json" -d '{"test":"data"}'` | JSON echo response |

### Category 2: Admin Dashboard Endpoints

| Test | Command | Expected Response |
|------|---------|-------------------|
| Admin Health | `curl -s http://localhost:8080/omninet/health` | JSON with status, uptime, version |
| Admin Stats | `curl -s http://localhost:8080/omninet/stats` | JSON with requests, connections, bytes stats |

### Category 3: HTTP Methods

```bash
# GET
curl -s http://localhost:8080/api/hello

# POST with JSON
curl -s -X POST http://localhost:8080/api/echo \
  -H "Content-Type: application/json" \
  -d '{"method":"POST","data":"test"}'

# PUT
curl -s -X PUT http://localhost:8080/api/data \
  -H "Content-Type: application/json" \
  -d '{"method":"PUT"}'

# DELETE
curl -s -X DELETE http://localhost:8080/api/data

# PATCH
curl -s -X PATCH http://localhost:8080/api/data \
  -H "Content-Type: application/json" \
  -d '{"method":"PATCH"}'
```

### Category 4: Headers and Content Types

```bash
# Custom headers
curl -s http://localhost:8080/api/headers \
  -H "X-Custom-Header: test-value" \
  -H "X-Request-ID: 12345"

# Accept JSON
curl -s http://localhost:8080/api/data \
  -H "Accept: application/json"

# Form data
curl -s -X POST http://localhost:8080/api/form \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "key=value&other=123"
```

### Category 5: Query Parameters

```bash
# Simple query
curl -s "http://localhost:8080/api/search?q=test"

# Multiple params
curl -s "http://localhost:8080/api/search?q=test&limit=10&offset=0"

# URL-encoded
curl -s "http://localhost:8080/api/search?q=hello%20world"
```

### Category 6: Authentication

```bash
# Basic Auth
curl -s http://localhost:8080/api/protected \
  -H "Authorization: Basic dXNlcm5hbWU6cGFzc3dvcmQ="

# Bearer Token
curl -s http://localhost:8080/api/protected \
  -H "Authorization: Bearer <token>"

# API Key
curl -s http://localhost:8080/api/data \
  -H "X-API-Key: my-secret-key"
```

### Category 7: Cookies

```bash
# Send cookies
curl -s http://localhost:8080/api/profile \
  -H "Cookie: session=abc123; user=admin"

# Check Set-Cookie
curl -sv http://localhost:8080/api/login 2>&1 | grep -i "set-cookie"
```

### Category 8: Error Handling

| Test | Command | Expected Code |
|------|---------|--------------|
| 400 Bad Request | `curl -s -X POST http://localhost:8080/api/invalid -d "invalid"` | 400 |
| 401 Unauthorized | `curl -s http://localhost:8080/api/protected` | 401 |
| 403 Forbidden | `curl -s http://localhost:8080/api/forbidden` | 403 |
| 404 Not Found | `curl -s http://localhost:8080/nonexistent` | 404 |
| 405 Method Not Allowed | `curl -s -X DELETE http://localhost:8080/api/hello` | 405 |
| 429 Rate Limited | `for i in {1..30}; do curl -s http://localhost:8080/api/hello; done` | 429 |

### Category 9: Static File Serving

```bash
# HTML files
curl -s http://localhost:8080/index.html
curl -s http://localhost:8080/page.html

# CSS files
curl -s http://localhost:8080/style.css

# JavaScript files
curl -s http://localhost:8080/app.js

# Check Content-Type headers
curl -sI http://localhost:8080/style.css | grep Content-Type
```

### Category 10: HTTPS/SSL Testing

```bash
# Start HTTPS server
./build/demos/https_server_demo &

# Test with self-signed cert (use -k flag)
curl -sk https://localhost:8443/api/hello
curl -sk https://localhost:8443/api/health
curl -sk https://localhost:8443/omninet/health

# Verbose SSL debugging
curl -svk https://localhost:8443/api/hello 2>&1 | head -30

# Stop HTTPS server
pkill -f https_server_demo
```

### Category 11: WebSocket Testing

```bash
# Requires websocat or similar
# Install: sudo apt install websocat

# Start WebSocket server
./build/demos/websocket_echo_demo &

# Connect and test
websocat ws://localhost:8081/echo
# Type messages to see echo responses

# Stop
pkill -f websocket_echo_demo
```

### Category 12: Performance and Load

```bash
# Single request latency
time curl -s http://localhost:8080/api/hello > /dev/null

# Concurrent requests (10 parallel)
for i in {1..10}; do curl -s http://localhost:8080/api/hello & done; wait

# Heavy load (100 requests)
for i in {1..100}; do curl -s http://localhost:8080/api/hello & done; wait

# Check stats after load
curl -s http://localhost:8080/omninet/stats
```

### Category 13: Keep-Alive and Timeouts

```bash
# Test keep-alive (multiple requests on same connection)
curl -s http://localhost:8080/api/hello
curl -s http://localhost:8080/api/health
# Both should work on same connection

# Request timeout
timeout 5 curl -s http://localhost:8080/api/slow
```

### Category 14: Response Format Tests

```bash
# JSON
curl -s http://localhost:8080/api/json

# Plain text
curl -s http://localhost:8080/api/text

# HTML
curl -s http://localhost:8080/api/html

# Check response headers
curl -sI http://localhost:8080/api/json | grep Content-Type
```

## Automated Test Script

Create a comprehensive test script:

```bash
#!/bin/bash
# test_omninet.sh - Comprehensive OmniNet test script

set -e
BASE_URL="${BASE_URL:-http://localhost:8080}"
RESULTS_FILE="${RESULTS_FILE:-test_results.txt}"

log() { echo "[$(date '+%H:%M:%S')] $1"; }

# Start server
log "Starting HTTP server..."
./build/demos/http_server_demo &
SERVER_PID=$!
sleep 3

# Test helper
test_pass() { echo "✅ PASS: $1"; }
test_fail() { echo "❌ FAIL: $1"; }

log "Running API tests..."
test "$(curl -s "$BASE_URL/api/hello" | grep -c 'Hello')" -gt 0 && test_pass "GET /api/hello" || test_fail "GET /api/hello"
test "$(curl -s "$BASE_URL/api/health" | grep -c 'healthy')" -gt 0 && test_pass "GET /api/health" || test_fail "GET /api/health"
test "$(curl -s "$BASE_URL/omninet/health" | grep -c 'status')" -gt 0 && test_pass "GET /omninet/health" || test_fail "GET /omninet/health"
test "$(curl -s "$BASE_URL/omninet/stats" | grep -c 'requests')" -gt 0 && test_pass "GET /omninet/stats" || test_fail "GET /omninet/stats"

log "Running error tests..."
test "$(curl -s -o /dev/null -w '%{http_code}' "$BASE_URL/nonexistent")" -eq 404 && test_pass "404 Not Found" || test_fail "404 Not Found"

# Stop server
log "Stopping server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

log "Tests complete!"
```

Save as `scripts/test_omninet.sh` and run:
```bash
chmod +x scripts/test_omninet.sh
./scripts/test_omninet.sh
```

## Cleanup Commands

Always stop servers after testing:

```bash
# Kill all OmniNet demos
pkill -f http_server_demo
pkill -f https_server_demo
pkill -f websocket_echo_demo
pkill -f static_files_demo
pkill -f dashboard_demo

# Or kill by port
fuser -k 8080/tcp
fuser -k 8443/tcp
fuser -k 8081/tcp
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Server won't start | Check port in use: `lsof -i :8080` |
| Connection refused | Wait longer: `sleep 5` |
| SSL errors | Use `-k` flag: `curl -sk` |
| Timeout | Increase timeout: `curl --max-time 30` |
| CORS errors | Check headers in response |
| Server hangs | Check logs: `cat ~/.omninet/logs/omninet.log` |

## Test Results Template

Use this template to document test results:

```markdown
## Test Results - YYYY-MM-DD

### HTTP Server (port 8080)

| Endpoint | Method | Status | Response |
|----------|--------|--------|----------|
| /api/hello | GET | ✅ PASS | `{"message":"Hello, World!"}` |
| /api/health | GET | ✅ PASS | `{"status":"healthy"}` |
| /omninet/health | GET | ✅ PASS | JSON with status |
| /nonexistent | GET | ✅ PASS | 404 error |

### HTTPS Server (port 8443)

| Endpoint | Method | Status | Response |
|----------|--------|--------|----------|
| /api/hello | GET | ✅ PASS | `{"message":"Hello, World!"}` |
| /omninet/health | GET | ✅ PASS | JSON with status |

### Summary
- Total: 6
- Passed: 6
- Failed: 0
```