/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-pds-header.h"

#include "ns3/assert.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(UetPdsHeader);
NS_OBJECT_ENSURE_REGISTERED(UetNegotiationOnOffHeader);

TypeId
UetPdsHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetPdsHeader")
                            .SetParent<Header>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetPdsHeader>();
    return tid;
}

TypeId
UetPdsHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UetPdsHeader::GetSerializedSize(UetPdsType type)
{
    switch (type)
    {
    case UetPdsType::RUD_REQUEST:
    case UetPdsType::ROD_REQUEST:
        return REQUEST_SIZE;
    case UetPdsType::ACK:
        return ACK_SIZE;
    case UetPdsType::ACK_CC:
        return ACK_CC_SIZE;
    case UetPdsType::ACK_CCX:
        return 0;
    case UetPdsType::NACK:
        return NACK_SIZE;
    case UetPdsType::CONTROL:
        return CONTROL_SIZE;
    case UetPdsType::UUD_REQUEST:
        return UUD_SIZE;
    }
    return 0;
}

uint32_t
UetPdsHeader::GetSerializedSize() const
{
    return GetSerializedSize(m_type);
}

uint16_t
UetPdsHeader::EncodePrologue() const
{
    const uint8_t subtype = m_type == UetPdsType::CONTROL ? static_cast<uint8_t>(m_controlType)
                                                          : static_cast<uint8_t>(m_nextHeader);
    return (static_cast<uint16_t>(m_type) << 11) | (static_cast<uint16_t>(subtype) << 7) |
           (m_flags & 0x7f);
}

void
UetPdsHeader::DecodePrologue(uint16_t value)
{
    m_type = static_cast<UetPdsType>((value >> 11) & 0x1f);
    const uint8_t subtype = (value >> 7) & 0x0f;
    if (m_type == UetPdsType::CONTROL)
    {
        m_controlType = static_cast<UetControlType>(subtype);
        m_nextHeader = UetNextHeader::NONE;
    }
    else
    {
        m_nextHeader = static_cast<UetNextHeader>(subtype);
    }
    m_flags = value & 0x7f;
}

void
UetPdsHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(IsValid(), "Cannot serialize an invalid UEC PDS header");
    i.WriteHtonU16(EncodePrologue());
    switch (m_type)
    {
    case UetPdsType::RUD_REQUEST:
    case UetPdsType::ROD_REQUEST:
        i.WriteHtonU16(m_clearPsnOffset);
        i.WriteHtonU32(m_psn);
        i.WriteHtonU16(m_sourcePdcId);
        i.WriteHtonU16(m_destinationPdcId);
        break;
    case UetPdsType::ACK:
    case UetPdsType::ACK_CC:
        i.WriteHtonU16(m_ackPsnOffset);
        i.WriteHtonU32(m_cumulativeAckPsn);
        i.WriteHtonU16(m_sourcePdcId);
        i.WriteHtonU16(m_destinationPdcId);
        if (m_type == UetPdsType::ACK_CC)
        {
            i.WriteU8((m_ccType << 4) | m_ccFlags);
            i.WriteU8(m_maximumPsnRange);
            i.WriteHtonU16(m_sackPsnOffset);
            i.WriteHtonU64(m_sackBitmap);
            i.WriteHtonU64(m_ackCcState);
        }
        break;
    case UetPdsType::NACK:
        i.WriteU8(m_nackCode);
        i.WriteU8(m_vendorCode);
        i.WriteHtonU32(m_nackPsn);
        i.WriteHtonU16(m_sourcePdcId);
        i.WriteHtonU16(m_destinationPdcId);
        i.WriteHtonU32(m_nackPayload);
        break;
    case UetPdsType::CONTROL:
        i.WriteHtonU16(m_probeOpaque);
        i.WriteHtonU32(m_psn);
        i.WriteHtonU16(m_sourcePdcId);
        i.WriteHtonU16(m_destinationPdcId);
        break;
    case UetPdsType::UUD_REQUEST:
        i.WriteHtonU16(0);
        break;
    case UetPdsType::ACK_CCX:
        break;
    }
}

