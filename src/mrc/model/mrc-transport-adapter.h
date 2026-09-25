/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef MRC_TRANSPORT_ADAPTER_H
#define MRC_TRANSPORT_ADAPTER_H

#include "mrc-header.h"
#include "mrc-nscc.h"

#include "ns3/ai-transport-endpoint.h"
#include "ns3/event-id.h"
#include "ns3/roce-simulation-tag.h"
#include "ns3/roce-v2-header.h"

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace ns3
{

class Socket;

enum class MrcQpError : uint8_t
{
    NONE = 0,
    RETRY_COUNTER_EXCEEDED = 1,
    REMOTE_INVALID_REQUEST = 2,
    REMOTE_OPERATION_ERROR = 3,
    CONNECTION_SETUP_TIMEOUT = 4,
    INCOMPATIBLE_ATTRIBUTES = 5,
};

enum class MrcConnectionState : uint8_t
{
    NEGOTIATING = 0,
    READY = 1,
    ERROR = 2,
};

struct MrcConnectionAttributes
{
    uint16_t maxWriteImmediateDestination{32};
    uint8_t maxMprDestination{8};
    bool dynamicMpr{false};
    bool trimNack{true};
    bool serviceTime{false};
};

enum class MrcEvState : uint8_t
{
    GOOD = 0,
    DENIED = 1,
    SKIP = 2,
    ASSUMED_BAD = 3,
};

/** MRC 1.0 comparison data path with multipath OOO placement and reliable recovery. */
class MrcTransportAdapter : public AiTransportEndpoint
{
  public:
    static constexpr uint16_t UDP_PORT = 4971;

    static TypeId GetTypeId();
    MrcTransportAdapter();
    ~MrcTransportAdapter() override;

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
    bool IsConnectionInError(uint32_t connectionId) const;
    MrcQpError GetConnectionError(uint32_t connectionId) const;
    MrcConnectionState GetConnectionState(uint32_t connectionId) const;
    MrcConnectionAttributes GetLocalConnectionAttributes() const;
    MrcConnectionAttributes GetNegotiatedConnectionAttributes(uint32_t connectionId) const;
    bool SetPeerConnectionAttributes(uint32_t endpointId,
                                     const MrcConnectionAttributes& attributes);
    bool CompleteOutOfBandSetup(uint32_t connectionId,
                                const MrcConnectionAttributes& peerAttributes);
    uint16_t SendEvProbe(uint32_t endpointId, uint32_t pathId = 0);
    uint16_t SendPortStatusUpdate(uint32_t endpointId,
                                  uint32_t portStatusMask,
                                  uint32_t pathId = 0);
    bool IsPathReachable(uint32_t endpointId, uint32_t pathId) const;
    Time GetPathRtt(uint32_t endpointId, uint32_t pathId) const;
    uint32_t GetPeerPortStatusMask(uint32_t endpointId) const;
    MrcEvState GetEvState(uint32_t connectionId, uint32_t pathId) const;
    bool SetEvDenied(uint32_t connectionId, uint32_t pathId, bool denied);

  private:
    struct Peer
    {
        Address address;
        uint16_t port{UDP_PORT};
    };

    struct PathState
    {
        uint64_t rateBps{0};
        Time nextSend{Seconds(0)};
        MrcEvState evState{MrcEvState::GOOD};
        EventId evTimer;
    };

    struct PendingPacket
    {
        Ptr<Packet> payload;
        RoceSimulationTag tag;
        MrcOpcode opcode{MrcOpcode::WRITE_ONLY};
        uint32_t sequence{0};
        uint16_t messageSequence{0};
        uint16_t receiveQueueMessageSequence{0};
        uint32_t packetOrder{0};
        uint32_t immediateData{0};
        uint32_t payloadBytes{0};
        uint32_t wireBytes{0};
        uint32_t pathId{0};
        uint32_t retransmissions{0};
        bool sent{false};
        bool reliabilityAcknowledged{false};
        bool fastRetransmitted{false};
        EventId timeout;
    };

    struct MessageState
    {
        uint32_t remainingPackets{0};
        bool writeImmediate{false};
    };

    struct ConnectionState
    {
        uint32_t remoteEndpointId{0};
        uint32_t nextSequence{1};
        uint32_t nextMessageSequence{1};
        uint32_t nextReceiveQueueMessageSequence{1};
        uint32_t nextPath{0};
        uint32_t congestionWindow{65536};
        uint32_t inflightBytes{0};
        uint32_t inflightPackets{0};
        uint32_t writeImmediateInflight{0};
        uint32_t maxMprPackets{1024};
        uint16_t maxWriteImmediateDestination{32};
        bool dynamicMpr{false};
        bool peerTrimNack{true};
        bool peerServiceTime{false};
        MrcConnectionState setupState{MrcConnectionState::NEGOTIATING};
        MrcQpError error{MrcQpError::NONE};
        EventId setupTimeout;
        Time retransmissionTimeout{MicroSeconds(50)};
        MrcNscc nscc;
        uint32_t previousReceivedByteUnits{0};
        std::vector<PathState> paths;
        std::deque<uint32_t> transmitQueue;
        std::map<uint32_t, PendingPacket> pending;
        std::unordered_map<uint64_t, MessageState> messages;
    };

    struct ReceivedMessage
    {
        uint64_t messageId{0};
        uint32_t totalBytes{0};
        uint64_t submittedTimeNs{0};
        uint32_t lastPacketOrder{0};
        uint32_t immediateData{0};
        bool writeImmediate{false};
        bool immediateDataSeen{false};
        bool lastSeen{false};
        bool placementComplete{false};
        std::set<uint32_t> packetOrders;
    };

    struct ReceiverState
    {
        uint32_t cumulativeAck{0};
        uint32_t highestPsn{0};
        uint64_t receivedBytes{0};
        uint16_t nextCompletionMsn{1};
        uint32_t stashedImmediateValues{0};
        std::set<uint32_t> receivedPsns;
        std::unordered_map<uint16_t, ReceivedMessage> messages;
    };

    struct EndpointRequestState
    {
        uint32_t endpointId{0};
        uint32_t pathId{0};
        MrcEndpointOperation operation{MrcEndpointOperation::EV_PROBE};
        Time sent{Seconds(0)};
        EventId timeout;
    };

    struct EndpointPathState
    {
        bool reachable{false};
        Time rtt{Seconds(0)};
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
    void SendTransportAck(const RoceSimulationTag& received, uint32_t cumulativeAck);
    void SendTransportNack(const RoceSimulationTag& received,
                           uint32_t packetSequence,
                           uint8_t syndrome);
    void SendReliabilitySack(const RoceSimulationTag& received,
                             const ReceiverState& state,
                             uint32_t acknowledgedPsn,
                             uint16_t timestamp,
                             bool congestionExperienced,
                             bool probeResponse = false,
                             uint16_t probeId = 0);
    void SendReliabilityNack(const RoceSimulationTag& received,
                             uint32_t packetSequence,
                             uint16_t timestamp,
                             MrcNackReason reason);
    void SendReliabilityProbe(uint32_t connectionId, uint32_t pathId);
    uint16_t SendEndpointRequest(uint32_t endpointId,
                                 MrcEndpointOperation operation,
                                 uint32_t portStatusMask,
                                 uint32_t pathId);
    void SendEndpointResponse(const RoceSimulationTag& received,
                              uint16_t requestId,
                              MrcEndpointOperation operation,
                              uint16_t timestamp);
    void HandleEndpointTimeout(uint64_t requestKey);
    std::optional<uint32_t> SelectActivePath(ConnectionState& state);
    bool UpdateEvState(uint32_t connectionId, uint32_t pathId, MrcEvState state);
    void RecoverSkippedEv(uint32_t connectionId, uint32_t pathId);
    void ProbeBadEv(uint32_t connectionId, uint32_t pathId);
    void HandleConnectionSetupTimeout(uint32_t connectionId);
    void ProcessAck(uint32_t connectionId, uint32_t cumulativeAck);
    void ProcessSack(uint32_t connectionId,
                     const MrcSethHeader& sack,
                     const MrcCcStateHeader& ccState);
    void ProcessNack(uint32_t connectionId, const MrcNethHeader& nack);
    void ProcessTransportNack(uint32_t connectionId, uint32_t sequence, uint8_t syndrome);
    void TransitionConnectionToError(uint32_t connectionId, MrcQpError error);
    void MarkReliabilityAcknowledged(ConnectionState& state, uint32_t sequence);
    void HandleTimeout(uint32_t connectionId, uint32_t sequence);
    uint64_t ReceiverKey(uint32_t sourceEndpointId, uint32_t connectionId) const;
    MrcOpcode SelectOpcode(uint32_t fragment, uint32_t fragments, bool writeImmediate) const;

    Ptr<Node> m_node;
    Ptr<Socket> m_receiveSocket;
    std::vector<Ptr<Socket>> m_pathSockets;
    AiTransportEndpointConfig m_config;
    uint32_t m_nextConnectionId{1};
    uint32_t m_pathMtu{9000};
    uint32_t m_pathCount{4};
    uint32_t m_receiveBitmapLength{4096};
    uint32_t m_maxWriteImmediateInflight{32};
    bool m_requireExplicitConnectionSetup{false};
    Time m_connectionSetupTimeout{MilliSeconds(1)};
    bool m_dynamicMprSupported{false};
    bool m_trimNackSupported{true};
    bool m_serviceTimeSupported{false};
    Time m_endpointResponseTimeout{MicroSeconds(50)};
    Time m_evSkipDuration{MicroSeconds(5)};
    Time m_evRecoveryProbeInterval{MicroSeconds(50)};
    uint32_t m_testDropDataSequenceOnce{0};
    bool m_testDropConsumed{false};
    uint16_t m_nextEndpointRequestId{1};
    std::unordered_map<uint32_t, Peer> m_peers;
    std::unordered_map<uint32_t, MrcConnectionAttributes> m_peerConnectionAttributes;
    std::unordered_map<uint32_t, ConnectionState> m_connections;
    std::unordered_map<uint64_t, ReceiverState> m_receivers;
    std::unordered_map<uint64_t, EndpointRequestState> m_endpointRequests;
    std::unordered_map<uint64_t, EndpointPathState> m_endpointPaths;
    std::unordered_map<uint32_t, uint32_t> m_peerPortStatusMasks;
    AiTransportCounters m_counters;
};

} // namespace ns3

#endif // MRC_TRANSPORT_ADAPTER_H
