/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "falcon-reliability.h"

#include "ns3/assert.h"

#include <limits>

namespace ns3
{

FalconReliabilityManager::FalconReliabilityManager(uint32_t initialDataPsn,
                                                   uint32_t initialRequestPsn)
    : m_nextDataPsn(initialDataPsn),
      m_nextRequestPsn(initialRequestPsn),
      m_receiverDataBase(initialDataPsn),
      m_receiverRequestBase(initialRequestPsn)
{
}

uint32_t
FalconReliabilityManager::AllocatePsn(FalconReliabilityWindow window)
{
    uint32_t& next = window == FalconReliabilityWindow::DATA ? m_nextDataPsn : m_nextRequestPsn;
    NS_ASSERT_MSG(next != std::numeric_limits<uint32_t>::max(),
                  "Falcon PSN wrap is not implemented");
    return next++;
}

void
FalconReliabilityManager::TrackTransmitted(FalconReliabilityWindow window, uint32_t psn)
{
    auto& outstanding = window == FalconReliabilityWindow::DATA ? m_outstandingData
                                                                : m_outstandingRequests;
    outstanding.emplace(psn, false);
}

bool
FalconReliabilityManager::IsRepresentable(uint32_t base, uint32_t psn, uint32_t width)
{
    return psn >= base && static_cast<uint64_t>(psn) < static_cast<uint64_t>(base) + width;
}

FalconReceiveResult
FalconReliabilityManager::InsertReceived(std::set<uint32_t>& values,
                                         uint32_t base,
                                         uint32_t width,
                                         uint32_t psn)
{
    if (psn < base || values.contains(psn))
    {
        return FalconReceiveResult::DUPLICATE;
    }
    if (!IsRepresentable(base, psn, width))
    {
        return FalconReceiveResult::OUT_OF_WINDOW;
    }
    values.insert(psn);
    return FalconReceiveResult::ACCEPTED;
}

FalconReceiveResult
FalconReliabilityManager::ReceiveData(uint32_t psn)
{
    return InsertReceived(m_receivedData, m_receiverDataBase, DATA_BITMAP_BITS, psn);
}

void
FalconReliabilityManager::AdvanceBase(uint32_t& base, std::set<uint32_t>& completed)
{
    while (completed.erase(base) != 0)
    {
        NS_ASSERT_MSG(base != std::numeric_limits<uint32_t>::max(),
                      "Falcon receiver PSN wrap is not implemented");
        ++base;
    }
}

bool
FalconReliabilityManager::AcknowledgeData(uint32_t psn)
{
    if (!m_receivedData.contains(psn) || psn < m_receiverDataBase)
    {
        return false;
    }
    m_acknowledgedData.insert(psn);
    AdvanceBase(m_receiverDataBase, m_acknowledgedData);
    for (auto it = m_receivedData.begin(); it != m_receivedData.end() && *it < m_receiverDataBase;)
    {
        it = m_receivedData.erase(it);
    }
    return true;
}

FalconReceiveResult
FalconReliabilityManager::ReceiveRequest(uint32_t psn)
{
    const FalconReceiveResult result = InsertReceived(m_receivedRequests,
                                                       m_receiverRequestBase,
                                                       REQUEST_BITMAP_BITS,
                                                       psn);
    if (result == FalconReceiveResult::ACCEPTED)
    {
        AdvanceBase(m_receiverRequestBase, m_receivedRequests);
    }
    return result;
}

void
FalconReliabilityManager::SetBitmapBit(Bitmap128& bitmap, uint32_t offset)
{
    NS_ASSERT(offset < DATA_BITMAP_BITS);
    bitmap[offset / 64] |= uint64_t{1} << (offset % 64);
}

bool
FalconReliabilityManager::GetBitmapBit(const Bitmap128& bitmap, uint32_t offset)
{
    NS_ASSERT(offset < DATA_BITMAP_BITS);
    return (bitmap[offset / 64] & (uint64_t{1} << (offset % 64))) != 0;
}

FalconReliabilityManager::Bitmap128
FalconReliabilityManager::BuildBitmap128(uint32_t base, const std::set<uint32_t>& values)
{
    Bitmap128 bitmap{};
    for (uint32_t psn : values)
    {
        if (IsRepresentable(base, psn, DATA_BITMAP_BITS))
        {
            SetBitmapBit(bitmap, psn - base);
        }
    }
    return bitmap;
}

uint64_t
FalconReliabilityManager::BuildBitmap64(uint32_t base, const std::set<uint32_t>& values)
{
    uint64_t bitmap = 0;
    for (uint32_t psn : values)
    {
        if (IsRepresentable(base, psn, REQUEST_BITMAP_BITS))
        {
            bitmap |= uint64_t{1} << (psn - base);
        }
    }
    return bitmap;
}

FalconBackHeader
FalconReliabilityManager::BuildBack(uint32_t connectionId,
                                    uint32_t timestamp1,
                                    uint32_t timestamp2,
                                    uint64_t congestionMetadata) const
{
    FalconBackHeader back;
    back.SetConnectionId(connectionId);
    back.SetReceiverDataWindowBase(m_receiverDataBase);
    back.SetReceiverRequestWindowBase(m_receiverRequestBase);
    back.SetTimestamp1(timestamp1);
    back.SetTimestamp2(timestamp2);
    back.SetCongestionMetadata(congestionMetadata);
    return back;
}

FalconEackHeader
FalconReliabilityManager::BuildEack(uint32_t connectionId,
                                    uint32_t timestamp1,
                                    uint32_t timestamp2,
                                    uint64_t congestionMetadata) const
{
    FalconEackHeader eack;
    FalconBackHeader back = BuildBack(connectionId, timestamp1, timestamp2, congestionMetadata);
    back.SetPacketType(FalconPacketType::EACK);
    eack.SetBack(back);
    eack.SetDataAckBitmap(BuildBitmap128(m_receiverDataBase, m_acknowledgedData));
    eack.SetDataReceivedBitmap(BuildBitmap128(m_receiverDataBase, m_receivedData));
    eack.SetRequestBitmap(BuildBitmap64(m_receiverRequestBase, m_receivedRequests));
    return eack;
}

std::vector<uint32_t>
FalconReliabilityManager::ApplyCumulativeBase(std::map<uint32_t, bool>& outstanding,
                                               uint32_t base)
{
    std::vector<uint32_t> acknowledged;
    for (auto it = outstanding.begin(); it != outstanding.end() && it->first < base;)
    {
        acknowledged.push_back(it->first);
        it = outstanding.erase(it);
    }
    return acknowledged;
}

std::vector<uint32_t>
FalconReliabilityManager::ProcessBack(const FalconBackHeader& back)
{
    std::vector<uint32_t> acknowledged =
        ApplyCumulativeBase(m_outstandingData, back.GetReceiverDataWindowBase());
    std::vector<uint32_t> requests =
        ApplyCumulativeBase(m_outstandingRequests, back.GetReceiverRequestWindowBase());
    acknowledged.insert(acknowledged.end(), requests.begin(), requests.end());
    return acknowledged;
}

std::vector<uint32_t>
FalconReliabilityManager::ProcessEack(const FalconEackHeader& eack)
{
    std::vector<uint32_t> acknowledged = ProcessBack(eack.GetBack());
    const uint32_t dataBase = eack.GetBack().GetReceiverDataWindowBase();
    const uint32_t requestBase = eack.GetBack().GetReceiverRequestWindowBase();
    const Bitmap128& ackBitmap = eack.GetDataAckBitmap();
    const Bitmap128& receivedBitmap = eack.GetDataReceivedBitmap();

    for (uint32_t offset = 0; offset < DATA_BITMAP_BITS; ++offset)
    {
        const uint64_t sequence = static_cast<uint64_t>(dataBase) + offset;
        if (sequence > std::numeric_limits<uint32_t>::max())
        {
            break;
        }
        auto packet = m_outstandingData.find(static_cast<uint32_t>(sequence));
        if (packet == m_outstandingData.end())
        {
            continue;
        }
        if (GetBitmapBit(ackBitmap, offset))
        {
            acknowledged.push_back(packet->first);
            m_outstandingData.erase(packet);
        }
        else if (GetBitmapBit(receivedBitmap, offset))
        {
            packet->second = true;
        }
    }

    const uint64_t requestBitmap = eack.GetRequestBitmap();
    for (uint32_t offset = 0; offset < REQUEST_BITMAP_BITS; ++offset)
    {
        if ((requestBitmap & (uint64_t{1} << offset)) == 0)
        {
            continue;
        }
        const uint64_t sequence = static_cast<uint64_t>(requestBase) + offset;
        if (sequence > std::numeric_limits<uint32_t>::max())
        {
            break;
        }
        auto packet = m_outstandingRequests.find(static_cast<uint32_t>(sequence));
        if (packet != m_outstandingRequests.end())
        {
            acknowledged.push_back(packet->first);
            m_outstandingRequests.erase(packet);
        }
    }
    return acknowledged;
}

bool
FalconReliabilityManager::ProcessNack(const FalconNackHeader& nack, uint32_t& retransmitPsn) const
{
    retransmitPsn = nack.GetNackPacketSequenceNumber();
    const auto& outstanding = nack.IsRequestWindow() ? m_outstandingRequests : m_outstandingData;
    return outstanding.contains(retransmitPsn);
}

uint32_t
FalconReliabilityManager::GetReceiverDataWindowBase() const
{
    return m_receiverDataBase;
}

uint32_t
FalconReliabilityManager::GetReceiverRequestWindowBase() const
{
    return m_receiverRequestBase;
}

uint32_t
FalconReliabilityManager::GetOutstandingCount(FalconReliabilityWindow window) const
{
    return window == FalconReliabilityWindow::DATA ? m_outstandingData.size()
                                                    : m_outstandingRequests.size();
}

bool
FalconReliabilityManager::IsOutstanding(FalconReliabilityWindow window, uint32_t psn) const
{
    const auto& outstanding = window == FalconReliabilityWindow::DATA ? m_outstandingData
                                                                      : m_outstandingRequests;
    return outstanding.contains(psn);
}

bool
FalconReliabilityManager::IsPeerReceivedData(uint32_t psn) const
{
    const auto packet = m_outstandingData.find(psn);
    return packet != m_outstandingData.end() && packet->second;
}

} // namespace ns3
