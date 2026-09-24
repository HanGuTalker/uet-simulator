/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "mrc-nscc.h"

#include <algorithm>

namespace ns3
{

void
MrcNscc::Initialize(uint32_t mtu,
                    uint32_t initialWindow,
                    uint32_t maximumWindow,
                    Time baseRtt,
                    Time targetQueueDelay)
{
    m_mtu = std::max(1U, mtu);
    // One full nominal packet (payload MTU plus the MRC nominal header allowance).
    m_minWindow = m_mtu + 40;
    m_maxWindow = std::max(m_minWindow, maximumWindow);
    m_cwnd = std::clamp(initialWindow, m_minWindow, m_maxWindow);
    m_baseRtt = baseRtt;
    m_targetQueueDelay = targetQueueDelay;
    m_acknowledgedSinceIncrease = 0;
}

uint32_t
MrcNscc::OnAck(uint32_t newlyReceivedBytes, bool ecnMarked, Time measuredRtt)
{
    if (measuredRtt < m_baseRtt)
    {
        m_baseRtt = measuredRtt;
    }
    const Time queueDelay = measuredRtt > m_baseRtt ? measuredRtt - m_baseRtt : NanoSeconds(0);
    if (ecnMarked && queueDelay >= m_targetQueueDelay)
    {
        m_cwnd = std::max(m_minWindow, m_cwnd / 2);
        m_acknowledgedSinceIncrease = 0;
    }
    else if (!ecnMarked)
    {
        m_acknowledgedSinceIncrease += newlyReceivedBytes;
        if (m_acknowledgedSinceIncrease >= m_cwnd)
        {
            m_cwnd = std::min(m_maxWindow, m_cwnd + m_mtu);
            m_acknowledgedSinceIncrease = 0;
        }
    }
    return m_cwnd;
}

uint32_t
MrcNscc::OnLoss()
{
    m_cwnd = std::max(m_minWindow, m_cwnd / 2);
    m_acknowledgedSinceIncrease = 0;
    return m_cwnd;
}

void
MrcNscc::SetCongestionWindow(uint32_t bytes)
{
    m_cwnd = std::clamp(bytes, m_minWindow, m_maxWindow);
    m_acknowledgedSinceIncrease = 0;
}

uint32_t
MrcNscc::GetCongestionWindow() const
{
    return m_cwnd;
}

Time
MrcNscc::GetBaseRtt() const
{
    return m_baseRtt;
}

} // namespace ns3
