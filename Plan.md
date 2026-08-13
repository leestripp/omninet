# OmniNet Game Networking — Plan

Feature request: OmniNet is embedded in a game engine. It needs game-style networking:
a **UDP server/client** plus **high-speed methods** (fast serialization, reliability
without TCP head-of-line blocking).

## 1. Research: how game engines send data

Sources reviewed: QuakeWorld NetChannel (1996), ENet (2004, used by Sauerbraten and
Godot), Valve's GameNetworkingSockets / Source engine model, QUIC (RFC 9000), KCP
(fast ARQ), and modern minimal channel protocols (netchan). Also the standard
reference articles by Glenn Fiedler (snapshot compression, ack systems).

### Key findings

1. **TCP is wrong for real-time game state.** Head-of-line blocking: one lost packet
   stalls everything behind it. Games need UDP with a *reliability layer on top*.
2. **The industry template is the reliable/unreliable split** (QuakeWorld, 1996):
   - *Unreliable/sequenced* for position updates and snapshots that expire in ms.
   - *Reliable/ordered* for chat, inventory, RPCs, map loads.
3. **Channels** (ENet): independent sequence counters per channel so reliable chat
   never blocks position updates. Three delivery modes:
   reliable-sequenced, unreliable-sequenced, unsequenced.
4. **Piggybacked ACKs**: games send a constant stream of small packets, so ACKs ride
   for free in the header (`ack` + 32-bit `ackBits` window — the Fiedler model).
   Dedicated ACK packets are avoided.
5. **Retransmission, not ordered byte streams**: unacked reliable payloads are resent
   with a *new* packet sequence after an RTO (~100ms); ordering is re-established at
   message level with a per-channel message sequence number.
6. **Snapshots + delta compression** (Valve Source / Quake 3): server sends world
   snapshots at a fixed tick rate (20–30 Hz), delta-encoded against the last snapshot
   the client acknowledged. Bandwidth stays low; loss of one snapshot is recovered by
   the next.
7. **Bit-packed serialization**: game state is written with bit-level writers
   (bools = 1 bit, quantized floats, varint lengths) instead of text/JSON. This is the
   single biggest bandwidth saver after delta compression.
8. **QUIC is mostly a mismatch for games**: mandatory TLS per packet, reliable-by-default
   streams, huge implementations. (Unreliable datagrams exist via RFC 9221 but are not
   the default.) Not adopted here.
9. **Connection model**: connection IDs instead of "connected" UDP sockets; short
   handshake (connect → accept), keepalive ping/pong, idle timeout. No encryption by
   default (game LAN/real-time traffic); TLS remains available via the existing TCP
   stack for non-real-time traffic.

### Design conclusions for OmniNet

- New `OmniNet::Game` namespace in `src/game/`, plus a low-level `OmniNet::UdpSocket`
  in `src/net/` next to the existing TCP `Socket`.
- Packet model: **one message per UDP packet** in v1 (aggregation is a later
  optimization; header leaves room for it).
- Header carries packet seq + piggyback ack/ackBits (Fiedler model).
- Per-channel reliability: reliable-ordered, unreliable-sequenced, unreliable.
  Reliable payloads retransmitted with new packet seq until acked; per-channel
  message seq restores ordering and dedups retransmits.
- System channel 0 reserved for handshake / ping / disconnect.
- Bit-level `BitWriter`/`BitReader` with varints and float quantization helpers as the
  "high speed methods" for serialization.
- Single-threaded `pump()` driving model (like `enet_host_service`) so the game loop
  owns the thread.

## 2. Architecture

```
src/net/UdpSocket.{h,cpp}        # non-blocking UDP socket, Endpoint, sendTo/recvFrom
src/game/BitStream.{h,cpp}       # BitWriter/BitReader: bit packing, varint, quantized floats
src/game/GameProtocol.h          # header format, flags, system messages (header-only)
src/game/GameConnection.{h,cpp}  # per-peer state: channels, seq/ack, retransmit queue
src/game/GameServer.{h,cpp}      # multi-client UDP server, handshake, pump(), callbacks
src/game/GameClient.{h,cpp}      # client: connect()/pump()/send(), callbacks
```

### Wire format v2 (little-endian)

Packet header (14 bytes):
```
 u16 connectionId      (0 = not yet assigned / handshake)
 u32 packetSeq
 u32 ack               highest packet seq received from peer
 u32 ackBits           bit i set => (ack - 1 - i) received
```

