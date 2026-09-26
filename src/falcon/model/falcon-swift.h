/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef FALCON_SWIFT_H
#define FALCON_SWIFT_H

#include "ns3/nstime.h"

#include <cstdint>

namespace ns3
{

struct FalconSwiftConfig
{
    double minFcwnd{0.25};
    double maxFcwnd{128.0};
    double minNcwnd{1.0};
    double maxNcwnd{128.0};
    double fabricAdditiveIncrement{1.0};
    double fabricMultiplicativeDecreaseFactor{0.5};
    double maxFabricMultiplicativeDecreaseFactor{0.5};
    double nicAdditiveIncrement{1.0};
    double maxNicMultiplicativeDecreaseFactor{0.5};
    uint8_t targetRxBufferLevel{16};
    double rttSmoothingAlpha{0.875};
    double delaySmoothingAlpha{0.875};
    Time baseDelayTarget{MicroSeconds(12)};
    double retransmitTimeoutScalar{2.0};
    uint32_t retransmitLimit{3};
    Time minRetransmissionTimeout{MicroSeconds(5)};
};

/** Configurable implementation of the Falcon 1.1 Swift pseudocode. */
class FalconSwift
{
  public:
    explicit FalconSwift(const FalconSwiftConfig& config = {});

    void Initialize(double fcwnd, double ncwnd, Time initialRtt);
    void ProcessAck(Time now,
                    Time rtt,
                    Time fabricDelay,
                    uint32_t packetsAcknowledged,
                    uint8_t rxBufferLevel);
    void ProcessRetransmit(Time now);
    void ProcessResourceExhaustionNack(Time now,
                                       Time rtt,
                                       Time fabricDelay,
                                       uint32_t packetsAcknowledged,
                                       uint8_t rxBufferLevel);

    double GetFabricWindow() const;
    double GetNicWindow() const;
    double GetEffectiveWindow() const;
    Time GetSmoothedRtt() const;
    Time GetSmoothedDelay() const;
    Time GetInterPacketGap() const;
    Time GetRetransmissionTimeout() const;
    uint32_t GetRetransmitCount() const;

  private:
    enum class NicDirection : uint8_t
    {
        INCREASE,
        DECREASE,
    };

    bool HasRttElapsed(Time now, Time marker) const;
    void UpdateFabricWindow(Time now, uint32_t packetsAcknowledged);
    void UpdateNicWindow(Time now, uint8_t rxBufferLevel, bool forceDecrease);
    void UpdateDerivedValues();

    FalconSwiftConfig m_config;
    double m_fcwnd{1.0};
    double m_ncwnd{1.0};
    Time m_smoothedRtt{MicroSeconds(12)};
    Time m_smoothedDelay{MicroSeconds(12)};
    Time m_fabricWindowMarker{Seconds(0)};
    Time m_nicWindowMarker{Seconds(0)};
    NicDirection m_nicDirection{NicDirection::INCREASE};
    Time m_interPacketGap{Seconds(0)};
    Time m_retransmissionTimeout{MicroSeconds(50)};
    uint32_t m_retransmitCount{0};
};

} // namespace ns3

#endif // FALCON_SWIFT_H
