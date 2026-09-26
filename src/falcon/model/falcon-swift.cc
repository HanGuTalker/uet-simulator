/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "falcon-swift.h"

#include <algorithm>

namespace ns3
{

FalconSwift::FalconSwift(const FalconSwiftConfig& config)
    : m_config(config)
{
    Initialize(config.maxFcwnd, config.maxNcwnd, config.baseDelayTarget);
}

void
FalconSwift::Initialize(double fcwnd, double ncwnd, Time initialRtt)
{
    m_fcwnd = std::clamp(fcwnd, m_config.minFcwnd, m_config.maxFcwnd);
    m_ncwnd = std::clamp(ncwnd, m_config.minNcwnd, m_config.maxNcwnd);
    m_smoothedRtt = initialRtt;
    m_smoothedDelay = m_config.baseDelayTarget;
    m_fabricWindowMarker = Seconds(0);
    m_nicWindowMarker = Seconds(0);
    m_nicDirection = NicDirection::INCREASE;
    m_retransmitCount = 0;
    UpdateDerivedValues();
}

bool
FalconSwift::HasRttElapsed(Time now, Time marker) const
{
    return now - marker >= m_smoothedRtt;
}

void
FalconSwift::UpdateFabricWindow(Time now, uint32_t packetsAcknowledged)
{
    const double delay = std::max(1.0, static_cast<double>(m_smoothedDelay.GetNanoSeconds()));
    const double target = static_cast<double>(m_config.baseDelayTarget.GetNanoSeconds());
    const double previous = m_fcwnd;
    if (delay <= target)
    {
        const double divisor = m_fcwnd >= 1.0 ? m_fcwnd : 1.0;
        m_fcwnd += m_config.fabricAdditiveIncrement * packetsAcknowledged / divisor;
    }
    else if (HasRttElapsed(now, m_fabricWindowMarker))
    {
        const double proportional =
            m_config.fabricMultiplicativeDecreaseFactor * (delay - target) / delay;
        const double reduction =
            std::min(proportional, m_config.maxFabricMultiplicativeDecreaseFactor);
        m_fcwnd *= 1.0 - reduction;
    }
    m_fcwnd = std::clamp(m_fcwnd, m_config.minFcwnd, m_config.maxFcwnd);
    if (m_fcwnd < previous || m_fcwnd == m_config.minFcwnd)
    {
        m_fabricWindowMarker = now;
    }
    else if (now - m_fabricWindowMarker > m_smoothedRtt)
    {
        m_fabricWindowMarker = now - m_smoothedRtt;
    }
}

void
FalconSwift::UpdateNicWindow(Time now, uint8_t rxBufferLevel, bool forceDecrease)
{
    const double previous = m_ncwnd;
    if (!forceDecrease && rxBufferLevel < m_config.targetRxBufferLevel)
    {
        if (m_nicDirection == NicDirection::DECREASE || HasRttElapsed(now, m_nicWindowMarker))
        {
            m_ncwnd += m_config.nicAdditiveIncrement;
            m_nicDirection = NicDirection::INCREASE;
        }
    }
    else if (m_nicDirection == NicDirection::INCREASE || HasRttElapsed(now, m_nicWindowMarker))
    {
        double reduction = m_config.maxNicMultiplicativeDecreaseFactor;
        if (!forceDecrease && rxBufferLevel != 0)
        {
            reduction = std::min(static_cast<double>(rxBufferLevel - m_config.targetRxBufferLevel) /
                                     rxBufferLevel,
                                 m_config.maxNicMultiplicativeDecreaseFactor);
        }
        m_ncwnd *= 1.0 - reduction;
        m_nicDirection = NicDirection::DECREASE;
    }
    m_ncwnd = std::clamp(m_ncwnd, m_config.minNcwnd, m_config.maxNcwnd);
    if (m_ncwnd != previous || m_ncwnd == m_config.minNcwnd || m_ncwnd == m_config.maxNcwnd)
    {
        m_nicWindowMarker = now;
    }
    else if (now - m_nicWindowMarker > m_smoothedRtt)
    {
        m_nicWindowMarker = now - m_smoothedRtt;
    }
}

void
FalconSwift::ProcessAck(Time now,
                        Time rtt,
                        Time fabricDelay,
                        uint32_t packetsAcknowledged,
                        uint8_t rxBufferLevel)
{
    const double rttAlpha = m_config.rttSmoothingAlpha;
    const double delayAlpha = m_config.delaySmoothingAlpha;
    m_smoothedRtt = NanoSeconds(static_cast<int64_t>(rttAlpha * m_smoothedRtt.GetNanoSeconds() +
                                                     (1.0 - rttAlpha) * rtt.GetNanoSeconds()));
    m_smoothedDelay =
        NanoSeconds(static_cast<int64_t>(delayAlpha * m_smoothedDelay.GetNanoSeconds() +
                                         (1.0 - delayAlpha) * fabricDelay.GetNanoSeconds()));
    UpdateFabricWindow(now, packetsAcknowledged);
    UpdateNicWindow(now, rxBufferLevel, false);
    m_retransmitCount = 0;
    UpdateDerivedValues();
}

void
FalconSwift::ProcessRetransmit(Time now)
{
    ++m_retransmitCount;
    if (m_retransmitCount == 1 && HasRttElapsed(now, m_fabricWindowMarker))
    {
        m_fcwnd *= 1.0 - m_config.maxFabricMultiplicativeDecreaseFactor;
        m_fabricWindowMarker = now;
    }
    else if (m_retransmitCount >= m_config.retransmitLimit)
    {
        m_fcwnd = m_config.minFcwnd;
        m_fabricWindowMarker = now;
    }
    m_fcwnd = std::clamp(m_fcwnd, m_config.minFcwnd, m_config.maxFcwnd);
    UpdateDerivedValues();
}

void
FalconSwift::ProcessResourceExhaustionNack(Time now,
                                           Time rtt,
                                           Time fabricDelay,
                                           uint32_t packetsAcknowledged,
                                           uint8_t rxBufferLevel)
{
    ProcessAck(now, rtt, fabricDelay, packetsAcknowledged, rxBufferLevel);
    UpdateNicWindow(now, rxBufferLevel, true);
    UpdateDerivedValues();
}

void
FalconSwift::UpdateDerivedValues()
{
    m_interPacketGap =
        m_fcwnd < 1.0 ? NanoSeconds(static_cast<int64_t>(m_smoothedRtt.GetNanoSeconds() / m_fcwnd))
                      : Seconds(0);
    m_retransmissionTimeout =
        std::max(NanoSeconds(static_cast<int64_t>(m_config.retransmitTimeoutScalar *
                                                  m_smoothedRtt.GetNanoSeconds())),
                 m_config.minRetransmissionTimeout);
}

double
FalconSwift::GetFabricWindow() const
{
    return m_fcwnd;
}

double
FalconSwift::GetNicWindow() const
{
    return m_ncwnd;
}

double
FalconSwift::GetEffectiveWindow() const
{
    return std::min(m_fcwnd, m_ncwnd);
}

Time
FalconSwift::GetSmoothedRtt() const
{
    return m_smoothedRtt;
}

Time
FalconSwift::GetSmoothedDelay() const
{
    return m_smoothedDelay;
}

Time
FalconSwift::GetInterPacketGap() const
{
    return m_interPacketGap;
}

Time
FalconSwift::GetRetransmissionTimeout() const
{
    return m_retransmissionTimeout;
}

uint32_t
FalconSwift::GetRetransmitCount() const
{
    return m_retransmitCount;
}

} // namespace ns3
