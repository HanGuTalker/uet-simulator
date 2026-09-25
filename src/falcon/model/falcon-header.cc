/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "falcon-header.h"

#include "ns3/assert.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(FalconBaseHeader);
NS_OBJECT_ENSURE_REGISTERED(FalconPushDataHeader);
NS_OBJECT_ENSURE_REGISTERED(FalconPullRequestHeader);

#define FALCON_HEADER_TYPEID(Class)                                                               \
    TypeId Class::GetTypeId()                                                                      \
    {                                                                                              \
        static TypeId tid = TypeId("ns3::" #Class)                                                \
                                .SetParent<Header>()                                               \
                                .SetGroupName("Falcon")                                           \
                                .AddConstructor<Class>();                                          \
        return tid;                                                                                \
    }                                                                                              \
    TypeId Class::GetInstanceTypeId() const                                                        \
    {                                                                                              \
        return GetTypeId();                                                                        \
    }                                                                                              \
    uint32_t Class::GetSerializedSize() const                                                      \
    {                                                                                              \
        return SERIALIZED_SIZE;                                                                    \
    }

FALCON_HEADER_TYPEID(FalconBaseHeader)
FALCON_HEADER_TYPEID(FalconPushDataHeader)
FALCON_HEADER_TYPEID(FalconPullRequestHeader)

void
FalconBaseHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_version <= 0xf, "Falcon version exceeds four bits");
    NS_ASSERT_MSG(m_destinationConnectionId <= 0xffffff,
                  "Falcon destination CID exceeds 24 bits");
    NS_ASSERT_MSG(m_destinationFunction <= 0xffffff,
                  "Falcon destination function exceeds 24 bits");
    NS_ASSERT_MSG(static_cast<uint8_t>(m_protocolType) <= 0x7,
                  "Falcon protocol type exceeds three bits");
    NS_ASSERT_MSG(static_cast<uint8_t>(m_packetType) <= 0xf,
                  "Falcon packet type exceeds four bits");

    i.WriteHtonU32((static_cast<uint32_t>(m_version) << 28) | m_destinationConnectionId);
    i.WriteU8((m_destinationFunction >> 16) & 0xff);
    i.WriteU8((m_destinationFunction >> 8) & 0xff);
    i.WriteU8(m_destinationFunction & 0xff);
    i.WriteU8((static_cast<uint8_t>(m_protocolType) << 5) |
              (static_cast<uint8_t>(m_packetType) << 1) | (m_ackRequest ? 1 : 0));
    i.WriteHtonU32(m_receiverDataWindowBase);
    i.WriteHtonU32(m_receiverRequestWindowBase);
    i.WriteHtonU32(m_packetSequenceNumber);
    i.WriteHtonU32(m_requestSequenceNumber);
}

