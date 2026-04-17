# OmniNet - Logger Implementation Plan

## System Information
<!-- Place system_info output here -->
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.8.0 |
| OS | Ubuntu 24.04 |
| Container | docker (ubuntu:24.04) |
| Display | X11, GNOME |
| Architecture | x86_64 |

## Project Info
- **Type**: C++ Networking Library (HTTP/WebSocket server framework)
- **Build System**: CMake
- **Source**: src/ (http, net, utils, dashboard, database, config, security modules)
- **Tests**: tests/ (GoogleTest-based)
- **Dependencies**: OpenSSL, SQLite3

## Current State of Logger

### Complete Implementation ✅
The `Logger` class is now fully implemented at `src/utils/Logger.h` and `src/utils/Logger.cpp`:
- **Log Levels**: DEBUG, INFO, WARN, ERROR, CRITICAL
- **Console Output**: Yes (errors to stderr, others to stdout)
- **File Output**: Yes (via `setLogFile()`)
- **Async Mode**: Yes (with in-memory queue)
- **Formatting**: `[LEVEL] [timestamp] [thread] message`
- **Log Rotation**: Implemented (rotate() method with configurable max size/files)
- **Config Integration**: `initFromConfig(const LoggingConfig&)` method
- **ScopedLogger**: For RAII-style logging

### LogReader Utility
`LogReader` class at `src/utils/LogReader.h` and `src/utils/LogReader.cpp`:
- Read last N lines, pagination support
- Filter by log level and pattern
- Tail mode for live log streaming
- LogEntry struct with JSON serialization

### Dashboard Integration
`DashboardHandler` at `src/dashboard/DashboardHandler.cpp`:
- `GET /omninet/api/logs` endpoint with JSON response
- Dashboard UI with Logs tab
- Filter by level, refresh, download functionality

## Tasks

### Task 1: Implement Log Rotation [DONE]
**File**: `src/utils/Logger.cpp`

Implemented the `rotate()` method:
- Renames current log to `.1`, `.2`, etc.
- Deletes oldest files beyond `m_maxRotatedFiles`
- Uses std::filesystem for portable file operations
- Calls `performRotation()` after each write to check size limit

### Task 2: Add Logger Initialization from Config [DONE]
**File**: `src/utils/Logger.h` / `src/utils/Logger.cpp`

Added `initFromConfig(const LoggingConfig&)` method:
- Parses log level from config string
- Enables file logging with rotation settings  
- Converts MB to bytes for maxFileSize
- Logs initialization message

### Task 3: Create Log Reader Utility [DONE]
**File**: `src/utils/LogReader.h` / `src/utils/LogReader.cpp`

Created LogReader class with:
- Read last N lines, read from offset for pagination
- Filter by log level and pattern matching
- Tail mode with callback for live updates
- File info methods (size, last modified, line count)
- Static parseLine() for parsing individual log lines
- LogEntry struct with JSON serialization

### Task 4: Add `/omninet/logs` API Endpoint [DONE]
**File**: `src/dashboard/DashboardHandler.cpp`

Added endpoint:
- `GET /omninet/api/logs` - List recent logs (JSON)
- Query params: `limit`, `level`, `pattern`
- Returns JSON with `logs` array and `count`

### Task 5: Add Log Viewer to Dashboard UI [DONE]
**File**: `src/dashboard/DashboardHandler.cpp`

Added Logs tab to dashboard HTML:
- Tab navigation (Overview, Logs, Users, Settings)
- Log table with Level, Timestamp, Message columns
- Level filter dropdown (DEBUG, INFO, WARN, ERROR, CRITICAL)
- Refresh and Download buttons
- JavaScript for fetching and displaying logs from API

### Task 6: Write Tests [DONE]
**File**: `tests/test_logger_new.cpp`

Added tests:
- Logger basic functionality
- Logger level filtering
- Logger initFromConfig
- Logger file output
- LogReader basic functionality
- LogReader level filtering
- LogReader pattern filter
- LogEntry JSON serialization

All 8 tests pass.

## Implementation Order
1. Implement log rotation (core functionality)
2. Create LogReader utility
3. Add API endpoint to Dashboard
4. Add UI components
5. Wire up config initialization
6. Write tests

## CodeMax.md Maintenance
- Mark tasks as `[DONE]` as completed
- Remove completed task items (don't delete file)
- Update todos as work progresses