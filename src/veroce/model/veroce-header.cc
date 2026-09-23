/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "veroce-header.h"

#include "ns3/assert.h"

#include <iomanip>

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(VeRoceMsnHeader);
NS_OBJECT_ENSURE_REGISTERED(VeRocePacketOffsetHeader);
NS_OBJECT_ENSURE_REGISTERED(VeRoceRqHeader);
NS_OBJECT_ENSURE_REGISTERED(VeRoceSackHeader);
NS_OBJECT_ENSURE_REGISTERED(VeRoceRttHeader);

#define VEROCE_SIMPLE_HEADER_IMPL(Class, TypeName, Setter, Getter, Member)                         \
    TypeId Class::GetTypeId()                                                                      \
    {                                                                                              \
        static TypeId tid =                                                                        \
            TypeId(TypeName).SetParent<Header>().SetGroupName("VeRoce").AddConstructor<Class>();   \
        return tid;                                                                                \
    }                                                                                              \
    TypeId Class::GetInstanceTypeId() const                                                        \
    {                                                                                              \
        return GetTypeId();                                                                        \
    }                                                                                              \
    uint32_t Class::GetSerializedSize() const                                                      \
    {                                                                                              \
        return SERIALIZED_SIZE;                                                                    \
    }                                                                                              \
    void Class::Serialize(Buffer::Iterator i) const                                                \
    {                                                                                              \
        NS_ASSERT_MSG(Member <= 0xffffff, "veRoCE 24-bit field overflow");                         \
        i.WriteU8(0);                                                                              \
        i.WriteU8((Member >> 16) & 0xff);                                                          \
        i.WriteU8((Member >> 8) & 0xff);                                                           \
        i.WriteU8(Member & 0xff);                                                                  \
    }                                                                                              \
    uint32_t Class::Deserialize(Buffer::Iterator i)                                                \
    {                                                                                              \
        i.ReadU8();                                                                                \
        Member = (static_cast<uint32_t>(i.ReadU8()) << 16) |                                       \
                 (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();                            \
        return SERIALIZED_SIZE;                                                                    \
    }                                                                                              \
    void Class::Print(std::ostream& os) const                                                      \
    {                                                                                              \
        os << #Getter << '=' << Member;                                                            \
    }                                                                                              \
    void Class::Setter(uint32_t value)                                                             \
    {                                                                                              \
        Member = value;                                                                            \
    }                                                                                              \
    uint32_t Class::Getter() const                                                                 \
    {                                                                                              \
        return Member;                                                                             \
    }

VEROCE_SIMPLE_HEADER_IMPL(VeRoceMsnHeader,
                          "ns3::VeRoceMsnHeader",
                          SetMessageSequence,
                          GetMessageSequence,
                          m_messageSequence)
VEROCE_SIMPLE_HEADER_IMPL(VeRocePacketOffsetHeader,
                          "ns3::VeRocePacketOffsetHeader",
                          SetPacketOrder,
                          GetPacketOrder,
                          m_packetOrder)
VEROCE_SIMPLE_HEADER_IMPL(VeRoceRqHeader,
                          "ns3::VeRoceRqHeader",
                          SetReceiveQueueSequence,
                          GetReceiveQueueSequence,
                          m_receiveQueueSequence)

#undef VEROCE_SIMPLE_HEADER_IMPL

TypeId
VeRoceSackHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::VeRoceSackHeader")
                            .SetParent<Header>()
                            .SetGroupName("VeRoce")
                            .AddConstructor<VeRoceSackHeader>();
    return tid;
}

TypeId
VeRoceSackHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
VeRoceSackHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
VeRoceSackHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_bitmapStartingPsn <= 0xffffff, "SACK starting PSN exceeds 24 bits");
    NS_ASSERT_MSG(m_bitmapValidLength <= MAX_BITMAP_BITS, "SACK bitmap length exceeds 128 bits");
    i.WriteU8(m_bitmapValidLength);
    i.WriteU8((m_bitmapStartingPsn >> 16) & 0xff);
    i.WriteU8((m_bitmapStartingPsn >> 8) & 0xff);
    i.WriteU8(m_bitmapStartingPsn & 0xff);
    for (uint32_t word : m_bitmap)
    {
        i.WriteHtonU32(word);
    }
}

