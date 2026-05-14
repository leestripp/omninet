#!/bin/bash
# OmniNet Integration Test Script
# Usage: ./scripts/run_tests.sh [http|https] [port]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
PROTOCOL="${1:-http}"
PORT="${2:-8080}"
BASE_URL="${PROTOCOL}://localhost:${PORT}"
RESULTS_FILE="${RESULTS_FILE:-test_results.md}"
SERVER_PID=""

# Counters
TOTAL=0
PASSED=0
FAILED=0

log_info() { echo -e "${YELLOW}[INFO]${NC} $1"; }
log_pass() { echo -e "${GREEN}✅ PASS:${NC} $1"; ((PASSED++)); ((TOTAL++)); }
log_fail() { echo -e "${RED}❌ FAIL:${NC} $1"; ((FAILED++)); ((TOTAL++)); }

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    pkill -f "http_server_demo" 2>/dev/null || true
    pkill -f "https_server_demo" 2>/dev/null || true
}

trap cleanup EXIT

# Start server
start_server() {
    local demo=$1
    log_info "Starting $demo..."
    if [ "$PROTOCOL" = "https" ]; then
        ./build/demos/https_server_demo &
    else
        ./build/demos/http_server_demo &
    fi
    SERVER_PID=$!
    log_info "Server PID: $SERVER_PID"
    sleep 4
}

# Test helper
test_endpoint() {
    local method="${1:-GET}"
    local endpoint="$2"
    local expected_pattern="$3"
    local description="$4"
    local data="${5:-}"
    local headers="${6:-}"
    
    local response
    local http_code
    
    if [ -n "$data" ]; then
        response=$(curl -s -w "\n%{http_code}" -X "$method" "$BASE_URL$endpoint" \
            -H "Content-Type: application/json" \
            -d "$data" 2>/dev/null)
    elif [ -n "$headers" ]; then
        response=$(curl -s -w "\n%{http_code}" -X "$method" "$BASE_URL$endpoint" \
            -H "$headers" 2>/dev/null)
    else
        response=$(curl -s -w "\n%{http_code}" -X "$method" "$BASE_URL$endpoint" 2>/dev/null)
    fi
    
    http_code=$(echo "$response" | tail -n 1)
    body=$(echo "$response" | head -n -1)
    
    if echo "$body" | grep -q "$expected_pattern"; then
        log_pass "$description"
        echo "   Response: ${body:0:100}..."
    else
        log_fail "$description"
        echo "   Expected pattern: $expected_pattern"
        echo "   Got: $body"
        echo "   HTTP Code: $http_code"
    fi
}

# Test HTTP code
test_http_code() {
    local method="${1:-GET}"
    local endpoint="$2"
    local expected_code="$3"
    local description="$4"
    
    local http_code
    http_code=$(curl -s -o /dev/null -w "%{http_code}" -X "$method" "$BASE_URL$endpoint" 2>/dev/null)
    
    if [ "$http_code" = "$expected_code" ]; then
        log_pass "$description"
    else
        log_fail "$description (got $http_code, expected $expected_code)"
    fi
}

# Run all tests
run_tests() {
    echo ""
    log_info "========================================"
    log_info "  OmniNet Integration Tests"
    log_info "  Protocol: $PROTOCOL"
    log_info "  Port: $PORT"
    log_info "  Base URL: $BASE_URL"
    log_info "========================================"
    echo ""
    
    # API Endpoints
    echo "--- API Endpoints ---"
    test_endpoint "GET" "/api/hello" "Hello" "GET /api/hello"
    test_endpoint "GET" "/api/health" "healthy" "GET /api/health"
    test_endpoint "GET" "/api/time" "timestamp" "GET /api/time"
    test_endpoint "POST" "/api/echo" "echo" "POST /api/echo"
    
    # Admin Endpoints
    echo ""
    echo "--- Admin Endpoints ---"
    test_endpoint "GET" "/omninet/health" "status" "GET /omninet/health"
    test_endpoint "GET" "/omninet/stats" "requests" "GET /omninet/stats"
    
    # Error Handling
    echo ""
    echo "--- Error Handling ---"
    test_http_code "GET" "/nonexistent" "404" "GET /nonexistent (404)"
    
    # Performance Tests
    echo ""
    echo "--- Performance Tests ---"
    local start_time=$(date +%s%3N)
    curl -s "$BASE_URL/api/hello" > /dev/null
    local end_time=$(date +%s%3N)
    local latency=$((end_time - start_time))
    log_info "Single request latency: ${latency}ms"
    if [ "$latency" -lt 1000 ]; then
        log_pass "Latency under 1000ms"
    else
        log_fail "Latency over 1000ms"
    fi
    
    # Results
    echo ""
    log_info "========================================"
    log_info "  Test Results"
    log_info "========================================"
    echo "  Total:  $TOTAL"
    echo -e "  ${GREEN}Passed:${NC} $PASSED"
    echo -e "  ${RED}Failed:${NC} $FAILED"
    echo ""
    
    if [ "$FAILED" -eq 0 ]; then
        log_info "All tests passed! 🎉"
    else
        log_info "Some tests failed. Check results above."
    fi
    
    # Save results to file
    {
        echo "# OmniNet Test Results"
        echo "Date: $(date)"
        echo "Protocol: $PROTOCOL"
        echo "Port: $PORT"
        echo ""
        echo "| Test | Status |"
        echo "|-------|--------|"
        echo "| Total | $TOTAL |"
        echo "| Passed | $PASSED |"
        echo "| Failed | $FAILED |"
    } > "$RESULTS_FILE"
}

# Main
main() {
    start_server
    run_tests
}

main "$@"