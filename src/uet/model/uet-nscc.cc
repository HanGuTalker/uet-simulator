/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-nscc.h"

#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UetNscc");
NS_OBJECT_ENSURE_REGISTERED(UetNscc);

TypeId
UetNscc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::UetNscc")
            .SetParent<Object>()
            .SetGroupName("Uet")
            .AddConstructor<UetNscc>()
            .AddAttribute("MaximumWindow",
                          "NSCC MaxWnd in bytes (default 1.5 times the 150000-byte base BDP).",
                          UintegerValue(225000),
                          MakeUintegerAccessor(&UetNscc::m_maxWindow),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("BaseRtt",
                          "Configured unloaded longest-path RTT.",
                          TimeValue(MicroSeconds(12)),
                          MakeTimeAccessor(&UetNscc::m_baseRtt),
                          MakeTimeChecker(NanoSeconds(128)))
            .AddAttribute("TargetQueueDelay",
                          "Configured NSCC target queueing delay.",
                          TimeValue(MicroSeconds(12)),
                          MakeTimeAccessor(&UetNscc::m_targetQueueDelay),
                          MakeTimeChecker(NanoSeconds(128)))
            .AddAttribute("LineRateBps",
                          "Source line rate used by the pacing model.",
                          UintegerValue(100000000000ULL),
                          MakeUintegerAccessor(&UetNscc::m_lineRateBps),
                          MakeUintegerChecker<uint64_t>(1));
    return tid;
}

UetNscc::UetNscc() = default;
UetNscc::~UetNscc() = default;

void
UetNscc::Initialize(uint32_t mtu, uint32_t initialWindow)
{
    m_mtu = std::max(1U, mtu);
    m_minWindow = m_mtu;
    m_cwnd = std::clamp(initialWindow, m_minWindow, m_maxWindow);
    m_inflight = 0;
    m_savedCwnd = 0;
    m_acknowledgedSinceIncrease = 0;
}

bool
UetNscc::CanSend(uint32_t bytes) const
{
    return m_inflight <= 0 || static_cast<uint64_t>(m_inflight) + bytes <= m_cwnd;
}

void
UetNscc::OnPacketSent(uint32_t bytes)
{
    m_inflight += bytes;
}

uint32_t
UetNscc::OnAck(uint32_t bytes,
               bool ecnMarked,
               Time measuredRtt,
               Time serviceTime,
               uint8_t receiverCwndPending,
               bool restoreCwnd)
{
    m_inflight = std::max<int64_t>(0, m_inflight - bytes);
    if (measuredRtt > serviceTime)
    {
        const Time networkRtt = measuredRtt - serviceTime;
        if (networkRtt < m_baseRtt)
        {
            m_baseRtt = networkRtt;
        }
        const Time queueDelay = networkRtt > m_baseRtt ? networkRtt - m_baseRtt : NanoSeconds(0);
        if (ecnMarked && queueDelay >= m_targetQueueDelay)
        {
            // NSCC bounds any single multiplicative-decrease jump to 0.5.
            m_cwnd = std::max(m_minWindow, m_cwnd / 2);
            m_acknowledgedSinceIncrease = 0;
        }
        else
        {
            m_acknowledgedSinceIncrease += bytes;
            if (m_acknowledgedSinceIncrease >= m_cwnd)
            {
                // Fair additive increase, one nominal MTU per acknowledged window.
                m_cwnd = std::min(m_maxWindow, m_cwnd + m_mtu);
                m_acknowledgedSinceIncrease = 0;
            }
        }
    }

    if (receiverCwndPending != 0)
    {
        if (m_savedCwnd == 0)
        {
            m_savedCwnd = m_cwnd;
        }
        const uint32_t receiverLimit = std::max(
            m_minWindow,
            static_cast<uint32_t>((static_cast<uint64_t>(m_maxWindow) *
                                   (127U - std::min<uint8_t>(127, receiverCwndPending))) /
                                  127U));
        m_cwnd = std::min(m_cwnd, receiverLimit);
    }
    else if (restoreCwnd && m_savedCwnd != 0)
    {
        m_cwnd = std::min(m_savedCwnd, m_maxWindow);
        m_savedCwnd = 0;
    }
    return m_cwnd;
}

uint32_t
UetNscc::OnLoss(uint32_t bytes)
{
    m_inflight = std::max<int64_t>(0, m_inflight - bytes);
    m_cwnd = std::max(m_minWindow, m_cwnd / 2);
    m_acknowledgedSinceIncrease = 0;
    return m_cwnd;
}

uint32_t UetNscc::GetCongestionWindow() const { return m_cwnd; }

void
UetNscc::SetCongestionWindow(uint32_t bytes)
{
    m_cwnd = std::clamp(bytes, m_minWindow, m_maxWindow);
    m_acknowledgedSinceIncrease = 0;
}

void
UetNscc::SetLineRateBps(uint64_t lineRateBps)
{
    m_lineRateBps = std::max<uint64_t>(1, lineRateBps);
}

uint64_t
UetNscc::GetLineRateBps() const
{
    return m_lineRateBps;
}
int64_t UetNscc::GetInflightBytes() const { return m_inflight; }
Time UetNscc::GetBaseRtt() const { return m_baseRtt; }

Time
UetNscc::GetPacingDelay(uint32_t bytes) const
{
    const uint64_t serializationNs =
        std::max<uint64_t>(1, (static_cast<uint64_t>(bytes) * 8ULL * 1000000000ULL) / m_lineRateBps);
    const uint64_t windowRateBps =
        std::max<uint64_t>(1, (static_cast<uint64_t>(m_cwnd) * 8ULL * 1000000000ULL) /
                                  std::max<int64_t>(1, m_baseRtt.GetNanoSeconds()));
    const uint64_t pacedRate = std::min(m_lineRateBps, windowRateBps);
    const uint64_t pacedNs =
        std::max<uint64_t>(serializationNs,
                           (static_cast<uint64_t>(bytes) * 8ULL * 1000000000ULL) / pacedRate);
    return NanoSeconds(pacedNs);
}

} // namespace ns3
