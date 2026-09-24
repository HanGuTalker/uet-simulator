/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "mrc-header.h"

#include "ns3/assert.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(MrcMethHeader);
NS_OBJECT_ENSURE_REGISTERED(MrcImmediateHeader);
NS_OBJECT_ENSURE_REGISTERED(MrcTimestampHeader);
NS_OBJECT_ENSURE_REGISTERED(MrcCcStateHeader);
NS_OBJECT_ENSURE_REGISTERED(MrcSethHeader);
NS_OBJECT_ENSURE_REGISTERED(MrcNethHeader);
NS_OBJECT_ENSURE_REGISTERED(MrcPethHeader);

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
MrcImmediateHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::MrcImmediateHeader")
                            .SetParent<Header>()
                            .SetGroupName("Mrc")
                            .AddConstructor<MrcImmediateHeader>();
    return tid;
}

TypeId
MrcImmediateHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
MrcImmediateHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
MrcImmediateHeader::Serialize(Buffer::Iterator i) const
{
    i.WriteHtonU32(m_immediateData);
}

uint32_t
MrcImmediateHeader::Deserialize(Buffer::Iterator i)
{
    m_immediateData = i.ReadNtohU32();
    return SERIALIZED_SIZE;
}

void
MrcImmediateHeader::Print(std::ostream& os) const
{
    os << "immediate=0x" << std::hex << m_immediateData << std::dec;
}

void
MrcImmediateHeader::SetImmediateData(uint32_t value)
{
    m_immediateData = value;
}

uint32_t
MrcImmediateHeader::GetImmediateData() const
{
    return m_immediateData;
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

#define MRC_HEADER_TYPEID(Class)                                                                   \
    TypeId Class::GetTypeId()                                                                      \
    {                                                                                              \
        static TypeId tid = TypeId("ns3::" #Class)                                                 \
                                .SetParent<Header>()                                               \
                                .SetGroupName("Mrc")                                               \
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

MRC_HEADER_TYPEID(MrcCcStateHeader)
MRC_HEADER_TYPEID(MrcSethHeader)
MRC_HEADER_TYPEID(MrcNethHeader)
MRC_HEADER_TYPEID(MrcPethHeader)

void
MrcCcStateHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_outOfOrderCount <= 0x7fff, "MRC OOO count exceeds 15 bits");
    NS_ASSERT_MSG(m_receiverWindowPenalty <= 0x7f, "MRC receiver penalty exceeds 7 bits");
    NS_ASSERT_MSG(m_receivedBytesUnits <= 0xffffff, "MRC received byte count exceeds 24 bits");
    i.WriteHtonU16(m_timestamp);
    i.WriteHtonU16(m_outOfOrderCount);
    i.WriteU8((m_restoreWindow ? 0x80 : 0) | m_receiverWindowPenalty);
    i.WriteU8((m_receivedBytesUnits >> 16) & 0xff);
    i.WriteU8((m_receivedBytesUnits >> 8) & 0xff);
    i.WriteU8(m_receivedBytesUnits & 0xff);
}

uint32_t
MrcCcStateHeader::Deserialize(Buffer::Iterator i)
{
    m_timestamp = i.ReadNtohU16();
    m_outOfOrderCount = i.ReadNtohU16() & 0x7fff;
    const uint8_t flow = i.ReadU8();
    m_restoreWindow = (flow & 0x80) != 0;
    m_receiverWindowPenalty = flow & 0x7f;
    m_receivedBytesUnits = (static_cast<uint32_t>(i.ReadU8()) << 16) |
                           (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();
    return SERIALIZED_SIZE;
}

void
MrcCcStateHeader::Print(std::ostream& os) const
{
    os << "timestamp=" << m_timestamp << " ooo=" << m_outOfOrderCount
       << " restore=" << m_restoreWindow << " penalty=" << +m_receiverWindowPenalty
       << " received256=" << m_receivedBytesUnits;
}

void
MrcCcStateHeader::SetTimestamp(uint16_t v)
{
    m_timestamp = v;
}

uint16_t
MrcCcStateHeader::GetTimestamp() const
{
    return m_timestamp;
}

void
MrcCcStateHeader::SetOutOfOrderCount(uint16_t v)
{
    m_outOfOrderCount = v;
}

uint16_t
MrcCcStateHeader::GetOutOfOrderCount() const
{
    return m_outOfOrderCount;
}

void
MrcCcStateHeader::SetRestoreWindow(bool v)
{
    m_restoreWindow = v;
}

bool
MrcCcStateHeader::GetRestoreWindow() const
{
    return m_restoreWindow;
}

void
MrcCcStateHeader::SetReceiverWindowPenalty(uint8_t v)
{
    m_receiverWindowPenalty = v;
}

uint8_t
MrcCcStateHeader::GetReceiverWindowPenalty() const
{
    return m_receiverWindowPenalty;
}

void
MrcCcStateHeader::SetReceivedBytesUnits(uint32_t v)
{
    m_receivedBytesUnits = v;
}

uint32_t
MrcCcStateHeader::GetReceivedBytesUnits() const
{
    return m_receivedBytesUnits;
}

void
MrcSethHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_congestionMark <= 2, "MRC congestion mark is reserved");
    NS_ASSERT_MSG(m_cumulativeAck <= 0xffffff, "MRC cumulative ACK exceeds 24 bits");
    const uint16_t flags =
        (static_cast<uint16_t>(m_congestionMark) << 5) | (m_probeResponse ? 0x0002 : 0);
    i.WriteHtonU16(flags);
    i.WriteHtonU16(static_cast<uint16_t>(m_acknowledgedPsnOffset));
    i.WriteHtonU32(m_entropy);
    i.WriteHtonU16(m_sourcePdcId);
    i.WriteHtonU16(m_destinationPdcId);
    i.WriteU8(0);
    i.WriteU8((m_cumulativeAck >> 16) & 0xff);
    i.WriteU8((m_cumulativeAck >> 8) & 0xff);
    i.WriteU8(m_cumulativeAck & 0xff);
    i.WriteU8((m_ccType << 4) | (m_ccFlags & 0xf));
    i.WriteU8(m_maximumPsnRange);
    i.WriteHtonU16(static_cast<uint16_t>(m_sackOffset));
    i.WriteHtonU64(m_bitmap);
}

