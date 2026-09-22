/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-pdc.h"

#include "uet-header.h"

#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UetPdc");
NS_OBJECT_ENSURE_REGISTERED(UetPdc);

TypeId
UetPdc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::UetPdc")
            .SetParent<Object>()
            .SetGroupName("Uet")
            .AddConstructor<UetPdc>()
            .AddAttribute("PdcId",
                          "Simulation-local Packet Delivery Context identifier.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&UetPdc::m_pdcId),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("DeliveryMode",
                          "AI Base packet delivery service used by this PDC.",
                          EnumValue(UetDeliveryMode::RUD),
                          MakeEnumAccessor<UetDeliveryMode>(&UetPdc::m_deliveryMode),
                          MakeEnumChecker(UetDeliveryMode::RUD, "Rud", UetDeliveryMode::ROD, "Rod"))
            .AddAttribute("RetransmissionTimeout",
                          "Initial retransmission timeout; an assumed value until calibrated.",
                          TimeValue(MicroSeconds(20)),
                          MakeTimeAccessor(&UetPdc::m_retransmissionTimeout),
                          MakeTimeChecker(NanoSeconds(1)))
            .AddAttribute("InitialCongestionWindow",
                          "Initial NSCC outstanding-byte limit.",
                          UintegerValue(65536),
                          MakeUintegerAccessor(&UetPdc::m_initialCongestionWindow),
                          MakeUintegerChecker<uint32_t>(1));
    return tid;
}

UetPdc::UetPdc()
{
    NS_LOG_FUNCTION(this);
}

UetPdc::~UetPdc()
{
    NS_LOG_FUNCTION(this);
}

uint32_t
UetPdc::GetPdcId() const
{
    return m_pdcId;
}

void
UetPdc::SetRemotePdcId(uint16_t pdcId)
{
    m_remotePdcId = pdcId;
}

uint16_t
UetPdc::GetRemotePdcId() const
{
    return m_remotePdcId;
}

UetDeliveryMode
UetPdc::GetDeliveryMode() const
{
    return m_deliveryMode;
}

Time
UetPdc::GetRetransmissionTimeout() const
{
    return m_retransmissionTimeout;
}

uint32_t
UetPdc::GetInitialCongestionWindow() const
{
    return m_initialCongestionWindow;
}

void
UetPdc::ConfigureStartPsn(uint32_t psn)
{
    m_configuredStartPsn = psn;
    m_hasConfiguredStartPsn = true;
}

bool
UetPdc::HasConfiguredStartPsn() const
{
    return m_hasConfiguredStartPsn;
}

uint32_t
UetPdc::GetConfiguredStartPsn() const
{
    return m_configuredStartPsn;
}

UetPdcState
UetPdc::GetState() const
{
    return m_state;
}

bool
UetPdc::CanTransitionTo(UetPdcState newState) const
{
    switch (m_state)
    {
    case UetPdcState::CLOSED:
        return newState == UetPdcState::OPENING;
    case UetPdcState::OPENING:
        return newState == UetPdcState::ACTIVE || newState == UetPdcState::CLOSED ||
               newState == UetPdcState::ERROR;
    case UetPdcState::ACTIVE:
        return newState == UetPdcState::CLOSING || newState == UetPdcState::ERROR;
    case UetPdcState::CLOSING:
        return newState == UetPdcState::CLOSED || newState == UetPdcState::ERROR;
    case UetPdcState::ERROR:
        return newState == UetPdcState::CLOSED;
    }
    return false;
}

bool
UetPdc::TransitionTo(UetPdcState newState)
{
    if (!CanTransitionTo(newState))
    {
        NS_LOG_WARN("Rejected PDC " << m_pdcId << " state transition from "
                                    << static_cast<uint32_t>(m_state) << " to "
                                    << static_cast<uint32_t>(newState));
        return false;
    }
    m_state = newState;
    return true;
}

bool
UetPdc::AcceptPacket(const UetHeader& header)
{
    if (m_state != UetPdcState::ACTIVE || header.GetPdcId() != m_pdcId ||
        header.GetDeliveryMode() != m_deliveryMode)
    {
        return false;
    }

    ++m_receivedPacketCount;
    m_lastReceivedSequenceNumber = header.GetSequenceNumber();
    m_lastReceivedMessageId = header.GetMessageId();
    return true;
}

uint64_t
UetPdc::GetReceivedPacketCount() const
{
    return m_receivedPacketCount;
}

uint32_t
UetPdc::GetLastReceivedSequenceNumber() const
{
    return m_lastReceivedSequenceNumber;
}

uint64_t
UetPdc::GetLastReceivedMessageId() const
{
    return m_lastReceivedMessageId;
}

} // namespace ns3
