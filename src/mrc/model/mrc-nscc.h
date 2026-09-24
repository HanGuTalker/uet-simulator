/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef MRC_NSCC_H
#define MRC_NSCC_H

#include "ns3/nstime.h"

#include <cstdint>

namespace ns3
{

/** Sender-side, SACK-clocked NSCC window state for one MRC QP. */
class MrcNscc
{
  public:
    void Initialize(uint32_t mtu,
                    uint32_t initialWindow,
                    uint32_t maximumWindow,
                    Time baseRtt,
                    Time targetQueueDelay);
    uint32_t OnAck(uint32_t newlyReceivedBytes, bool ecnMarked, Time measuredRtt);
    uint32_t OnLoss();
    void SetCongestionWindow(uint32_t bytes);
    uint32_t GetCongestionWindow() const;
    Time GetBaseRtt() const;

  private:
    uint32_t m_mtu{4096};
    uint32_t m_cwnd{65536};
    uint32_t m_minWindow{4096};
    uint32_t m_maxWindow{225000};
    uint64_t m_acknowledgedSinceIncrease{0};
    Time m_baseRtt{MicroSeconds(12)};
    Time m_targetQueueDelay{MicroSeconds(12)};
};

} // namespace ns3

#endif // MRC_NSCC_H
