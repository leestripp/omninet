# OmniNet Documentation Index

## Getting Started
- [README.md](../README.md) — project overview, features, build instructions
- [AGENTS.md](../AGENTS.md) — contributor/agent guide: layout, build, test conventions
- [Plan.md](../Plan.md) — game networking roadmap (phases, wire format, design rationale)

## Game Networking (UDP)
- Plan.md §2 — wire format v2: 14-byte packet header, aggregated message records, fragmentation
- `src/game/` headers are the API reference:
  - `GameServer.h` / `GameClient.h` — pump()-driven server/client, challenge-response
    handshake (DoS-hardened), `enableDtls()` / `connectDtls()` for encrypted transport
  - `GameConnection.h` — channels (Reliable / UnreliableSequenced / Unreliable),
    seq/ack, RTT estimation, `ConnectionStats` (counters + bandwidth rates)
  - `BitStream.h` — bit-level writer/reader, varints, quantized floats
  - `DeltaSnapshot.h` — snapshot delta compression (XOR/RLE) with baseline registry
- `src/net/UdpSocket.h` — non-blocking dual-stack (IPv4/IPv6) UDP socket
- `src/net/DtlsSession.h` — pump-driven DTLS peer (mem-BIO; fits the game loop)
- `src/net/DtlsSocket.h` — blocking DTLS building block with optional cookie exchange

## HTTP / Server APIs
- [api/index.md](api/index.md) — API docs hub
- [api/http-server.md](api/http-server.md) — HTTP server
- [api/routes.md](api/routes.md) — routing
- [api/async.md](api/async.md) — async primitives
- [api/io.md](api/io.md) — I/O layer
- [api/database.md](api/database.md) — database (SQLite, sqlite-vec)
- [api/expected.md](api/expected.md) — Result/Expected error handling
- [api/logger.md](api/logger.md) — logging
- [api/threadpool.md](api/threadpool.md) — thread pool
- [api/token.md](api/token.md) — tokens/auth
- [api/dashboard.md](api/dashboard.md) — dashboard

## Guides
- [demos.md](demos.md) — demo programs walkthrough (incl. `game_echo`)
- [migration/](migration/) — migration guides
- [xdg-standard.md](xdg-standard.md) — XDG paths convention

## Tests & Benchmarks
- `tests/` — 29 ctest suites (run `ctest --test-dir build`)
- `benchmarks/game_network_bench.cpp` — pps / Mbps / loss-simulation harness
