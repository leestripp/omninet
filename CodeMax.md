# OmniNet Library Fixes - CodeMax.md

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.18.22 |
| OS | Arch Linux |
| Container | docker (archlinux:latest) |
| Architecture | x86_64 |
| Build System | CMake |
| Language | C++20 |

## Project Info
- **Build System:** CMake with static library `libOmniNet.a`
- **Entry Point:** `src/OmniNet.cpp`
- **Key Components:**
  - HTTP Server (`src/http/`)
  - WebSocket (`src/websocket/`)
  - Crypto (`src/crypto/`)
  - Dashboard (`src/dashboard/`)
  - Database (`src/db/`)
- **Dependencies:** OpenSSL 3.x, SQLite3

## Issues to Fix (from prompt.txt)

### Issue 1: TEST Macro Conflict (HIGH Priority)
**File:** `tests/test_all.cpp:31`
**Problem:** `TEST(name)` macro conflicts with user-defined test macros
**Fix:** Rename to `OMNINET_TEST(name)` in test files

### Issue 2: No User Test Framework (MEDIUM Priority)
**Problem:** No public test header for library users
**Fix:** Create `include/OmniNet/Test.h` with public test macros

### Issue 3: Version Macro Warnings (LOW Priority)
**File:** `src/OmniNet.h:11-17`
**Problem:** Version macros can be redefined
**Fix:** Already using `#ifndef` guards - add `pragma once` as additional protection

### Issue 4: OpenSSL Deprecated Function (MEDIUM Priority)
**File:** `src/net/TLSSocket.cpp` (error logging in handshake)
**Problem:** `ERR_func_error_string()` is deprecated in OpenSSL 3.0
**Fix:** Replace with `ERR_reason_error_string()` which is the modern alternative

### Issue 5: Database API Enhancement (LOW Priority)
**File:** `src/db/Database.h`
**Problem:** No easy iteration over query results
**Fix:** Add begin()/end() methods for range-based for loop support

## Todo List

- [x] Fix Issue 1: Rename TEST macro to OMNINET_TEST in test_all.cpp
- [x] Fix Issue 2: Create public include/OmniNet/Test.h header
- [x] Fix Issue 3: Add pragma once to OmniNet.h
- [x] Fix Issue 4: Replace ERR_func_error_string with ERR_reason_error_string
- [x] Fix Issue 5: Add iterator support to Database (QueryResult)
- [x] Build and verify all changes

## Completed Fixes

### Fix #1: TEST Macro Conflict (DONE)
- Renamed `TEST(name)` to `OMNINET_TEST(name)` in tests/test_all.cpp
- This prevents conflicts with user-defined test macros

### Fix #2: User Test Framework (DONE)
- Created `include/OmniNet/Test.h` with public test macros
- Includes: `OMNINET_TEST`, `OMNINET_REGISTER_TEST`, `OMNINET_RUN_TESTS`
- Includes assertion macros: `OMNINET_ASSERT_TRUE`, `OMNINET_ASSERT_EQ`, etc.

### Fix #3: Version Macro Warnings (DONE)
- Added `pragma once` to `src/OmniNet.h` as additional include guard
- Note: Warnings from CMake still occur due to how CMake passes -D definitions

### Fix #4: OpenSSL Deprecated Function (DONE)
- Replaced `ERR_func_error_string(e)` with `ERR_reason_error_string(e)` in TLSSocket.cpp
- The `ERR_func_error_string` is deprecated in OpenSSL 3.0

### Fix #5: Database API Enhancement (DONE)
- Added `QueryResult` class with iterator support
- Supports range-based for loop: `for (const auto& row : db.query("SELECT * FROM users"))`
- Includes `begin()`, `end()` and `Iterator` class

## TODO Items (Pending Implementation)

### TODO-1: gzip compression (HttpResponse.cpp:232)
**File:** `src/http/HttpResponse.cpp:232`
**Task:** Implement gzip compression for HTTP responses

### TODO-2: Read and append file content (HttpResponse.cpp:281)
**File:** `src/http/HttpResponse.cpp:281`
**Task:** Implement reading and appending file content for static file serving

### TODO-3: If-None-Match / If-Modified-Since handling (HttpResponse.cpp:586)
**File:** `src/http/HttpResponse.cpp:586`
**Task:** Implement proper If-None-Match / If-Modified-Since handling for caching

### TODO-4: Route grouping with prefix (Router.cpp:72)
**File:** `src/http/Router.cpp:72`
**Task:** Implement grouping with prefix for route organization

### TODO-5: Static file serving (Router.cpp:120)
**File:** `src/http/Router.cpp:120`
**Task:** Implement static file serving via router

### TODO-6: Auth middleware (UserStore.cpp:225)
**File:** `src/db/UserStore.cpp:225`
**Task:** Implement auth middleware for user authentication

### TODO-7: Extract credentials and verify (UserStore.cpp:231)
**File:** `src/db/UserStore.cpp:231`
**Task:** Extract credentials and verify user credentials

### TODO-8: Parse version in migration (Database.cpp:444)
**File:** `src/db/Database.cpp:444`
**Task:** Parse version from migration entries

### TODO-9: Certificate generation with OpenSSL (omninet-certs/main.cpp:258)
**File:** `tools/omninet-certs/main.cpp:258`
**Task:** Implement actual certificate generation with OpenSSL

### TODO-10: Rules listing (omninet-firewall/main.cpp:206)
**File:** `tools/omninet-firewall/main.cpp:206`
**Task:** Implement firewall rules listing functionality

### TODO-11: Whitelist (omninet-firewall/main.cpp:210)
**File:** `tools/omninet-firewall/main.cpp:210`
**Task:** Implement firewall whitelist functionality

### TODO-12: JSON output (omninet-status/main.cpp:458)
**File:** `tools/omninet-status/main.cpp:458`
**Task:** Implement JSON output for omninet-status tool

## Todo List

- [x] Fix Issue 1: Rename TEST macro to OMNINET_TEST in test_all.cpp
- [x] Fix Issue 2: Create public include/OmniNet/Test.h header
- [x] Fix Issue 3: Add pragma once to OmniNet.h
- [x] Fix Issue 4: Replace ERR_func_error_string with ERR_reason_error_string
- [x] Fix Issue 5: Add iterator support to Database (QueryResult)
- [x] Build and verify all changes
- [x] TODO-1: Implement gzip compression
- [x] TODO-2: Implement file content reading
- [x] TODO-3: Implement cache validation headers
- [x] TODO-4: Implement route grouping
- [x] TODO-5: Implement static file serving
- [x] TODO-6: Implement auth middleware
- [x] TODO-7: Implement credential verification
- [x] TODO-8: Implement migration version parsing
- [x] TODO-9: Implement certificate generation
- [x] TODO-10: Implement rules listing
- [x] TODO-11: Implement firewall whitelist
- [x] TODO-12: Implement JSON output for status