uint32_t
UetPdsHeader::Deserialize(Buffer::Iterator i)
{
    DecodePrologue(i.ReadNtohU16());
    switch (m_type)
    {
    case UetPdsType::RUD_REQUEST:
    case UetPdsType::ROD_REQUEST:
        m_clearPsnOffset = i.ReadNtohU16();
        m_psn = i.ReadNtohU32();
        m_sourcePdcId = i.ReadNtohU16();
        m_destinationPdcId = i.ReadNtohU16();
        break;
    case UetPdsType::ACK:
    case UetPdsType::ACK_CC:
        m_ackPsnOffset = i.ReadNtohU16();
        m_cumulativeAckPsn = i.ReadNtohU32();
        m_sourcePdcId = i.ReadNtohU16();
        m_destinationPdcId = i.ReadNtohU16();
        if (m_type == UetPdsType::ACK_CC)
        {
            const uint8_t cc = i.ReadU8();
            m_ccType = cc >> 4;
            m_ccFlags = cc & 0x0f;
            m_maximumPsnRange = i.ReadU8();
            m_sackPsnOffset = i.ReadNtohU16();
            m_sackBitmap = i.ReadNtohU64();
            m_ackCcState = i.ReadNtohU64();
        }
        break;
    case UetPdsType::NACK:
        m_nackCode = i.ReadU8();
        m_vendorCode = i.ReadU8();
        m_nackPsn = i.ReadNtohU32();
        m_sourcePdcId = i.ReadNtohU16();
        m_destinationPdcId = i.ReadNtohU16();
        m_nackPayload = i.ReadNtohU32();
        break;
    case UetPdsType::CONTROL:
        m_probeOpaque = i.ReadNtohU16();
        m_psn = i.ReadNtohU32();
        m_sourcePdcId = i.ReadNtohU16();
        m_destinationPdcId = i.ReadNtohU16();
        break;
    case UetPdsType::UUD_REQUEST:
        i.ReadNtohU16();
        break;
    default:
        return 0;
    }
    return GetSerializedSize();
}

void
UetPdsHeader::Print(std::ostream& os) const
{
    os << "type=" << +static_cast<uint8_t>(m_type)
       << (m_type == UetPdsType::CONTROL ? " ctl=" : " next=")
       << +(m_type == UetPdsType::CONTROL ? static_cast<uint8_t>(m_controlType)
                                          : static_cast<uint8_t>(m_nextHeader))
       << " flags=" << +m_flags;
}

bool
UetPdsHeader::IsValid() const
{
    const uint32_t size = GetSerializedSize(m_type);
    if (size == 0 || static_cast<uint8_t>(m_nextHeader) > 0x0f || m_flags > 0x7f)
    {
        return false;
    }
    switch (m_type)
    {
    case UetPdsType::RUD_REQUEST:
    case UetPdsType::ROD_REQUEST:
        return (m_flags & ~0x1c) == 0;
    case UetPdsType::ACK:
        return (m_flags & ~0x3e) == 0;
    case UetPdsType::ACK_CC:
        return (m_flags & ~0x3e) == 0 && m_ccType == 0 && m_ccFlags == 0;
    case UetPdsType::NACK:
        return m_nextHeader == UetNextHeader::NONE && (m_flags & ~0x38) == 0;
    case UetPdsType::CONTROL:
        return static_cast<uint8_t>(m_controlType) <=
                   static_cast<uint8_t>(UetControlType::NEGOTIATION) &&
               (m_flags & ~0x3c) == 0;
    case UetPdsType::UUD_REQUEST:
        return true;
    case UetPdsType::ACK_CCX:
        return false;
    }
    return false;
}

#define UET_PDS_ACCESSOR(Name, Type, Member)                                                       \
    void UetPdsHeader::Set##Name(Type value)                                                       \
    {                                                                                              \
        Member = value;                                                                            \
    }                                                                                              \
    Type UetPdsHeader::Get##Name() const                                                           \
    {                                                                                              \
        return Member;                                                                             \
    }

