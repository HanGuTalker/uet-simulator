/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-ses-header.h"

#include "ns3/assert.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(UetSesStandardHeader);
NS_OBJECT_ENSURE_REGISTERED(UetSesMediumHeader);
NS_OBJECT_ENSURE_REGISTERED(UetAtomicExtensionHeader);
NS_OBJECT_ENSURE_REGISTERED(UetSesResponseHeader);

TypeId
UetSesStandardHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetSesStandardHeader")
                            .SetParent<Header>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetSesStandardHeader>();
    return tid;
}

TypeId
UetSesStandardHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UetSesStandardHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
UetSesStandardHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(IsValid(), "Cannot serialize an invalid UEC standard SES header");
    uint32_t firstWord = static_cast<uint32_t>(m_opcode) << 24;
    firstWord |= m_deliveryComplete ? (1U << 21) : 0;
    firstWord |= m_initiatorError ? (1U << 20) : 0;
    firstWord |= m_relativeAddressing ? (1U << 19) : 0;
    firstWord |= m_headerDataPresent ? (1U << 18) : 0;
    firstWord |= m_endOfMessage ? (1U << 17) : 0;
    firstWord |= m_startOfMessage ? (1U << 16) : 0;
    firstWord |= m_messageId;
    i.WriteHtonU32(firstWord);
    i.WriteHtonU32((static_cast<uint32_t>(m_riGeneration) << 24) | (m_jobId & 0xffffff));
    i.WriteHtonU32((static_cast<uint32_t>(m_pidOnFep & 0x0fff) << 16) |
                   (m_resourceIndex & 0x0fff));
    i.WriteHtonU64(m_bufferOffset);
    i.WriteHtonU32(m_initiator);
    i.WriteHtonU64(m_memoryKey);
    if (m_startOfMessage)
    {
        i.WriteHtonU64(m_headerData);
        i.WriteHtonU32(m_requestLength);
    }
    else
    {
        i.WriteHtonU32(m_payloadLength & 0x3fff);
        i.WriteHtonU32(m_messageOffset);
        i.WriteHtonU32(m_requestLength);
    }
}

uint32_t
UetSesStandardHeader::Deserialize(Buffer::Iterator i)
{
    const uint32_t firstWord = i.ReadNtohU32();
    m_wireValid = (firstWord & 0xc0c00000U) == 0;
    m_opcode = static_cast<UetSesOpcode>((firstWord >> 24) & 0x3f);
    m_deliveryComplete = (firstWord & (1U << 21)) != 0;
    m_initiatorError = (firstWord & (1U << 20)) != 0;
    m_relativeAddressing = (firstWord & (1U << 19)) != 0;
    m_headerDataPresent = (firstWord & (1U << 18)) != 0;
    m_endOfMessage = (firstWord & (1U << 17)) != 0;
    m_startOfMessage = (firstWord & (1U << 16)) != 0;
    m_messageId = firstWord & 0xffff;
    const uint32_t identityWord = i.ReadNtohU32();
    m_riGeneration = identityWord >> 24;
    m_jobId = identityWord & 0xffffff;
    const uint32_t addressingWord = i.ReadNtohU32();
    m_wireValid = m_wireValid && (addressingWord & 0xf000f000U) == 0;
    m_pidOnFep = (addressingWord >> 16) & 0x0fff;
    m_resourceIndex = addressingWord & 0x0fff;
    m_bufferOffset = i.ReadNtohU64();
    m_initiator = i.ReadNtohU32();
    m_memoryKey = i.ReadNtohU64();
    if (m_startOfMessage)
    {
        m_headerData = i.ReadNtohU64();
        m_requestLength = i.ReadNtohU32();
        m_payloadLength = 0;
        m_messageOffset = 0;
    }
    else
    {
        const uint32_t lengthWord = i.ReadNtohU32();
        m_wireValid = m_wireValid && (lengthWord & 0xffffc000U) == 0;
        m_payloadLength = lengthWord & 0x3fff;
        m_messageOffset = i.ReadNtohU32();
        m_requestLength = i.ReadNtohU32();
    }
    return SERIALIZED_SIZE;
}

