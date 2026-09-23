/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef VEROCE_HEADER_H
#define VEROCE_HEADER_H

#include "ns3/header.h"

#include <array>
#include <cstdint>

namespace ns3
{

/** Four-byte MSNETH: reserved octet followed by the 24-bit message sequence number. */
class VeRoceMsnHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    void SetMessageSequence(uint32_t value);
    uint32_t GetMessageSequence() const;

  private:
    uint32_t m_messageSequence{0};
};

/** Four-byte POETH: reserved octet followed by the 24-bit in-message packet order. */
class VeRocePacketOffsetHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;
    static constexpr uint32_t UNAVAILABLE = 0xffffff;
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    void SetPacketOrder(uint32_t value);
    uint32_t GetPacketOrder() const;

  private:
    uint32_t m_packetOrder{0};
};

/** Four-byte RQETH: reserved octet followed by the 24-bit receive-queue MSN. */
class VeRoceRqHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    void SetReceiveQueueSequence(uint32_t value);
    uint32_t GetReceiveQueueSequence() const;

  private:
    uint32_t m_receiveQueueSequence{0};
};

/** Twenty-byte SACKETH carrying a 24-bit starting PSN and a 128-bit receive bitmap. */
class VeRoceSackHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 20;
    static constexpr uint8_t MAX_BITMAP_BITS = 128;
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    void SetBitmapStartingPsn(uint32_t value);
    uint32_t GetBitmapStartingPsn() const;
    void SetBitmapValidLength(uint8_t value);
    uint8_t GetBitmapValidLength() const;
    void SetReceived(uint8_t offset, bool value = true);
    bool IsReceived(uint8_t offset) const;

  private:
    uint32_t m_bitmapStartingPsn{0};
    uint8_t m_bitmapValidLength{0};
    std::array<uint32_t, 4> m_bitmap{};
};

/** Twenty-byte RTTReqETH/RTTRspETH wire format. */
class VeRoceRttHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 20;
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
    void SetContextId(uint32_t value);
    uint32_t GetContextId() const;
    void SetTimestamp(uint32_t index, uint32_t value);
    uint32_t GetTimestamp(uint32_t index) const;

  private:
    uint32_t m_contextId{0};
    std::array<uint32_t, 4> m_timestamps{};
};

} // namespace ns3

#endif // VEROCE_HEADER_H
