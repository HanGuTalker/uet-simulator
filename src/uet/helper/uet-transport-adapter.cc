/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-transport-adapter.h"

#include "ns3/boolean.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/uet-endpoint.h"
#include "ns3/uet-pdc.h"
#include "ns3/uet-pds-header.h"
#include "ns3/uet-ses-header.h"
#include "ns3/uet-simulation-tag.h"
#include "ns3/uet-udp-transport.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(UetTransportAdapter);

TypeId
UetTransportAdapter::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetTransportAdapter")
                            .SetParent<AiTransportEndpoint>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetTransportAdapter>();
    return tid;
}

UetTransportAdapter::UetTransportAdapter() = default;
UetTransportAdapter::~UetTransportAdapter() = default;

AiTransportProtocol
UetTransportAdapter::GetProtocol() const
{
    return AiTransportProtocol::UEC;
}

AiTransportCapabilities
UetTransportAdapter::GetCapabilities() const
{
    AiTransportCapabilities capabilities;
    capabilities.reliableUnordered = true;
    capabilities.reliableOrdered = true;
    capabilities.unreliableUnordered = true;
    capabilities.selectiveAcknowledgment = true;
    capabilities.packetSpraying = false;
    capabilities.perPathCongestionControl = false;
    capabilities.endpointTrimming = true;
    capabilities.jobScheduling = true;
    return capabilities;
}

bool
UetTransportAdapter::Initialize(Ptr<Node> node, const AiTransportEndpointConfig& config)
{
    if (!node || m_endpoint || config.endpointId == 0 || config.lineRateBps == 0)
    {
        return false;
    }
    m_node = node;
    m_config = config;
    m_endpoint = CreateObject<UetEndpoint>();
    m_endpoint->SetAttribute("EndpointId", UintegerValue(config.endpointId));
    m_endpoint->SetAttribute("PayloadMtu", UintegerValue(config.payloadMtuBytes));
    m_endpoint->SetAttribute("MaxRetransmissions", UintegerValue(config.maxRetransmissions));
    m_endpoint->SetAttribute("NsccLineRateBps", UintegerValue(config.lineRateBps));
    m_endpoint->SetAttribute("NsccInitialWindow", UintegerValue(config.initialWindowBytes));
    m_endpoint->SetAttribute("NsccMaximumWindow", UintegerValue(config.maximumWindowBytes));
    m_endpoint->SetAttribute("NsccBaseRtt", TimeValue(config.baseRtt));
    m_endpoint->SetAttribute("NsccTargetQueueDelay", TimeValue(config.targetQueueDelay));
    if (config.workConservingScheduler)
    {
        m_endpoint->ConfigureJobScheduler(config.lineRateBps);
    }

    m_transport = CreateObject<UetUdpTransport>();
    if (!m_transport->Bind(node, m_endpoint))
    {
        m_transport = nullptr;
        m_endpoint = nullptr;
        m_node = nullptr;
        return false;
    }

    m_endpoint->TraceConnectWithoutContext(
        "PacketTx",
        MakeCallback(&UetTransportAdapter::ForwardPacketTx, this));
    m_endpoint->TraceConnectWithoutContext(
        "PacketRx",
        MakeCallback(&UetTransportAdapter::ForwardPacketRx, this));
    m_endpoint->TraceConnectWithoutContext(
        "Retransmission",
        MakeCallback(&UetTransportAdapter::ForwardRetransmission, this));
    m_endpoint->TraceConnectWithoutContext(
        "Timeout",
        MakeCallback(&UetTransportAdapter::ForwardTimeout, this));
    m_endpoint->TraceConnectWithoutContext("Nack",
                                           MakeCallback(&UetTransportAdapter::ForwardNack, this));
    m_endpoint->TraceConnectWithoutContext(
        "CongestionWindow",
        MakeCallback(&UetTransportAdapter::ForwardCongestionWindow, this));
    m_endpoint->TraceConnectWithoutContext("EcnReceived",
                                           MakeCallback(&UetTransportAdapter::ForwardEcn, this));
    m_endpoint->TraceConnectWithoutContext(
        "PacketTrimmed",
        MakeCallback(&UetTransportAdapter::ForwardTrimmed, this));
    m_endpoint->TraceConnectWithoutContext(
        "PathSelected",
        MakeCallback(&UetTransportAdapter::ForwardPathSelected, this));
    m_endpoint->TraceConnectWithoutContext(
        "ReorderDepth",
        MakeCallback(&UetTransportAdapter::ForwardReorderDepth, this));
    m_endpoint->TraceConnectWithoutContext(
        "MessageComplete",
        MakeCallback(&UetTransportAdapter::ForwardMessageComplete, this));
    return true;
}