uint32_t
MrcSethHeader::Deserialize(Buffer::Iterator i)
{
    const uint16_t flags = i.ReadNtohU16();
    m_congestionMark = (flags >> 5) & 0x3;
    m_probeResponse = (flags & 0x0002) != 0;
    m_acknowledgedPsnOffset = static_cast<int16_t>(i.ReadNtohU16());
    m_entropy = i.ReadNtohU32();
    m_sourcePdcId = i.ReadNtohU16();
    m_destinationPdcId = i.ReadNtohU16();
    i.ReadU8();
    m_cumulativeAck = (static_cast<uint32_t>(i.ReadU8()) << 16) |
                      (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();
    const uint8_t cc = i.ReadU8();
    m_ccType = cc >> 4;
    m_ccFlags = cc & 0xf;
    m_maximumPsnRange = i.ReadU8();
    m_sackOffset = static_cast<int16_t>(i.ReadNtohU16());
    m_bitmap = i.ReadNtohU64();
    return SERIALIZED_SIZE;
}

void
MrcSethHeader::Print(std::ostream& os) const
{
    os << "cack=" << m_cumulativeAck << " ackoff=" << m_acknowledgedPsnOffset
       << " sackoff=" << m_sackOffset << " bitmap=0x" << std::hex << m_bitmap << std::dec
       << " m=" << +m_congestionMark << " probe=" << m_probeResponse;
}

void
MrcSethHeader::SetCongestionMark(uint8_t v)
{
    m_congestionMark = v;
}

uint8_t
MrcSethHeader::GetCongestionMark() const
{
    return m_congestionMark;
}

void
MrcSethHeader::SetProbeResponse(bool v)
{
    m_probeResponse = v;
}

bool
MrcSethHeader::IsProbeResponse() const
{
    return m_probeResponse;
}

void
MrcSethHeader::SetAcknowledgedPsnOffset(int16_t v)
{
    m_acknowledgedPsnOffset = v;
}

int16_t
MrcSethHeader::GetAcknowledgedPsnOffset() const
{
    return m_acknowledgedPsnOffset;
}

void
MrcSethHeader::SetEntropy(uint32_t v)
{
    m_entropy = v;
}

uint32_t
MrcSethHeader::GetEntropy() const
{
    return m_entropy;
}

void
MrcSethHeader::SetSourcePdcId(uint16_t v)
{
    m_sourcePdcId = v;
}

uint16_t
MrcSethHeader::GetSourcePdcId() const
{
    return m_sourcePdcId;
}

void
MrcSethHeader::SetDestinationPdcId(uint16_t v)
{
    m_destinationPdcId = v;
}

uint16_t
MrcSethHeader::GetDestinationPdcId() const
{
    return m_destinationPdcId;
}

void
MrcSethHeader::SetCumulativeAck(uint32_t v)
{
    m_cumulativeAck = v;
}

uint32_t
MrcSethHeader::GetCumulativeAck() const
{
    return m_cumulativeAck;
}

void
MrcSethHeader::SetCcType(uint8_t v)
{
    m_ccType = v;
}

uint8_t
MrcSethHeader::GetCcType() const
{
    return m_ccType;
}

void
MrcSethHeader::SetMaximumPsnRange(uint8_t v)
{
    m_maximumPsnRange = v;
}

uint8_t
MrcSethHeader::GetMaximumPsnRange() const
{
    return m_maximumPsnRange;
}

void
MrcSethHeader::SetSackOffset(int16_t v)
{
    m_sackOffset = v;
}

int16_t
MrcSethHeader::GetSackOffset() const
{
    return m_sackOffset;
}

void
MrcSethHeader::SetBitmap(uint64_t v)
{
    m_bitmap = v;
}

uint64_t
MrcSethHeader::GetBitmap() const
{
    return m_bitmap;
}

bool
MrcSethHeader::IsReceived(uint32_t o) const
{
    return o < BITMAP_BITS && (m_bitmap & (1ULL << o));
}

void
MrcNethHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_nackPsn <= 0xffffff, "MRC NACK PSN exceeds 24 bits");
    i.WriteHtonU16(0);
    i.WriteU8(static_cast<uint8_t>(m_reason));
    i.WriteU8(m_vendorInfo);
    i.WriteHtonU32(m_entropy);
    i.WriteHtonU16(m_sourcePdcId);
    i.WriteHtonU16(m_destinationPdcId);
    i.WriteU8(0);
    i.WriteU8((m_nackPsn >> 16) & 0xff);
    i.WriteU8((m_nackPsn >> 8) & 0xff);
    i.WriteU8(m_nackPsn & 0xff);
    i.WriteU8(m_ccType << 4);
    i.WriteU8(0);
    i.WriteHtonU16(m_timestamp);
}

