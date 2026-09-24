/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "veroce-trim-queue-disc.h"

#include "ns3/drop-tail-queue.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/object-factory.h"
#include "ns3/queue-size.h"
#include "ns3/roce-simulation-tag.h"
#include "ns3/roce-v2-header.h"
#include "ns3/udp-header.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(VeRoceTrimQueueDisc);

TypeId
VeRoceTrimQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::VeRoceTrimQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("VeRoce")
            .AddConstructor<VeRoceTrimQueueDisc>()
            .AddAttribute("MaxSize",
                          "Hard queue limit in bytes.",
                          QueueSizeValue(QueueSize("2MB")),
                          MakeQueueSizeAccessor(&QueueDisc::SetMaxSize, &QueueDisc::GetMaxSize),
                          MakeQueueSizeChecker())
            .AddAttribute("MarkThresholdBytes",
                          "Queue occupancy at which ECN-capable packets are marked.",
                          UintegerValue(8192),
                          MakeUintegerAccessor(&VeRoceTrimQueueDisc::m_markThresholdBytes),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("TrimThresholdBytes",
                          "Projected queue occupancy at which trimmable packets lose payload.",
                          UintegerValue(16384),
                          MakeUintegerAccessor(&VeRoceTrimQueueDisc::m_trimThresholdBytes),
                          MakeUintegerChecker<uint32_t>(1))
            .AddTraceSource("Trim",
                            "A semantic packet was trimmed; values are original and final bytes.",
                            MakeTraceSourceAccessor(&VeRoceTrimQueueDisc::m_trimTrace),
                            "ns3::VeRoceTrimQueueDisc::TrimTracedCallback");
    return tid;
}

VeRoceTrimQueueDisc::VeRoceTrimQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE)
{
}

VeRoceTrimQueueDisc::~VeRoceTrimQueueDisc() = default;

Ptr<QueueDiscItem>
VeRoceTrimQueueDisc::Trim(Ptr<QueueDiscItem> item)
{
    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem>(item);
    if (!ipv4Item || ipv4Item->GetHeader().GetProtocol() != 17 ||
        (ipv4Item->GetHeader().GetTos() >> 2) != TRIMMABLE_DSCP)
    {
        return item;
    }
    Ptr<Packet> packet = item->GetPacket()->Copy();
    RoceSimulationTag tag;
    if (!packet->PeekPacketTag(tag) || tag.GetPayloadBytes() == 0)
    {
        return item;
    }
    UdpHeader udp;
    if (packet->RemoveHeader(udp) == 0 ||
        packet->GetSize() < tag.GetPayloadBytes() + RoceInvariantCrcTrailer::SERIALIZED_SIZE)
    {
        return item;
    }
    const uint32_t originalSize = item->GetSize();
    RoceInvariantCrcTrailer oldCrc;
    packet->RemoveTrailer(oldCrc);
    packet->RemoveAtEnd(tag.GetPayloadBytes());
    RoceInvariantCrcTrailer newCrc;
    newCrc.SetCrc(RoceInvariantCrcTrailer::Calculate(packet));
    packet->AddTrailer(newCrc);
    udp.ForceChecksum(0);
    udp.ForcePayloadSize(0);
    udp.InitializeChecksum(ipv4Item->GetHeader().GetSource(),
                           ipv4Item->GetHeader().GetDestination(),
                           17);
    packet->AddHeader(udp);

    Ipv4Header header = ipv4Item->GetHeader();
    header.SetTos((TRIMMED_DSCP << 2) | (header.GetTos() & 0x03));
    header.SetPayloadSize(packet->GetSize());
    Ptr<Ipv4QueueDiscItem> trimmed =
        Create<Ipv4QueueDiscItem>(packet, item->GetAddress(), item->GetProtocol(), header);
    trimmed->SetTxQueueIndex(item->GetTxQueueIndex());
    m_trimTrace(packet, originalSize, trimmed->GetSize());
    return trimmed;
}

bool
VeRoceTrimQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    const uint32_t currentBytes = GetCurrentSize().GetValue();
    Ptr<QueueDiscItem> candidate = item;
    if (currentBytes + item->GetSize() > m_trimThresholdBytes)
    {
        candidate = Trim(item);
    }
    if (GetCurrentSize() + candidate > GetMaxSize())
    {
        DropBeforeEnqueue(candidate, LIMIT_EXCEEDED_DROP);
        return false;
    }
    if (currentBytes + candidate->GetSize() > m_markThresholdBytes)
    {
        Mark(candidate, CONGESTION_MARK);
    }
    return GetInternalQueue(0)->Enqueue(candidate);
}

Ptr<QueueDiscItem>
VeRoceTrimQueueDisc::DoDequeue()
{
    return GetInternalQueue(0)->Dequeue();
}

Ptr<const QueueDiscItem>
VeRoceTrimQueueDisc::DoPeek()
{
    return GetInternalQueue(0)->Peek();
}

bool
VeRoceTrimQueueDisc::CheckConfig()
{
    if (GetNQueueDiscClasses() != 0 || GetNPacketFilters() != 0 ||
        GetMaxSize().GetUnit() != QueueSizeUnit::BYTES ||
        m_markThresholdBytes > m_trimThresholdBytes ||
        m_trimThresholdBytes >= GetMaxSize().GetValue())
    {
        return false;
    }
    if (GetNInternalQueues() == 0)
    {
        AddInternalQueue(
            CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>("MaxSize",
                                                                     QueueSizeValue(GetMaxSize())));
    }
    return GetNInternalQueues() == 1;
}

void
VeRoceTrimQueueDisc::InitializeParams()
{
}

} // namespace ns3
