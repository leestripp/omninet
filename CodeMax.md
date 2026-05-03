# OmniNet Fixes - CodeMax.md

## System Information
- Kernel: Linux 6.8.0
- OS: Ubuntu 24.04
- Architecture: x86_64
- Build System: CMake
- Language: C++20

## Project Info
- **Build System:** CMake with static library `libOmniNet.a`
- **Entry Point:** `src/OmniNet.cpp`
- **Key Components:**
  - HTTP Server (`src/http/`)
  - WebSocket (`src/websocket/`)
  - Crypto (`src/crypto/`)
  - Dashboard (`src/dashboard/`)
  - Database (`src/db/`)
- **Dependencies:** OpenSSL, SQLite3
- **Demos:** http_server_demo, https_server_demo, dashboard_demo, websocket_echo_demo

## Issues to Fix

### 1. Issue #4: StringView Lifetime (parseFormBody)
**Status:** Analysis complete - Current code appears CORRECT
**File:** `src/http/HttpRequest.cpp`
**Issue:** The prompt mentions `string_view` lifetime issue, but code inspection shows:
- `urlDecode()` returns `std::string` (not `string_view`)
- `QueryParams` stores `std::string` values
- `parseFormBody()` returns by value (not by reference)

The current implementation is safe. The comment in prompt.txt may be outdated.

### 2. Issue #5: Password Hashing Backward Compatibility (LOW Priority)
**File:** `src/crypto/Password.cpp`
**Issue:** `verifyPassword()` only accepts hashes with 32-char or 64-char salts
**Fix:** 
- Accept various hash formats (legacy + new)
- Provide better error logging for debugging

### 3. Issue #3: Router Debug Output Verbosity (LOW Priority)  
**File:** `src/http/Router.cpp`
**Issue:** Router logs every route check
**Fix:** Add log level configuration or reduce debug output

### 4. Issue #1: Empty Response on POST (MEDIUM Priority)
**File:** `src/http/HttpConnection.cpp`
**Issue:** Server returns empty response on POST, then crashes with SIGSEGV
**Observation:** This may be related to redirect handling

### 5. Issue #2: Thread Pool Database Access (MEDIUM Priority)
**File:** `src/http/HttpConnection.cpp` + database files
**Issue:** SIGSEGV crashes during database operations in worker threads
**Observation:** SQLite connections may need thread-local handling

## Todo List

- [x] Fix Issue #5: Password hashing backward compatibility
- [x] Fix Issue #3: Reduce Router debug output verbosity
- [ ] Investigate Issue #1: Empty response on POST (redirect handling)
- [ ] Investigate Issue #2: Thread safety in database access
- [ ] Verify all fixes with build and tests

## Completed Fixes

### Fix #5: Password Hashing Backward Compatibility (DONE)
- Added support for legacy hash format (`salt:hash` without iterations)
- Added explicit salt length validation with clear error path

### Fix #3: Router Debug Output Verbosity (DONE)
- Disabled verbose debug logging by setting `DEBUG_ROUTER` to 0
- Debug output can be re-enabled by setting `DEBUG_ROUTER` to 1

### Issue #4: StringView Lifetime (NO ACTION NEEDED)
- Verified current code is safe
- `urlDecode()` returns `std::string` (owned memory)
- `QueryParams` stores `std::string` values
- No string_view dangling pointers

## Pending Investigations

### Issue #1: Empty Response on POST
- May be related to redirect handling in Response
- Need to investigate `Response::redirect()` implementation

### Issue #2: Thread Pool Database Access
- SIGSEGV during SQLite operations in worker threads
- May need thread-local database connections