bool
UetTransportAdapter::AddPeer(uint32_t endpointId, const Address& address)
{
    if (!m_transport || endpointId == 0)
    {
        return false;
    }
    if (Ipv4Address::IsMatchingType(address))
    {
        m_transport->AddPeer(endpointId, Ipv4Address::ConvertFrom(address));
        return true;
    }
    if (Ipv6Address::IsMatchingType(address))
    {
        m_transport->AddPeer(endpointId, Ipv6Address::ConvertFrom(address));
        return true;
    }
    return false;
}

uint32_t
UetTransportAdapter::OpenConnection(const AiTransportConnectionConfig& config)
{
    if (!m_endpoint || config.remoteEndpointId == 0 ||
        config.reliability == AiTransportReliability::UNRELIABLE_UNORDERED)
    {
        return 0;
    }
    const UetDeliveryMode mode =
        config.reliability == AiTransportReliability::RELIABLE_ORDERED ? UetDeliveryMode::ROD
                                                                       : UetDeliveryMode::RUD;
    auto pdc = m_endpoint->CreatePdc(mode, config.initialWindowBytes, config.lineRateBps);
    if (!pdc || !m_endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::OPENING))
    {
        return 0;
    }
    pdc->SetAttribute("RetransmissionTimeout", TimeValue(config.retransmissionTimeout));
    return pdc->GetPdcId();
}

bool
UetTransportAdapter::Submit(const AiTransportRequest& request)
{
    if (!m_endpoint || !request.payload || request.remoteEndpointId == 0)
    {
        return false;
    }
    if (request.operation != AiTransportOperation::MESSAGE &&
        request.operation != AiTransportOperation::SEND)
    {
        return false;
    }
    Ptr<Packet> payload = request.payload->Copy();
    switch (request.reliability)
    {
    case AiTransportReliability::RELIABLE_UNORDERED:
        return m_endpoint->SendRudMessage(request.remoteEndpointId,
                                          request.connectionId,
                                          request.messageId,
                                          payload);
    case AiTransportReliability::RELIABLE_ORDERED:
        return m_endpoint->SendRodMessage(request.remoteEndpointId,
                                          request.connectionId,
                                          request.messageId,
                                          payload);
    case AiTransportReliability::UNRELIABLE_UNORDERED:
        return m_endpoint->SendUudDatagram(request.remoteEndpointId, payload);
    }
    return false;
}

uint32_t
UetTransportAdapter::GetCongestionWindow(uint32_t connectionId) const
{
    return m_endpoint ? m_endpoint->GetCongestionWindow(connectionId) : 0;
}

bool
UetTransportAdapter::SetCongestionWindow(uint32_t connectionId, uint32_t bytes)
{
    return m_endpoint && m_endpoint->SetPdcCongestionWindow(connectionId, bytes);
}

bool
UetTransportAdapter::SetConnectionRate(uint32_t connectionId, uint64_t rateBps)
{
    return m_endpoint && m_endpoint->SetPdcLineRate(connectionId, rateBps);
}

