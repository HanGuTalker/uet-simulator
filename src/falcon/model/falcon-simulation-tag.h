/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef FALCON_SIMULATION_TAG_H
#define FALCON_SIMULATION_TAG_H

#include "ns3/tag.h"

#include <cstdint>

namespace ns3
{

/** Simulation-only message metadata; it contributes no bytes to the Falcon wire image. */
class FalconSimulationTag : public Tag
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
    void SetConnectionId(uint32_t value);
    uint32_t GetConnectionId() const;
    void SetMessageId(uint64_t value);
    uint64_t GetMessageId() const;
    void SetPayloadBytes(uint32_t value);
    uint32_t GetPayloadBytes() const;
    void SetTotalMessageBytes(uint32_t value);
    uint32_t GetTotalMessageBytes() const;
    void SetSubmittedTimeNs(uint64_t value);
    uint64_t GetSubmittedTimeNs() const;
    void SetFragment(uint32_t value);
    uint32_t GetFragment() const;
    void SetFragmentCount(uint32_t value);
    uint32_t GetFragmentCount() const;

  private:
    uint32_t m_sourceEndpointId{0};
    uint32_t m_destinationEndpointId{0};
    uint32_t m_connectionId{0};
    uint64_t m_messageId{0};
    uint32_t m_payloadBytes{0};
    uint32_t m_totalMessageBytes{0};
    uint64_t m_submittedTimeNs{0};
    uint32_t m_fragment{0};
    uint32_t m_fragmentCount{0};
};

} // namespace ns3

#endif // FALCON_SIMULATION_TAG_H
