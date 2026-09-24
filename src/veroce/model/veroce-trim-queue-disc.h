/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef VEROCE_TRIM_QUEUE_DISC_H
#define VEROCE_TRIM_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/traced-callback.h"

#include <cstdint>

namespace ns3
{

/** FIFO switch queue that trims eligible veRoCE data packets under congestion. */
class VeRoceTrimQueueDisc : public QueueDisc
{
  public:
    using TrimTracedCallback = void (*)(Ptr<const Packet>, uint32_t, uint32_t);

    static constexpr uint8_t TRIMMABLE_DSCP = 8;
    static constexpr uint8_t TRIMMED_DSCP = 9;
    static constexpr const char* LIMIT_EXCEEDED_DROP = "veRoCE trim queue limit exceeded";
    static constexpr const char* CONGESTION_MARK = "veRoCE trim queue congestion mark";

    static TypeId GetTypeId();
    VeRoceTrimQueueDisc();
    ~VeRoceTrimQueueDisc() override;

  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;
    bool CheckConfig() override;
    void InitializeParams() override;
    Ptr<QueueDiscItem> Trim(Ptr<QueueDiscItem> item);

    uint32_t m_markThresholdBytes{8192};
    uint32_t m_trimThresholdBytes{16384};
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_trimTrace;
};

} // namespace ns3

#endif // VEROCE_TRIM_QUEUE_DISC_H
