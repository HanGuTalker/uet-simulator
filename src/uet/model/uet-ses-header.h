/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_SES_HEADER_H
#define UET_SES_HEADER_H

#include "ns3/header.h"

#include <cstdint>

namespace ns3
{

/** UEC 1.0.3 Table 3-17 request opcodes used by this model. */
enum class UetSesOpcode : uint8_t
{
    NO_OP = 0x00,
    WRITE = 0x01,
    ATOMIC = 0x03,
    SEND = 0x05,
    DATAGRAM_SEND = 0x07,
    DEFERRABLE_SEND = 0x08,
};

/** UEC response opcodes from Table 3-18. */
enum class UetSesResponseOpcode : uint8_t
{
    DEFAULT_RESPONSE = 0x00,
    RESPONSE = 0x01,
    RESPONSE_WITH_DATA = 0x02,
    NO_RESPONSE = 0x03,
};

/** AI Base semantic return codes used by the executable model. */
enum class UetSesReturnCode : uint8_t
{
    NULL_STATUS = 0x00,
    OK = 0x01,
    BAD_GENERATION = 0x02,
    NO_MATCH = 0x05,
    UNSUPPORTED_OPERATION = 0x06,
    UNSUPPORTED_SIZE = 0x07,
    AMO_UNSUPPORTED_OPERATION = 0x0f,
    AMO_UNSUPPORTED_DATATYPE = 0x10,
    AMO_UNSUPPORTED_SIZE = 0x11,
    AMO_UNALIGNED = 0x12,
    PERMISSION_VIOLATION = 0x17,
    OPERATION_VIOLATION = 0x18,
    BAD_INDEX = 0x19,
    BAD_PID = 0x1a,
    BAD_JOB_ID = 0x1b,
};

/** Atomic extension operation and datatype values modeled for AI Base. */
enum class UetAtomicOpcode : uint8_t
{
    WRITE = 0x00,
    SUM = 0x01,
    BOR = 0x02,
    BAND = 0x03,
    BXOR = 0x04,
    MIN = 0x05,
    MAX = 0x06,
};

enum class UetAtomicDatatype : uint8_t
{
    UINT8 = 0x00,
    UINT16 = 0x01,
    UINT32 = 0x02,
    UINT64 = 0x03,
    INT8 = 0x04,
    INT16 = 0x05,
    INT32 = 0x06,
    INT64 = 0x07,
};

/**
 * UEC standard SES request header (UET_HDR_REQUEST_STD, Figures 3-9/3-10).
 */
class UetSesStandardHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 44;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    bool IsValid() const;

    void SetOpcode(UetSesOpcode opcode);
    UetSesOpcode GetOpcode() const;
    void SetStartOfMessage(bool value);
    bool IsStartOfMessage() const;
    void SetEndOfMessage(bool value);
    bool IsEndOfMessage() const;
    void SetMessageId(uint16_t value);
    uint16_t GetMessageId() const;
    void SetRequestLength(uint32_t value);
    uint32_t GetRequestLength() const;
    void SetPayloadLength(uint16_t value);
    uint16_t GetPayloadLength() const;
    void SetMessageOffset(uint32_t value);
    uint32_t GetMessageOffset() const;
    void SetDeliveryComplete(bool value);
    bool IsDeliveryComplete() const;
    void SetInitiatorError(bool value);
    bool HasInitiatorError() const;
    void SetRelativeAddressing(bool value);
    bool IsRelativeAddressing() const;
    void SetHeaderDataPresent(bool value);
    bool HasHeaderData() const;
    void SetRiGeneration(uint8_t value);
    uint8_t GetRiGeneration() const;
    void SetJobId(uint32_t value);
    uint32_t GetJobId() const;
    void SetPidOnFep(uint16_t value);
    uint16_t GetPidOnFep() const;
    void SetResourceIndex(uint16_t value);
    uint16_t GetResourceIndex() const;
    void SetBufferOffset(uint64_t value);
    uint64_t GetBufferOffset() const;
    void SetInitiator(uint32_t value);
    uint32_t GetInitiator() const;
    void SetMemoryKey(uint64_t value);
    uint64_t GetMemoryKey() const;
    void SetHeaderData(uint64_t value);
    uint64_t GetHeaderData() const;

