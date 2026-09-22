/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_HEADER_H
#define UET_HEADER_H

#include "uet-types.h"

#include "ns3/header.h"

#include <cstdint>

namespace ns3
{

/**
 * Common packet-level header used by the UET simulation model.
 *
 * This stable 44-byte representation carries protocol-visible semantics. It is
 * intentionally independent from a hardware wire image so that packet logic
 * can be implemented before individual SES formats are added.
 */
class UetHeader : public Header
{
  public:
    static constexpr uint8_t VERSION = 1;
    static constexpr uint32_t SERIALIZED_SIZE = 44;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    bool IsValid() const;

    void SetPacketType(UetPacketType packetType);
    UetPacketType GetPacketType() const;
    void SetDeliveryMode(UetDeliveryMode deliveryMode);
    UetDeliveryMode GetDeliveryMode() const;
    void SetFlag(UetHeaderFlag flag, bool enabled = true);
    bool HasFlag(UetHeaderFlag flag) const;

    void SetSourceEndpointId(uint32_t endpointId);
    uint32_t GetSourceEndpointId() const;
    void SetDestinationEndpointId(uint32_t endpointId);
    uint32_t GetDestinationEndpointId() const;
    void SetPdcId(uint32_t pdcId);
    uint32_t GetPdcId() const;
    void SetSequenceNumber(uint32_t sequenceNumber);
    uint32_t GetSequenceNumber() const;
    void SetAcknowledgedSequenceNumber(uint32_t sequenceNumber);
    uint32_t GetAcknowledgedSequenceNumber() const;
    void SetMessageId(uint64_t messageId);
    uint64_t GetMessageId() const;
    void SetPayloadLength(uint32_t payloadLength);
    uint32_t GetPayloadLength() const;
    void SetFragmentOffset(uint32_t fragmentOffset);
    uint32_t GetFragmentOffset() const;
    void SetPathId(uint16_t pathId);
    uint16_t GetPathId() const;

  private:
    static constexpr uint8_t KNOWN_FLAGS = 0x1f;

    uint8_t m_version{VERSION};
    UetPacketType m_packetType{UetPacketType::DATA};
    UetDeliveryMode m_deliveryMode{UetDeliveryMode::RUD};
    uint8_t m_flags{0};
    uint32_t m_sourceEndpointId{0};
    uint32_t m_destinationEndpointId{0};
    uint32_t m_pdcId{0};
    uint32_t m_sequenceNumber{0};
    uint32_t m_acknowledgedSequenceNumber{0};
    uint64_t m_messageId{0};
    uint32_t m_payloadLength{0};
    uint32_t m_fragmentOffset{0};
    uint16_t m_pathId{0};
    uint16_t m_reserved{0};
};

} // namespace ns3

#endif // UET_HEADER_H