bool
UetTransportAdapter::ConfigureJobScheduler(uint64_t lineRateBps)
{
    if (!m_endpoint || lineRateBps == 0)
    {
        return false;
    }
    m_endpoint->ConfigureJobScheduler(lineRateBps);
    return true;
}

bool
UetTransportAdapter::AssignConnectionToJob(uint32_t connectionId,
                                            uint32_t jobId,
                                            uint32_t weight)
{
    return m_endpoint && m_endpoint->AssignPdcToJob(connectionId, jobId, weight);
}

AiTransportCounters
UetTransportAdapter::GetCounters() const
{
    AiTransportCounters counters;
    if (m_transport)
    {
        counters.transmittedDatagrams = m_transport->GetTransmittedDatagrams();
        counters.receivedDatagrams = m_transport->GetReceivedDatagrams();
        counters.mtuDrops = m_transport->GetMtuDrops();
        counters.integrityDrops = m_transport->GetCrcDrops();
    }
    return counters;
}

Ptr<UetEndpoint>
UetTransportAdapter::GetUetEndpoint() const
{
    return m_endpoint;
}

void UetTransportAdapter::ForwardPacketTx(Ptr<const Packet> p, uint32_t c, uint32_t path)
{
    NotifyPacketTx(p, c, path);
}
void UetTransportAdapter::ForwardPacketRx(Ptr<const Packet> p, uint32_t c, uint32_t path)
{
    NotifyPacketRx(p, c, path);
    if (!p)
    {
        return;
    }
    UetSimulationTag route;
    if (!p->PeekPacketTag(route))
    {
        return;
    }
    auto copy = p->Copy();
    UetPdsHeader pds;
    if (copy->RemoveHeader(pds) == 0 ||
        (pds.GetType() != UetPdsType::RUD_REQUEST &&
         pds.GetType() != UetPdsType::ROD_REQUEST))
    {
        return;
    }
    UetSesStandardHeader ses;
    if (copy->RemoveHeader(ses) != 0)
    {
        NotifyPayloadRx(route.GetSourceEndpointId(), ses.GetPayloadLength());
    }
}
void UetTransportAdapter::ForwardRetransmission(uint32_t c, uint32_t s)
{
    NotifyRetransmission(c, s);
}
void UetTransportAdapter::ForwardTimeout(uint32_t c, uint32_t s)
{
    NotifyTimeout(c, s);
}
void UetTransportAdapter::ForwardNack(uint32_t c, uint32_t s)
{
    NotifyNack(c, s);
}
void UetTransportAdapter::ForwardCongestionWindow(uint32_t c, uint32_t oldValue, uint32_t newValue)
{
    NotifyCongestionWindow(c, oldValue, newValue);
}
void UetTransportAdapter::ForwardEcn(uint32_t c, uint32_t path)
{
    NotifyEcnReceived(c, path);
}
void UetTransportAdapter::ForwardTrimmed(Ptr<const Packet> p, uint32_t c, uint32_t path)
{
    NotifyPacketTrimmed(p, c, path);
}
void UetTransportAdapter::ForwardPathSelected(uint32_t c, uint32_t s, uint32_t path)
{
    NotifyPathSelected(c, s, path);
}
void UetTransportAdapter::ForwardReorderDepth(uint32_t c, uint32_t oldValue, uint32_t newValue)
{
    NotifyReorderDepth(c, oldValue, newValue);
}
void UetTransportAdapter::ForwardMessageComplete(uint32_t c,
                                                 uint64_t id,
                                                 uint32_t bytes,
                                                 Time latency)
{
    NotifyMessageComplete(c, id, bytes, latency);
}

void
UetTransportAdapter::DoDispose()
{
    if (m_transport)
    {
        m_transport->Dispose();
        m_transport = nullptr;
    }
    if (m_endpoint)
    {
        m_endpoint->Dispose();
        m_endpoint = nullptr;
    }
    m_node = nullptr;
    AiTransportEndpoint::DoDispose();
}

} // namespace ns3