void
UetSesStandardHeader::Print(std::ostream& os) const
{
    os << "opcode=" << +static_cast<uint8_t>(m_opcode) << " som=" << m_startOfMessage
       << " eom=" << m_endOfMessage << " mid=" << m_messageId
       << " requestLength=" << m_requestLength;
}

bool
UetSesStandardHeader::IsValid() const
{
    return m_wireValid && static_cast<uint8_t>(m_opcode) <= 0x3f && m_messageId != 0 &&
           m_jobId <= 0xffffff && m_pidOnFep <= 0x0fff && m_resourceIndex <= 0x0fff &&
           (m_startOfMessage || m_payloadLength <= 0x3fff);
}

#define UET_SES_ACCESSOR(Name, Type, Member)                                                       \
    void UetSesStandardHeader::Set##Name(Type value)                                               \
    {                                                                                              \
        Member = value;                                                                            \
    }                                                                                              \
    Type UetSesStandardHeader::Get##Name() const                                                   \
    {                                                                                              \
        return Member;                                                                             \
    }

UET_SES_ACCESSOR(Opcode, UetSesOpcode, m_opcode)
UET_SES_ACCESSOR(MessageId, uint16_t, m_messageId)
UET_SES_ACCESSOR(RequestLength, uint32_t, m_requestLength)
UET_SES_ACCESSOR(PayloadLength, uint16_t, m_payloadLength)
UET_SES_ACCESSOR(MessageOffset, uint32_t, m_messageOffset)
UET_SES_ACCESSOR(RiGeneration, uint8_t, m_riGeneration)
UET_SES_ACCESSOR(JobId, uint32_t, m_jobId)
UET_SES_ACCESSOR(PidOnFep, uint16_t, m_pidOnFep)
UET_SES_ACCESSOR(ResourceIndex, uint16_t, m_resourceIndex)
UET_SES_ACCESSOR(BufferOffset, uint64_t, m_bufferOffset)
UET_SES_ACCESSOR(Initiator, uint32_t, m_initiator)
UET_SES_ACCESSOR(MemoryKey, uint64_t, m_memoryKey)
UET_SES_ACCESSOR(HeaderData, uint64_t, m_headerData)

#undef UET_SES_ACCESSOR

void
UetSesStandardHeader::SetStartOfMessage(bool value)
{
    m_startOfMessage = value;
}

bool
UetSesStandardHeader::IsStartOfMessage() const
{
    return m_startOfMessage;
}

void
UetSesStandardHeader::SetEndOfMessage(bool value)
{
    m_endOfMessage = value;
}

bool
UetSesStandardHeader::IsEndOfMessage() const
{
    return m_endOfMessage;
}

#define UET_SES_BOOL_ACCESSOR(Name, Member)                                                        \
    void UetSesStandardHeader::Set##Name(bool value)                                               \
    {                                                                                              \
        Member = value;                                                                            \
    }

UET_SES_BOOL_ACCESSOR(DeliveryComplete, m_deliveryComplete)
UET_SES_BOOL_ACCESSOR(InitiatorError, m_initiatorError)
UET_SES_BOOL_ACCESSOR(RelativeAddressing, m_relativeAddressing)
UET_SES_BOOL_ACCESSOR(HeaderDataPresent, m_headerDataPresent)

#undef UET_SES_BOOL_ACCESSOR

bool
UetSesStandardHeader::IsDeliveryComplete() const
{
    return m_deliveryComplete;
}

bool
UetSesStandardHeader::HasInitiatorError() const
{
    return m_initiatorError;
}

bool
UetSesStandardHeader::IsRelativeAddressing() const
{
    return m_relativeAddressing;
}

bool
UetSesStandardHeader::HasHeaderData() const
{
    return m_headerDataPresent;
}

TypeId
UetSesMediumHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetSesMediumHeader")
                            .SetParent<Header>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetSesMediumHeader>();
    return tid;
}

