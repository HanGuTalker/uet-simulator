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

bool IsMrcWriteOpcode(MrcOpcode opcode);
bool IsMrcFirstOpcode(MrcOpcode opcode);
bool IsMrcLastOpcode(MrcOpcode opcode);

} // namespace ns3

#endif // MRC_HEADER_H