uint32_t
VeRoceSackHeader::Deserialize(Buffer::Iterator i)
{
    m_bitmapValidLength = i.ReadU8();
    m_bitmapStartingPsn = (static_cast<uint32_t>(i.ReadU8()) << 16) |
                          (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();
    for (uint32_t& word : m_bitmap)
    {
        word = i.ReadNtohU32();
    }
    return SERIALIZED_SIZE;
}

void
VeRoceSackHeader::Print(std::ostream& os) const
{
    os << "start_psn=" << m_bitmapStartingPsn << " valid=" << +m_bitmapValidLength << " bitmap=0x"
       << std::hex;
    for (uint32_t word : m_bitmap)
    {
        os << std::setw(8) << std::setfill('0') << word;
    }
    os << std::dec;
}

void
VeRoceSackHeader::SetBitmapStartingPsn(uint32_t value)
{
    m_bitmapStartingPsn = value;
}

uint32_t
VeRoceSackHeader::GetBitmapStartingPsn() const
{
    return m_bitmapStartingPsn;
}

void
VeRoceSackHeader::SetBitmapValidLength(uint8_t value)
{
    m_bitmapValidLength = value;
}

uint8_t
VeRoceSackHeader::GetBitmapValidLength() const
{
    return m_bitmapValidLength;
}

void
VeRoceSackHeader::SetReceived(uint8_t offset, bool value)
{
    NS_ASSERT_MSG(offset < MAX_BITMAP_BITS, "SACK bit offset exceeds 127");
    const uint32_t word = 3 - offset / 32;
    const uint32_t mask = 1u << (offset % 32);
    m_bitmap[word] = value ? (m_bitmap[word] | mask) : (m_bitmap[word] & ~mask);
}

bool
VeRoceSackHeader::IsReceived(uint8_t offset) const
{
    if (offset >= MAX_BITMAP_BITS)
    {
        return false;
    }
    return (m_bitmap[3 - offset / 32] & (1u << (offset % 32))) != 0;
}

TypeId
VeRoceRttHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::VeRoceRttHeader")
                            .SetParent<Header>()
                            .SetGroupName("VeRoce")
                            .AddConstructor<VeRoceRttHeader>();
    return tid;
}

TypeId
VeRoceRttHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
VeRoceRttHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
VeRoceRttHeader::Serialize(Buffer::Iterator i) const
{
    i.WriteHtonU32(m_contextId);
    for (uint32_t timestamp : m_timestamps)
    {
        i.WriteHtonU32(timestamp);
    }
}

uint32_t
VeRoceRttHeader::Deserialize(Buffer::Iterator i)
{
    m_contextId = i.ReadNtohU32();
    for (uint32_t& timestamp : m_timestamps)
    {
        timestamp = i.ReadNtohU32();
    }
    return SERIALIZED_SIZE;
}

void
VeRoceRttHeader::Print(std::ostream& os) const
{
    os << "cc_context=" << m_contextId << " timestamps=" << m_timestamps[0] << ','
       << m_timestamps[1] << ',' << m_timestamps[2] << ',' << m_timestamps[3];
}

void
VeRoceRttHeader::SetContextId(uint32_t value)
{
    m_contextId = value;
}

uint32_t
VeRoceRttHeader::GetContextId() const
{
    return m_contextId;
}

void
VeRoceRttHeader::SetTimestamp(uint32_t index, uint32_t value)
{
    NS_ASSERT_MSG(index < m_timestamps.size(), "veRoCE RTT timestamp index out of range");
    m_timestamps[index] = value;
}

uint32_t
VeRoceRttHeader::GetTimestamp(uint32_t index) const
{
    NS_ASSERT_MSG(index < m_timestamps.size(), "veRoCE RTT timestamp index out of range");
    return m_timestamps[index];
}

} // namespace ns3