  private:
    UetSesOpcode m_opcode{UetSesOpcode::SEND};
    bool m_startOfMessage{true};
    bool m_endOfMessage{true};
    bool m_deliveryComplete{false};
    bool m_initiatorError{false};
    bool m_relativeAddressing{false};
    bool m_headerDataPresent{false};
    uint16_t m_messageId{0};
    uint8_t m_riGeneration{0};
    uint32_t m_jobId{0};
    uint16_t m_pidOnFep{0};
    uint16_t m_resourceIndex{0};
    uint64_t m_bufferOffset{0};
    uint32_t m_initiator{0};
    uint64_t m_memoryKey{0};
    uint64_t m_headerData{0};
    uint32_t m_requestLength{0};
    uint16_t m_payloadLength{0};
    uint32_t m_messageOffset{0};
    bool m_wireValid{true};
};

/** Four-byte atomic operation extension (Figure 3-16). */
class UetAtomicExtensionHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    bool IsValid() const;

    void SetAtomicOpcode(UetAtomicOpcode value);
    UetAtomicOpcode GetAtomicOpcode() const;
    void SetAtomicDatatype(UetAtomicDatatype value);
    UetAtomicDatatype GetAtomicDatatype() const;
    void SetSemanticControl(uint8_t value);
    uint8_t GetSemanticControl() const;
    uint32_t GetDatatypeSize() const;

  private:
    UetAtomicOpcode m_atomicOpcode{UetAtomicOpcode::SUM};
    UetAtomicDatatype m_atomicDatatype{UetAtomicDatatype::UINT64};
    uint8_t m_semanticControl{0};
    uint8_t m_reserved{0};
};

/** Sixteen-byte standard SES response (Figure 3-18). */
class UetSesResponseHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 16;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    bool IsValid() const;

    void SetList(uint8_t value);
    uint8_t GetList() const;
    void SetOpcode(UetSesResponseOpcode value);
    UetSesResponseOpcode GetOpcode() const;
    void SetReturnCode(UetSesReturnCode value);
    UetSesReturnCode GetReturnCode() const;
    void SetMessageId(uint16_t value);
    uint16_t GetMessageId() const;
    void SetRiGeneration(uint8_t value);
    uint8_t GetRiGeneration() const;
    void SetJobId(uint32_t value);
    uint32_t GetJobId() const;
    void SetModifiedLength(uint32_t value);
    uint32_t GetModifiedLength() const;

  private:
    uint8_t m_list{0};
    UetSesResponseOpcode m_opcode{UetSesResponseOpcode::DEFAULT_RESPONSE};
    UetSesReturnCode m_returnCode{UetSesReturnCode::OK};
    uint16_t m_messageId{0};
    uint8_t m_riGeneration{0};
    uint32_t m_jobId{0};
    uint32_t m_modifiedLength{0};
    uint32_t m_reserved{0};
};

/**
 * UEC optimized 32-byte SES request header (UET_HDR_REQUEST_MEDIUM, Figure 3-14).
 */
class UetSesMediumHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 32;
    static constexpr uint16_t MAX_REQUEST_LENGTH = 0x3fff;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    bool IsValid() const;

    void SetOpcode(UetSesOpcode opcode);
    UetSesOpcode GetOpcode() const;
    void SetRequestLength(uint16_t value);
    uint16_t GetRequestLength() const;

  private:
    UetSesOpcode m_opcode{UetSesOpcode::DATAGRAM_SEND};
    uint16_t m_requestLength{0};
    bool m_wireValid{true};
};

} // namespace ns3

#endif // UET_SES_HEADER_H
