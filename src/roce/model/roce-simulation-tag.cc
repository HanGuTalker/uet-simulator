/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "roce-simulation-tag.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(RoceSimulationTag);

TypeId
RoceSimulationTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoceSimulationTag")
                            .SetParent<Tag>()
                            .SetGroupName("Roce")
                            .AddConstructor<RoceSimulationTag>();
    return tid;
}

TypeId
RoceSimulationTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RoceSimulationTag::GetSerializedSize() const
{
    return 40;
}

void
RoceSimulationTag::Serialize(TagBuffer b) const
{
    b.WriteU32(m_sourceEndpointId);
    b.WriteU32(m_destinationEndpointId);
    b.WriteU32(m_connectionId);
    b.WriteU64(m_messageId);
    b.WriteU32(m_payloadBytes);
    b.WriteU32(m_totalMessageBytes);
    b.WriteU64(m_submittedTimeNs);
    b.WriteU32(m_pathId);
}

void
RoceSimulationTag::Deserialize(TagBuffer b)
{
    m_sourceEndpointId = b.ReadU32();
    m_destinationEndpointId = b.ReadU32();
    m_connectionId = b.ReadU32();
    m_messageId = b.ReadU64();
    m_payloadBytes = b.ReadU32();
    m_totalMessageBytes = b.ReadU32();
    m_submittedTimeNs = b.ReadU64();
    m_pathId = b.ReadU32();
}

void
RoceSimulationTag::Print(std::ostream& os) const
{
    os << "src=" << m_sourceEndpointId << " dst=" << m_destinationEndpointId
       << " qp=" << m_connectionId << " message=" << m_messageId << " payload=" << m_payloadBytes
       << " total=" << m_totalMessageBytes << " submitted_ns=" << m_submittedTimeNs
       << " path=" << m_pathId;
}

#define ROCE_TAG_ACCESSOR(Name, Type, Member)                                                      \
    void RoceSimulationTag::Set##Name(Type value)                                                  \
    {                                                                                              \
        Member = value;                                                                            \
    }                                                                                              \
    Type RoceSimulationTag::Get##Name() const                                                      \
    {                                                                                              \
        return Member;                                                                             \
    }

ROCE_TAG_ACCESSOR(SourceEndpointId, uint32_t, m_sourceEndpointId)
ROCE_TAG_ACCESSOR(DestinationEndpointId, uint32_t, m_destinationEndpointId)
ROCE_TAG_ACCESSOR(ConnectionId, uint32_t, m_connectionId)
ROCE_TAG_ACCESSOR(MessageId, uint64_t, m_messageId)
ROCE_TAG_ACCESSOR(PayloadBytes, uint32_t, m_payloadBytes)
ROCE_TAG_ACCESSOR(TotalMessageBytes, uint32_t, m_totalMessageBytes)
ROCE_TAG_ACCESSOR(SubmittedTimeNs, uint64_t, m_submittedTimeNs)
ROCE_TAG_ACCESSOR(PathId, uint32_t, m_pathId)

#undef ROCE_TAG_ACCESSOR

} // namespace ns3
