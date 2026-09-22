/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_NSCC_H
#define UET_NSCC_H

#include "ns3/nstime.h"
#include "ns3/object.h"

#include <cstdint>

namespace ns3
{

/** Sender-side Network-Signal Congestion Control state for one CCC. */
class UetNscc : public Object
{
  public:
    static TypeId GetTypeId();

    UetNscc();
    ~UetNscc() override;

    void Initialize(uint32_t mtu, uint32_t initialWindow);
    bool CanSend(uint32_t bytes) const;
    void OnPacketSent(uint32_t bytes);
    uint32_t OnAck(uint32_t bytes,
                   bool ecnMarked,
                   Time measuredRtt,
                   Time serviceTime,
                   uint8_t receiverCwndPending,
                   bool restoreCwnd);
    uint32_t OnLoss(uint32_t bytes);

    uint32_t GetCongestionWindow() const;
    void SetCongestionWindow(uint32_t bytes);
    void SetLineRateBps(uint64_t lineRateBps);
    uint64_t GetLineRateBps() const;
    int64_t GetInflightBytes() const;
    Time GetBaseRtt() const;
    Time GetPacingDelay(uint32_t bytes) const;

  private:
    uint32_t m_mtu{4096};
    uint32_t m_cwnd{65536};
    uint32_t m_savedCwnd{0};
    uint32_t m_maxWindow{225000};
    uint32_t m_minWindow{4096};
    int64_t m_inflight{0};
    Time m_baseRtt{MicroSeconds(12)};
    Time m_targetQueueDelay{MicroSeconds(12)};
    uint64_t m_lineRateBps{100000000000ULL};
    uint64_t m_acknowledgedSinceIncrease{0};
};

} // namespace ns3

#endif // UET_NSCC_H