TypeId
UetSesMediumHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UetSesMediumHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
UetSesMediumHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(IsValid(), "Cannot serialize an invalid UEC medium SES header");
    const uint32_t firstWord =
        (static_cast<uint32_t>(m_opcode) << 24) | (1U << 17) | (1U << 16) | m_requestLength;
    i.WriteHtonU32(firstWord);
    i.WriteHtonU32(0); // ri_generation and JobID
    i.WriteHtonU32(0); // PIDonFEP and resource_index
    i.WriteHtonU64(0); // header_data / buffer_offset
    i.WriteHtonU32(0); // initiator
    i.WriteHtonU64(0); // match_bits / memory_key
}

uint32_t
UetSesMediumHeader::Deserialize(Buffer::Iterator i)
{
    const uint32_t firstWord = i.ReadNtohU32();
    m_wireValid = (firstWord & 0xc0c0c000U) == 0 && (firstWord & 0x00030000U) == 0x00030000U;
    m_opcode = static_cast<UetSesOpcode>((firstWord >> 24) & 0x3f);
    m_requestLength = firstWord & 0x3fff;
    i.ReadNtohU32();
    const uint32_t addressingWord = i.ReadNtohU32();
    m_wireValid = m_wireValid && (addressingWord & 0xf000f000U) == 0;
    i.ReadNtohU64();
    i.ReadNtohU32();
    i.ReadNtohU64();
    return SERIALIZED_SIZE;
}

void
UetSesMediumHeader::Print(std::ostream& os) const
{
    os << "opcode=" << +static_cast<uint8_t>(m_opcode) << " requestLength=" << m_requestLength;
}

bool
UetSesMediumHeader::IsValid() const
{
    return m_wireValid && static_cast<uint8_t>(m_opcode) <= 0x3f &&
           m_requestLength <= MAX_REQUEST_LENGTH;
}

void
UetSesMediumHeader::SetOpcode(UetSesOpcode opcode)
{
    m_opcode = opcode;
}

UetSesOpcode
UetSesMediumHeader::GetOpcode() const
{
    return m_opcode;
}

void
UetSesMediumHeader::SetRequestLength(uint16_t value)
{
    m_requestLength = value;
}

uint16_t
UetSesMediumHeader::GetRequestLength() const
{
    return m_requestLength;
}

TypeId
UetAtomicExtensionHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetAtomicExtensionHeader")
                            .SetParent<Header>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetAtomicExtensionHeader>();
    return tid;
}

TypeId
UetAtomicExtensionHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UetAtomicExtensionHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
UetAtomicExtensionHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(IsValid(), "Cannot serialize invalid UEC atomic extension");
    i.WriteU8(static_cast<uint8_t>(m_atomicOpcode));
    i.WriteU8(static_cast<uint8_t>(m_atomicDatatype));
    i.WriteU8(m_semanticControl);
    i.WriteU8(0);
}

uint32_t
UetAtomicExtensionHeader::Deserialize(Buffer::Iterator i)
{
    m_atomicOpcode = static_cast<UetAtomicOpcode>(i.ReadU8());
    m_atomicDatatype = static_cast<UetAtomicDatatype>(i.ReadU8());
    m_semanticControl = i.ReadU8();
    m_reserved = i.ReadU8();
    return SERIALIZED_SIZE;
}

void
UetAtomicExtensionHeader::Print(std::ostream& os) const
{
    os << "atomicOpcode=" << +static_cast<uint8_t>(m_atomicOpcode)
       << " datatype=" << +static_cast<uint8_t>(m_atomicDatatype)
       << " control=" << +m_semanticControl;
}

bool
UetAtomicExtensionHeader::IsValid() const
{
    return static_cast<uint8_t>(m_atomicOpcode) <= static_cast<uint8_t>(UetAtomicOpcode::MAX) &&
           static_cast<uint8_t>(m_atomicDatatype) <=
               static_cast<uint8_t>(UetAtomicDatatype::INT64) &&
           m_reserved == 0;
}

void UetAtomicExtensionHeader::SetAtomicOpcode(UetAtomicOpcode value) { m_atomicOpcode = value; }
UetAtomicOpcode UetAtomicExtensionHeader::GetAtomicOpcode() const { return m_atomicOpcode; }
void UetAtomicExtensionHeader::SetAtomicDatatype(UetAtomicDatatype value) { m_atomicDatatype = value; }
UetAtomicDatatype UetAtomicExtensionHeader::GetAtomicDatatype() const { return m_atomicDatatype; }
void UetAtomicExtensionHeader::SetSemanticControl(uint8_t value) { m_semanticControl = value; }
uint8_t UetAtomicExtensionHeader::GetSemanticControl() const { return m_semanticControl; }

