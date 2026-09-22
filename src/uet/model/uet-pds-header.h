/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_PDS_HEADER_H
#define UET_PDS_HEADER_H

#include "ns3/header.h"

#include <cstdint>

namespace ns3
{

/** UEC 1.0.3 Table 3-32 PDS packet type encodings. */
enum class UetPdsType : uint8_t
{
    RUD_REQUEST = 2,
    ROD_REQUEST = 3,
    UUD_REQUEST = 6,
    ACK = 7,
    ACK_CC = 8,
    ACK_CCX = 9,
    NACK = 10,
    CONTROL = 11,
};

/** UEC 1.0.3 Table 3-38 PDS control packet type encodings. */
enum class UetControlType : uint8_t
{
    NOOP = 0,
    ACK_REQUEST = 1,
    CLEAR_COMMAND = 2,
    CLEAR_REQUEST = 3,
    CLOSE_COMMAND = 4,
    CLOSE_REQUEST = 5,
    PROBE = 6,
    CREDIT = 7,
    CREDIT_REQUEST = 8,
    NEGOTIATION = 9,
};

/** UEC 1.0.3 Table 3-16 PDS next-header encodings used by AI Base. */
enum class UetNextHeader : uint8_t
{
    NONE = 0,
    REQUEST_SMALL = 1,
    REQUEST_MEDIUM = 2,
    REQUEST_STANDARD = 3,
    RESPONSE = 4,
    RESPONSE_DATA = 5,
    RESPONSE_DATA_SMALL = 6,
    CP_ACK = 9,
};

/**
 * UEC 1.0.3 PDS header formats from sections 3.5.10.2-3.5.10.12.
 *
 * The serialized length is selected by pds.type: 12 bytes for RUD/ROD
 * Request, ACK, and Control, 16 bytes for NACK, and 4 bytes for UUD Request.
 */
class UetPdsHeader : public Header
{
  public:
    static constexpr uint32_t REQUEST_SIZE = 12;
    static constexpr uint32_t ACK_SIZE = 12;
    static constexpr uint32_t ACK_CC_SIZE = 32;
    static constexpr uint32_t NACK_SIZE = 16;
    static constexpr uint32_t UUD_SIZE = 4;
    static constexpr uint32_t CONTROL_SIZE = 12;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    bool IsValid() const;
    static uint32_t GetSerializedSize(UetPdsType type);

    void SetType(UetPdsType type);
    UetPdsType GetType() const;
    void SetNextHeader(UetNextHeader nextHeader);
    UetNextHeader GetNextHeader() const;
    void SetFlags(uint8_t flags);
    uint8_t GetFlags() const;
    void SetControlType(UetControlType value);
    UetControlType GetControlType() const;
    void SetProbeOpaque(uint16_t value);
    uint16_t GetProbeOpaque() const;

    void SetClearPsnOffset(uint16_t value);
    uint16_t GetClearPsnOffset() const;
    void SetPsn(uint32_t value);
    uint32_t GetPsn() const;
    void SetSourcePdcId(uint16_t value);
    uint16_t GetSourcePdcId() const;
    void SetDestinationPdcId(uint16_t value);
    uint16_t GetDestinationPdcId() const;

    void SetAckPsnOffset(uint16_t value);
    uint16_t GetAckPsnOffset() const;
    void SetCumulativeAckPsn(uint32_t value);
    uint32_t GetCumulativeAckPsn() const;
    void SetCcType(uint8_t value);
    uint8_t GetCcType() const;
    void SetCcFlags(uint8_t value);
    uint8_t GetCcFlags() const;
    void SetMaximumPsnRange(uint8_t value);
    uint8_t GetMaximumPsnRange() const;
    void SetSackPsnOffset(uint16_t value);
    uint16_t GetSackPsnOffset() const;
    void SetSackBitmap(uint64_t value);
    uint64_t GetSackBitmap() const;
    void SetAckCcState(uint64_t value);
    uint64_t GetAckCcState() const;

    void SetNsccState(uint16_t serviceTime,
                      bool restoreCwnd,
                      uint8_t receiverCwndPending,
                      uint32_t receivedBytes,
                      uint16_t outOfOrderCount);
    uint16_t GetNsccServiceTime() const;
    bool GetNsccRestoreCwnd() const;
    uint8_t GetNsccReceiverCwndPending() const;
    uint32_t GetNsccReceivedBytes() const;
    uint16_t GetNsccOutOfOrderCount() const;

    void SetNackCode(uint8_t value);
    uint8_t GetNackCode() const;
    void SetVendorCode(uint8_t value);
    uint8_t GetVendorCode() const;
    void SetNackPsn(uint32_t value);
    uint32_t GetNackPsn() const;
    void SetNackPayload(uint32_t value);
    uint32_t GetNackPayload() const;

  private:
    uint16_t EncodePrologue() const;
    void DecodePrologue(uint16_t value);

    UetPdsType m_type{UetPdsType::UUD_REQUEST};
    UetNextHeader m_nextHeader{UetNextHeader::NONE};
    UetControlType m_controlType{UetControlType::NOOP};
    uint8_t m_flags{0};
    uint16_t m_probeOpaque{0};
    uint16_t m_clearPsnOffset{0};
    uint32_t m_psn{0};
    uint16_t m_sourcePdcId{0};
    uint16_t m_destinationPdcId{0};
    uint16_t m_ackPsnOffset{0};
    uint32_t m_cumulativeAckPsn{0};
    uint8_t m_ccType{0};
    uint8_t m_ccFlags{0};
    uint8_t m_maximumPsnRange{0};
    uint16_t m_sackPsnOffset{0};
    uint64_t m_sackBitmap{0};
    uint64_t m_ackCcState{0};
    uint8_t m_nackCode{0};
    uint8_t m_vendorCode{0};
    uint32_t m_nackPsn{0};
    uint32_t m_nackPayload{0};
};

/** UEC 1.0.3 Table 3-69 PDS_NEG_ON_OFF negotiation payload. */
class UetNegotiationOnOffHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 8;
    static constexpr uint8_t NEGOTIATION_TYPE = 1;
    static constexpr uint8_t LENGTH_IN_WORDS = 2;
    static constexpr uint32_t SYN_RETX_TRANSFER = 1U << 0;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    bool IsValid() const;
    void SetFeatureMask(uint32_t value);
    uint32_t GetFeatureMask() const;

  private:
    uint8_t m_type{NEGOTIATION_TYPE};
    uint8_t m_length{LENGTH_IN_WORDS};
    uint16_t m_reserved{0};
    uint32_t m_featureMask{0};
};

} // namespace ns3

#endif // UET_PDS_HEADER_H
