/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef ROCE_V2_TRANSPORT_ADAPTER_H
#define ROCE_V2_TRANSPORT_ADAPTER_H

#include "roce-simulation-tag.h"
#include "roce-v2-header.h"

#include "ns3/ai-transport-endpoint.h"
#include "ns3/event-id.h"

#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>

namespace ns3
{

class Socket;

/**
 * RoCEv2 reliable-connected transport with go-back-N recovery and DCQCN rate control.
 *
 * The class is both the protocol implementation and the common transport adapter. Protocol state
 * remains in the RoCE module; only normalized controls and traces cross the base-class boundary.
 */
class RoceV2TransportAdapter : public AiTransportEndpoint
{
  public:
    static constexpr uint16_t UDP_PORT = 4791;

    static TypeId GetTypeId();

    RoceV2TransportAdapter();
    ~RoceV2TransportAdapter() override;

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
        RoceSimulationTag tag;
        RoceOpcode opcode{RoceOpcode::RC_SEND_ONLY};
        uint32_t sequence{0};
        uint32_t payloadBytes{0};
        uint32_t wireBytes{0};
        uint32_t retransmissions{0};
        bool sent{false};
        EventId timeout;
    };

    struct MessageState
    {
        uint32_t bytes{0};
        uint32_t remainingPackets{0};
        Time submitted{Seconds(0)};
    };

    struct ConnectionState
    {
        uint32_t remoteEndpointId{0};
        uint32_t nextSequence{1};
        uint32_t congestionWindow{65536};
        uint32_t inflightBytes{0};
        uint64_t lineRateBps{0};
        uint64_t currentRateBps{0};
        uint64_t targetRateBps{0};
        Time retransmissionTimeout{MicroSeconds(50)};
        double dcqcnAlpha{0.0};
        uint32_t recoveryStage{0};
        EventId recoveryEvent;
        Time nextSend{Seconds(0)};
        std::deque<uint32_t> transmitQueue;
        std::map<uint32_t, PendingPacket> pending;
        std::unordered_map<uint64_t, MessageState> messages;
    };

    void DoDispose() override;
    void Receive(Ptr<Socket> socket);
    void TryTransmit(uint32_t connectionId);
    void TransmitSequence(uint32_t connectionId, uint32_t sequence, bool retransmission);
    bool SendWirePacket(Ptr<Packet> packet,
                        const RoceSimulationTag& tag,
                        uint32_t connectionId,
                        uint32_t pathId = 0);
    void SendAck(const RoceSimulationTag& received, uint32_t sequence, bool nack);
    void SendCnp(const RoceSimulationTag& received);
    void ProcessAck(uint32_t connectionId, uint32_t sequence);
    void ProcessNack(uint32_t connectionId, uint32_t expectedSequence);
    void HandleTimeout(uint32_t connectionId, uint32_t sequence);
    void ProcessCnp(uint32_t connectionId);
    void RecoverDcqcnRate(uint32_t connectionId);
    RoceOpcode SelectOpcode(AiTransportOperation operation,
                            uint32_t fragment,
                            uint32_t fragments) const;
    uint64_t ReceiverKey(uint32_t sourceEndpointId, uint32_t connectionId) const;

    Ptr<Node> m_node;
    Ptr<Socket> m_socket;
    AiTransportEndpointConfig m_config;
    uint32_t m_nextConnectionId{1};
    uint32_t m_pathMtu{9000};
    double m_dcqcnG{1.0 / 256.0};
    double m_dcqcnMinRateFactor{0.01};
    uint64_t m_dcqcnAdditiveIncreaseBps{5000000000ULL};
    Time m_dcqcnRecoveryPeriod{MicroSeconds(55)};
    Time m_cnpInterval{MicroSeconds(50)};
    std::unordered_map<uint32_t, Peer> m_peers;
    std::unordered_map<uint32_t, ConnectionState> m_connections;
    std::unordered_map<uint64_t, uint32_t> m_expectedReceiveSequence;
    std::unordered_map<uint64_t, Time> m_lastCnp;
    AiTransportCounters m_counters;
};

} // namespace ns3

#endif // ROCE_V2_TRANSPORT_ADAPTER_H
