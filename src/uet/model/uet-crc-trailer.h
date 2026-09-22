/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_CRC_TRAILER_H
#define UET_CRC_TRAILER_H

#include "ns3/packet.h"
#include "ns3/trailer.h"

#include <cstdint>

namespace ns3
{

/** UET CRC32C (Castagnoli) trailer used when transport data protection is CRC. */
class UetCrcTrailer : public Trailer
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

} // namespace ns3

#endif // UET_CRC_TRAILER_H