uint32_t
UetAtomicExtensionHeader::GetDatatypeSize() const
{
    switch (m_atomicDatatype)
    {
    case UetAtomicDatatype::UINT8:
    case UetAtomicDatatype::INT8:
        return 1;
    case UetAtomicDatatype::UINT16:
    case UetAtomicDatatype::INT16:
        return 2;
    case UetAtomicDatatype::UINT32:
    case UetAtomicDatatype::INT32:
        return 4;
    case UetAtomicDatatype::UINT64:
    case UetAtomicDatatype::INT64:
        return 8;
    }
    return 0;
}

TypeId
UetSesResponseHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetSesResponseHeader")
                            .SetParent<Header>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetSesResponseHeader>();
    return tid;
}

TypeId UetSesResponseHeader::GetInstanceTypeId() const { return GetTypeId(); }
uint32_t UetSesResponseHeader::GetSerializedSize() const { return SERIALIZED_SIZE; }

void
UetSesResponseHeader::Serialize(Buffer::Iterator i) const
{
    NS_ASSERT_MSG(IsValid(), "Cannot serialize invalid UEC SES response");
    i.WriteU8((m_list << 6) | static_cast<uint8_t>(m_opcode));
    i.WriteU8(static_cast<uint8_t>(m_returnCode));
    i.WriteHtonU16(m_messageId);
    i.WriteHtonU32((static_cast<uint32_t>(m_riGeneration) << 24) | (m_jobId & 0xffffff));
    i.WriteHtonU32(m_modifiedLength);
    i.WriteHtonU32(0);
}

uint32_t
UetSesResponseHeader::Deserialize(Buffer::Iterator i)
{
    const uint8_t first = i.ReadU8();
    m_list = first >> 6;
    m_opcode = static_cast<UetSesResponseOpcode>(first & 0x3f);
    m_returnCode = static_cast<UetSesReturnCode>(i.ReadU8() & 0x3f);
    m_messageId = i.ReadNtohU16();
    const uint32_t identity = i.ReadNtohU32();
    m_riGeneration = identity >> 24;
    m_jobId = identity & 0xffffff;
    m_modifiedLength = i.ReadNtohU32();
    m_reserved = i.ReadNtohU32();
    return SERIALIZED_SIZE;
}

void
UetSesResponseHeader::Print(std::ostream& os) const
{
    os << "responseOpcode=" << +static_cast<uint8_t>(m_opcode)
       << " returnCode=" << +static_cast<uint8_t>(m_returnCode)
       << " messageId=" << m_messageId << " modifiedLength=" << m_modifiedLength;
}

bool
UetSesResponseHeader::IsValid() const
{
    return m_list <= 3 && static_cast<uint8_t>(m_opcode) <= 0x3f &&
           static_cast<uint8_t>(m_returnCode) <= 0x3f && m_reserved == 0;
}

#define UET_RESPONSE_ACCESSOR(Name, Type, Member)                                                  \
    void UetSesResponseHeader::Set##Name(Type value) { Member = value; }                           \
    Type UetSesResponseHeader::Get##Name() const { return Member; }

UET_RESPONSE_ACCESSOR(List, uint8_t, m_list)
UET_RESPONSE_ACCESSOR(Opcode, UetSesResponseOpcode, m_opcode)
UET_RESPONSE_ACCESSOR(ReturnCode, UetSesReturnCode, m_returnCode)
UET_RESPONSE_ACCESSOR(MessageId, uint16_t, m_messageId)
UET_RESPONSE_ACCESSOR(RiGeneration, uint8_t, m_riGeneration)
UET_RESPONSE_ACCESSOR(JobId, uint32_t, m_jobId)
UET_RESPONSE_ACCESSOR(ModifiedLength, uint32_t, m_modifiedLength)

#undef UET_RESPONSE_ACCESSOR

} // namespace ns3
