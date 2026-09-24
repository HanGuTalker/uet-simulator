/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "roce-v2-header.h"

#include "ns3/assert.h"

#include <iomanip>
#include <vector>

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(RoceBthHeader);
NS_OBJECT_ENSURE_REGISTERED(RoceRethHeader);
NS_OBJECT_ENSURE_REGISTERED(RoceAethHeader);
NS_OBJECT_ENSURE_REGISTERED(RoceCnpHeader);
NS_OBJECT_ENSURE_REGISTERED(RoceInvariantCrcTrailer);

TypeId
RoceBthHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoceBthHeader")
                            .SetParent<Header>()
                            .SetGroupName("Roce")
                            .AddConstructor<RoceBthHeader>();
    return tid;
}

TypeId
RoceBthHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RoceBthHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
RoceBthHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_padCount <= 3, "BTH pad count exceeds two bits");
    NS_ASSERT_MSG(m_destinationQp <= 0xffffff, "BTH destination QP exceeds 24 bits");
    NS_ASSERT_MSG(m_packetSequence <= 0xffffff, "BTH PSN exceeds 24 bits");
    i.WriteU8(static_cast<uint8_t>(m_opcode));
    i.WriteU8((m_solicited ? 0x80 : 0) | ((m_padCount & 0x3) << 4));
    i.WriteHtonU16(m_partitionKey);
    i.WriteU8(0);
    i.WriteU8((m_destinationQp >> 16) & 0xff);
    i.WriteU8((m_destinationQp >> 8) & 0xff);
    i.WriteU8(m_destinationQp & 0xff);
    i.WriteU8((m_ackRequest ? 0x80 : 0) | (m_retransmission ? 0x40 : 0));
    i.WriteU8((m_packetSequence >> 16) & 0xff);
    i.WriteU8((m_packetSequence >> 8) & 0xff);
    i.WriteU8(m_packetSequence & 0xff);
}

uint32_t
RoceBthHeader::Deserialize(Buffer::Iterator i)
{
    m_opcode = static_cast<RoceOpcode>(i.ReadU8());
    const uint8_t flags = i.ReadU8();
    m_solicited = (flags & 0x80) != 0;
    m_padCount = (flags >> 4) & 0x3;
    m_partitionKey = i.ReadNtohU16();
    i.ReadU8();
    m_destinationQp = (static_cast<uint32_t>(i.ReadU8()) << 16) |
                      (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();
    const uint8_t ackFlags = i.ReadU8();
    m_ackRequest = (ackFlags & 0x80) != 0;
    m_retransmission = (ackFlags & 0x40) != 0;
    m_packetSequence = (static_cast<uint32_t>(i.ReadU8()) << 16) |
                       (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();
    return SERIALIZED_SIZE;
}

void
RoceBthHeader::Print(std::ostream& os) const
{
    os << "opcode=0x" << std::hex << +static_cast<uint8_t>(m_opcode) << std::dec
       << " dqpn=" << m_destinationQp << " psn=" << m_packetSequence << " ack=" << m_ackRequest
       << " retrans=" << m_retransmission;
}

#define ROCE_ACCESSOR(Class, Name, Type, Member)                                                   \
    void Class::Set##Name(Type value)                                                              \
    {                                                                                              \
        Member = value;                                                                            \
    }                                                                                              \
    Type Class::Get##Name() const                                                                  \
    {                                                                                              \
        return Member;                                                                             \
    }

ROCE_ACCESSOR(RoceBthHeader, Opcode, RoceOpcode, m_opcode)
ROCE_ACCESSOR(RoceBthHeader, PadCount, uint8_t, m_padCount)
ROCE_ACCESSOR(RoceBthHeader, PartitionKey, uint16_t, m_partitionKey)
ROCE_ACCESSOR(RoceBthHeader, DestinationQp, uint32_t, m_destinationQp)
ROCE_ACCESSOR(RoceBthHeader, PacketSequence, uint32_t, m_packetSequence)

void
RoceBthHeader::SetSolicited(bool value)
{
    m_solicited = value;
}

bool
RoceBthHeader::IsSolicited() const
{
    return m_solicited;
}

void
RoceBthHeader::SetAckRequest(bool value)
{
    m_ackRequest = value;
}

bool
RoceBthHeader::IsAckRequested() const
{
    return m_ackRequest;
}

void
RoceBthHeader::SetRetransmission(bool value)
{
    m_retransmission = value;
}

bool
RoceBthHeader::IsRetransmission() const
{
    return m_retransmission;
}

TypeId
RoceRethHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoceRethHeader")
                            .SetParent<Header>()
                            .SetGroupName("Roce")
                            .AddConstructor<RoceRethHeader>();
    return tid;
}

TypeId
RoceRethHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RoceRethHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
RoceRethHeader::Serialize(Buffer::Iterator i) const
{
    i.WriteHtonU64(m_virtualAddress);
    i.WriteHtonU32(m_remoteKey);
    i.WriteHtonU32(m_dmaLength);
}

uint32_t
RoceRethHeader::Deserialize(Buffer::Iterator i)
{
    m_virtualAddress = i.ReadNtohU64();
    m_remoteKey = i.ReadNtohU32();
    m_dmaLength = i.ReadNtohU32();
    return SERIALIZED_SIZE;
}

void
RoceRethHeader::Print(std::ostream& os) const
{
    os << "va=0x" << std::hex << m_virtualAddress << " rkey=0x" << m_remoteKey << std::dec
       << " length=" << m_dmaLength;
}

ROCE_ACCESSOR(RoceRethHeader, VirtualAddress, uint64_t, m_virtualAddress)
ROCE_ACCESSOR(RoceRethHeader, RemoteKey, uint32_t, m_remoteKey)
ROCE_ACCESSOR(RoceRethHeader, DmaLength, uint32_t, m_dmaLength)

TypeId
RoceAethHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoceAethHeader")
                            .SetParent<Header>()
                            .SetGroupName("Roce")
                            .AddConstructor<RoceAethHeader>();
    return tid;
}

