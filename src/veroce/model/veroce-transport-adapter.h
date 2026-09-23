/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef VEROCE_TRANSPORT_ADAPTER_H
#define VEROCE_TRANSPORT_ADAPTER_H

#include "veroce-header.h"

#include "ns3/ai-transport-endpoint.h"
#include "ns3/event-id.h"
#include "ns3/roce-simulation-tag.h"
#include "ns3/roce-v2-header.h"

#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

namespace ns3
{

class Socket;

/** veRoCE P2 data-plane adapter with out-of-order DDP, SACK and path-wise FCC. */
class VeRoceTransportAdapter : public AiTransportEndpoint
{
  public:
    static constexpr uint16_t UDP_PORT = 4794;

    static TypeId GetTypeId();
    VeRoceTransportAdapter();
    ~VeRoceTransportAdapter() override;

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

    struct PathState
    {
        uint64_t lineRateBps{0};
        uint64_t currentRateBps{0};
        uint64_t targetRateBps{0};
        double alpha{0.0};
        uint32_t recoveryStage{0};
        Time nextSend{Seconds(0)};
        EventId recoveryEvent;
    };

    struct PendingPacket
    {
        Ptr<Packet> payload;
        RoceSimulationTag tag;
        RoceOpcode opcode{RoceOpcode::RC_WRITE_ONLY};
        uint32_t sequence{0};
        uint32_t messageSequence{0};
        uint32_t packetOrder{0};
        uint32_t payloadBytes{0};
        uint32_t wireBytes{0};
        uint32_t pathId{0};
        uint32_t retransmissions{0};
        bool sent{false};
        EventId timeout;
    };

    struct MessageState
    {
        uint32_t bytes{0};
        uint32_t remainingPackets{0};
    };

    struct ConnectionState
    {
        uint32_t remoteEndpointId{0};
        uint32_t nextSequence{1};
        uint32_t nextMessageSequence{1};
        uint32_t nextPath{0};
        uint32_t congestionWindow{65536};
        uint32_t inflightBytes{0};
        uint32_t retransmitFrontier{0};
        Time retransmitFrontierUpdated{Seconds(0)};
        Time retransmissionTimeout{MicroSeconds(50)};
        std::deque<uint32_t> transmitQueue;
        std::map<uint32_t, PendingPacket> pending;
        std::unordered_map<uint64_t, MessageState> messages;
        std::vector<PathState> paths;
    };

    struct ReceivedMessage
    {
        uint64_t messageId{0};
        uint32_t totalBytes{0};
        uint64_t submittedTimeNs{0};
        uint32_t lastPacketOrder{0};
        bool lastSeen{false};
        bool completed{false};
        std::set<uint32_t> packetOrders;
    };

    struct ReceiverState
    {
        uint32_t acknowledgedPsn{0};
        uint32_t highestPsn{0};
        uint32_t acknowledgedMsn{0};
        uint32_t reorderDepth{0};
        std::set<uint32_t> receivedPsns;
        std::map<uint32_t, ReceivedMessage> messages;
    };

    void DoDispose() override;
    void Receive(Ptr<Socket> socket);
    void TryTransmit(uint32_t connectionId);
    void TransmitSequence(uint32_t connectionId, uint32_t sequence, bool retransmission);
    bool SendWirePacket(Ptr<Packet> packet,
                        RoceSimulationTag tag,
                        uint32_t connectionId,
                        uint32_t sequence,
                        uint32_t pathId);
    void SendAcknowledgment(const RoceSimulationTag& received,
                            const ReceiverState& state,
                            bool selective);
    void SendCnp(const RoceSimulationTag& received);
    void ProcessAck(uint32_t connectionId, uint32_t acknowledgedPsn);
    void ProcessSack(uint32_t connectionId, uint32_t acknowledgedPsn, const VeRoceSackHeader& sack);
    void HandleTimeout(uint32_t connectionId, uint32_t sequence);
    void ProcessCnp(uint32_t connectionId, uint32_t pathId);
    void RecoverPathRate(uint32_t connectionId, uint32_t pathId);
    RoceOpcode SelectOpcode(AiTransportOperation operation,
                            uint32_t fragment,
                            uint32_t fragments) const;
    uint64_t ReceiverKey(uint32_t sourceEndpointId, uint32_t connectionId) const;
    bool CompleteReceivedMessage(const RoceSimulationTag& tag,
                                 uint32_t messageSequence,
                                 uint32_t packetOrder,
                                 bool lastPacket,
                                 ReceiverState& state);

    Ptr<Node> m_node;
    Ptr<Socket> m_receiveSocket;
    std::vector<Ptr<Socket>> m_pathSockets;
    AiTransportEndpointConfig m_config;
    uint32_t m_nextConnectionId{1};
    uint32_t m_pathMtu{9000};
    uint32_t m_pathCount{4};
    uint32_t m_receiveBitmapLength{4096};
    uint32_t m_lazySackThreshold{4};
    double m_fccGain{1.0 / 256.0};
    double m_fccMinRateFactor{0.01};
    uint64_t m_fccAdditiveIncreaseBps{5000000000ULL};
    Time m_fccRecoveryPeriod{MicroSeconds(55)};
    Time m_cnpInterval{MicroSeconds(50)};
    std::unordered_map<uint32_t, Peer> m_peers;
    std::unordered_map<uint32_t, ConnectionState> m_connections;
    std::unordered_map<uint64_t, ReceiverState> m_receivers;
    std::unordered_map<uint64_t, Time> m_lastCnp;
    AiTransportCounters m_counters;
};

} // namespace ns3

#endif // VEROCE_TRANSPORT_ADAPTER_H
