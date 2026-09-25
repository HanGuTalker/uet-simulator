/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef FALCON_HEADER_H
#define FALCON_HEADER_H

#include "ns3/header.h"

#include <cstdint>

namespace ns3
{

/** OCP Falcon 1.1 upper-layer protocol encodings. */
enum class FalconProtocolType : uint8_t
{
    RDMA = 0x2,
    NVME = 0x3,
};

/** OCP Falcon 1.1 packet type encodings. */
enum class FalconPacketType : uint8_t
{
    PULL_REQUEST = 0x0,
    PULL_DATA = 0x3,
    PUSH_DATA = 0x5,
    RESYNC = 0x6,
    NACK = 0x8,
    BACK = 0x9,
    EACK = 0xa,
};

/**
 * The 24-byte Falcon base header carried by Pull Request, Pull Data, Push Data and Resync.
 *
 * ACK and NACK packets use their own layouts and are intentionally not represented by this class.
 */
class FalconBaseHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 24;
    static constexpr uint8_t VERSION_1 = 1;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetVersion(uint8_t value);
    uint8_t GetVersion() const;
    bool HasValidReservedField() const;
    void SetDestinationConnectionId(uint32_t value);
    uint32_t GetDestinationConnectionId() const;
    void SetDestinationFunction(uint32_t value);
    uint32_t GetDestinationFunction() const;
    void SetProtocolType(FalconProtocolType value);
    FalconProtocolType GetProtocolType() const;
    void SetPacketType(FalconPacketType value);
    FalconPacketType GetPacketType() const;
    void SetAckRequest(bool value);
    bool GetAckRequest() const;
    void SetReceiverDataWindowBase(uint32_t value);
    uint32_t GetReceiverDataWindowBase() const;
    void SetReceiverRequestWindowBase(uint32_t value);
    uint32_t GetReceiverRequestWindowBase() const;
    void SetPacketSequenceNumber(uint32_t value);
    uint32_t GetPacketSequenceNumber() const;
    void SetRequestSequenceNumber(uint32_t value);
    uint32_t GetRequestSequenceNumber() const;

  private:
    uint8_t m_version{VERSION_1};
    uint8_t m_reservedVersion{0};
    uint32_t m_destinationConnectionId{0};
    uint32_t m_destinationFunction{0};
    FalconProtocolType m_protocolType{FalconProtocolType::RDMA};
    FalconPacketType m_packetType{FalconPacketType::PUSH_DATA};
    bool m_ackRequest{false};
    uint32_t m_receiverDataWindowBase{0};
    uint32_t m_receiverRequestWindowBase{0};
    uint32_t m_packetSequenceNumber{0};
    uint32_t m_requestSequenceNumber{0};
};

/** Four-byte Push Data suffix: reserved zero followed by request length. */
class FalconPushDataHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetRequestLength(uint16_t value);
    uint16_t GetRequestLength() const;
    bool HasValidReservedField() const;

  private:
    uint16_t m_reserved{0};
    uint16_t m_requestLength{0};
};

/** Eight-byte Pull Request suffix: reserved, request length, then another reserved field. */
class FalconPullRequestHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 8;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetRequestLength(uint16_t value);
    uint16_t GetRequestLength() const;
    bool HasValidReservedField() const;

  private:
    uint16_t m_reservedPrefix{0};
    uint16_t m_requestLength{0};
    uint32_t m_reservedSuffix{0};
};

} // namespace ns3

#endif // FALCON_HEADER_H
