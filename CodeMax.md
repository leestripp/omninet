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

## CLI Tools Implemented

All tools are named with underscores (e.g., `omninet_status`) for compatibility:

### 1. **omninet_status** - Overall System Status
```
omninet_status [--verbose] [--json] [--report]
```
Features: Config validity, database health, certificate status, blocked IPs count, recent logs

### 2. **omninet_health** - Health Check Tool
```
omninet_health <server-url> [endpoints...] [--all] [--expected TEXT] [--timeout N]
```
Features: HTTP health check, response time measurement, JSON output

### 3. **omninet_log** - Log Viewer & Analyzer
```
omninet_log [--follow] [--tail] [--level LEVEL] [--grep PATTERN] [--stats]
```
Features: Real-time tail, log filtering, grep pattern search, statistics

### 4. **omninet_firewall** - Intrusion Detection CLI
```
omninet_firewall list|block|unblock|logs [--duration] [--all]
```
Features: Manage blocked IPs, view intrusion logs

### 5. **omninet_users** - User Management CLI
```
omninet_users list|add|remove|show|sessions|revoke [--role]
```
Features: User CRUD, session management

### 6. **omninet_db** - Database CLI
```
omninet_db info|tables|query|vacuum|backup|restore
```
Features: SQLite inspection, query execution, backup/restore

### 7. **omninet_certs** - Certificate Manager
```
omninet_certs list|info|check|generate [--days N]
```
Features: Certificate listing, expiry checking, info display

### 8. **omninet_ctl** - Server Process Manager
```
omninet_ctl list|status|start|stop|restart
```
Features: PID file management, process control

### 9. **omninet_mon** - Real-time Server Monitor
```
omninet_mon [--url URL] [--interval N]
```
Features: Real-time statistics display

### 10. **omninet_init** - Initial Setup Tool
```
omninet_init [--force] [--non-interactive] [--port N] [--address]
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
├── omninet_status/
│   └── main.cpp
├── omninet_health/
│   └── main.cpp
├── omninet_log/
│   └── main.cpp
├── omninet_firewall/
│   └── main.cpp
├── omninet_users/
│   └── main.cpp
├── omninet_db/
│   └── main.cpp
├── omninet_certs/
│   └── main.cpp
├── omninet_ctl/
│   └── main.cpp
├── omninet_mon/
│   └── main.cpp
└── omninet_init/
    └── main.cpp
```

## Build System

Tools are built via CMake when `tools/CMakeLists.txt` is included:

```bash
cmake --build build -j$(nproc)
```

Executables are placed in `build/tools/`:
- `omninet_status`
- `omninet_health`
- `omninet_log`
- `omninet_firewall`
- `omninet_users`
- `omninet_db`
- `omninet_certs`
- `omninet_ctl`
- `omninet_mon`
- `omninet_init`

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
- [x] omninet_status
- [x] omninet_health
- [x] omninet_log
- [x] omninet_firewall
- [x] omninet_users
- [x] omninet_db
- [x] omninet_certs
- [x] omninet_ctl
- [x] omninet_mon
- [x] omninet_init

### ✅ Phase 3: Build System
- [x] tools/CMakeLists.txt
- [x] Updated root CMakeLists.txt
- [x] Build verified successfully

## Known Issues / TODO

1. **omninet_init hangs** - May have an issue with interactive input
2. **omninet_certs generate** - Certificate generation requires full OpenSSL integration
3. **File includes** - Need to create symlinks or wrapper scripts for `omninet-*` (dash) versions

## Installation

After building, binaries are in `build/tools/`. To install system-wide:
```bash
sudo cp build/tools/omninet_* /usr/local/bin/
```

Or create symlinks without underscores:
```bash
cd build/tools
for f in omninet_*; do sudo ln -sf "$PWD/$f" "/usr/local/bin/${f//_/-}"; done
```
