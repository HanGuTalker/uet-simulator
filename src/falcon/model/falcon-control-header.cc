/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "falcon-control-header.h"

#include "ns3/assert.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(FalconBackHeader);
NS_OBJECT_ENSURE_REGISTERED(FalconEackHeader);
NS_OBJECT_ENSURE_REGISTERED(FalconNackHeader);

#define FALCON_CONTROL_TYPEID(Class)                                                              \
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

FALCON_CONTROL_TYPEID(FalconBackHeader)
FALCON_CONTROL_TYPEID(FalconEackHeader)
FALCON_CONTROL_TYPEID(FalconNackHeader)

void
FalconBackHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_version <= 0xf, "Falcon BACK version exceeds four bits");
    NS_ASSERT_MSG(m_connectionId <= 0xffffff, "Falcon BACK CID exceeds 24 bits");
    NS_ASSERT_MSG(m_packetType == FalconPacketType::BACK || m_packetType == FalconPacketType::EACK,
                  "Falcon ACK packet type must be BACK or EACK");
    i.WriteHtonU32((static_cast<uint32_t>(m_version) << 28) | m_connectionId);
    i.WriteHtonU32(static_cast<uint32_t>(m_packetType) << 1);
    i.WriteHtonU32(m_receiverDataWindowBase);
    i.WriteHtonU32(m_receiverRequestWindowBase);
    i.WriteHtonU32(m_timestamp1);
    i.WriteHtonU32(m_timestamp2);
    i.WriteHtonU64(m_congestionMetadata);
}

uint32_t
FalconBackHeader::Deserialize(Buffer::Iterator i)
{
    const uint32_t versionAndCid = i.ReadNtohU32();
    m_version = versionAndCid >> 28;
    m_reservedVersion = (versionAndCid >> 24) & 0xf;
    m_connectionId = versionAndCid & 0xffffff;
    const uint32_t type = i.ReadNtohU32();
    m_reservedType = type >> 5;
    m_packetType = static_cast<FalconPacketType>((type >> 1) & 0xf);
    m_reservedR = (type & 1) != 0;
    m_receiverDataWindowBase = i.ReadNtohU32();
    m_receiverRequestWindowBase = i.ReadNtohU32();
    m_timestamp1 = i.ReadNtohU32();
    m_timestamp2 = i.ReadNtohU32();
    m_congestionMetadata = i.ReadNtohU64();
    return SERIALIZED_SIZE;
}

void
FalconBackHeader::Print(std::ostream& os) const
{
    os << "version=" << +m_version << " cid=" << m_connectionId
       << " type=" << +static_cast<uint8_t>(m_packetType)
       << " dbpsn=" << m_receiverDataWindowBase << " rbpsn=" << m_receiverRequestWindowBase
       << " t1=" << m_timestamp1 << " t2=" << m_timestamp2;
}

void FalconBackHeader::SetVersion(uint8_t value) { m_version = value; }
uint8_t FalconBackHeader::GetVersion() const { return m_version; }
void FalconBackHeader::SetConnectionId(uint32_t value) { m_connectionId = value; }
uint32_t FalconBackHeader::GetConnectionId() const { return m_connectionId; }
void FalconBackHeader::SetPacketType(FalconPacketType value) { m_packetType = value; }
FalconPacketType FalconBackHeader::GetPacketType() const { return m_packetType; }
void FalconBackHeader::SetReceiverDataWindowBase(uint32_t value)
{
    m_receiverDataWindowBase = value;
}
uint32_t FalconBackHeader::GetReceiverDataWindowBase() const
{
    return m_receiverDataWindowBase;
}
void FalconBackHeader::SetReceiverRequestWindowBase(uint32_t value)
{
    m_receiverRequestWindowBase = value;
}
uint32_t FalconBackHeader::GetReceiverRequestWindowBase() const
{
    return m_receiverRequestWindowBase;
}
void FalconBackHeader::SetTimestamp1(uint32_t value) { m_timestamp1 = value; }
uint32_t FalconBackHeader::GetTimestamp1() const { return m_timestamp1; }
void FalconBackHeader::SetTimestamp2(uint32_t value) { m_timestamp2 = value; }
uint32_t FalconBackHeader::GetTimestamp2() const { return m_timestamp2; }
void FalconBackHeader::SetCongestionMetadata(uint64_t value) { m_congestionMetadata = value; }
uint64_t FalconBackHeader::GetCongestionMetadata() const { return m_congestionMetadata; }
bool
FalconBackHeader::HasValidReservedFields() const
{
    return m_reservedVersion == 0 && m_reservedType == 0 && !m_reservedR;
}

