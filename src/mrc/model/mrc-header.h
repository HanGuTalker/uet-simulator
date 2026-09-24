/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef MRC_HEADER_H
#define MRC_HEADER_H

#include "ns3/header.h"

#include <cstdint>

namespace ns3
{

/** MRC 1.0 data and control opcodes. */
enum class MrcOpcode : uint8_t
{
    WRITE_FIRST = 0xc6,
    WRITE_MIDDLE = 0xc7,
    WRITE_LAST = 0xc8,
    WRITE_LAST_IMMEDIATE = 0xc9,
    WRITE_ONLY = 0xca,
    WRITE_ONLY_IMMEDIATE = 0xcb,
    TRANSPORT_ACK = 0xd1,
    ENDPOINT_REQUEST = 0xd8,
    ENDPOINT_RESPONSE = 0xd9,
    RELIABILITY_SACK = 0xdc,
    RELIABILITY_NACK = 0xdd,
    RELIABILITY_PROBE = 0xde,
};

enum class MrcNackReason : uint8_t
{
    TRIMMED = 0x01,
    TRIMMED_LAST_HOP = 0x02,
    NO_BITMAP = 0x06,
    NO_PACKET_BUFFER = 0x07,
    NO_RESOURCE = 0x0a,
    PSN_OUT_OF_RANGE = 0x0b,
    UNEXPECTED_EVENT = 0x19,
};

/** Four-byte MRC Message Extended Transport Header. */
class MrcMethHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetReceiveQueueMessageSequence(uint16_t value);
    uint16_t GetReceiveQueueMessageSequence() const;
    void SetMessageSequence(uint16_t value);
    uint16_t GetMessageSequence() const;

  private:
    uint16_t m_receiveQueueMessageSequence{0};
    uint16_t m_messageSequence{0};
};

/** Four-byte Immediate Data field carried by Last/Only WriteIMM packets. */
class MrcImmediateHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetImmediateData(uint32_t value);
    uint32_t GetImmediateData() const;

  private:
    uint32_t m_immediateData{0};
};

/** Four-byte MRC Requestor Timestamp Extended Header. */
class MrcTimestampHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetTimestamp(uint16_t value);
    uint16_t GetTimestamp() const;
    void SetImplementationDefinedResolution(bool value);
    bool HasImplementationDefinedResolution() const;
    void SetFormatType(uint8_t value);
    uint8_t GetFormatType() const;

  private:
    uint16_t m_timestamp{0};
    bool m_implementationDefinedResolution{false};
    uint8_t m_formatType{1};
};

/** Eight-byte CC_STATE carried after SETH. */
class MrcCcStateHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 8;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetTimestamp(uint16_t value);
    uint16_t GetTimestamp() const;
    void SetOutOfOrderCount(uint16_t value);
    uint16_t GetOutOfOrderCount() const;
    void SetRestoreWindow(bool value);
    bool GetRestoreWindow() const;
    void SetReceiverWindowPenalty(uint8_t value);
    uint8_t GetReceiverWindowPenalty() const;
    void SetReceivedBytesUnits(uint32_t value);
    uint32_t GetReceivedBytesUnits() const;

  private:
    uint16_t m_timestamp{0};
    uint16_t m_outOfOrderCount{0};
    bool m_restoreWindow{false};
    uint8_t m_receiverWindowPenalty{0};
    uint32_t m_receivedBytesUnits{0};
};

/** Twenty-eight-byte MRC Reliability SACK header, followed on wire by CC_STATE. */
class MrcSethHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 28;
    static constexpr uint32_t BITMAP_BITS = 64;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetCongestionMark(uint8_t value);
    uint8_t GetCongestionMark() const;
    void SetProbeResponse(bool value);
    bool IsProbeResponse() const;
    void SetAcknowledgedPsnOffset(int16_t value);
    int16_t GetAcknowledgedPsnOffset() const;
    void SetEntropy(uint32_t value);
    uint32_t GetEntropy() const;
    void SetSourcePdcId(uint16_t value);
    uint16_t GetSourcePdcId() const;
    void SetDestinationPdcId(uint16_t value);
    uint16_t GetDestinationPdcId() const;
    void SetCumulativeAck(uint32_t value);
    uint32_t GetCumulativeAck() const;
    void SetCcType(uint8_t value);
    uint8_t GetCcType() const;
    void SetMaximumPsnRange(uint8_t value);
    uint8_t GetMaximumPsnRange() const;
    void SetSackOffset(int16_t value);
    int16_t GetSackOffset() const;
    void SetBitmap(uint64_t value);
    uint64_t GetBitmap() const;
    bool IsReceived(uint32_t offset) const;

  private:
    uint8_t m_congestionMark{0};
    bool m_probeResponse{false};
    int16_t m_acknowledgedPsnOffset{0};
    uint32_t m_entropy{0};
    uint16_t m_sourcePdcId{0};
    uint16_t m_destinationPdcId{0};
    uint32_t m_cumulativeAck{0};
    uint8_t m_ccType{0};
    uint8_t m_ccFlags{0};
    uint8_t m_maximumPsnRange{0};
    int16_t m_sackOffset{0};
    uint64_t m_bitmap{0};
};

/** Twenty-byte MRC Reliability NACK header. */
class MrcNethHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 20;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetReason(MrcNackReason value);
    MrcNackReason GetReason() const;
    void SetEntropy(uint32_t value);
    uint32_t GetEntropy() const;
    void SetSourcePdcId(uint16_t value);
    void SetDestinationPdcId(uint16_t value);
    void SetNackPsn(uint32_t value);
    uint32_t GetNackPsn() const;
    void SetTimestamp(uint16_t value);
    uint16_t GetTimestamp() const;

  private:
    MrcNackReason m_reason{MrcNackReason::UNEXPECTED_EVENT};
    uint8_t m_vendorInfo{0};
    uint32_t m_entropy{0};
    uint16_t m_sourcePdcId{0};
    uint16_t m_destinationPdcId{0};
    uint32_t m_nackPsn{0};
    uint8_t m_ccType{2};
    uint16_t m_timestamp{0};
};

/** Sixteen-byte MRC Reliability Probe request header. */
class MrcPethHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 16;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetProbeId(uint16_t value);
    uint16_t GetProbeId() const;
    void SetSourcePdcId(uint16_t value);
    void SetDestinationPdcId(uint16_t value);
    void SetTimestamp(uint16_t value);
    uint16_t GetTimestamp() const;

  private:
    uint8_t m_vendorInfo{0};
    uint16_t m_probeId{0};
    uint16_t m_sourcePdcId{0};
    uint16_t m_destinationPdcId{0};
    uint16_t m_timestamp{0};
};

bool IsMrcWriteOpcode(MrcOpcode opcode);
bool IsMrcFirstOpcode(MrcOpcode opcode);
bool IsMrcLastOpcode(MrcOpcode opcode);

} // namespace ns3

#endif // MRC_HEADER_H