TypeId
RoceAethHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RoceAethHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
RoceAethHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_messageSequence <= 0xffffff, "AETH MSN exceeds 24 bits");
    i.WriteU8(m_syndrome);
    i.WriteU8((m_messageSequence >> 16) & 0xff);
    i.WriteU8((m_messageSequence >> 8) & 0xff);
    i.WriteU8(m_messageSequence & 0xff);
}

uint32_t
RoceAethHeader::Deserialize(Buffer::Iterator i)
{
    m_syndrome = i.ReadU8();
    m_messageSequence = (static_cast<uint32_t>(i.ReadU8()) << 16) |
                        (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();
    return SERIALIZED_SIZE;
}

void
RoceAethHeader::Print(std::ostream& os) const
{
    os << "syndrome=0x" << std::hex << +m_syndrome << std::dec << " msn=" << m_messageSequence;
}

ROCE_ACCESSOR(RoceAethHeader, Syndrome, uint8_t, m_syndrome)
ROCE_ACCESSOR(RoceAethHeader, MessageSequence, uint32_t, m_messageSequence)

TypeId
RoceCnpHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoceCnpHeader")
                            .SetParent<Header>()
                            .SetGroupName("Roce")
                            .AddConstructor<RoceCnpHeader>();
    return tid;
}

TypeId
RoceCnpHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RoceCnpHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
RoceCnpHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_sourceQp <= 0xffffff, "CNP source QP exceeds 24 bits");
    i.WriteU8(0);
    i.WriteU8((m_sourceQp >> 16) & 0xff);
    i.WriteU8((m_sourceQp >> 8) & 0xff);
    i.WriteU8(m_sourceQp & 0xff);
    for (uint8_t byte = 0; byte < 12; ++byte)
    {
        i.WriteU8(0);
    }
}

uint32_t
RoceCnpHeader::Deserialize(Buffer::Iterator i)
{
    i.ReadU8();
    m_sourceQp = (static_cast<uint32_t>(i.ReadU8()) << 16) |
                 (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();
    for (uint8_t byte = 0; byte < 12; ++byte)
    {
        i.ReadU8();
    }
    return SERIALIZED_SIZE;
}

void
RoceCnpHeader::Print(std::ostream& os) const
{
    os << "source_qp=" << m_sourceQp;
}

ROCE_ACCESSOR(RoceCnpHeader, SourceQp, uint32_t, m_sourceQp)

#undef ROCE_ACCESSOR

TypeId
RoceInvariantCrcTrailer::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoceInvariantCrcTrailer")
                            .SetParent<Trailer>()
                            .SetGroupName("Roce")
                            .AddConstructor<RoceInvariantCrcTrailer>();
    return tid;
}

