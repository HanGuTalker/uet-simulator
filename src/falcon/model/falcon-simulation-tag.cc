/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "falcon-simulation-tag.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(FalconSimulationTag);

TypeId
FalconSimulationTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::FalconSimulationTag")
                            .SetParent<Tag>()
                            .SetGroupName("Falcon")
                            .AddConstructor<FalconSimulationTag>();
    return tid;
}

TypeId FalconSimulationTag::GetInstanceTypeId() const { return GetTypeId(); }
uint32_t FalconSimulationTag::GetSerializedSize() const { return 48; }

void
FalconSimulationTag::Serialize(TagBuffer b) const
{
    b.WriteU32(m_sourceEndpointId);
    b.WriteU32(m_destinationEndpointId);
    b.WriteU32(m_connectionId);
    b.WriteU64(m_messageId);
    b.WriteU32(m_payloadBytes);
    b.WriteU32(m_totalMessageBytes);
    b.WriteU64(m_submittedTimeNs);
    b.WriteU32(m_fragment);
    b.WriteU32(m_fragmentCount);
}

void
FalconSimulationTag::Deserialize(TagBuffer b)
{
    m_sourceEndpointId = b.ReadU32();
    m_destinationEndpointId = b.ReadU32();
    m_connectionId = b.ReadU32();
    m_messageId = b.ReadU64();
    m_payloadBytes = b.ReadU32();
    m_totalMessageBytes = b.ReadU32();
    m_submittedTimeNs = b.ReadU64();
    m_fragment = b.ReadU32();
    m_fragmentCount = b.ReadU32();
}

void
FalconSimulationTag::Print(std::ostream& os) const
{
    os << "src=" << m_sourceEndpointId << " dst=" << m_destinationEndpointId
       << " cid=" << m_connectionId << " message=" << m_messageId
       << " payload=" << m_payloadBytes << " fragment=" << m_fragment << "/"
       << m_fragmentCount;
}

#define FALCON_TAG_ACCESSOR(Name, Type, Member)                                                    \
    void FalconSimulationTag::Set##Name(Type value) { Member = value; }                            \
    Type FalconSimulationTag::Get##Name() const { return Member; }

FALCON_TAG_ACCESSOR(SourceEndpointId, uint32_t, m_sourceEndpointId)
FALCON_TAG_ACCESSOR(DestinationEndpointId, uint32_t, m_destinationEndpointId)
FALCON_TAG_ACCESSOR(ConnectionId, uint32_t, m_connectionId)
FALCON_TAG_ACCESSOR(MessageId, uint64_t, m_messageId)
FALCON_TAG_ACCESSOR(PayloadBytes, uint32_t, m_payloadBytes)
FALCON_TAG_ACCESSOR(TotalMessageBytes, uint32_t, m_totalMessageBytes)
FALCON_TAG_ACCESSOR(SubmittedTimeNs, uint64_t, m_submittedTimeNs)
FALCON_TAG_ACCESSOR(Fragment, uint32_t, m_fragment)
FALCON_TAG_ACCESSOR(FragmentCount, uint32_t, m_fragmentCount)

#undef FALCON_TAG_ACCESSOR

} // namespace ns3
