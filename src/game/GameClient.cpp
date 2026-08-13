// OmniNet - UDP game client
#include "game/GameClient.h"

#include <utility>

namespace OmniNet::Game {

GameClient::GameClient()
    : m_channelModes(DEFAULT_CHANNEL_COUNT, ChannelMode::Reliable) {
    m_channelModes[SYSTEM_CHANNEL] = ChannelMode::Unreliable;
}

GameClient::~GameClient() {
    disconnect();
}

void GameClient::setChannelMode(uint8_t channel, ChannelMode mode) {
    if (channel >= m_channelCount) return;
    m_channelModes[channel] = mode;
    if (m_connection)
        m_connection->setChannelMode(channel, mode);
}

void GameClient::sendSystem(uint8_t systemMsgType,
                            std::span<const uint8_t> extra) {
    if (!m_connection) return;
    std::vector<uint8_t> payload(1 + extra.size());
    payload[0] = systemMsgType;
    if (!extra.empty())
        std::copy(extra.begin(), extra.end(), payload.begin() + 1);

    // Flush immediately: handshake and keepalive must not wait for a pump
    if (!m_connection->queueMessage(SYSTEM_CHANNEL, payload)) return;
    std::vector<std::vector<uint8_t>> packets;
    m_connection->buildPackets(packets);
    for (const auto& p : packets)
        (void)m_socket.sendTo(p, m_serverEndpoint);
}

// ============================================================================
// Lifecycle
// ============================================================================

Result<void, SocketError>
GameClient::connect(const std::string& host, uint16_t port, int timeoutMs) {
    if (m_connected) return std::unexpected(SocketError::AlreadyConnected);

    auto ep = UdpSocket::resolve(host, port);
    if (!ep) return std::unexpected(ep.error());
    m_serverEndpoint = *ep;

    auto opened = m_socket.open();
    if (!opened) return opened;

    m_connection = std::make_unique<GameConnection>(m_serverEndpoint, 0, m_channelCount);
    for (uint8_t ch = 0; ch < m_channelCount; ++ch)
        m_connection->setChannelMode(ch, m_channelModes[ch]);
    m_connection->setResendInterval(m_resendInterval);

    // Handshake: send Connect, retry every 500ms until Accept or timeout
    uint8_t connectExtra[2] = {
        static_cast<uint8_t>(GAME_PROTOCOL_VERSION & 0xFF),
        static_cast<uint8_t>((GAME_PROTOCOL_VERSION >> 8) & 0xFF),
    };

    SteadyTime deadline = SteadyClock::now() + std::chrono::milliseconds(timeoutMs);
    while (SteadyClock::now() < deadline) {
        sendSystem(SystemMsg::Connect, connectExtra);  // bare Connect: get challenge

        int slice = 500;
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - SteadyClock::now()).count();
        if (remaining < slice) slice = static_cast<int>(remaining);
        if (slice <= 0) break;

        if (!m_socket.waitForData(slice)) continue;

        auto dg = m_socket.recvFrom(GAME_MTU);
        if (!dg || dg->from != m_serverEndpoint) continue;

        PacketHeader header;
        std::span<const uint8_t> records;
        if (!decodePacketHeader(dg->data, header, records)) continue;

        m_connection->onPacketReceived(header);

        MessageRecord rec;
        if (!decodeRecord(records, rec)) continue;
        if (rec.channel != SYSTEM_CHANNEL || rec.payload.empty()) continue;

        if (rec.payload[0] == SystemMsg::Challenge && rec.payload.size() >= 5) {
            // Address proof: echo the token in the next Connect
            uint8_t withToken[6] = {
                connectExtra[0], connectExtra[1],
                rec.payload[1], rec.payload[2], rec.payload[3], rec.payload[4],
            };
            sendSystem(SystemMsg::Connect, withToken);
            continue;
        }
        if (rec.payload[0] == SystemMsg::Accept && rec.payload.size() >= 3) {
            ConnectionId id = static_cast<uint16_t>(rec.payload[1] | (rec.payload[2] << 8));
            m_connection->setConnectionId(id);
            m_connected = true;
            return {};
        }
        if (rec.payload[0] == SystemMsg::Deny)
            return std::unexpected(SocketError::ConnectionRefused);
    }

    m_connection.reset();
    m_socket.close();
    return std::unexpected(SocketError::Timeout);
}

void GameClient::disconnect() {
    if (m_connected && m_connection)
        sendSystem(SystemMsg::Disconnect);
    m_connected = false;
    m_connection.reset();
    m_socket.close();
}

ConnectionId GameClient::connectionId() const {
    return m_connection ? m_connection->connectionId() : 0;
}

// ============================================================================
// Messaging
// ============================================================================

bool GameClient::send(uint8_t channel, std::span<const uint8_t> payload) {
    if (!m_connected || !m_connection) return false;
    if (!m_connection->queueMessage(channel, payload)) return false;
    std::vector<std::vector<uint8_t>> packets;
    m_connection->buildPackets(packets);
    for (const auto& p : packets)
        (void)m_socket.sendTo(p, m_serverEndpoint);
    return true;
}

void GameClient::handleDatagram(const UdpDatagram& datagram) {
    if (datagram.from != m_serverEndpoint) return;

    m_connection->processPacket(datagram.data,
        [this](uint8_t channel, std::span<const uint8_t> payload) {
            if (channel == SYSTEM_CHANNEL) {
                if (!payload.empty() && payload[0] == SystemMsg::Disconnect) {
                    m_connected = false;
                    if (m_onDisconnect) m_onDisconnect();
                }
                return;  // Ping/Accept need no handling here
            }
            if (m_onMessage)
                m_onMessage(channel, payload);
        });
}

// ============================================================================
// Main loop
// ============================================================================

void GameClient::pump(int timeoutMs) {
    if (!m_connected || !m_connection) return;

    m_socket.waitForData(timeoutMs);

    for (;;) {
        auto dg = m_socket.recvFrom(GAME_MTU);
        if (!dg) break;
        handleDatagram(*dg);
    }

    if (!m_connected) return;  // Disconnect may have arrived above

    SteadyTime now = SteadyClock::now();

    std::vector<std::vector<uint8_t>> packets;
    m_connection->collectRetransmits(now, packets);
    m_connection->buildPackets(packets);  // flush anything queued outside send()
    for (const auto& p : packets)
        (void)m_socket.sendTo(p, m_serverEndpoint);

    if (m_connection->needsKeepalive(now))
        sendSystem(SystemMsg::Ping);

    if (now - m_connection->lastReceiveTime() >= m_serverTimeout) {
        m_connected = false;
        if (m_onDisconnect) m_onDisconnect();
    }
}

} // namespace OmniNet::Game