uint32_t
FalconBaseHeader::Deserialize(Buffer::Iterator i)
{
    const uint32_t connection = i.ReadNtohU32();
    m_version = connection >> 28;
    m_reservedVersion = (connection >> 24) & 0xf;
    m_destinationConnectionId = connection & 0xffffff;
    m_destinationFunction = (static_cast<uint32_t>(i.ReadU8()) << 16) |
                            (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();
    const uint8_t type = i.ReadU8();
    m_protocolType = static_cast<FalconProtocolType>((type >> 5) & 0x7);
    m_packetType = static_cast<FalconPacketType>((type >> 1) & 0xf);
    m_ackRequest = (type & 1) != 0;
    m_receiverDataWindowBase = i.ReadNtohU32();
    m_receiverRequestWindowBase = i.ReadNtohU32();
    m_packetSequenceNumber = i.ReadNtohU32();
    m_requestSequenceNumber = i.ReadNtohU32();
    return SERIALIZED_SIZE;
}

void
FalconBaseHeader::Print(std::ostream& os) const
{
    os << "version=" << +m_version << " dcid=" << m_destinationConnectionId
       << " function=" << m_destinationFunction << " protocol="
       << +static_cast<uint8_t>(m_protocolType) << " type="
       << +static_cast<uint8_t>(m_packetType) << " ar=" << m_ackRequest
       << " dbpsn=" << m_receiverDataWindowBase << " rbpsn=" << m_receiverRequestWindowBase
       << " psn=" << m_packetSequenceNumber << " rsn=" << m_requestSequenceNumber;
}

void
FalconBaseHeader::SetVersion(uint8_t value)
{
    m_version = value;
}

uint8_t
FalconBaseHeader::GetVersion() const
{
    return m_version;
}

bool
FalconBaseHeader::HasValidReservedField() const
{
    return m_reservedVersion == 0;
}

void
FalconBaseHeader::SetDestinationConnectionId(uint32_t value)
{
    m_destinationConnectionId = value;
}

uint32_t
FalconBaseHeader::GetDestinationConnectionId() const
{
    return m_destinationConnectionId;
}

void
FalconBaseHeader::SetDestinationFunction(uint32_t value)
{
    m_destinationFunction = value;
}

uint32_t
FalconBaseHeader::GetDestinationFunction() const
{
    return m_destinationFunction;
}

void
FalconBaseHeader::SetProtocolType(FalconProtocolType value)
{
    m_protocolType = value;
}

FalconProtocolType
FalconBaseHeader::GetProtocolType() const
{
    return m_protocolType;
}

void
FalconBaseHeader::SetPacketType(FalconPacketType value)
{
    m_packetType = value;
}

FalconPacketType
FalconBaseHeader::GetPacketType() const
{
    return m_packetType;
}

void
FalconBaseHeader::SetAckRequest(bool value)
{
    m_ackRequest = value;
}

bool
FalconBaseHeader::GetAckRequest() const
{
    return m_ackRequest;
}

void
FalconBaseHeader::SetReceiverDataWindowBase(uint32_t value)
{
    m_receiverDataWindowBase = value;
}

uint32_t
FalconBaseHeader::GetReceiverDataWindowBase() const
{
    return m_receiverDataWindowBase;
}

void
FalconBaseHeader::SetReceiverRequestWindowBase(uint32_t value)
{
    m_receiverRequestWindowBase = value;
}

uint32_t
FalconBaseHeader::GetReceiverRequestWindowBase() const
{
    return m_receiverRequestWindowBase;
}

void
FalconBaseHeader::SetPacketSequenceNumber(uint32_t value)
{
    m_packetSequenceNumber = value;
}

uint32_t
FalconBaseHeader::GetPacketSequenceNumber() const
{
    return m_packetSequenceNumber;
}

void
FalconBaseHeader::SetRequestSequenceNumber(uint32_t value)
{
    m_requestSequenceNumber = value;
}

uint32_t
FalconBaseHeader::GetRequestSequenceNumber() const
{
    return m_requestSequenceNumber;
}

void
FalconPushDataHeader::Serialize(Buffer::Iterator i) const
{
    i.WriteHtonU16(0);
    i.WriteHtonU16(m_requestLength);
}

uint32_t
FalconPushDataHeader::Deserialize(Buffer::Iterator i)
{
    m_reserved = i.ReadNtohU16();
    m_requestLength = i.ReadNtohU16();
    return SERIALIZED_SIZE;
}

void
FalconPushDataHeader::Print(std::ostream& os) const
{
    os << "reserved=" << m_reserved << " request-length=" << m_requestLength;
}

void
FalconPushDataHeader::SetRequestLength(uint16_t value)
{
    m_requestLength = value;
}

uint16_t
FalconPushDataHeader::GetRequestLength() const
{
    return m_requestLength;
}

bool
FalconPushDataHeader::HasValidReservedField() const
{
    return m_reserved == 0;
}

void
FalconPullRequestHeader::Serialize(Buffer::Iterator i) const
{
    i.WriteHtonU16(0);
    i.WriteHtonU16(m_requestLength);
    i.WriteHtonU32(0);
}

uint32_t
FalconPullRequestHeader::Deserialize(Buffer::Iterator i)
{
    m_reservedPrefix = i.ReadNtohU16();
    m_requestLength = i.ReadNtohU16();
    m_reservedSuffix = i.ReadNtohU32();
    return SERIALIZED_SIZE;
}

void
FalconPullRequestHeader::Print(std::ostream& os) const
{
    os << "reserved-prefix=" << m_reservedPrefix << " request-length=" << m_requestLength
       << " reserved-suffix=" << m_reservedSuffix;
}

void
FalconPullRequestHeader::SetRequestLength(uint16_t value)
{
    m_requestLength = value;
}

uint16_t
FalconPullRequestHeader::GetRequestLength() const
{
    return m_requestLength;
}

bool
FalconPullRequestHeader::HasValidReservedField() const
{
    return m_reservedPrefix == 0 && m_reservedSuffix == 0;
}

#undef FALCON_HEADER_TYPEID

} // namespace ns3