uint32_t
MrcNethHeader::Deserialize(Buffer::Iterator i)
{
    i.ReadNtohU16();
    m_reason = static_cast<MrcNackReason>(i.ReadU8());
    m_vendorInfo = i.ReadU8();
    m_entropy = i.ReadNtohU32();
    m_sourcePdcId = i.ReadNtohU16();
    m_destinationPdcId = i.ReadNtohU16();
    i.ReadU8();
    m_nackPsn = (static_cast<uint32_t>(i.ReadU8()) << 16) |
                (static_cast<uint32_t>(i.ReadU8()) << 8) | i.ReadU8();
    m_ccType = i.ReadU8() >> 4;
    i.ReadU8();
    m_timestamp = i.ReadNtohU16();
    return SERIALIZED_SIZE;
}

void
MrcNethHeader::Print(std::ostream& os) const
{
    os << "reason=0x" << std::hex << +static_cast<uint8_t>(m_reason) << std::dec
       << " psn=" << m_nackPsn << " entropy=" << m_entropy << " timestamp=" << m_timestamp;
}

void
MrcNethHeader::SetReason(MrcNackReason v)
{
    m_reason = v;
}

MrcNackReason
MrcNethHeader::GetReason() const
{
    return m_reason;
}

void
MrcNethHeader::SetEntropy(uint32_t v)
{
    m_entropy = v;
}

uint32_t
MrcNethHeader::GetEntropy() const
{
    return m_entropy;
}

void
MrcNethHeader::SetSourcePdcId(uint16_t v)
{
    m_sourcePdcId = v;
}

void
MrcNethHeader::SetDestinationPdcId(uint16_t v)
{
    m_destinationPdcId = v;
}

void
MrcNethHeader::SetNackPsn(uint32_t v)
{
    m_nackPsn = v;
}

uint32_t
MrcNethHeader::GetNackPsn() const
{
    return m_nackPsn;
}

void
MrcNethHeader::SetTimestamp(uint16_t v)
{
    m_timestamp = v;
}

uint16_t
MrcNethHeader::GetTimestamp() const
{
    return m_timestamp;
}

void
MrcPethHeader::Serialize(Buffer::Iterator i) const
{
    i.WriteHtonU16(0);
    i.WriteU8(0);
    i.WriteU8(m_vendorInfo);
    i.WriteHtonU16(m_probeId);
    i.WriteHtonU16(0);
    i.WriteHtonU16(m_sourcePdcId);
    i.WriteHtonU16(m_destinationPdcId);
    i.WriteHtonU16(m_timestamp);
    i.WriteHtonU16(0x0001);
}

uint32_t
MrcPethHeader::Deserialize(Buffer::Iterator i)
{
    i.ReadNtohU16();
    i.ReadU8();
    m_vendorInfo = i.ReadU8();
    m_probeId = i.ReadNtohU16();
    i.ReadNtohU16();
    m_sourcePdcId = i.ReadNtohU16();
    m_destinationPdcId = i.ReadNtohU16();
    m_timestamp = i.ReadNtohU16();
    i.ReadNtohU16();
    return SERIALIZED_SIZE;
}

void
MrcPethHeader::Print(std::ostream& os) const
{
    os << "probe=" << m_probeId << " timestamp=" << m_timestamp;
}

void
MrcPethHeader::SetProbeId(uint16_t v)
{
    m_probeId = v;
}

uint16_t
MrcPethHeader::GetProbeId() const
{
    return m_probeId;
}

void
MrcPethHeader::SetSourcePdcId(uint16_t v)
{
    m_sourcePdcId = v;
}

void
MrcPethHeader::SetDestinationPdcId(uint16_t v)
{
    m_destinationPdcId = v;
}

void
MrcPethHeader::SetTimestamp(uint16_t v)
{
    m_timestamp = v;
}

uint16_t
MrcPethHeader::GetTimestamp() const
{
    return m_timestamp;
}

#undef MRC_HEADER_TYPEID

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
