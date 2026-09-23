/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_TRANSPORT_ADAPTER_H
#define UET_TRANSPORT_ADAPTER_H

#include "ns3/ai-transport-endpoint.h"

namespace ns3
{

class UetEndpoint;
class UetUdpTransport;

/** Common AI-transport adapter for the UEC AI Base endpoint. */
class UetTransportAdapter : public AiTransportEndpoint
{
  public:
    static TypeId GetTypeId();

    UetTransportAdapter();
    ~UetTransportAdapter() override;

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

    Ptr<UetEndpoint> GetUetEndpoint() const;

  private:
    void DoDispose() override;
    void ForwardPacketTx(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId);
    void ForwardPacketRx(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId);
    void ForwardRetransmission(uint32_t pdcId, uint32_t psn);
    void ForwardTimeout(uint32_t pdcId, uint32_t psn);
    void ForwardNack(uint32_t pdcId, uint32_t psn);
    void ForwardCongestionWindow(uint32_t pdcId, uint32_t oldBytes, uint32_t newBytes);
    void ForwardEcn(uint32_t pdcId, uint32_t pathId);
    void ForwardTrimmed(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId);
    void ForwardPathSelected(uint32_t pdcId, uint32_t psn, uint32_t pathId);
    void ForwardReorderDepth(uint32_t pdcId, uint32_t oldDepth, uint32_t newDepth);
    void ForwardMessageComplete(uint32_t pdcId, uint64_t messageId, uint32_t bytes, Time latency);

    Ptr<Node> m_node;
    Ptr<UetEndpoint> m_endpoint;
    Ptr<UetUdpTransport> m_transport;
    AiTransportEndpointConfig m_config;
};

} // namespace ns3

#endif // UET_TRANSPORT_ADAPTER_H
