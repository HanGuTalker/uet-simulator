/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef AI_TRANSPORT_ENDPOINT_H
#define AI_TRANSPORT_ENDPOINT_H

#include "ai-transport-types.h"

#include "ns3/address.h"
#include "ns3/object.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class Node;

/**
 * Protocol-neutral endpoint contract used by AI workload experiments.
 *
 * Implementations adapt protocol-specific connection, reliability and congestion-control state to
 * this interface. Protocol wire headers and state machines remain in their own modules.
 */
class AiTransportEndpoint : public Object
{
  public:
    using PacketPathTracedCallback = void (*)(Ptr<const Packet>, uint32_t, uint32_t);
    using ConnectionSequenceTracedCallback = void (*)(uint32_t, uint32_t);
    using ConnectionTripleTracedCallback = void (*)(uint32_t, uint32_t, uint32_t);
    using PayloadTracedCallback = void (*)(uint32_t, uint32_t);
    using MessageCompleteTracedCallback = void (*)(uint32_t, uint64_t, uint32_t, Time);

    static TypeId GetTypeId();

    ~AiTransportEndpoint() override;

    virtual AiTransportProtocol GetProtocol() const = 0;
    virtual AiTransportCapabilities GetCapabilities() const = 0;
    virtual bool Initialize(Ptr<Node> node, const AiTransportEndpointConfig& config) = 0;
    virtual bool AddPeer(uint32_t endpointId, const Address& address) = 0;
    virtual uint32_t OpenConnection(const AiTransportConnectionConfig& config) = 0;
    virtual bool Submit(const AiTransportRequest& request) = 0;

    virtual uint32_t GetCongestionWindow(uint32_t connectionId) const = 0;
    virtual bool SetCongestionWindow(uint32_t connectionId, uint32_t bytes) = 0;
    virtual bool SetConnectionRate(uint32_t connectionId, uint64_t rateBps) = 0;
    virtual bool ConfigureJobScheduler(uint64_t lineRateBps) = 0;
    virtual bool AssignConnectionToJob(uint32_t connectionId, uint32_t jobId, uint32_t weight) = 0;
    virtual AiTransportCounters GetCounters() const = 0;

  protected:
    void NotifyPacketTx(Ptr<const Packet> packet, uint32_t connectionId, uint32_t pathId);
    void NotifyPacketRx(Ptr<const Packet> packet, uint32_t connectionId, uint32_t pathId);
    void NotifyPayloadRx(uint32_t sourceEndpointId, uint32_t payloadBytes);
    void NotifyRetransmission(uint32_t connectionId, uint32_t sequenceNumber);
    void NotifyTimeout(uint32_t connectionId, uint32_t sequenceNumber);
    void NotifyNack(uint32_t connectionId, uint32_t sequenceNumber);
    void NotifyCongestionWindow(uint32_t connectionId, uint32_t oldBytes, uint32_t newBytes);
    void NotifyEcnReceived(uint32_t connectionId, uint32_t pathId);
    void NotifyPacketTrimmed(Ptr<const Packet> packet, uint32_t connectionId, uint32_t pathId);
    void NotifyPathSelected(uint32_t connectionId, uint32_t sequenceNumber, uint32_t pathId);
    void NotifyReorderDepth(uint32_t connectionId, uint32_t oldDepth, uint32_t newDepth);
    void NotifyMessageComplete(uint32_t connectionId,
                               uint64_t messageId,
                               uint32_t bytes,
                               Time latency);

  private:
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_packetTxTrace;
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_packetRxTrace;
    TracedCallback<uint32_t, uint32_t> m_payloadRxTrace;
    TracedCallback<uint32_t, uint32_t> m_retransmissionTrace;
    TracedCallback<uint32_t, uint32_t> m_timeoutTrace;
    TracedCallback<uint32_t, uint32_t> m_nackTrace;
    TracedCallback<uint32_t, uint32_t, uint32_t> m_congestionWindowTrace;
    TracedCallback<uint32_t, uint32_t> m_ecnReceivedTrace;
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_packetTrimmedTrace;
    TracedCallback<uint32_t, uint32_t, uint32_t> m_pathSelectedTrace;
    TracedCallback<uint32_t, uint32_t, uint32_t> m_reorderDepthTrace;
    TracedCallback<uint32_t, uint64_t, uint32_t, Time> m_messageCompleteTrace;
};

} // namespace ns3

#endif // AI_TRANSPORT_ENDPOINT_H
