/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UetHeader");
NS_OBJECT_ENSURE_REGISTERED(UetHeader);

TypeId
UetHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetHeader")
                            .SetParent<Header>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetHeader>();
    return tid;
}

TypeId
UetHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UetHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
UetHeader::Serialize(Buffer::Iterator i) const
{
    i.WriteU8(m_version);
    i.WriteU8(static_cast<uint8_t>(m_packetType));
    i.WriteU8(static_cast<uint8_t>(m_deliveryMode));
    i.WriteU8(m_flags);
    i.WriteHtonU32(m_sourceEndpointId);
    i.WriteHtonU32(m_destinationEndpointId);
    i.WriteHtonU32(m_pdcId);
    i.WriteHtonU32(m_sequenceNumber);
    i.WriteHtonU32(m_acknowledgedSequenceNumber);
    i.WriteHtonU64(m_messageId);
    i.WriteHtonU32(m_payloadLength);
    i.WriteHtonU32(m_fragmentOffset);
    i.WriteHtonU16(m_pathId);
    i.WriteHtonU16(m_reserved);
}

uint32_t
UetHeader::Deserialize(Buffer::Iterator i)
{
    m_version = i.ReadU8();
    m_packetType = static_cast<UetPacketType>(i.ReadU8());
    m_deliveryMode = static_cast<UetDeliveryMode>(i.ReadU8());
    m_flags = i.ReadU8();
    m_sourceEndpointId = i.ReadNtohU32();
    m_destinationEndpointId = i.ReadNtohU32();
    m_pdcId = i.ReadNtohU32();
    m_sequenceNumber = i.ReadNtohU32();
    m_acknowledgedSequenceNumber = i.ReadNtohU32();
    m_messageId = i.ReadNtohU64();
    m_payloadLength = i.ReadNtohU32();
    m_fragmentOffset = i.ReadNtohU32();
    m_pathId = i.ReadNtohU16();
    m_reserved = i.ReadNtohU16();
    return SERIALIZED_SIZE;
}

void
UetHeader::Print(std::ostream& os) const
{
    os << "version=" << +m_version << " type=" << +static_cast<uint8_t>(m_packetType)
       << " mode=" << +static_cast<uint8_t>(m_deliveryMode) << " flags=" << +m_flags
       << " src=" << m_sourceEndpointId << " dst=" << m_destinationEndpointId << " pdc=" << m_pdcId
       << " psn=" << m_sequenceNumber << " ackPsn=" << m_acknowledgedSequenceNumber
       << " message=" << m_messageId << " payload=" << m_payloadLength
       << " offset=" << m_fragmentOffset << " path=" << m_pathId;
}

bool
UetHeader::IsValid() const
{
    return m_version == VERSION && static_cast<uint8_t>(m_packetType) <= 3 &&
           static_cast<uint8_t>(m_deliveryMode) <= 2 && (m_flags & ~KNOWN_FLAGS) == 0 &&
           m_reserved == 0;
}

void
UetHeader::SetPacketType(UetPacketType packetType)
{
    m_packetType = packetType;
}

UetPacketType
UetHeader::GetPacketType() const
{
    return m_packetType;
}

void
UetHeader::SetDeliveryMode(UetDeliveryMode deliveryMode)
{
    m_deliveryMode = deliveryMode;
}

UetDeliveryMode
UetHeader::GetDeliveryMode() const
{
    return m_deliveryMode;
}

void
UetHeader::SetFlag(UetHeaderFlag flag, bool enabled)
{
    const auto bit = static_cast<uint8_t>(flag);
    m_flags = enabled ? (m_flags | bit) : (m_flags & ~bit);
}

bool
UetHeader::HasFlag(UetHeaderFlag flag) const
{
    return (m_flags & static_cast<uint8_t>(flag)) != 0;
}

#define UET_DEFINE_ACCESSORS(Name, Type, Member)                                                   \
    void UetHeader::Set##Name(Type value)                                                          \
    {                                                                                              \
        Member = value;                                                                            \
    }                                                                                              \
    Type UetHeader::Get##Name() const                                                              \
    {                                                                                              \
        return Member;                                                                             \
    }

UET_DEFINE_ACCESSORS(SourceEndpointId, uint32_t, m_sourceEndpointId)
UET_DEFINE_ACCESSORS(DestinationEndpointId, uint32_t, m_destinationEndpointId)
UET_DEFINE_ACCESSORS(PdcId, uint32_t, m_pdcId)
UET_DEFINE_ACCESSORS(SequenceNumber, uint32_t, m_sequenceNumber)
UET_DEFINE_ACCESSORS(AcknowledgedSequenceNumber, uint32_t, m_acknowledgedSequenceNumber)
UET_DEFINE_ACCESSORS(MessageId, uint64_t, m_messageId)
UET_DEFINE_ACCESSORS(PayloadLength, uint32_t, m_payloadLength)
UET_DEFINE_ACCESSORS(FragmentOffset, uint32_t, m_fragmentOffset)
UET_DEFINE_ACCESSORS(PathId, uint16_t, m_pathId)

#undef UET_DEFINE_ACCESSORS

} // namespace ns3
