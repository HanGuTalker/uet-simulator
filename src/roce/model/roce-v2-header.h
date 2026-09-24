/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef ROCE_V2_HEADER_H
#define ROCE_V2_HEADER_H

#include "ns3/header.h"
#include "ns3/packet.h"
#include "ns3/trailer.h"

#include <cstdint>

namespace ns3
{

/** InfiniBand transport opcodes used by the RoCEv2 RC model. */
enum class RoceOpcode : uint8_t
{
    RC_SEND_FIRST = 0x00,
    RC_SEND_MIDDLE = 0x01,
    RC_SEND_LAST = 0x02,
    RC_SEND_ONLY = 0x04,
    RC_WRITE_FIRST = 0x06,
    RC_WRITE_MIDDLE = 0x07,
    RC_WRITE_LAST = 0x08,
    RC_WRITE_ONLY = 0x0a,
    RC_READ_REQUEST = 0x0c,
    RC_READ_RESPONSE_FIRST = 0x0d,
    RC_READ_RESPONSE_MIDDLE = 0x0e,
    RC_READ_RESPONSE_LAST = 0x0f,
    RC_READ_RESPONSE_ONLY = 0x10,
    RC_ACK = 0x11,
    RC_SACK = 0x18,
    RC_ACK_RSP = 0x19,
    RC_SACK_RSP = 0x1e,
    CNP = 0x81,
    RTT_REQUEST = 0x82,
    RTT_RESPONSE = 0x83,
    SLOW_PATH = 0x84,
};

/** 12-byte InfiniBand Base Transport Header carried by RoCEv2. */
class RoceBthHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 12;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetOpcode(RoceOpcode value);
    RoceOpcode GetOpcode() const;
    void SetSolicited(bool value);
    bool IsSolicited() const;
    void SetPadCount(uint8_t value);
    uint8_t GetPadCount() const;
    void SetPartitionKey(uint16_t value);
    uint16_t GetPartitionKey() const;
    void SetDestinationQp(uint32_t value);
    uint32_t GetDestinationQp() const;
    void SetAckRequest(bool value);
    bool IsAckRequested() const;
    void SetRetransmission(bool value);
    bool IsRetransmission() const;
    void SetTimestampHeader(bool value);
    bool HasTimestampHeader() const;
    void SetPacketSequence(uint32_t value);
    uint32_t GetPacketSequence() const;

  private:
    RoceOpcode m_opcode{RoceOpcode::RC_SEND_ONLY};
    bool m_solicited{false};
    uint8_t m_padCount{0};
    uint16_t m_partitionKey{0xffff};
    uint32_t m_destinationQp{0};
    bool m_ackRequest{false};
    bool m_retransmission{false};
    bool m_timestampHeader{false};
    uint32_t m_packetSequence{0};
};

/** 16-byte RDMA Extended Transport Header. */
class RoceRethHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 16;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetVirtualAddress(uint64_t value);
    uint64_t GetVirtualAddress() const;
    void SetRemoteKey(uint32_t value);
    uint32_t GetRemoteKey() const;
    void SetDmaLength(uint32_t value);
    uint32_t GetDmaLength() const;

  private:
    uint64_t m_virtualAddress{0};
    uint32_t m_remoteKey{0};
    uint32_t m_dmaLength{0};
};

/** 4-byte ACK Extended Transport Header: syndrome plus 24-bit MSN. */
class RoceAethHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;
    static constexpr uint8_t ACK_SYNDROME = 0x00;
    static constexpr uint8_t SEQUENCE_NAK_SYNDROME = 0x60;
    static constexpr uint8_t INVALID_REQUEST_NAK_SYNDROME = 0x61;
    static constexpr uint8_t REMOTE_ACCESS_NAK_SYNDROME = 0x62;
    static constexpr uint8_t REMOTE_OPERATION_NAK_SYNDROME = 0x63;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetSyndrome(uint8_t value);
    uint8_t GetSyndrome() const;
    void SetMessageSequence(uint32_t value);
    uint32_t GetMessageSequence() const;

  private:
    uint8_t m_syndrome{ACK_SYNDROME};
    uint32_t m_messageSequence{0};
};

/** 16-byte congestion notification payload following a CNP BTH. */
class RoceCnpHeader : public Header
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 16;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetSourceQp(uint32_t value);
    uint32_t GetSourceQp() const;

  private:
    uint32_t m_sourceQp{0};
};

/** Four-byte invariant CRC trailer for the modeled UDP payload. */
class RoceInvariantCrcTrailer : public Trailer
{
  public:
    static constexpr uint32_t SERIALIZED_SIZE = 4;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator end) const override;
    uint32_t Deserialize(Buffer::Iterator end) override;
    void Print(std::ostream& os) const override;

    void SetCrc(uint32_t value);
    uint32_t GetCrc() const;
    static uint32_t Calculate(Ptr<const Packet> packet);

  private:
    uint32_t m_crc{0};
};

bool IsRoceDataOpcode(RoceOpcode opcode);
bool IsRoceFirstOpcode(RoceOpcode opcode);
bool IsRoceLastOpcode(RoceOpcode opcode);
bool IsRoceWriteOpcode(RoceOpcode opcode);
bool IsRoceReadRequestOpcode(RoceOpcode opcode);
bool IsRoceReadResponseOpcode(RoceOpcode opcode);

} // namespace ns3

#endif // ROCE_V2_HEADER_H
