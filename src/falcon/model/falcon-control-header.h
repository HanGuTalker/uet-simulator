/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef FALCON_CONTROL_HEADER_H
#define FALCON_CONTROL_HEADER_H

#include "falcon-header.h"

#include "ns3/header.h"

#include <array>
#include <cstdint>

namespace ns3
{

enum class FalconNackCode : uint8_t
{
    RESOURCE_EXHAUSTION = 1,
    RECEIVER_NOT_READY = 2,
    XLR_DROP = 4,
    ULP_COMPLETE_IN_ERROR = 6,
    ULP_NON_RECOVERABLE_ERROR = 7,
    INVALID_CONNECTION_ID = 8,
};

/** The 32-byte Base ACK header. EACK reuses it with packet type EACK. */
class FalconBackHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 32;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetVersion(uint8_t value);
    uint8_t GetVersion() const;
    void SetConnectionId(uint32_t value);
    uint32_t GetConnectionId() const;
    void SetPacketType(FalconPacketType value);
    FalconPacketType GetPacketType() const;
    void SetReceiverDataWindowBase(uint32_t value);
    uint32_t GetReceiverDataWindowBase() const;
    void SetReceiverRequestWindowBase(uint32_t value);
    uint32_t GetReceiverRequestWindowBase() const;
    void SetTimestamp1(uint32_t value);
    uint32_t GetTimestamp1() const;
    void SetTimestamp2(uint32_t value);
    uint32_t GetTimestamp2() const;
    void SetCongestionMetadata(uint64_t value);
    uint64_t GetCongestionMetadata() const;
    bool HasValidReservedFields() const;

  private:
    uint8_t m_version{FalconBaseHeader::VERSION_1};
    uint8_t m_reservedVersion{0};
    uint32_t m_connectionId{0};
    uint32_t m_reservedType{0};
    FalconPacketType m_packetType{FalconPacketType::BACK};
    bool m_reservedR{false};
    uint32_t m_receiverDataWindowBase{0};
    uint32_t m_receiverRequestWindowBase{0};
    uint32_t m_timestamp1{0};
    uint32_t m_timestamp2{0};
    uint64_t m_congestionMetadata{0};
};

/** The 72-byte Extended ACK: BACK plus two 128-bit and one 64-bit bitmap. */
class FalconEackHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 72;
    using Bitmap128 = std::array<uint64_t, 2>;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetBack(const FalconBackHeader& value);
    const FalconBackHeader& GetBack() const;
    void SetDataAckBitmap(const Bitmap128& value);
    const Bitmap128& GetDataAckBitmap() const;
    void SetDataReceivedBitmap(const Bitmap128& value);
    const Bitmap128& GetDataReceivedBitmap() const;
    void SetRequestBitmap(uint64_t value);
    uint64_t GetRequestBitmap() const;

  private:
    FalconBackHeader m_back;
    Bitmap128 m_dataAckBitmap{};
    Bitmap128 m_dataReceivedBitmap{};
    uint64_t m_requestBitmap{0};
};

/** The 40-byte Falcon Negative ACK header. */
class FalconNackHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 40;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetVersion(uint8_t value);
    uint8_t GetVersion() const;
    void SetConnectionId(uint32_t value);
    uint32_t GetConnectionId() const;
    void SetReceiverDataWindowBase(uint32_t value);
    uint32_t GetReceiverDataWindowBase() const;
    void SetReceiverRequestWindowBase(uint32_t value);
    uint32_t GetReceiverRequestWindowBase() const;
    void SetNackPacketSequenceNumber(uint32_t value);
    uint32_t GetNackPacketSequenceNumber() const;
    void SetTimestamp1(uint32_t value);
    uint32_t GetTimestamp1() const;
    void SetTimestamp2(uint32_t value);
    uint32_t GetTimestamp2() const;
    void SetCongestionMetadata(uint64_t value);
    uint64_t GetCongestionMetadata() const;
    void SetNackCode(FalconNackCode value);
    FalconNackCode GetNackCode() const;
    void SetRnrTimeout(uint8_t value);
    uint8_t GetRnrTimeout() const;
    void SetRequestWindow(bool value);
    bool IsRequestWindow() const;
    void SetUlpNackCode(uint8_t value);
    uint8_t GetUlpNackCode() const;
    bool HasValidReservedFields() const;

  private:
    uint8_t m_version{FalconBaseHeader::VERSION_1};
    uint8_t m_reservedVersion{0};
    uint32_t m_connectionId{0};
    uint32_t m_reservedType{0};
    bool m_reservedR{false};
    uint32_t m_receiverDataWindowBase{0};
    uint32_t m_receiverRequestWindowBase{0};
    uint32_t m_nackPacketSequenceNumber{0};
    uint32_t m_timestamp1{0};
    uint32_t m_timestamp2{0};
    uint64_t m_congestionMetadata{0};
    FalconNackCode m_nackCode{FalconNackCode::RESOURCE_EXHAUSTION};
    uint8_t m_reservedNackHigh{0};
    uint8_t m_rnrTimeout{0};
    bool m_requestWindow{false};
    uint8_t m_reservedNackLow{0};
    uint8_t m_ulpNackCode{0};
};

} // namespace ns3

#endif // FALCON_CONTROL_HEADER_H