Then one or more message records until end of datagram (aggregation):
```
 u8  channel           (0 = system)
 u8  flags             bit0 reliable | bit1 sequenced | bit2 fragment
 u32 msgSeq            per-channel message sequence (0 when N/A)
 [u16 fragmentIndex, u16 fragmentCount]   only when bit2 set
 u16 payloadLen
 ... payload
```

Record header: 8 bytes (12 with fragment fields). Packets are packed to
GAME_MTU (1200 B). Reliable messages larger than one packet are fragmented
automatically and reassembled in order. RTT is estimated Jacobson/Karels-style
(RFC 6298) with Karn's rule; RTO = clamp(srtt + 4*rttvar, resendInterval, 3s).

### Channel modes

| Mode                  | Guaranteed | Ordered | Use case                    |
|-----------------------|------------|---------|-----------------------------|
| Reliable (default ch) | yes        | yes     | chat, RPC, inventory        |
| UnreliableSequenced   | no         | latest  | position/snapshot updates   |
| Unreliable            | no         | no      | effects, voice, pings       |

## 3. Phases

- [x] Phase 0 — Research (this document)
- [x] Phase 1 — `UdpSocket` + `UdpEndpoint` (non-blocking, poll-based wait)
- [x] Phase 1 — `BitWriter`/`BitReader` high-speed serialization
- [x] Phase 1 — Protocol header + `GameConnection` (seq/ack/retransmit/channels)
- [x] Phase 1 — `GameServer` / `GameClient` with handshake, keepalive, timeouts
- [x] Phase 1 — Tests: `test_udp_socket`, `test_bitstream`, `test_game_connection`,
      `test_game_network` (loopback end-to-end)
- [x] Phase 1 — Demo: `game_echo_demo`
- [x] Phase 2 — Multi-message aggregation per packet (amortize header)
- [x] Phase 2 — RTT estimation + adaptive resend (Jacobson/Karels + Karn's rule)
- [x] Phase 2 — Fragmentation/reassembly for payloads > MTU (~1200 B)
- [x] Phase 3 — Snapshot delta-compression helpers (`DeltaEncoder`/`DeltaDecoder`, XOR/RLE diff)
- [x] Phase 3 — `DtlsSocket` (OpenSSL DTLS over UDP; building-block API, echo-tested)
- [x] Phase 3 — Benchmarks: `game_network_bench` (pps, Mbps, retransmits/RTT under simulated loss)
- [x] Phase 4 — DoS hardening: challenge-response handshake (protocol v3)
- [x] Phase 4 — Bandwidth-rate stats: bytesSent/bytesReceived totals + ~1s
      window send/recv rates, lazily refreshed in `stats()`
- [x] Phase 4 — IPv6 support (dual-stack UdpSocket/GameServer/GameClient; v4-mapped normalization)
- [x] Phase 5 — DTLS cookie exchange (`requireCookie` on `DtlsSocket::accept`):
      nonblocking fd + DTLS-timer-aware handshake loops; DTLSv1_listen phase
      runs blocking with SO_RCVTIMEO slices (OpenSSL maps dgram EAGAIN to
      SSL_ERROR_SYSCALL, not WANT_READ); HMAC cookie over packed addr||port
- [x] Phase 5 — Per-client DTLS: `DtlsSession` (mem-BIO, pump-driven) wired into
      `GameServer::enableDtls` / `GameClient::connectDtls`; game handshake runs
      inside the encrypted channel; cookie exchange supported; e2e tested
- [x] Phase 5 — Connection migration: server re-keys a client's endpoint when a
      keepalive with a known connId arrives from a new endpoint (repeatable,
      spoof-resistant; DTLS clients re-handshake instead)

## 4. Public API sketch (Phase 1, as implemented)

```cpp
// Server
OmniNet::Game::GameServer server;
server.bind("0.0.0.0", 7777);
server.onConnect([](Game::ConnectionId id, const UdpEndpoint& ep) { /* ... */ });
server.onMessage([](Game::ConnectionId id, uint8_t channel,
                    std::span<const uint8_t> data) {
    server.send(id, channel, data);                 // echo
});
while (running) server.pump(1 /*ms*/);

// Client
OmniNet::Game::GameClient client;
client.onMessage([](uint8_t channel, std::span<const uint8_t> data) { /* ... */ });
client.connect("127.0.0.1", 7777, 3000 /*ms timeout*/);
client.send(Game::Channel::Reliable, payload);
while (running) client.pump(1);

// High-speed serialization
Game::BitWriter w;
w.writeBits(x, 12); w.writeFloat(y); w.writeVarInt(len); w.writeString(name);
socketBytes = w.bytes();
Game::BitReader r(socketBytes);
auto x2 = r.readBits(12); auto y2 = r.readFloat();
```
