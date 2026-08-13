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

void GameServer::flushConnection(GameConnection& conn) {
    std::vector<std::vector<uint8_t>> packets;
    conn.buildPackets(packets);
    for (const auto& p : packets)
        (void)m_socket.sendTo(p, conn.remote());
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
        GameConnection temp(from, 0, m_channelCount);
        sendSystem(temp, SystemMsg::Deny);
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

        GameConnection temp(from, 0, m_channelCount);
        uint8_t extra[4] = {
            static_cast<uint8_t>(ph.token & 0xFF),
            static_cast<uint8_t>((ph.token >> 8) & 0xFF),
            static_cast<uint8_t>((ph.token >> 16) & 0xFF),
            static_cast<uint8_t>((ph.token >> 24) & 0xFF),
        };
        sendSystem(temp, SystemMsg::Challenge, extra);
        return;
    }

    // Phase 2: token verified — the endpoint owns its address. Commit state.
    m_pending.erase(pendingIt);

    if (m_clients.size() >= m_maxClients) {
        GameConnection temp(from, 0, m_channelCount);
        sendSystem(temp, SystemMsg::Deny);
        return;
    }

    ConnectionId id = m_nextId++;
    if (m_nextId == 0) m_nextId = 1;  // 0 is reserved

    auto conn = std::make_unique<GameConnection>(from, id, m_channelCount);
    for (uint8_t ch = 0; ch < m_channelCount; ++ch)
        conn->setChannelMode(ch, m_channelModes[ch]);
    conn->setResendInterval(m_resendInterval);

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
    PacketHeader header;
    std::span<const uint8_t> records;
    if (!decodePacketHeader(datagram.data, header, records)) return;

    auto keyIt = m_byEndpoint.find(endpointKey(datagram.from));
    if (keyIt == m_byEndpoint.end()) {
        // Unknown endpoint: only a system Connect record may create a client
        MessageRecord rec;
        if (decodeRecord(records, rec) && rec.channel == SYSTEM_CHANNEL &&
            !rec.payload.empty() && rec.payload[0] == SystemMsg::Connect)
            handleConnect(datagram.from, rec.payload);
        return;
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

    // Retransmits + queued messages + keepalives + timeouts
    std::vector<ConnectionId> timedOut;
    for (auto& [id, conn] : m_clients) {
        std::vector<std::vector<uint8_t>> packets;
        conn->collectRetransmits(now, packets);
        conn->buildPackets(packets);  // flush anything queued outside send()
        for (const auto& p : packets)
            (void)m_socket.sendTo(p, conn->remote());

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
