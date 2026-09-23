/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ai-transport-endpoint.h"

#include "ns3/trace-source-accessor.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(AiTransportEndpoint);

TypeId
AiTransportEndpoint::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AiTransportEndpoint")
            .SetParent<Object>()
            .SetGroupName("AiTransport")
            .AddTraceSource("PacketTx",
                            "Packet submitted to the protocol wire binding.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_packetTxTrace),
                            "ns3::AiTransportEndpoint::PacketPathTracedCallback")
            .AddTraceSource("PacketRx",
                            "Packet accepted from the protocol wire binding.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_packetRxTrace),
                            "ns3::AiTransportEndpoint::PacketPathTracedCallback")
            .AddTraceSource("PayloadRx",
                            "Application payload bytes accepted from one source endpoint.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_payloadRxTrace),
                            "ns3::AiTransportEndpoint::PayloadTracedCallback")
            .AddTraceSource("Retransmission",
                            "Protocol retransmission event.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_retransmissionTrace),
                            "ns3::AiTransportEndpoint::ConnectionSequenceTracedCallback")
            .AddTraceSource("Timeout",
                            "Protocol timeout event.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_timeoutTrace),
                            "ns3::AiTransportEndpoint::ConnectionSequenceTracedCallback")
            .AddTraceSource("Nack",
                            "Negative acknowledgement or equivalent recovery signal.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_nackTrace),
                            "ns3::AiTransportEndpoint::ConnectionSequenceTracedCallback")
            .AddTraceSource("CongestionWindow",
                            "Protocol sending-window change in bytes.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_congestionWindowTrace),
                            "ns3::AiTransportEndpoint::ConnectionTripleTracedCallback")
            .AddTraceSource("EcnReceived",
                            "ECN or equivalent explicit congestion signal.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_ecnReceivedTrace),
                            "ns3::AiTransportEndpoint::ConnectionSequenceTracedCallback")
            .AddTraceSource("PacketTrimmed",
                            "Payload trimming event when supported by the protocol.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_packetTrimmedTrace),
                            "ns3::AiTransportEndpoint::PacketPathTracedCallback")
            .AddTraceSource("PathSelected",
                            "Path selected for a protocol packet.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_pathSelectedTrace),
                            "ns3::AiTransportEndpoint::ConnectionTripleTracedCallback")
            .AddTraceSource("ReorderDepth",
                            "Receive reorder-depth change.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_reorderDepthTrace),
                            "ns3::AiTransportEndpoint::ConnectionTripleTracedCallback")
            .AddTraceSource("MessageComplete",
                            "Workload message completion.",
                            MakeTraceSourceAccessor(&AiTransportEndpoint::m_messageCompleteTrace),
                            "ns3::AiTransportEndpoint::MessageCompleteTracedCallback");
    return tid;
}

AiTransportEndpoint::~AiTransportEndpoint() = default;

void AiTransportEndpoint::NotifyPacketTx(Ptr<const Packet> p, uint32_t c, uint32_t path)
{
    m_packetTxTrace(p, c, path);
}
void AiTransportEndpoint::NotifyPacketRx(Ptr<const Packet> p, uint32_t c, uint32_t path)
{
    m_packetRxTrace(p, c, path);
}
void AiTransportEndpoint::NotifyPayloadRx(uint32_t source, uint32_t bytes)
{
    m_payloadRxTrace(source, bytes);
}
void AiTransportEndpoint::NotifyRetransmission(uint32_t c, uint32_t s)
{
    m_retransmissionTrace(c, s);
}
void AiTransportEndpoint::NotifyTimeout(uint32_t c, uint32_t s)
{
    m_timeoutTrace(c, s);
}
void AiTransportEndpoint::NotifyNack(uint32_t c, uint32_t s)
{
    m_nackTrace(c, s);
}
void AiTransportEndpoint::NotifyCongestionWindow(uint32_t c, uint32_t oldValue, uint32_t newValue)
{
    m_congestionWindowTrace(c, oldValue, newValue);
}
void AiTransportEndpoint::NotifyEcnReceived(uint32_t c, uint32_t path)
{
    m_ecnReceivedTrace(c, path);
}
void AiTransportEndpoint::NotifyPacketTrimmed(Ptr<const Packet> p, uint32_t c, uint32_t path)
{
    m_packetTrimmedTrace(p, c, path);
}
void AiTransportEndpoint::NotifyPathSelected(uint32_t c, uint32_t s, uint32_t path)
{
    m_pathSelectedTrace(c, s, path);
}
void AiTransportEndpoint::NotifyReorderDepth(uint32_t c, uint32_t oldValue, uint32_t newValue)
{
    m_reorderDepthTrace(c, oldValue, newValue);
}
void
AiTransportEndpoint::NotifyMessageComplete(uint32_t c, uint64_t id, uint32_t bytes, Time latency)
{
    m_messageCompleteTrace(c, id, bytes, latency);
}

} // namespace ns3
