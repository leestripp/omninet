// OmniNet - UDP game client: connect, pump, channel-based messaging
#ifndef OMNINET_GAME_CLIENT_H
#define OMNINET_GAME_CLIENT_H
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

#include "game/GameConnection.h"
#include "net/UdpSocket.h"
#include "utils/Expected.h"

namespace OmniNet::Game {

// ============================================================================
// GameClient
// ============================================================================

// UDP game client. Drives a single GameConnection to the server.
// Not thread-safe: call everything from one thread.
class GameClient {
public:
    using MessageCallback =
        std::function<void(uint8_t channel, std::span<const uint8_t> payload)>;
    using DisconnectCallback = std::function<void()>;

    GameClient();
    ~GameClient();

    GameClient(const GameClient&) = delete;
    GameClient& operator=(const GameClient&) = delete;

    // --- Configuration ---

    void setChannelCount(uint8_t count) { m_channelCount = count; }
    void setChannelMode(uint8_t channel, ChannelMode mode);
    void setServerTimeout(std::chrono::milliseconds timeout) { m_serverTimeout = timeout; }
    void setResendInterval(std::chrono::milliseconds interval) { m_resendInterval = interval; }

    // --- Callbacks ---

    void onMessage(MessageCallback cb)       { m_onMessage = std::move(cb); }
    void onDisconnect(DisconnectCallback cb) { m_onDisconnect = std::move(cb); }

    // --- Lifecycle ---

    // Blocking connect with timeout. Retries the handshake internally.
    // Returns SocketError::Timeout if the server did not answer in time.
    [[nodiscard]] Result<void, SocketError>
    connect(const std::string& host, uint16_t port, int timeoutMs = 5000);

    // Graceful disconnect (sends Disconnect, closes socket)
    void disconnect();

    bool isConnected() const { return m_connected; }
    ConnectionId connectionId() const;
    const UdpEndpoint& serverEndpoint() const { return m_serverEndpoint; }

    // --- Messaging ---

    bool send(uint8_t channel, std::span<const uint8_t> payload);

    // --- Main loop ---

    // Wait up to timeoutMs for traffic, process datagrams, retransmissions,
    // keepalives, and server timeout detection.
    void pump(int timeoutMs = 0);

    const GameConnection* connection() const { return m_connection.get(); }

private:
    void sendSystem(uint8_t systemMsgType, std::span<const uint8_t> extra = {});
    void handleDatagram(const UdpDatagram& datagram);

    UdpSocket m_socket;
    std::unique_ptr<GameConnection> m_connection;
    UdpEndpoint m_serverEndpoint;
    bool m_connected = false;

    uint8_t m_channelCount = DEFAULT_CHANNEL_COUNT;
    std::vector<ChannelMode> m_channelModes;
    std::chrono::milliseconds m_serverTimeout{10000};
    std::chrono::milliseconds m_resendInterval{100};

    MessageCallback m_onMessage;
    DisconnectCallback m_onDisconnect;
};

} // namespace OmniNet::Game

#endif // OMNINET_GAME_CLIENT_H