TypeId
RoceInvariantCrcTrailer::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RoceInvariantCrcTrailer::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
RoceInvariantCrcTrailer::Serialize(Buffer::Iterator end) const
{
    end.Prev(SERIALIZED_SIZE);
    end.WriteHtonU32(m_crc);
}

uint32_t
RoceInvariantCrcTrailer::Deserialize(Buffer::Iterator end)
{
    end.Prev(SERIALIZED_SIZE);
    m_crc = end.ReadNtohU32();
    return SERIALIZED_SIZE;
}

void
RoceInvariantCrcTrailer::Print(std::ostream& os) const
{
    os << "icrc=0x" << std::hex << m_crc << std::dec;
}

void
RoceInvariantCrcTrailer::SetCrc(uint32_t value)
{
    m_crc = value;
}

uint32_t
RoceInvariantCrcTrailer::GetCrc() const
{
    return m_crc;
}

uint32_t
RoceInvariantCrcTrailer::Calculate(Ptr<const Packet> packet)
{
    std::vector<uint8_t> bytes(packet ? packet->GetSize() : 0);
    if (packet && !bytes.empty())
    {
        packet->CopyData(bytes.data(), bytes.size());
    }
    uint32_t crc = 0xffffffffU;
    for (uint8_t byte : bytes)
    {
        crc ^= byte;
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xedb88320U : 0U);
        }
    }
    return ~crc;
}

bool
IsRoceDataOpcode(RoceOpcode opcode)
{
    return opcode == RoceOpcode::RC_SEND_FIRST || opcode == RoceOpcode::RC_SEND_MIDDLE ||
           opcode == RoceOpcode::RC_SEND_LAST || opcode == RoceOpcode::RC_SEND_ONLY ||
           IsRoceWriteOpcode(opcode) || IsRoceReadRequestOpcode(opcode) ||
           IsRoceReadResponseOpcode(opcode);
}

bool
IsRoceFirstOpcode(RoceOpcode opcode)
{
    return opcode == RoceOpcode::RC_SEND_FIRST || opcode == RoceOpcode::RC_SEND_ONLY ||
           opcode == RoceOpcode::RC_WRITE_FIRST || opcode == RoceOpcode::RC_WRITE_ONLY ||
           opcode == RoceOpcode::RC_READ_REQUEST || opcode == RoceOpcode::RC_READ_RESPONSE_FIRST ||
           opcode == RoceOpcode::RC_READ_RESPONSE_ONLY;
}

bool
IsRoceLastOpcode(RoceOpcode opcode)
{
    return opcode == RoceOpcode::RC_SEND_LAST || opcode == RoceOpcode::RC_SEND_ONLY ||
           opcode == RoceOpcode::RC_WRITE_LAST || opcode == RoceOpcode::RC_WRITE_ONLY ||
           opcode == RoceOpcode::RC_READ_REQUEST || opcode == RoceOpcode::RC_READ_RESPONSE_LAST ||
           opcode == RoceOpcode::RC_READ_RESPONSE_ONLY;
}

bool
IsRoceWriteOpcode(RoceOpcode opcode)
{
    return opcode == RoceOpcode::RC_WRITE_FIRST || opcode == RoceOpcode::RC_WRITE_MIDDLE ||
           opcode == RoceOpcode::RC_WRITE_LAST || opcode == RoceOpcode::RC_WRITE_ONLY;
}

bool
IsRoceReadRequestOpcode(RoceOpcode opcode)
{
    return opcode == RoceOpcode::RC_READ_REQUEST;
}

bool
IsRoceReadResponseOpcode(RoceOpcode opcode)
{
    return opcode == RoceOpcode::RC_READ_RESPONSE_FIRST ||
           opcode == RoceOpcode::RC_READ_RESPONSE_MIDDLE ||
           opcode == RoceOpcode::RC_READ_RESPONSE_LAST ||
           opcode == RoceOpcode::RC_READ_RESPONSE_ONLY;
}

} // namespace ns3
