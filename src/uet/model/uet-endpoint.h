/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_ENDPOINT_H
#define UET_ENDPOINT_H

#include "uet-header.h"
#include "uet-ses-header.h"

#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace ns3
{

class UetPdc;
class UetPdsHeader;
class UetSesEngine;
class UetNscc;
class UetSimulationTag;
enum class UetControlType : uint8_t;

/**
 * @defgroup uet Ultra Ethernet Transport
 * Packet-level UEC AI Base simulation models.
 */

/**
 * @ingroup uet
 * AI Base endpoint configuration and stable trace surface.
 */
class UetEndpoint : public Object
{
  public:
    using TransmitCallback = Callback<void, Ptr<const Packet>>;
    using PacketPathTracedCallback = void (*)(Ptr<const Packet>, uint32_t, uint32_t);
    using PdcSequenceTracedCallback = void (*)(uint32_t, uint32_t);
    using PdcTripleTracedCallback = void (*)(uint32_t, uint32_t, uint32_t);
    using PdcStateTracedCallback = void (*)(uint32_t, UetPdcState, UetPdcState);
    using MessageCompleteTracedCallback = void (*)(uint32_t, uint64_t, uint32_t, Time);

    static TypeId GetTypeId();

    UetEndpoint();
    ~UetEndpoint() override;

    UetProfile GetProfile() const;
    uint32_t GetEndpointId() const;
    uint32_t GetPayloadMtu() const;
    bool IsTrimmingSupported() const;
    bool IsPacketSprayingEnabled() const;

    Ptr<UetPdc> CreatePdc(UetDeliveryMode deliveryMode,
                          uint32_t initialWindow = 0,
                          uint64_t lineRateBps = 0);
    Ptr<UetPdc> GetPdc(uint32_t pdcId) const;
    uint32_t GetPdcCount() const;
    bool TransitionPdc(uint32_t pdcId, UetPdcState newState);
    bool RemovePdc(uint32_t pdcId);
    UetReceiveStatus ReceivePacket(Ptr<const Packet> packet);
    void SetTransmitCallback(TransmitCallback callback);
    bool SendRudMessage(uint32_t remoteEndpointId,
                        uint32_t pdcId,
                        uint64_t messageId,
                        Ptr<const Packet> payload);
    bool SendRodMessage(uint32_t remoteEndpointId,
                        uint32_t pdcId,
                        uint64_t messageId,
                        Ptr<const Packet> payload);
    bool SendUudDatagram(uint32_t remoteEndpointId, Ptr<const Packet> payload);
    bool SendSesTransaction(uint32_t remoteEndpointId,
                            uint32_t pdcId,
                            UetDeliveryMode deliveryMode,
                            const UetSesStandardHeader& request,
                            Ptr<const Packet> payload,
                            const UetAtomicExtensionHeader* atomic = nullptr);
    bool RequestPeerClear(uint32_t remoteEndpointId, uint32_t pdcId, uint32_t psn);
    bool ClosePdc(uint32_t remoteEndpointId, uint32_t pdcId);
    Ptr<UetSesEngine> GetSesEngine() const;
    uint32_t GetOutstandingPacketCount(uint32_t pdcId) const;
    uint32_t GetRetainedResponseCount(uint32_t pdcId) const;
    uint32_t GetPeerClearPsn(uint32_t pdcId) const;
    uint32_t GetCongestionWindow(uint32_t pdcId) const;
    bool SetPdcLineRate(uint32_t pdcId, uint64_t lineRateBps);
    bool SetPdcCongestionWindow(uint32_t pdcId, uint32_t bytes);
    void ConfigureJobScheduler(uint64_t lineRateBps);
    bool AssignPdcToJob(uint32_t pdcId, uint32_t jobId, uint32_t weight);
    bool GetSesResponse(uint16_t messageId, UetSesResponseHeader& response) const;

    void NotifyPacketTx(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId);
    void NotifyPacketRx(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId);
    void NotifyPdcStateChange(uint32_t pdcId, UetPdcState oldState, UetPdcState newState);
    void NotifyAck(uint32_t pdcId, uint32_t psn);
    void NotifyNack(uint32_t pdcId, uint32_t psn);
    void NotifyTimeout(uint32_t pdcId, uint32_t psn);
    void NotifyRetransmission(uint32_t pdcId, uint32_t psn);
    void NotifyCongestionWindow(uint32_t pdcId, uint32_t oldBytes, uint32_t newBytes);
    void NotifyEcnReceived(uint32_t pdcId, uint32_t pathId);
    void NotifyPathSelected(uint32_t pdcId, uint32_t psn, uint32_t pathId);
    void NotifyPacketTrimmed(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId);
    void NotifyReorderDepth(uint32_t pdcId, uint32_t oldDepth, uint32_t newDepth);
    void NotifyMessageComplete(uint32_t pdcId, uint64_t messageId, uint32_t bytes, Time latency);

  private:
    struct OutstandingPacket
    {
        Ptr<Packet> packet;
        EventId timeout;
        uint32_t retransmissions{0};
        Time lastTransmitTime{Seconds(0)};
        Time currentRto{Seconds(0)};
        uint32_t wireBytes{0};
    };

    struct ReliableMessageState
    {
        std::map<uint32_t, uint32_t> fragments;
        bool sawEnd{false};
        uint32_t totalLength{0};
        Time sendTime{Seconds(0)};
        UetSesReturnCode returnCode{UetSesReturnCode::OK};
        uint32_t modifiedLength{0};
        bool deliveryComplete{false};
        uint8_t riGeneration{0};
        uint32_t jobId{0};
    };

    void DoDispose() override;
    bool SendReliableMessage(uint32_t remoteEndpointId,
                             uint32_t pdcId,
                             uint64_t messageId,
                             Ptr<const Packet> payload,
                             UetDeliveryMode deliveryMode);
    bool SendReliableTransaction(uint32_t remoteEndpointId,
                                 uint32_t pdcId,
                                 UetDeliveryMode deliveryMode,
                                 const UetSesStandardHeader& request,
                                 Ptr<const Packet> payload,
                                 const UetAtomicExtensionHeader* atomic);
    void TransmitTrackedPacket(uint32_t pdcId, uint32_t sequenceNumber, bool retransmission);
    void EnqueueTrackedPacket(uint32_t pdcId, uint32_t sequenceNumber, bool retransmission);
    void DrainTransmitQueue(uint32_t pdcId);
    void ScheduleJobDrain(Time delay = NanoSeconds(0));
    void DrainJobQueues();
    void HandleRetransmissionTimeout(uint32_t pdcId, uint32_t sequenceNumber);
    void HandleRudData(Ptr<UetPdc> pdc,
                       const UetHeader& header,
                       const UetSesStandardHeader& sesHeader,
                       const UetAtomicExtensionHeader* atomic,
                       Ptr<const Packet> payload);
    void HandleRodData(Ptr<UetPdc> pdc,
                       const UetHeader& header,
                       const UetSesStandardHeader& sesHeader,
                       const UetAtomicExtensionHeader* atomic,
                       Ptr<const Packet> payload);
    UetReceiveStatus HandleUudDatagram(Ptr<const Packet> packet,
                                       const UetPdsHeader& header,
                                       Ptr<Packet> payload);
    std::optional<UetSesResponseHeader> DeliverReliableFragment(
        Ptr<UetPdc> pdc,
        const UetHeader& header,
        const UetSesStandardHeader& sesHeader,
        const UetAtomicExtensionHeader* atomic,
        Ptr<const Packet> payload);
    void HandleAck(uint32_t pdcId, uint32_t sequenceNumber);
    void HandleAckCc(uint32_t pdcId, const UetPdsHeader& header);
    void ApplyNsccAck(uint32_t pdcId,
                      uint32_t sequenceNumber,
                      bool ecnMarked,
                      uint16_t serviceTime,
                      uint8_t receiverCwndPending,
                      bool restoreCwnd);
    void ApplyNsccLoss(uint32_t pdcId, uint32_t sequenceNumber);
    void RecordAckReceipt(uint32_t pdcId, uint32_t sequenceNumber);
    void ProcessClearPsn(uint32_t pdcId, uint32_t clearPsn);
    void HandleNack(uint32_t pdcId, uint32_t sequenceNumber, UetDeliveryMode deliveryMode);
    void ScheduleControlPacket(
        const UetHeader& receivedHeader,
        UetPacketType packetType,
        std::optional<UetSesResponseHeader> sesResponse = std::nullopt,
        uint8_t nackCode = 0);
    void TransmitControlPacket(UetHeader header,
                               std::optional<UetSesResponseHeader> sesResponse = std::nullopt,
                               uint8_t nackCode = 0);
    bool TransmitPdsControl(uint32_t remoteEndpointId,
                            uint32_t pdcId,
                            UetControlType controlType,
                            uint32_t value,
                            bool requestAck);
    UetReceiveStatus HandlePdsControl(Ptr<const Packet> packet,
                                      const UetPdsHeader& header,
                                      const UetSimulationTag& route);
    void CancelPdcEvents(uint32_t pdcId);

    UetProfile m_profile{UetProfile::AI_BASE};
    uint32_t m_endpointId{0};
    uint32_t m_payloadMtu{4096};
    Time m_ackDelay{NanoSeconds(0)};
    Time m_nicProcessingDelay{NanoSeconds(100)};
    uint32_t m_maxPdcCount{1024};
    uint32_t m_maxRetransmissions{8};
    uint64_t m_nsccLineRateBps{100000000000ULL};
    uint32_t m_nsccMaximumWindow{225000};
    uint32_t m_nsccInitialWindow{65536};
    Time m_nsccBaseRtt{MicroSeconds(12)};
    Time m_nsccTargetQueueDelay{MicroSeconds(12)};
    bool m_trimmingSupported{true};
    bool m_packetSprayingEnabled{true};
    uint32_t m_nextPdcId{1};
    std::unordered_map<uint32_t, Ptr<UetPdc>> m_pdcs;
    TransmitCallback m_transmitCallback;
    std::unordered_map<uint32_t, uint32_t> m_nextTxSequence;
    std::unordered_map<uint32_t, uint32_t> m_startTxSequence;
    std::unordered_map<uint64_t, uint32_t> m_inboundPdcMap;
    std::unordered_map<uint32_t, std::map<uint32_t, OutstandingPacket>> m_outstandingPackets;
    std::unordered_map<uint32_t, std::set<uint32_t>> m_receivedSequences;
    std::unordered_map<uint32_t, uint32_t> m_highestReceivedSequence;
    std::unordered_map<uint32_t, uint32_t> m_cumulativeAckSequence;
    std::unordered_map<uint32_t, std::set<uint32_t>> m_receivedAckSequences;
    std::unordered_map<uint32_t, uint32_t> m_clearTxSequence;
    std::unordered_map<uint32_t, uint32_t> m_peerClearSequence;
    std::unordered_map<uint32_t, std::set<uint32_t>> m_retainedResponses;
    std::unordered_map<uint32_t, std::map<uint32_t, UetSesResponseHeader>> m_semanticResponses;
    std::unordered_map<uint16_t, UetSesResponseHeader> m_receivedSesResponses;
    std::unordered_map<uint32_t, uint64_t> m_receivedByteCount;
    std::unordered_map<uint32_t, uint32_t> m_expectedRodSequence;
    std::unordered_map<uint32_t, bool> m_rodGoBackNActive;
    std::unordered_map<uint32_t, std::unordered_map<uint64_t, ReliableMessageState>> m_reassembly;
    std::unordered_map<uint32_t, Ptr<UetNscc>> m_nscc;
    std::unordered_map<uint32_t, std::deque<std::pair<uint32_t, bool>>> m_transmitQueues;
    std::unordered_map<uint32_t, EventId> m_pacingEvents;
    std::unordered_map<uint32_t, Time> m_nextPacedSend;
    std::set<uint32_t> m_drainingQueues;
    struct JobScheduleState
    {
        uint32_t weight{1};
        int64_t currentWeight{0};
        std::vector<uint32_t> pdcs;
        std::size_t pdcCursor{0};
    };
    bool m_jobSchedulerEnabled{false};
    uint64_t m_jobSchedulerLineRateBps{0};
    EventId m_jobSchedulerEvent;
    Time m_nextJobSchedulerSend{Seconds(0)};
    std::map<uint32_t, JobScheduleState> m_jobSchedules;
    std::vector<uint32_t> m_jobOrder;
    std::unordered_map<uint32_t, uint32_t> m_pdcJobs;
    Ptr<UetSesEngine> m_sesEngine;

    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_packetTxTrace;
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_packetRxTrace;
    TracedCallback<uint32_t, UetPdcState, UetPdcState> m_pdcStateChangeTrace;
    TracedCallback<uint32_t, uint32_t> m_ackTrace;
    TracedCallback<uint32_t, uint32_t> m_nackTrace;
    TracedCallback<uint32_t, uint32_t> m_timeoutTrace;
    TracedCallback<uint32_t, uint32_t> m_retransmissionTrace;
    TracedCallback<uint32_t, uint32_t, uint32_t> m_congestionWindowTrace;
    TracedCallback<uint32_t, uint32_t> m_ecnReceivedTrace;
    TracedCallback<uint32_t, uint32_t, uint32_t> m_pathSelectedTrace;
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_packetTrimmedTrace;
    TracedCallback<uint32_t, uint32_t, uint32_t> m_reorderDepthTrace;
    TracedCallback<uint32_t, uint64_t, uint32_t, Time> m_messageCompleteTrace;
};

} // namespace ns3

#endif // UET_ENDPOINT_H