void
FalconEackHeader::Serialize(Buffer::Iterator i) const
{
    FalconBackHeader back = m_back;
    back.SetPacketType(FalconPacketType::EACK);
    back.Serialize(i);
    i.Next(FalconBackHeader::SERIALIZED_SIZE);
    i.WriteHtonU64(m_dataAckBitmap[0]);
    i.WriteHtonU64(m_dataAckBitmap[1]);
    i.WriteHtonU64(m_dataReceivedBitmap[0]);
    i.WriteHtonU64(m_dataReceivedBitmap[1]);
    i.WriteHtonU64(m_requestBitmap);
}

uint32_t
FalconEackHeader::Deserialize(Buffer::Iterator i)
{
    m_back.Deserialize(i);
    i.Next(FalconBackHeader::SERIALIZED_SIZE);
    m_dataAckBitmap[0] = i.ReadNtohU64();
    m_dataAckBitmap[1] = i.ReadNtohU64();
    m_dataReceivedBitmap[0] = i.ReadNtohU64();
    m_dataReceivedBitmap[1] = i.ReadNtohU64();
    m_requestBitmap = i.ReadNtohU64();
    return SERIALIZED_SIZE;
}

void
FalconEackHeader::Print(std::ostream& os) const
{
    os << "back={";
    m_back.Print(os);
    os << "} request-bitmap=0x" << std::hex << m_requestBitmap << std::dec;
}

void
FalconEackHeader::SetBack(const FalconBackHeader& value)
{
    m_back = value;
    m_back.SetPacketType(FalconPacketType::EACK);
}
const FalconBackHeader& FalconEackHeader::GetBack() const { return m_back; }
void FalconEackHeader::SetDataAckBitmap(const Bitmap128& value) { m_dataAckBitmap = value; }
const FalconEackHeader::Bitmap128& FalconEackHeader::GetDataAckBitmap() const
{
    return m_dataAckBitmap;
}
void
FalconEackHeader::SetDataReceivedBitmap(const Bitmap128& value)
{
    m_dataReceivedBitmap = value;
}
const FalconEackHeader::Bitmap128& FalconEackHeader::GetDataReceivedBitmap() const
{
    return m_dataReceivedBitmap;
}
void FalconEackHeader::SetRequestBitmap(uint64_t value) { m_requestBitmap = value; }
uint64_t FalconEackHeader::GetRequestBitmap() const { return m_requestBitmap; }

void
FalconNackHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(m_version <= 0xf, "Falcon NACK version exceeds four bits");
    NS_ASSERT_MSG(m_connectionId <= 0xffffff, "Falcon NACK CID exceeds 24 bits");
    NS_ASSERT_MSG(m_rnrTimeout <= 0x1f, "Falcon RNR timeout exceeds five bits");
    i.WriteHtonU32((static_cast<uint32_t>(m_version) << 28) | m_connectionId);
    i.WriteHtonU32(static_cast<uint32_t>(FalconPacketType::NACK) << 1);
    i.WriteHtonU32(m_receiverDataWindowBase);
    i.WriteHtonU32(m_receiverRequestWindowBase);
    i.WriteHtonU32(m_nackPacketSequenceNumber);
    i.WriteHtonU32(m_timestamp1);
    i.WriteHtonU32(m_timestamp2);
    i.WriteHtonU64(m_congestionMetadata);
    const uint32_t nack = (static_cast<uint32_t>(m_nackCode) << 24) |
                          (static_cast<uint32_t>(m_rnrTimeout) << 16) |
                          (m_requestWindow ? 0x8000 : 0) | m_ulpNackCode;
    i.WriteHtonU32(nack);
}

