/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_PDC_H
#define UET_PDC_H

#include "uet-types.h"

#include "ns3/nstime.h"
#include "ns3/object.h"

#include <cstdint>

namespace ns3
{

class UetHeader;

/**
 * @ingroup uet
 * Configuration and lifecycle state for one Packet Delivery Context.
 */
class UetPdc : public Object
{
  public:
    static TypeId GetTypeId();

    UetPdc();
    ~UetPdc() override;

    uint32_t GetPdcId() const;
    void SetRemotePdcId(uint16_t pdcId);
    uint16_t GetRemotePdcId() const;
    UetDeliveryMode GetDeliveryMode() const;
    Time GetRetransmissionTimeout() const;
    uint32_t GetInitialCongestionWindow() const;
    void ConfigureStartPsn(uint32_t psn);
    bool HasConfiguredStartPsn() const;
    uint32_t GetConfiguredStartPsn() const;
    UetPdcState GetState() const;
    bool CanTransitionTo(UetPdcState newState) const;
    bool TransitionTo(UetPdcState newState);

    bool AcceptPacket(const UetHeader& header);
    uint64_t GetReceivedPacketCount() const;
    uint32_t GetLastReceivedSequenceNumber() const;
    uint64_t GetLastReceivedMessageId() const;

  private:
    uint32_t m_pdcId{0};
    uint16_t m_remotePdcId{0};
    UetDeliveryMode m_deliveryMode{UetDeliveryMode::RUD};
    Time m_retransmissionTimeout{MicroSeconds(20)};
    uint32_t m_initialCongestionWindow{65536};
    bool m_hasConfiguredStartPsn{false};
    uint32_t m_configuredStartPsn{0};
    UetPdcState m_state{UetPdcState::CLOSED};
    uint64_t m_receivedPacketCount{0};
    uint32_t m_lastReceivedSequenceNumber{0};
    uint64_t m_lastReceivedMessageId{0};
};

} // namespace ns3

#endif // UET_PDC_H
