/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef FALCON_RELIABILITY_H
#define FALCON_RELIABILITY_H

#include "falcon-control-header.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace ns3
{

enum class FalconReliabilityWindow : uint8_t
{
    DATA,
    REQUEST,
};

enum class FalconReceiveResult : uint8_t
{
    ACCEPTED,
    DUPLICATE,
    OUT_OF_WINDOW,
};

/**
 * Protocol state for Falcon's independent data and request reliability windows.
 *
 * This class is deliberately independent of sockets and congestion control.  It owns PSN
 * allocation, receiver window/bitmap state and sender ACK processing so the later transport
 * adapter can exercise the same state machine over any ns-3 topology.
 */
class FalconReliabilityManager
{
  public:
    static constexpr uint32_t DATA_BITMAP_BITS = 128;
    static constexpr uint32_t REQUEST_BITMAP_BITS = 64;

    explicit FalconReliabilityManager(uint32_t initialDataPsn = 0,
                                      uint32_t initialRequestPsn = 0);

    uint32_t AllocatePsn(FalconReliabilityWindow window);
    void TrackTransmitted(FalconReliabilityWindow window, uint32_t psn);

    FalconReceiveResult ReceiveData(uint32_t psn);
    bool AcknowledgeData(uint32_t psn);
    FalconReceiveResult ReceiveRequest(uint32_t psn);

    FalconBackHeader BuildBack(uint32_t connectionId,
                               uint32_t timestamp1 = 0,
                               uint32_t timestamp2 = 0,
                               uint64_t congestionMetadata = 0) const;
    FalconEackHeader BuildEack(uint32_t connectionId,
                               uint32_t timestamp1 = 0,
                               uint32_t timestamp2 = 0,
                               uint64_t congestionMetadata = 0) const;

    std::vector<uint32_t> ProcessBack(const FalconBackHeader& back);
    std::vector<uint32_t> ProcessEack(const FalconEackHeader& eack);
    bool ProcessNack(const FalconNackHeader& nack, uint32_t& retransmitPsn) const;

    uint32_t GetReceiverDataWindowBase() const;
    uint32_t GetReceiverRequestWindowBase() const;
    uint32_t GetOutstandingCount(FalconReliabilityWindow window) const;
    bool IsOutstanding(FalconReliabilityWindow window, uint32_t psn) const;
    bool IsPeerReceivedData(uint32_t psn) const;

  private:
    using Bitmap128 = FalconEackHeader::Bitmap128;

    static bool IsRepresentable(uint32_t base, uint32_t psn, uint32_t width);
    static void SetBitmapBit(Bitmap128& bitmap, uint32_t offset);
    static bool GetBitmapBit(const Bitmap128& bitmap, uint32_t offset);
    static uint64_t BuildBitmap64(uint32_t base, const std::set<uint32_t>& values);
    static Bitmap128 BuildBitmap128(uint32_t base, const std::set<uint32_t>& values);

    FalconReceiveResult InsertReceived(std::set<uint32_t>& values,
                                       uint32_t base,
                                       uint32_t width,
                                       uint32_t psn);
    static void AdvanceBase(uint32_t& base, std::set<uint32_t>& completed);
    std::vector<uint32_t> ApplyCumulativeBase(std::map<uint32_t, bool>& outstanding,
                                               uint32_t base);

    uint32_t m_nextDataPsn;
    uint32_t m_nextRequestPsn;
    uint32_t m_receiverDataBase;
    uint32_t m_receiverRequestBase;
    std::set<uint32_t> m_receivedData;
    std::set<uint32_t> m_acknowledgedData;
    std::set<uint32_t> m_receivedRequests;
    // The bool records that the peer has received a data packet but has not yet ULP-ACKed it.
    std::map<uint32_t, bool> m_outstandingData;
    std::map<uint32_t, bool> m_outstandingRequests;
};

} // namespace ns3

#endif // FALCON_RELIABILITY_H
