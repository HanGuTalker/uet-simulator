/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-crc-trailer.h"

#include <vector>

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(UetCrcTrailer);

TypeId
UetCrcTrailer::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetCrcTrailer")
                            .SetParent<Trailer>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetCrcTrailer>();
    return tid;
}

TypeId UetCrcTrailer::GetInstanceTypeId() const { return GetTypeId(); }
uint32_t UetCrcTrailer::GetSerializedSize() const { return SERIALIZED_SIZE; }

void
UetCrcTrailer::Serialize(Buffer::Iterator end) const
{
    end.Prev(SERIALIZED_SIZE);
    end.WriteHtonU32(m_crc);
}

uint32_t
UetCrcTrailer::Deserialize(Buffer::Iterator end)
{
    end.Prev(SERIALIZED_SIZE);
    m_crc = end.ReadNtohU32();
    return SERIALIZED_SIZE;
}

void UetCrcTrailer::Print(std::ostream& os) const { os << "crc32c=0x" << std::hex << m_crc << std::dec; }
void UetCrcTrailer::SetCrc(uint32_t value) { m_crc = value; }
uint32_t UetCrcTrailer::GetCrc() const { return m_crc; }

uint32_t
UetCrcTrailer::Calculate(Ptr<const Packet> packet)
{
    std::vector<uint8_t> bytes(packet ? packet->GetSize() : 0);
    if (packet && !bytes.empty())
    {
        packet->CopyData(bytes.data(), bytes.size());
    }
    uint32_t crc = 0xffffffffU;
    for (uint8_t byte : bytes)
    {
        crc ^= byte;
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0x82f63b78U : 0U);
        }
    }
    return ~crc;
}

} // namespace ns3
