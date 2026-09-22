/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_SIMULATION_TAG_H
#define UET_SIMULATION_TAG_H

#include "ns3/tag.h"

#include <cstdint>

namespace ns3
{

/**
 * Simulation-only routing metadata that is not part of the modeled wire image.
 *
 * IP/UDP encapsulation is outside the current UET module. This packet tag lets
 * endpoint tests model the corresponding source, destination, and entropy path
 * without adding non-UEC bytes to packet size.
 */
class UetSimulationTag : public Tag
{
  public:
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer buffer) const override;
    void Deserialize(TagBuffer buffer) override;
    void Print(std::ostream& os) const override;

    void SetSourceEndpointId(uint32_t value);
    uint32_t GetSourceEndpointId() const;
    void SetDestinationEndpointId(uint32_t value);
    uint32_t GetDestinationEndpointId() const;
    void SetPathId(uint32_t value);
    uint32_t GetPathId() const;
    void SetTrimmed(bool value);
    bool IsTrimmed() const;
    void SetOriginalPayloadLength(uint32_t value);
    uint32_t GetOriginalPayloadLength() const;
    void SetEcnMarked(bool value);
    bool IsEcnMarked() const;

  private:
    uint32_t m_sourceEndpointId{0};
    uint32_t m_destinationEndpointId{0};
    uint32_t m_pathId{0};
    bool m_trimmed{false};
    uint32_t m_originalPayloadLength{0};
    bool m_ecnMarked{false};
};

} // namespace ns3

#endif // UET_SIMULATION_TAG_H
