// OmniNet - UDP game server
#include "game/GameServer.h"

#include <random>
#include <utility>

namespace OmniNet::Game {

GameServer::GameServer()
    : m_channelModes(DEFAULT_CHANNEL_COUNT, ChannelMode::Reliable) {
    m_channelModes[SYSTEM_CHANNEL] = ChannelMode::Unreliable;
}

GameServer::~GameServer() {
    close();
}

Result<void, SocketError> GameServer::bind(const std::string& address, uint16_t port) {
    auto res = m_socket.bind(address, port);
    if (!res) return res;
    // Generous kernel buffers for tick-rate bursts (best-effort)
    (void)m_socket.setReceiveBufferSize(1 << 20);
    (void)m_socket.setSendBufferSize(1 << 20);
    return {};
}

void GameServer::close() {
    // Best-effort graceful disconnect
    for (auto& [id, conn] : m_clients)
        sendSystem(*conn, SystemMsg::Disconnect);
    m_clients.clear();
    m_byEndpoint.clear();
    m_socket.close();
}

void GameServer::setChannelMode(uint8_t channel, ChannelMode mode) {
    if (channel >= m_channelCount) return;
    m_channelModes[channel] = mode;
    for (auto& [id, conn] : m_clients)
        conn->setChannelMode(channel, mode);
}

uint64_t GameServer::endpointKey(const UdpEndpoint& ep) {
    // FNV-1a over "address:port"
    std::string s = ep.toString();
    uint64_t h = 1469598103934665603ULL;
    for (char c : s) {
        h ^= static_cast<uint8_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

// ============================================================================
// Messaging
// ============================================================================

void GameServer::enableDtls(const std::string& certFile, const std::string& keyFile,
                            bool requireCookie) {
    m_dtlsEnabled = true;
    m_dtlsCookie = requireCookie;
    m_dtlsCert = certFile;
    m_dtlsKey = keyFile;
}

void GameServer::flushDtls(DtlsSession& session, const UdpEndpoint& to) {
    std::vector<std::vector<uint8_t>> out;
    session.pollOutgoing(out);
    for (const auto& d : out)
        (void)m_socket.sendTo(d, to);
}

void GameServer::sendPacketsTo(GameConnection& conn,
                               const std::vector<std::vector<uint8_t>>& packets) {
    if (conn.dtls) {
        for (const auto& p : packets)
            (void)conn.dtls->encrypt(p);
        flushDtls(*conn.dtls, conn.remote());
        return;
    }
    for (const auto& p : packets)
        (void)m_socket.sendTo(p, conn.remote());
}

void GameServer::flushConnection(GameConnection& conn) {
    std::vector<std::vector<uint8_t>> packets;
    conn.buildPackets(packets);
    sendPacketsTo(conn, packets);
}

void GameServer::sendSystem(GameConnection& conn, uint8_t systemMsgType,
                            std::span<const uint8_t> extra) {
    std::vector<uint8_t> payload(1 + extra.size());
    payload[0] = systemMsgType;
    if (!extra.empty())
        std::copy(extra.begin(), extra.end(), payload.begin() + 1);

    // System messages ride the system channel (unreliable) and flush now:
    // handshake and keepalive traffic must not wait for the next pump.
    if (conn.queueMessage(SYSTEM_CHANNEL, payload))
        flushConnection(conn);
}

// Send a system message to an endpoint that has no GameConnection yet
// (handshake phase). When DTLS is enabled the packet must be encrypted
// through the endpoint's session — an established client's session, or a
// completed handshake session still in the pending map.
void GameServer::sendSystemToEndpoint(const UdpEndpoint& from, uint8_t systemMsgType,
                                      std::span<const uint8_t> extra) {
    GameConnection temp(from, 0, m_channelCount);
    std::vector<uint8_t> payload(1 + extra.size());
    payload[0] = systemMsgType;
    if (!extra.empty())
        std::copy(extra.begin(), extra.end(), payload.begin() + 1);
    if (!temp.queueMessage(SYSTEM_CHANNEL, payload)) return;
    std::vector<std::vector<uint8_t>> packets;
    temp.buildPackets(packets);

    DtlsSession* session = nullptr;
    if (m_dtlsEnabled) {
        uint64_t key = endpointKey(from);
        auto keyIt = m_byEndpoint.find(key);
        if (keyIt != m_byEndpoint.end()) {
            auto connIt = m_clients.find(keyIt->second);
            if (connIt != m_clients.end()) session = connIt->second->dtls.get();
        }
        if (!session) {
            auto hs = m_dtlsHandshaking.find(key);
            if (hs != m_dtlsHandshaking.end() && hs->second.session->isEstablished())
                session = hs->second.session.get();
        }
        if (!session) return;  // cannot reach a DTLS client in plaintext
        for (const auto& p : packets) (void)session->encrypt(p);
        flushDtls(*session, from);
        return;
    }
    for (const auto& p : packets)
        (void)m_socket.sendTo(p, from);
}

bool GameServer::send(ConnectionId id, uint8_t channel,
                      std::span<const uint8_t> payload) {
    auto it = m_clients.find(id);
    if (it == m_clients.end()) return false;
    if (!it->second->queueMessage(channel, payload)) return false;
    flushConnection(*it->second);
    return true;
}

void GameServer::broadcast(uint8_t channel, std::span<const uint8_t> payload) {
    for (auto& [id, conn] : m_clients) {
        if (conn->queueMessage(channel, payload))
            flushConnection(*conn);
    }
}

void GameServer::disconnectClient(ConnectionId id) {
    auto it = m_clients.find(id);
    if (it == m_clients.end()) return;
    sendSystem(*it->second, SystemMsg::Disconnect);
    removeClient(id, false);
}

void GameServer::removeClient(ConnectionId id, bool notify) {
    auto it = m_clients.find(id);
    if (it == m_clients.end()) return;
    m_byEndpoint.erase(endpointKey(it->second->remote()));
    m_clients.erase(it);
    if (notify && m_onDisconnect)
        m_onDisconnect(id);
}

// ============================================================================
// Receive path
// ============================================================================

void GameServer::handleConnect(const UdpEndpoint& from,
                               std::span<const uint8_t> payload) {
    // payload: [Connect, u16 version LE, (optional) u32 challenge token LE]
    if (payload.size() < 3) return;
    uint16_t version = static_cast<uint16_t>(payload[1] | (payload[2] << 8));
    if (version != GAME_PROTOCOL_VERSION) {
        sendSystemToEndpoint(from, SystemMsg::Deny);
        return;
    }

    uint64_t key = endpointKey(from);
    uint32_t token = 0;
    if (payload.size() >= 7)
        token = static_cast<uint32_t>(payload[3]) |
                (static_cast<uint32_t>(payload[4]) << 8) |
                (static_cast<uint32_t>(payload[5]) << 16) |
                (static_cast<uint32_t>(payload[6]) << 24);

    auto pendingIt = m_pending.find(key);
    if (pendingIt == m_pending.end() || pendingIt->second.token != token || token == 0) {
        // Phase 1 (or bad token): issue a stateless-ish challenge. No client
        // state is created until the endpoint proves it owns its address.
        if (m_pending.size() >= m_maxPending) {
            if (pendingIt == m_pending.end()) return;  // silent drop: anti-flood
        }
        static std::mt19937 rng{std::random_device{}()};
        PendingHandshake ph;
        ph.token = rng();
        if (ph.token == 0) ph.token = 1;
        ph.deadline = SteadyClock::now() + m_pendingTimeout;
        m_pending[key] = ph;

        uint8_t extra[4] = {
            static_cast<uint8_t>(ph.token & 0xFF),
            static_cast<uint8_t>((ph.token >> 8) & 0xFF),
            static_cast<uint8_t>((ph.token >> 16) & 0xFF),
            static_cast<uint8_t>((ph.token >> 24) & 0xFF),
        };
        sendSystemToEndpoint(from, SystemMsg::Challenge, extra);
        return;
    }

    // Phase 2: token verified — the endpoint owns its address. Commit state.
    m_pending.erase(pendingIt);

    if (m_clients.size() >= m_maxClients) {
        sendSystemToEndpoint(from, SystemMsg::Deny);
        return;
    }

    ConnectionId id = m_nextId++;
    if (m_nextId == 0) m_nextId = 1;  // 0 is reserved

    auto conn = std::make_unique<GameConnection>(from, id, m_channelCount);
    for (uint8_t ch = 0; ch < m_channelCount; ++ch)
        conn->setChannelMode(ch, m_channelModes[ch]);
    conn->setResendInterval(m_resendInterval);

    if (m_dtlsEnabled) {
        // Adopt the established DTLS session for this endpoint
        auto hs = m_dtlsHandshaking.find(key);
        if (hs == m_dtlsHandshaking.end() || !hs->second.session->isEstablished())
            return;  // no encrypted channel yet: ignore
        conn->dtls = std::move(hs->second.session);
        m_dtlsHandshaking.erase(hs);
    }

    uint8_t acceptExtra[2] = {
        static_cast<uint8_t>(id & 0xFF),
        static_cast<uint8_t>((id >> 8) & 0xFF),
    };
    sendSystem(*conn, SystemMsg::Accept, acceptExtra);

    m_byEndpoint.emplace(key, id);
    m_clients.emplace(id, std::move(conn));

    if (m_onConnect)
        m_onConnect(id, from);
}

void GameServer::handleDatagram(const UdpDatagram& datagram) {
    if (!m_dtlsEnabled) {
        processPlainPacket(datagram.from, datagram.data);
        return;
    }

    uint64_t key = endpointKey(datagram.from);

    // Established client: decrypt then process the plaintext packet
    auto keyIt = m_byEndpoint.find(key);
    if (keyIt != m_byEndpoint.end()) {
        auto connIt = m_clients.find(keyIt->second);
        if (connIt != m_clients.end() && connIt->second->dtls) {
            auto& session = *connIt->second->dtls;
            session.inputDatagram(datagram.data);
            flushDtls(session, datagram.from);
            std::vector<std::vector<uint8_t>> plains;
            session.pollPlaintext(plains);
            for (const auto& p : plains)
                processPlainPacket(datagram.from, p);
        }
        return;
    }

    // Otherwise: DTLS handshake in progress (or a new client)
    auto& pending = m_dtlsHandshaking[key];
    if (!pending.session) {
        if (m_dtlsHandshaking.size() >= m_maxPending) return;  // anti-flood
        auto session = std::make_unique<DtlsSession>();
        if (!session->initServer(m_dtlsCert, m_dtlsKey, m_dtlsCookie)) return;
        sockaddr_in pa{};
        pa.sin_family = AF_INET;
        pa.sin_port = htons(datagram.from.port);
        ::inet_pton(AF_INET, datagram.from.address.c_str(), &pa.sin_addr);
        session->setPeerAddress(pa);  // IPv4 cookie binding (v6: zero addr)
        pending.session = std::move(session);
        pending.endpoint = datagram.from;
        pending.deadline = SteadyClock::now() + std::chrono::seconds(10);
    }
    pending.session->inputDatagram(datagram.data);
    pending.session->drive();
    flushDtls(*pending.session, pending.endpoint);

    if (pending.session->isEstablished()) {
        std::vector<std::vector<uint8_t>> plains;
        pending.session->pollPlaintext(plains);
        for (const auto& p : plains)
            processPlainPacket(datagram.from, p);
    }
}

void GameServer::processPlainPacket(const UdpEndpoint& from, std::span<const uint8_t> data) {
    UdpDatagram datagram;
    datagram.from = from;
    datagram.data.assign(data.begin(), data.end());
    PacketHeader header;
    std::span<const uint8_t> records;
    if (!decodePacketHeader(datagram.data, header, records)) return;

    auto keyIt = m_byEndpoint.find(endpointKey(datagram.from));
    if (keyIt == m_byEndpoint.end()) {
        // Connection migration: a packet from a NEW endpoint carrying a known
        // connectionId migrates that client (NAT rebind, network switch).
        // Lightweight proof: the packet must contain a system keepalive Ping.
        // (DTLS mode: the datagram only decrypts under the old endpoint's
        // session, so migration cannot trigger here — clients re-handshake.)
        if (header.connectionId != 0 && !m_dtlsEnabled) {
            auto migIt = m_clients.find(header.connectionId);
            if (migIt != m_clients.end()) {
                bool hasPing = false;
                auto scan = records;
                MessageRecord scanRec;
                while (!scan.empty() && decodeRecord(scan, scanRec)) {
                    if (scanRec.channel == SYSTEM_CHANNEL && !scanRec.payload.empty() &&
                        scanRec.payload[0] == SystemMsg::Ping) {
                        hasPing = true;
                        break;
                    }
                }
                if (hasPing) {
                    GameConnection& conn = *migIt->second;
                    m_byEndpoint.erase(endpointKey(conn.remote()));
                    m_byEndpoint.emplace(endpointKey(datagram.from), conn.connectionId());
                    conn.setRemote(datagram.from);
                    keyIt = m_byEndpoint.find(endpointKey(datagram.from));
                }
            }
        }
        if (keyIt == m_byEndpoint.end()) {
            // Unknown endpoint: only a system Connect record may create a client
            MessageRecord rec;
            if (decodeRecord(records, rec) && rec.channel == SYSTEM_CHANNEL &&
                !rec.payload.empty() && rec.payload[0] == SystemMsg::Connect)
                handleConnect(datagram.from, rec.payload);
            return;
        }
    }

    auto connIt = m_clients.find(keyIt->second);
    if (connIt == m_clients.end()) return;
    GameConnection& conn = *connIt->second;
    ConnectionId id = conn.connectionId();

    bool alive = conn.processPacket(datagram.data,
        [this, id, &conn](uint8_t channel, std::span<const uint8_t> payload) {
            if (channel == SYSTEM_CHANNEL) {
                if (payload.empty()) return;
                switch (payload[0]) {
                    case SystemMsg::Connect:
                        // Duplicate connect (Accept was lost): resend Accept
                        handleConnect(conn.remote(), payload);
                        break;
                    case SystemMsg::Disconnect:
                        removeClient(id, true);
                        break;
                    case SystemMsg::Ping:
                    default:
                        break;  // keepalive: receipt alone refreshed the timeout
                }
                return;
            }
            if (m_onMessage)
                m_onMessage(id, channel, payload);
        });
    (void)alive;  // malformed records are simply ignored
}

// ============================================================================
// Main loop
// ============================================================================

void GameServer::pump(int timeoutMs) {
    if (!m_socket.isOpen()) return;

    m_socket.waitForData(timeoutMs);

    // Drain all pending datagrams
    for (;;) {
        auto dg = m_socket.recvFrom(GAME_MTU);
        if (!dg) break;  // WouldBlock or error: nothing more to read
        handleDatagram(*dg);
    }

    SteadyTime now = SteadyClock::now();

    // Sweep expired unverified handshakes
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (now >= it->second.deadline) it = m_pending.erase(it);
        else ++it;
    }

    // DTLS handshake maintenance: retransmission timers + stale sweeps
    if (m_dtlsEnabled) {
        for (auto it = m_dtlsHandshaking.begin(); it != m_dtlsHandshaking.end();) {
            auto& pend = it->second;
            if (!pend.session->isEstablished() && now >= pend.deadline) {
                it = m_dtlsHandshaking.erase(it);
                continue;
            }
            if (pend.session->nextTimeoutMs() == 0) {
                pend.session->handleTimeout();
                pend.session->drive();
                flushDtls(*pend.session, pend.endpoint);
            }
            ++it;
        }
    }

    // Retransmits + queued messages + keepalives + timeouts
    std::vector<ConnectionId> timedOut;
    for (auto& [id, conn] : m_clients) {
        std::vector<std::vector<uint8_t>> packets;
        conn->collectRetransmits(now, packets);
        conn->buildPackets(packets);  // flush anything queued outside send()
        sendPacketsTo(*conn, packets);

        if (conn->needsKeepalive(now))
            sendSystem(*conn, SystemMsg::Ping);

        if (now - conn->lastReceiveTime() >= m_clientTimeout)
            timedOut.push_back(id);
    }
    for (ConnectionId id : timedOut)
        removeClient(id, true);
}

bool GameServer::hasClient(ConnectionId id) const {
    return m_clients.find(id) != m_clients.end();
}

const GameConnection* GameServer::client(ConnectionId id) const {
    auto it = m_clients.find(id);
    return it != m_clients.end() ? it->second.get() : nullptr;
}

} // namespace OmniNet::Game