uint32_t
FalconNackHeader::Deserialize(Buffer::Iterator i)
{
    const uint32_t versionAndCid = i.ReadNtohU32();
    m_version = versionAndCid >> 28;
    m_reservedVersion = (versionAndCid >> 24) & 0xf;
    m_connectionId = versionAndCid & 0xffffff;
    const uint32_t type = i.ReadNtohU32();
    m_reservedType = type >> 5;
    m_reservedR = (type & 1) != 0;
    m_receiverDataWindowBase = i.ReadNtohU32();
    m_receiverRequestWindowBase = i.ReadNtohU32();
    m_nackPacketSequenceNumber = i.ReadNtohU32();
    m_timestamp1 = i.ReadNtohU32();
    m_timestamp2 = i.ReadNtohU32();
    m_congestionMetadata = i.ReadNtohU64();
    const uint32_t nack = i.ReadNtohU32();
    m_nackCode = static_cast<FalconNackCode>(nack >> 24);
    m_reservedNackHigh = (nack >> 21) & 0x7;
    m_rnrTimeout = (nack >> 16) & 0x1f;
    m_requestWindow = (nack & 0x8000) != 0;
    m_reservedNackLow = (nack >> 8) & 0x7f;
    m_ulpNackCode = nack & 0xff;
    return SERIALIZED_SIZE;
}

void
FalconNackHeader::Print(std::ostream& os) const
{
    os << "version=" << +m_version << " cid=" << m_connectionId
       << " dbpsn=" << m_receiverDataWindowBase << " rbpsn=" << m_receiverRequestWindowBase
       << " nack-psn=" << m_nackPacketSequenceNumber
       << " code=" << +static_cast<uint8_t>(m_nackCode) << " rnr=" << +m_rnrTimeout
       << " request-window=" << m_requestWindow << " ulp-code=" << +m_ulpNackCode;
}

void FalconNackHeader::SetVersion(uint8_t value) { m_version = value; }
uint8_t FalconNackHeader::GetVersion() const { return m_version; }
void FalconNackHeader::SetConnectionId(uint32_t value) { m_connectionId = value; }
uint32_t FalconNackHeader::GetConnectionId() const { return m_connectionId; }
void FalconNackHeader::SetReceiverDataWindowBase(uint32_t value)
{
    m_receiverDataWindowBase = value;
}
uint32_t FalconNackHeader::GetReceiverDataWindowBase() const
{
    return m_receiverDataWindowBase;
}
void FalconNackHeader::SetReceiverRequestWindowBase(uint32_t value)
{
    m_receiverRequestWindowBase = value;
}
uint32_t FalconNackHeader::GetReceiverRequestWindowBase() const
{
    return m_receiverRequestWindowBase;
}
void FalconNackHeader::SetNackPacketSequenceNumber(uint32_t value)
{
    m_nackPacketSequenceNumber = value;
}
uint32_t FalconNackHeader::GetNackPacketSequenceNumber() const
{
    return m_nackPacketSequenceNumber;
}
void FalconNackHeader::SetTimestamp1(uint32_t value) { m_timestamp1 = value; }
uint32_t FalconNackHeader::GetTimestamp1() const { return m_timestamp1; }
void FalconNackHeader::SetTimestamp2(uint32_t value) { m_timestamp2 = value; }
uint32_t FalconNackHeader::GetTimestamp2() const { return m_timestamp2; }
void FalconNackHeader::SetCongestionMetadata(uint64_t value) { m_congestionMetadata = value; }
uint64_t FalconNackHeader::GetCongestionMetadata() const { return m_congestionMetadata; }
void FalconNackHeader::SetNackCode(FalconNackCode value) { m_nackCode = value; }
FalconNackCode FalconNackHeader::GetNackCode() const { return m_nackCode; }
void FalconNackHeader::SetRnrTimeout(uint8_t value) { m_rnrTimeout = value; }
uint8_t FalconNackHeader::GetRnrTimeout() const { return m_rnrTimeout; }
void FalconNackHeader::SetRequestWindow(bool value) { m_requestWindow = value; }
bool FalconNackHeader::IsRequestWindow() const { return m_requestWindow; }
void FalconNackHeader::SetUlpNackCode(uint8_t value) { m_ulpNackCode = value; }
uint8_t FalconNackHeader::GetUlpNackCode() const { return m_ulpNackCode; }
bool
FalconNackHeader::HasValidReservedFields() const
{
    return m_reservedVersion == 0 && m_reservedType == 0 && !m_reservedR &&
           m_reservedNackHigh == 0 && m_reservedNackLow == 0;
}

#undef FALCON_CONTROL_TYPEID

} // namespace ns3
