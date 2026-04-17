# OmniNet Sysadmin CLI Tools - Implementation Plan

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.18.16 |
| OS | Arch Linux |
| Container | podman (archlinux:latest) |
| Display | Wayland, GNOME |
| Architecture | x86_64 |

## Project Overview

OmniNet is a modern C++20 web framework providing:
- HTTP/HTTPS server with routing
- WebSocket support
- Server-Sent Events (SSE)
- SQLite database integration
- User authentication & sessions
- Intrusion detection
- Dashboard web interface
- Rate limiting
- TLS/SSL support

## Configuration Files (`~/.omninet/`)

| File/Directory | Purpose |
|----------------|---------|
| `config.json` | Main configuration (server defaults, CORS, TLS paths, logging) |
| `omninet.db` | SQLite database (users, sessions, intrusion logs) |
| `omninet.log` | Application log file |
| `certs/` | TLS certificates and keys |

All tools are named with dashes (omninet-*) for standard Linux CLI convention.

## CLI Tools Implemented

All tools use dashes (e.g., `omninet-status`) for standard CLI naming:

### 1. **omninet-status** - Overall System Status
```
omninet-status [--verbose] [--json] [--report]
```
Features: Config validity, database health, certificate status, blocked IPs count, recent logs

### 2. **omninet-health** - Health Check Tool
```
omninet-health <server-url> [endpoints...] [--all] [--expected TEXT] [--timeout N]
```
Features: HTTP health check, response time measurement, JSON output

### 3. **omninet-log** - Log Viewer & Analyzer
```
omninet-log [--follow] [--tail] [--level LEVEL] [--grep PATTERN] [--stats]
```
Features: Real-time tail, log filtering, grep pattern search, statistics

### 4. **omninet-firewall** - Intrusion Detection CLI
```
omninet-firewall list|block|unblock|logs [--duration] [--all]
```
Features: Manage blocked IPs, view intrusion logs

### 5. **omninet-users** - User Management CLI
```
omninet-users list|add|remove|show|sessions|revoke [--role]
```
Features: User CRUD, session management

### 6. **omninet-db** - Database CLI
```
omninet-db info|tables|query|vacuum|backup|restore
```
Features: SQLite inspection, query execution, backup/restore

### 7. **omninet-certs** - Certificate Manager
```
omninet-certs list|info|check|generate [--days N]
```
Features: Certificate listing, expiry checking, info display

### 8. **omninet-ctl** - Server Process Manager
```
omninet-ctl list|status|start|stop|restart
```
Features: PID file management, process control

### 9. **omninet-mon** - Real-time Server Monitor
```
omninet-mon [--url URL] [--interval N]
```
Features: Real-time statistics display

### 10. **omninet-init** - Initial Setup Tool
```
omninet-init [--force] [--non-interactive] [--port N] [--address]
```
Features: Create ~/.omninet structure, default config

## Common CLI Options

All tools support:
```
--help, -h           # Show help
--version, -v        # Show version
--json, -j           # JSON output
--quiet, -q          # Suppress output
--verbose, -v        # Verbose output
```

## Directory Structure

```
tools/
├── CMakeLists.txt
├── tools_common/
│   ├── CMakeLists.txt
│   ├── common.h/cpp       # Paths, time, file helpers
│   ├── colors.h/cpp       # Terminal colors, ANSI formatting
│   ├── table.h/cpp         # Table, key-value, progress bars
│   ├── config.h/cpp        # Config reading
│   ├── database.h/cpp     # DB access, intrusion, user stores
│   └── http.h/cpp         # HTTP client, health checker
├── omninet-status/
│   └── main.cpp
├── omninet-health/
│   └── main.cpp
├── omninet-log/
│   └── main.cpp
├── omninet-firewall/
│   └── main.cpp
├── omninet-users/
│   └── main.cpp
├── omninet-db/
│   └── main.cpp
├── omninet-certs/
│   └── main.cpp
├── omninet-ctl/
│   └── main.cpp
├── omninet-mon/
│   └── main.cpp
└── omninet-init/
    └── main.cpp
```

## Build System

Tools are built via CMake when `tools/CMakeLists.txt` is included:

```bash
cmake --build build -j$(nproc)
```

Executables are placed in `build/tools/`:
- `omninet-status`
- `omninet-health`
- `omninet-log`
- `omninet-firewall`
- `omninet-users`
- `omninet-db`
- `omninet-certs`
- `omninet-ctl`
- `omninet-mon`
- `omninet-init`

## Progress

### ✅ Phase 1: Shared Library
- [x] Create directory structure (tools/tools_common/)
- [x] Implement common.h - paths, time, file helpers
- [x] Implement colors.h - terminal colors, ANSI formatting
- [x] Implement table.h - table, key-value, progress bars
- [x] Implement config.h - config reading
- [x] Implement database.h - DB access, intrusion, user stores
- [x] Implement http.h - HTTP client, health checker

### ✅ Phase 2: Core Tools
- [x] omninet-status
- [x] omninet-health
- [x] omninet-log
- [x] omninet-firewall
- [x] omninet-users
- [x] omninet-db
- [x] omninet-certs
- [x] omninet-ctl
- [x] omninet-mon
- [x] omninet-init

### ✅ Phase 3: Build System
- [x] tools/CMakeLists.txt
- [x] Updated root CMakeLists.txt
- [x] Build verified successfully

## Known Issues / TODO

1. ~~**omninet-init hangs**~~ - Verified working with both interactive stdin and --admin-password
2. ~~**omninet-certs generate**~~ - Now shows progress indicator "Generating certificate... OK/FAILED"

All CLI tools are complete and working!

## Installation

After building, binaries are in `build/tools/`. To install system-wide:
```bash
sudo cp build/tools/omninet-* /usr/local/bin/
```

Or create symlinks without underscores:
```
