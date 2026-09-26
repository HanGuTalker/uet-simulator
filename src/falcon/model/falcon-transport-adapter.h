/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef FALCON_TRANSPORT_ADAPTER_H
#define FALCON_TRANSPORT_ADAPTER_H

#include "falcon-reliability.h"
#include "falcon-simulation-tag.h"
#include "falcon-swift.h"

#include "ns3/ai-transport-endpoint.h"
#include "ns3/event-id.h"

#include <deque>
#include <map>
#include <set>
#include <unordered_map>

namespace ns3
{

class Socket;

/** Falcon 1.1 comparison data path using Push Data, EACK and NACK. */
class FalconTransportAdapter : public AiTransportEndpoint
{
  public:
    static constexpr uint16_t UDP_PORT = 4793;

    static TypeId GetTypeId();
    FalconTransportAdapter();
    ~FalconTransportAdapter() override;

    AiTransportProtocol GetProtocol() const override;
    AiTransportCapabilities GetCapabilities() const override;
    bool Initialize(Ptr<Node> node, const AiTransportEndpointConfig& config) override;
    bool AddPeer(uint32_t endpointId, const Address& address) override;
    uint32_t OpenConnection(const AiTransportConnectionConfig& config) override;
    bool Submit(const AiTransportRequest& request) override;
    uint32_t GetCongestionWindow(uint32_t connectionId) const override;
    bool SetCongestionWindow(uint32_t connectionId, uint32_t bytes) override;
    bool SetConnectionRate(uint32_t connectionId, uint64_t rateBps) override;
    bool ConfigureJobScheduler(uint64_t lineRateBps) override;
    bool AssignConnectionToJob(uint32_t connectionId, uint32_t jobId, uint32_t weight) override;
    AiTransportCounters GetCounters() const override;

  private:
    struct Peer
    {
        Address address;
        uint16_t port{UDP_PORT};
    };

    struct PendingPacket
    {
        Ptr<Packet> payload;
        FalconSimulationTag tag;
        uint32_t psn{0};
        uint32_t wireBytes{0};
        uint32_t retransmissions{0};
        bool sent{false};
        EventId timeout;
    };

    struct ConnectionState
    {
        explicit ConnectionState(uint32_t initialPsn = 0)
            : reliability(initialPsn, initialPsn)
        {
        }

        uint32_t remoteEndpointId{0};
        uint32_t congestionWindow{65536};
        uint32_t inflightBytes{0};
        uint32_t inflightPackets{0};
        uint64_t rateBps{0};
        Time retransmissionTimeout{MicroSeconds(50)};
        Time nextSend{Seconds(0)};
        FalconReliabilityManager reliability;
        FalconSwift swift;
        std::deque<uint32_t> transmitQueue;
        std::map<uint32_t, PendingPacket> pending;
    };

    struct ReceiveMessage
    {
        uint32_t totalBytes{0};
        uint32_t fragmentCount{0};
        uint64_t submittedTimeNs{0};
        std::set<uint32_t> fragments;
    };

    void DoDispose() override;
    void Receive(Ptr<Socket> socket);
    void ReceiveData(Ptr<Packet> packet, const FalconSimulationTag& tag);
    void ReceiveControl(Ptr<Packet> packet, const FalconSimulationTag& tag, FalconPacketType type);
    void TryTransmit(uint32_t connectionId);
    void Transmit(uint32_t connectionId, uint32_t psn, bool retransmission);
    void HandleTimeout(uint32_t connectionId, uint32_t psn);
    bool SendWire(Ptr<Packet> packet, const FalconSimulationTag& tag, uint32_t connectionId);
    void SendEack(const FalconSimulationTag& received, FalconReliabilityManager& reliability);
    void SendNack(const FalconSimulationTag& received, uint32_t psn);
    void RetireAcknowledged(uint32_t connectionId, const std::vector<uint32_t>& acknowledged);
    uint32_t GetSwiftWindowBytes(const ConnectionState& state) const;
    void NotifySwiftWindowChange(uint32_t connectionId, uint32_t oldWindowBytes);
    uint64_t ReceiverKey(uint32_t endpointId, uint32_t connectionId) const;

    Ptr<Node> m_node;
    Ptr<Socket> m_socket;
    AiTransportEndpointConfig m_config;
    uint32_t m_nextConnectionId{1};
    uint32_t m_pathMtu{9000};
    std::unordered_map<uint32_t, Peer> m_peers;
    std::unordered_map<uint32_t, ConnectionState> m_connections;
    std::unordered_map<uint64_t, FalconReliabilityManager> m_receivers;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, ReceiveMessage>> m_receiveMessages;
    AiTransportCounters m_counters;
};

} // namespace ns3

#endif // FALCON_TRANSPORT_ADAPTER_H
