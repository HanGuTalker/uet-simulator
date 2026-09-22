/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-simulation-tag.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(UetSimulationTag);

TypeId
UetSimulationTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetSimulationTag")
                            .SetParent<Tag>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetSimulationTag>();
    return tid;
}

TypeId
UetSimulationTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UetSimulationTag::GetSerializedSize() const
{
    return 24;
}

void
UetSimulationTag::Serialize(TagBuffer buffer) const
{
    buffer.WriteU32(m_sourceEndpointId);
    buffer.WriteU32(m_destinationEndpointId);
    buffer.WriteU32(m_pathId);
    buffer.WriteU32(m_trimmed ? 1 : 0);
    buffer.WriteU32(m_originalPayloadLength);
    buffer.WriteU32(m_ecnMarked ? 1 : 0);
}

void
UetSimulationTag::Deserialize(TagBuffer buffer)
{
    m_sourceEndpointId = buffer.ReadU32();
    m_destinationEndpointId = buffer.ReadU32();
    m_pathId = buffer.ReadU32();
    m_trimmed = buffer.ReadU32() != 0;
    m_originalPayloadLength = buffer.ReadU32();
    m_ecnMarked = buffer.ReadU32() != 0;
}

void
UetSimulationTag::Print(std::ostream& os) const
{
    os << "src=" << m_sourceEndpointId << " dst=" << m_destinationEndpointId
       << " path=" << m_pathId << " trimmed=" << m_trimmed
       << " originalPayload=" << m_originalPayloadLength;
    os << " ecn=" << m_ecnMarked;
}

#define UET_TAG_ACCESSOR(Name, Member)                                                             \
    void UetSimulationTag::Set##Name(uint32_t value)                                               \
    {                                                                                              \
        Member = value;                                                                            \
    }                                                                                              \
    uint32_t UetSimulationTag::Get##Name() const                                                   \
    {                                                                                              \
        return Member;                                                                             \
    }

UET_TAG_ACCESSOR(SourceEndpointId, m_sourceEndpointId)
UET_TAG_ACCESSOR(DestinationEndpointId, m_destinationEndpointId)
UET_TAG_ACCESSOR(PathId, m_pathId)

#undef UET_TAG_ACCESSOR

void
UetSimulationTag::SetTrimmed(bool value)
{
    m_trimmed = value;
}

bool
UetSimulationTag::IsTrimmed() const
{
    return m_trimmed;
}

void
UetSimulationTag::SetOriginalPayloadLength(uint32_t value)
{
    m_originalPayloadLength = value;
}

uint32_t
UetSimulationTag::GetOriginalPayloadLength() const
{
    return m_originalPayloadLength;
}

void
UetSimulationTag::SetEcnMarked(bool value)
{
    m_ecnMarked = value;
}

bool
UetSimulationTag::IsEcnMarked() const
{
    return m_ecnMarked;
}

} // namespace ns3
