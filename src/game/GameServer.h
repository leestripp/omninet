// OmniNet - UDP game server: multi-client, channel-based, reliable UDP
#ifndef OMNINET_GAME_SERVER_H
#define OMNINET_GAME_SERVER_H
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

#include "game/GameConnection.h"
#include "net/UdpSocket.h"
#include "utils/Expected.h"

namespace OmniNet::Game {

// ============================================================================
// GameServer
// ============================================================================

// UDP game server. One socket, many clients, channel-based delivery.
// Drive from the game loop with pump() (enet_host_service-style).
// Not thread-safe: call everything from one thread.
class GameServer {
public:
    using ConnectCallback =
        std::function<void(ConnectionId id, const UdpEndpoint& endpoint)>;
    using MessageCallback =
        std::function<void(ConnectionId id, uint8_t channel,
                           std::span<const uint8_t> payload)>;
    using DisconnectCallback = std::function<void(ConnectionId id)>;

    GameServer();
    ~GameServer();

    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;

    // --- Lifecycle ---

    [[nodiscard]] Result<void, SocketError> bind(const std::string& address, uint16_t port);
    void close();
    bool isRunning() const { return m_socket.isOpen(); }
    uint16_t port() const { return m_socket.localPort(); }

    // --- Configuration ---

    void setMaxClients(size_t maxClients) { m_maxClients = maxClients; }
    void setChannelCount(uint8_t count) { m_channelCount = count; }
    void setChannelMode(uint8_t channel, ChannelMode mode);
    void setClientTimeout(std::chrono::milliseconds timeout) { m_clientTimeout = timeout; }
    void setResendInterval(std::chrono::milliseconds interval) { m_resendInterval = interval; }

    // --- Callbacks ---

    void onConnect(ConnectCallback cb)    { m_onConnect = std::move(cb); }
    void onMessage(MessageCallback cb)    { m_onMessage = std::move(cb); }
    void onDisconnect(DisconnectCallback cb) { m_onDisconnect = std::move(cb); }

    // --- Messaging ---

    bool send(ConnectionId id, uint8_t channel, std::span<const uint8_t> payload);
    void broadcast(uint8_t channel, std::span<const uint8_t> payload);
    void disconnectClient(ConnectionId id);

    // --- Main loop ---

    // Wait up to timeoutMs for traffic, process all pending datagrams,
    // retransmissions, keepalives, and client timeouts.
    void pump(int timeoutMs = 0);

    // --- Introspection ---

    size_t clientCount() const { return m_clients.size(); }
    bool hasClient(ConnectionId id) const;
    const GameConnection* client(ConnectionId id) const;

private:
    void flushConnection(GameConnection& conn);
    void handleDatagram(const UdpDatagram& datagram);
    void handleConnect(const UdpEndpoint& from, std::span<const uint8_t> payload);
    void sendSystem(GameConnection& conn, uint8_t systemMsgType,
                    std::span<const uint8_t> extra = {});
    void removeClient(ConnectionId id, bool notify);

    static uint64_t endpointKey(const UdpEndpoint& ep);

    UdpSocket m_socket;

    std::unordered_map<ConnectionId, std::unique_ptr<GameConnection>> m_clients;
    std::unordered_map<uint64_t, ConnectionId> m_byEndpoint;

    ConnectionId m_nextId = 1;
    // Unverified handshake attempts, keyed by endpoint: no GameConnection
    // (and no onConnect) until the client echoes the challenge token.
    struct PendingHandshake {
        uint32_t token = 0;
        std::chrono::steady_clock::time_point deadline;
    };
    std::unordered_map<uint64_t, PendingHandshake> m_pending;
    std::chrono::milliseconds m_pendingTimeout{3000};
    size_t m_maxClients = 64;
    size_t m_maxPending = 64;  // unverified handshakes (anti-flood cap)
    uint8_t m_channelCount = DEFAULT_CHANNEL_COUNT;
    std::vector<ChannelMode> m_channelModes;
    std::chrono::milliseconds m_clientTimeout{10000};
    std::chrono::milliseconds m_resendInterval{100};

    ConnectCallback m_onConnect;
    MessageCallback m_onMessage;
    DisconnectCallback m_onDisconnect;
};

} // namespace OmniNet::Game

#endif // OMNINET_GAME_SERVER_H