UET_PDS_ACCESSOR(Type, UetPdsType, m_type)
UET_PDS_ACCESSOR(NextHeader, UetNextHeader, m_nextHeader)
UET_PDS_ACCESSOR(Flags, uint8_t, m_flags)
UET_PDS_ACCESSOR(ControlType, UetControlType, m_controlType)
UET_PDS_ACCESSOR(ProbeOpaque, uint16_t, m_probeOpaque)
UET_PDS_ACCESSOR(ClearPsnOffset, uint16_t, m_clearPsnOffset)
UET_PDS_ACCESSOR(Psn, uint32_t, m_psn)
UET_PDS_ACCESSOR(SourcePdcId, uint16_t, m_sourcePdcId)
UET_PDS_ACCESSOR(DestinationPdcId, uint16_t, m_destinationPdcId)
UET_PDS_ACCESSOR(AckPsnOffset, uint16_t, m_ackPsnOffset)
UET_PDS_ACCESSOR(CumulativeAckPsn, uint32_t, m_cumulativeAckPsn)
UET_PDS_ACCESSOR(CcType, uint8_t, m_ccType)
UET_PDS_ACCESSOR(CcFlags, uint8_t, m_ccFlags)
UET_PDS_ACCESSOR(MaximumPsnRange, uint8_t, m_maximumPsnRange)
UET_PDS_ACCESSOR(SackPsnOffset, uint16_t, m_sackPsnOffset)
UET_PDS_ACCESSOR(SackBitmap, uint64_t, m_sackBitmap)
UET_PDS_ACCESSOR(AckCcState, uint64_t, m_ackCcState)
UET_PDS_ACCESSOR(NackCode, uint8_t, m_nackCode)
UET_PDS_ACCESSOR(VendorCode, uint8_t, m_vendorCode)
UET_PDS_ACCESSOR(NackPsn, uint32_t, m_nackPsn)
UET_PDS_ACCESSOR(NackPayload, uint32_t, m_nackPayload)

#undef UET_PDS_ACCESSOR

void
UetPdsHeader::SetNsccState(uint16_t serviceTime,
                           bool restoreCwnd,
                           uint8_t receiverCwndPending,
                           uint32_t receivedBytes,
                           uint16_t outOfOrderCount)
{
    NS_ASSERT_MSG(receiverCwndPending <= 0x7f, "NSCC receiver cwnd pending exceeds 7 bits");
    NS_ASSERT_MSG(receivedBytes <= 0xffffff, "NSCC received bytes exceeds 24 bits");
    m_ackCcState = (static_cast<uint64_t>(serviceTime) << 48) |
                   (static_cast<uint64_t>(restoreCwnd) << 47) |
                   (static_cast<uint64_t>(receiverCwndPending) << 40) |
                   (static_cast<uint64_t>(receivedBytes) << 16) | outOfOrderCount;
}

uint16_t
UetPdsHeader::GetNsccServiceTime() const
{
    return m_ackCcState >> 48;
}

bool
UetPdsHeader::GetNsccRestoreCwnd() const
{
    return (m_ackCcState & (1ULL << 47)) != 0;
}

uint8_t
UetPdsHeader::GetNsccReceiverCwndPending() const
{
    return (m_ackCcState >> 40) & 0x7f;
}

uint32_t
UetPdsHeader::GetNsccReceivedBytes() const
{
    return (m_ackCcState >> 16) & 0xffffff;
}

uint16_t
UetPdsHeader::GetNsccOutOfOrderCount() const
{
    return m_ackCcState & 0xffff;
}

TypeId
UetNegotiationOnOffHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetNegotiationOnOffHeader")
                            .SetParent<Header>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetNegotiationOnOffHeader>();
    return tid;
}

TypeId
UetNegotiationOnOffHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UetNegotiationOnOffHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
UetNegotiationOnOffHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(IsValid(), "Cannot serialize an invalid PDS_NEG_ON_OFF payload");
    i.WriteU8(m_type);
    i.WriteU8(m_length);
    i.WriteHtonU16(m_reserved);
    i.WriteHtonU32(m_featureMask);
}

uint32_t
UetNegotiationOnOffHeader::Deserialize(Buffer::Iterator i)
{
    m_type = i.ReadU8();
    m_length = i.ReadU8();
    m_reserved = i.ReadNtohU16();
    m_featureMask = i.ReadNtohU32();
    return SERIALIZED_SIZE;
}

void
UetNegotiationOnOffHeader::Print(std::ostream& os) const
{
    os << "neg_type=" << +m_type << " neg_length=" << +m_length
       << " feature_mask=" << m_featureMask;
}

bool
UetNegotiationOnOffHeader::IsValid() const
{
    return m_type == NEGOTIATION_TYPE && m_length == LENGTH_IN_WORDS && m_reserved == 0 &&
           (m_featureMask & ~SYN_RETX_TRANSFER) == 0;
}

void
UetNegotiationOnOffHeader::SetFeatureMask(uint32_t value)
{
    m_featureMask = value;
}

uint32_t
UetNegotiationOnOffHeader::GetFeatureMask() const
{
    return m_featureMask;
}

} // namespace ns3
