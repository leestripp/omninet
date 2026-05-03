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