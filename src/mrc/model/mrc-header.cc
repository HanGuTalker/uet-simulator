/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "mrc-header.h"

#include "ns3/assert.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(MrcMethHeader);
NS_OBJECT_ENSURE_REGISTERED(MrcTimestampHeader);

TypeId
MrcMethHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::MrcMethHeader")
                            .SetParent<Header>()
                            .SetGroupName("Mrc")
                            .AddConstructor<MrcMethHeader>();
    return tid;
}

TypeId
MrcMethHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
MrcMethHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
MrcMethHeader::Serialize(Buffer::Iterator i) const
{
    i.WriteHtonU16(m_receiveQueueMessageSequence);
    i.WriteHtonU16(m_messageSequence);
}

uint32_t
MrcMethHeader::Deserialize(Buffer::Iterator i)
{
    m_receiveQueueMessageSequence = i.ReadNtohU16();
    m_messageSequence = i.ReadNtohU16();
    return SERIALIZED_SIZE;
}

void
MrcMethHeader::Print(std::ostream& os) const
{
    os << "rqmsn=" << m_receiveQueueMessageSequence << " msn=" << m_messageSequence;
}

void
MrcMethHeader::SetReceiveQueueMessageSequence(uint16_t value)
{
    m_receiveQueueMessageSequence = value;
}

uint16_t
MrcMethHeader::GetReceiveQueueMessageSequence() const
{
    return m_receiveQueueMessageSequence;
}

void
MrcMethHeader::SetMessageSequence(uint16_t value)
{
    m_messageSequence = value;
}

uint16_t
MrcMethHeader::GetMessageSequence() const
{
    return m_messageSequence;
}

TypeId
MrcTimestampHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::MrcTimestampHeader")
                            .SetParent<Header>()
                            .SetGroupName("Mrc")
                            .AddConstructor<MrcTimestampHeader>();
    return tid;
}

TypeId
MrcTimestampHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
MrcTimestampHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
MrcTimestampHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_formatType <= 0xf, "MRC TSETH format type exceeds four bits");
    i.WriteHtonU16(m_timestamp);
    i.WriteHtonU16((m_implementationDefinedResolution ? 0x8000 : 0) | m_formatType);
}

uint32_t
MrcTimestampHeader::Deserialize(Buffer::Iterator i)
{
    m_timestamp = i.ReadNtohU16();
    const uint16_t flags = i.ReadNtohU16();
    m_implementationDefinedResolution = (flags & 0x8000) != 0;
    m_formatType = flags & 0xf;
    return SERIALIZED_SIZE;
}

void
MrcTimestampHeader::Print(std::ostream& os) const
{
    os << "timestamp=" << m_timestamp << " tsr=" << m_implementationDefinedResolution
       << " ftype=" << +m_formatType;
}

void
MrcTimestampHeader::SetTimestamp(uint16_t value)
{
    m_timestamp = value;
}

uint16_t
MrcTimestampHeader::GetTimestamp() const
{
    return m_timestamp;
}

void
MrcTimestampHeader::SetImplementationDefinedResolution(bool value)
{
    m_implementationDefinedResolution = value;
}

bool
MrcTimestampHeader::HasImplementationDefinedResolution() const
{
    return m_implementationDefinedResolution;
}

void
MrcTimestampHeader::SetFormatType(uint8_t value)
{
    m_formatType = value;
}

uint8_t
MrcTimestampHeader::GetFormatType() const
{
    return m_formatType;
}

bool
IsMrcWriteOpcode(MrcOpcode opcode)
{
    return static_cast<uint8_t>(opcode) >= static_cast<uint8_t>(MrcOpcode::WRITE_FIRST) &&
           static_cast<uint8_t>(opcode) <= static_cast<uint8_t>(MrcOpcode::WRITE_ONLY_IMMEDIATE);
}

bool
IsMrcFirstOpcode(MrcOpcode opcode)
{
    return opcode == MrcOpcode::WRITE_FIRST || opcode == MrcOpcode::WRITE_ONLY ||
           opcode == MrcOpcode::WRITE_ONLY_IMMEDIATE;
}

bool
IsMrcLastOpcode(MrcOpcode opcode)
{
    return opcode == MrcOpcode::WRITE_LAST || opcode == MrcOpcode::WRITE_LAST_IMMEDIATE ||
           opcode == MrcOpcode::WRITE_ONLY || opcode == MrcOpcode::WRITE_ONLY_IMMEDIATE;
}

} // namespace ns3
