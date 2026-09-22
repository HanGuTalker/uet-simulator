/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-endpoint.h"

#include "uet-header.h"
#include "uet-nscc.h"
#include "uet-pdc.h"
#include "uet-pds-header.h"
#include "uet-ses-engine.h"
#include "uet-ses-header.h"
#include "uet-simulation-tag.h"

#include "ns3/boolean.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/timestamp-tag.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <vector>

namespace ns3
{

namespace
{

constexpr uint8_t PDS_REQUEST_RETX = 0x10;
constexpr uint8_t PDS_REQUEST_ACK_REQUEST = 0x08;
constexpr uint8_t PDS_REQUEST_SYN = 0x04;
constexpr uint8_t UET_ROD_OOO = 0x0d;
constexpr uint8_t UET_PKT_NOT_RCVD = 0x12;
constexpr uint8_t UET_TRIMMED = 0x01;

bool
PsnIsAfter(uint32_t lhs, uint32_t rhs)
{
    const uint32_t distance = lhs - rhs;
    return distance != 0 && distance < (1U << 31);
}

bool
PsnIsBefore(uint32_t lhs, uint32_t rhs)
{
    return PsnIsAfter(rhs, lhs);
}

uint32_t
PsnForwardDistance(uint32_t from, uint32_t to)
{
    return to - from;
}

uint64_t
MakeInboundPdcKey(uint32_t sourceEndpointId, uint16_t sourcePdcId)
{
    return (static_cast<uint64_t>(sourceEndpointId) << 16) | sourcePdcId;
}

UetSimulationTag
MakeSimulationTag(uint32_t sourceEndpointId, uint32_t destinationEndpointId, uint32_t pathId)
{
    UetSimulationTag tag;
    tag.SetSourceEndpointId(sourceEndpointId);
    tag.SetDestinationEndpointId(destinationEndpointId);
    tag.SetPathId(pathId);
    return tag;
}

} // namespace

NS_LOG_COMPONENT_DEFINE("UetEndpoint");
NS_OBJECT_ENSURE_REGISTERED(UetEndpoint);

TypeId
UetEndpoint::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::UetEndpoint")
            .SetParent<Object>()
            .SetGroupName("Uet")
            .AddConstructor<UetEndpoint>()
            .AddAttribute("EndpointId",
                          "Simulation-local endpoint identifier used for packet dispatch.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&UetEndpoint::m_endpointId),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Profile",
                          "The implemented UET profile. This baseline accepts AI Base only.",
                          EnumValue(UetProfile::AI_BASE),
                          MakeEnumAccessor<UetProfile>(&UetEndpoint::m_profile),
                          MakeEnumChecker(UetProfile::AI_BASE, "AiBase"))
            .AddAttribute("PayloadMtu",
                          "Maximum UET payload bytes per packet; an experiment input.",
                          UintegerValue(4096),
                          MakeUintegerAccessor(&UetEndpoint::m_payloadMtu),
                          MakeUintegerChecker<uint32_t>(256, 65535))
            .AddAttribute("AckDelay",
                          "Endpoint ACK generation delay.",
                          TimeValue(NanoSeconds(0)),
                          MakeTimeAccessor(&UetEndpoint::m_ackDelay),
                          MakeTimeChecker(NanoSeconds(0)))
            .AddAttribute("NicProcessingDelay",
                          "Abstract endpoint processing delay; an assumed value until calibrated.",
                          TimeValue(NanoSeconds(100)),
                          MakeTimeAccessor(&UetEndpoint::m_nicProcessingDelay),
                          MakeTimeChecker(NanoSeconds(0)))
            .AddAttribute("MaxPdcCount",
                          "Maximum concurrently modeled PDCs at this endpoint.",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&UetEndpoint::m_maxPdcCount),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("MaxRetransmissions",
                          "Maximum retransmissions per reliable packet before the PDC fails.",
                          UintegerValue(8),
                          MakeUintegerAccessor(&UetEndpoint::m_maxRetransmissions),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("NsccLineRateBps",
                          "Per-PDC NSCC pacing ceiling in bits per second.",
                          UintegerValue(100000000000ULL),
                          MakeUintegerAccessor(&UetEndpoint::m_nsccLineRateBps),
                          MakeUintegerChecker<uint64_t>(1))
            .AddAttribute("NsccMaximumWindow",
                          "Per-PDC NSCC maximum congestion window in bytes.",
                          UintegerValue(225000),
                          MakeUintegerAccessor(&UetEndpoint::m_nsccMaximumWindow),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("NsccInitialWindow",
                          "Initial congestion window assigned when each PDC is created.",
                          UintegerValue(65536),
                          MakeUintegerAccessor(&UetEndpoint::m_nsccInitialWindow),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("NsccBaseRtt",
                          "Initial unloaded RTT supplied to each NSCC instance.",
                          TimeValue(MicroSeconds(12)),
                          MakeTimeAccessor(&UetEndpoint::m_nsccBaseRtt),
                          MakeTimeChecker(NanoSeconds(128)))
            .AddAttribute("NsccTargetQueueDelay",
                          "Target queueing delay supplied to each NSCC instance.",
                          TimeValue(MicroSeconds(12)),
                          MakeTimeAccessor(&UetEndpoint::m_nsccTargetQueueDelay),
                          MakeTimeChecker(NanoSeconds(128)))
            .AddAttribute(
                "TrimmingSupported",
                "Whether the endpoint can process trimmed packets (mandatory for AI Base).",
                BooleanValue(true),
                MakeBooleanAccessor(&UetEndpoint::m_trimmingSupported),
                MakeBooleanChecker())
            .AddAttribute("PacketSprayingEnabled",
                          "Whether eligible delivery modes may select a path per packet.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&UetEndpoint::m_packetSprayingEnabled),
                          MakeBooleanChecker())
            .AddTraceSource("PacketTx",
                            "Packet submitted by the UET endpoint.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_packetTxTrace),
                            "ns3::UetEndpoint::PacketPathTracedCallback")
            .AddTraceSource("PacketRx",
                            "Packet accepted by the UET endpoint.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_packetRxTrace),
                            "ns3::UetEndpoint::PacketPathTracedCallback")
            .AddTraceSource("PdcStateChange",
                            "PDC lifecycle transition.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_pdcStateChangeTrace),
                            "ns3::UetEndpoint::PdcStateTracedCallback")
            .AddTraceSource("Ack",
                            "ACK generated or processed by the endpoint.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_ackTrace),
                            "ns3::UetEndpoint::PdcSequenceTracedCallback")
            .AddTraceSource("Nack",
                            "NACK generated or processed by the endpoint.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_nackTrace),
                            "ns3::UetEndpoint::PdcSequenceTracedCallback")
            .AddTraceSource("Timeout",
                            "Retransmission timer expiration.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_timeoutTrace),
                            "ns3::UetEndpoint::PdcSequenceTracedCallback")
            .AddTraceSource("Retransmission",
                            "Packet selected for retransmission.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_retransmissionTrace),
                            "ns3::UetEndpoint::PdcSequenceTracedCallback")
            .AddTraceSource("CongestionWindow",
                            "NSCC outstanding-byte limit change.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_congestionWindowTrace),
                            "ns3::UetEndpoint::PdcTripleTracedCallback")
            .AddTraceSource("EcnReceived",
                            "ECN feedback observed for a path.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_ecnReceivedTrace),
                            "ns3::UetEndpoint::PdcSequenceTracedCallback")
            .AddTraceSource("PathSelected",
                            "Path selected for a packet sequence number.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_pathSelectedTrace),
                            "ns3::UetEndpoint::PdcTripleTracedCallback")
            .AddTraceSource("PacketTrimmed",
                            "Trimmed packet observed by the endpoint.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_packetTrimmedTrace),
                            "ns3::UetEndpoint::PacketPathTracedCallback")
            .AddTraceSource("ReorderDepth",
                            "Receive reorder depth change.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_reorderDepthTrace),
                            "ns3::UetEndpoint::PdcTripleTracedCallback")
            .AddTraceSource("MessageComplete",
                            "SES message completion with modeled latency.",
                            MakeTraceSourceAccessor(&UetEndpoint::m_messageCompleteTrace),
                            "ns3::UetEndpoint::MessageCompleteTracedCallback");
    return tid;
}

UetEndpoint::UetEndpoint()
    : m_sesEngine(CreateObject<UetSesEngine>())
{
    NS_LOG_FUNCTION(this);
}

UetEndpoint::~UetEndpoint()
{
    NS_LOG_FUNCTION(this);
}

void
UetEndpoint::DoDispose()
{
    for (auto& [pdcId, packets] : m_outstandingPackets)
    {
        for (auto& [sequenceNumber, outstanding] : packets)
        {
            outstanding.timeout.Cancel();
        }
    }
    for (auto& [pdcId, event] : m_pacingEvents)
    {
        event.Cancel();
    }
    m_jobSchedulerEvent.Cancel();
    m_outstandingPackets.clear();
    m_pacingEvents.clear();
    m_transmitQueues.clear();
    m_nextPacedSend.clear();
    m_drainingQueues.clear();
    m_jobSchedules.clear();
    m_jobOrder.clear();
    m_pdcJobs.clear();
    m_startTxSequence.clear();
    m_inboundPdcMap.clear();
    m_receivedSequences.clear();
    m_highestReceivedSequence.clear();
    m_cumulativeAckSequence.clear();
    m_receivedAckSequences.clear();
    m_clearTxSequence.clear();
    m_peerClearSequence.clear();
    m_retainedResponses.clear();
    m_semanticResponses.clear();
    m_receivedSesResponses.clear();
    m_receivedByteCount.clear();
    m_expectedRodSequence.clear();
    m_rodGoBackNActive.clear();
    m_reassembly.clear();
    m_nscc.clear();
    m_sesEngine = nullptr;
    m_transmitCallback = TransmitCallback();
    m_pdcs.clear();
    Object::DoDispose();
}

UetProfile
UetEndpoint::GetProfile() const
{
    return m_profile;
}

uint32_t
UetEndpoint::GetEndpointId() const
{
    return m_endpointId;
}

uint32_t
UetEndpoint::GetPayloadMtu() const
{
    return m_payloadMtu;
}

bool
UetEndpoint::IsTrimmingSupported() const
{
    return m_trimmingSupported;
}

bool
UetEndpoint::IsPacketSprayingEnabled() const
{
    return m_packetSprayingEnabled;
}

Ptr<UetPdc>
UetEndpoint::CreatePdc(UetDeliveryMode deliveryMode,
                       uint32_t initialWindow,
                       uint64_t lineRateBps)
{
    // UEC UUD is connectionless and therefore does not use a PDC.
    if (deliveryMode == UetDeliveryMode::UUD)
    {
        return nullptr;
    }
    if (m_pdcs.size() >= m_maxPdcCount)
    {
        NS_LOG_WARN("Endpoint " << m_endpointId << " reached MaxPdcCount");
        return nullptr;
    }

    uint32_t pdcId = m_nextPdcId;
    while (pdcId == 0 || m_pdcs.contains(pdcId))
    {
        ++pdcId;
    }
    m_nextPdcId = pdcId + 1;
    if (m_nextPdcId == 0)
    {
        m_nextPdcId = 1;
    }

    auto pdc = CreateObject<UetPdc>();
    pdc->SetAttribute("PdcId", UintegerValue(pdcId));
    pdc->SetAttribute("DeliveryMode", EnumValue(deliveryMode));
    pdc->SetAttribute("InitialCongestionWindow",
                      UintegerValue(initialWindow == 0 ? m_nsccInitialWindow : initialWindow));
    m_pdcs.emplace(pdcId, pdc);
    auto nscc = CreateObject<UetNscc>();
    nscc->SetAttribute("LineRateBps",
                       UintegerValue(lineRateBps == 0 ? m_nsccLineRateBps : lineRateBps));
    nscc->SetAttribute("MaximumWindow", UintegerValue(m_nsccMaximumWindow));
    nscc->SetAttribute("BaseRtt", TimeValue(m_nsccBaseRtt));
    nscc->SetAttribute("TargetQueueDelay", TimeValue(m_nsccTargetQueueDelay));
    nscc->Initialize(m_payloadMtu, pdc->GetInitialCongestionWindow());
    m_nscc.emplace(pdcId, nscc);
    return pdc;
}

Ptr<UetPdc>
UetEndpoint::GetPdc(uint32_t pdcId) const
{
    auto iterator = m_pdcs.find(pdcId);
    return iterator == m_pdcs.end() ? nullptr : iterator->second;
}

uint32_t
UetEndpoint::GetPdcCount() const
{
    return static_cast<uint32_t>(m_pdcs.size());
}

bool
UetEndpoint::TransitionPdc(uint32_t pdcId, UetPdcState newState)
{
    auto pdc = GetPdc(pdcId);
    if (!pdc)
    {
        return false;
    }

    const auto oldState = pdc->GetState();
    if (newState == UetPdcState::ACTIVE && pdc->GetRemotePdcId() == 0)
    {
        // Explicitly activated PDCs are retained for deterministic unit setups.
        // Protocol-established PDCs set the independently allocated remote ID first.
        pdc->SetRemotePdcId(pdcId);
    }
    if (!pdc->TransitionTo(newState))
    {
        return false;
    }
    NotifyPdcStateChange(pdcId, oldState, newState);
    return true;
}

bool
UetEndpoint::RemovePdc(uint32_t pdcId)
{
    auto pdc = GetPdc(pdcId);
    if (!pdc || pdc->GetState() != UetPdcState::CLOSED)
    {
        return false;
    }
    CancelPdcEvents(pdcId);
    m_nextTxSequence.erase(pdcId);
    m_startTxSequence.erase(pdcId);
    m_receivedSequences.erase(pdcId);
    m_highestReceivedSequence.erase(pdcId);
    m_cumulativeAckSequence.erase(pdcId);
    m_receivedAckSequences.erase(pdcId);
    m_clearTxSequence.erase(pdcId);
    m_peerClearSequence.erase(pdcId);
    m_retainedResponses.erase(pdcId);
    m_semanticResponses.erase(pdcId);
    m_receivedByteCount.erase(pdcId);
    m_expectedRodSequence.erase(pdcId);
    m_rodGoBackNActive.erase(pdcId);
    m_reassembly.erase(pdcId);
    m_nscc.erase(pdcId);
    for (auto iterator = m_inboundPdcMap.begin(); iterator != m_inboundPdcMap.end();)
    {
        if (iterator->second == pdcId)
        {
            iterator = m_inboundPdcMap.erase(iterator);
        }
        else
        {
            ++iterator;
        }
    }
    m_pdcs.erase(pdcId);
    return true;
}

UetReceiveStatus
UetEndpoint::ReceivePacket(Ptr<const Packet> packet)
{
    if (!packet || packet->GetSize() < UetPdsHeader::UUD_SIZE)
    {
        return UetReceiveStatus::PACKET_TOO_SHORT;
    }

    UetSimulationTag route;
    if (!packet->PeekPacketTag(route))
    {
        return UetReceiveStatus::INVALID_HEADER;
    }
    if (route.GetDestinationEndpointId() != m_endpointId)
    {
        return UetReceiveStatus::WRONG_ENDPOINT;
    }

    uint8_t prologue[2];
    packet->CopyData(prologue, sizeof(prologue));
    const auto pdsType = static_cast<UetPdsType>((prologue[0] >> 3) & 0x1f);
    const uint32_t pdsSize = UetPdsHeader::GetSerializedSize(pdsType);
    if (pdsSize == 0)
    {
        return UetReceiveStatus::INVALID_HEADER;
    }
    if (packet->GetSize() < pdsSize)
    {
        return UetReceiveStatus::PACKET_TOO_SHORT;
    }

    Ptr<Packet> copy = packet->Copy();
    UetPdsHeader pdsHeader;
    if (copy->RemoveHeader(pdsHeader) != pdsSize || !pdsHeader.IsValid())
    {
        return UetReceiveStatus::INVALID_HEADER;
    }

    // Only reliable Requests have a defined trim recovery procedure.  A trimmed
    // UUD, ACK/NACK, or Control packet is discarded without changing protocol state.
    if (route.IsTrimmed() && pdsType != UetPdsType::RUD_REQUEST &&
        pdsType != UetPdsType::ROD_REQUEST)
    {
        return UetReceiveStatus::ACCEPTED;
    }

    if (pdsType == UetPdsType::UUD_REQUEST)
    {
        return HandleUudDatagram(packet, pdsHeader, copy);
    }

    if (pdsType == UetPdsType::CONTROL)
    {
        return HandlePdsControl(packet, pdsHeader, route);
    }

    if (pdsType == UetPdsType::ACK || pdsType == UetPdsType::ACK_CC || pdsType == UetPdsType::NACK)
    {
        const uint32_t pdcId = pdsHeader.GetDestinationPdcId();
        auto pdc = GetPdc(pdcId);
        if (!pdc)
        {
            return UetReceiveStatus::UNKNOWN_PDC;
        }
        if (pdc->GetState() != UetPdcState::ACTIVE)
        {
            if (pdc->GetState() == UetPdcState::CLOSING && pdsType != UetPdsType::NACK)
            {
                NotifyPacketRx(packet, pdcId, route.GetPathId());
                TransitionPdc(pdcId, UetPdcState::CLOSED);
                return UetReceiveStatus::ACCEPTED;
            }
            if (pdc->GetState() != UetPdcState::OPENING || pdsHeader.GetSourcePdcId() == 0)
            {
                return UetReceiveStatus::INACTIVE_PDC;
            }
            pdc->SetRemotePdcId(pdsHeader.GetSourcePdcId());
            TransitionPdc(pdcId, UetPdcState::ACTIVE);
        }
        NotifyPacketRx(packet, pdcId, route.GetPathId());
        if (pdsHeader.GetNextHeader() == UetNextHeader::RESPONSE)
        {
            if (copy->GetSize() < UetSesResponseHeader::SERIALIZED_SIZE)
            {
                return UetReceiveStatus::PACKET_TOO_SHORT;
            }
            UetSesResponseHeader sesResponse;
            copy->RemoveHeader(sesResponse);
            if (!sesResponse.IsValid())
            {
                return UetReceiveStatus::INVALID_HEADER;
            }
            m_receivedSesResponses[sesResponse.GetMessageId()] = sesResponse;
        }
        else if (pdsHeader.GetNextHeader() != UetNextHeader::NONE)
        {
            return UetReceiveStatus::INVALID_HEADER;
        }
        if (pdsType == UetPdsType::ACK)
        {
            const int32_t offset = static_cast<int16_t>(pdsHeader.GetAckPsnOffset());
            HandleAck(pdcId, pdsHeader.GetCumulativeAckPsn() + offset);
        }
        else if (pdsType == UetPdsType::ACK_CC)
        {
            HandleAckCc(pdcId, pdsHeader);
        }
        else
        {
            HandleNack(pdcId, pdsHeader.GetNackPayload(), pdc->GetDeliveryMode());
        }
        return UetReceiveStatus::ACCEPTED;
    }

    if (pdsType != UetPdsType::RUD_REQUEST && pdsType != UetPdsType::ROD_REQUEST)
    {
        return UetReceiveStatus::INVALID_HEADER;
    }
    if (route.IsTrimmed())
    {
        const UetDeliveryMode mode = pdsType == UetPdsType::RUD_REQUEST
                                         ? UetDeliveryMode::RUD
                                         : UetDeliveryMode::ROD;
        uint32_t localPdcId = pdsHeader.GetDestinationPdcId();
        Ptr<UetPdc> localPdc;
        if ((pdsHeader.GetFlags() & PDS_REQUEST_SYN) != 0)
        {
            const uint64_t key =
                MakeInboundPdcKey(route.GetSourceEndpointId(), pdsHeader.GetSourcePdcId());
            auto existing = m_inboundPdcMap.find(key);
            if (existing == m_inboundPdcMap.end())
            {
                // A trimmed packet carries insufficient state to establish a PDC.
                return UetReceiveStatus::UNKNOWN_PDC;
            }
            else
            {
                localPdcId = existing->second;
                localPdc = GetPdc(localPdcId);
            }
        }
        else
        {
            localPdc = GetPdc(localPdcId);
        }
        if (!localPdc || localPdc->GetState() != UetPdcState::ACTIVE)
        {
            return UetReceiveStatus::UNKNOWN_PDC;
        }
        UetHeader trimmed;
        trimmed.SetPacketType(UetPacketType::DATA);
        trimmed.SetDeliveryMode(mode);
        trimmed.SetSourceEndpointId(route.GetSourceEndpointId());
        trimmed.SetDestinationEndpointId(m_endpointId);
        trimmed.SetPdcId(localPdcId);
        trimmed.SetSequenceNumber(pdsHeader.GetPsn());
        trimmed.SetAcknowledgedSequenceNumber(pdsHeader.GetPsn());
        trimmed.SetPathId(route.GetPathId());
        NotifyPacketRx(packet, localPdcId, route.GetPathId());
        NotifyPacketTrimmed(packet, localPdcId, route.GetPathId());
        ScheduleControlPacket(trimmed, UetPacketType::NACK, std::nullopt, UET_TRIMMED);
        return UetReceiveStatus::ACCEPTED;
    }
    if (pdsHeader.GetNextHeader() != UetNextHeader::REQUEST_STANDARD ||
        copy->GetSize() < UetSesStandardHeader::SERIALIZED_SIZE)
    {
        return UetReceiveStatus::INVALID_HEADER;
    }

    const UetDeliveryMode deliveryMode =
        pdsType == UetPdsType::RUD_REQUEST ? UetDeliveryMode::RUD : UetDeliveryMode::ROD;
    const bool syn = (pdsHeader.GetFlags() & PDS_REQUEST_SYN) != 0;
    uint32_t pdcId = pdsHeader.GetDestinationPdcId();
    Ptr<UetPdc> pdc;
    if (syn)
    {
        if (pdsHeader.GetSourcePdcId() == 0)
        {
            return UetReceiveStatus::INVALID_HEADER;
        }
        const uint64_t key =
            MakeInboundPdcKey(route.GetSourceEndpointId(), pdsHeader.GetSourcePdcId());
        auto existing = m_inboundPdcMap.find(key);
        if (existing == m_inboundPdcMap.end())
        {
            pdc = CreatePdc(deliveryMode);
            if (!pdc)
            {
                return UetReceiveStatus::UNKNOWN_PDC;
            }
            pdcId = pdc->GetPdcId();
            pdc->SetRemotePdcId(pdsHeader.GetSourcePdcId());
            TransitionPdc(pdcId, UetPdcState::OPENING);
            TransitionPdc(pdcId, UetPdcState::ACTIVE);
            m_inboundPdcMap.emplace(key, pdcId);
            const uint32_t psnOffset = pdsHeader.GetDestinationPdcId() & 0x0fff;
            const uint32_t startPsn = pdsHeader.GetPsn() - psnOffset;
            m_highestReceivedSequence[pdcId] = startPsn - 1;
            m_cumulativeAckSequence[pdcId] = startPsn - 1;
            m_peerClearSequence[pdcId] = startPsn - 1;
            if (deliveryMode == UetDeliveryMode::ROD)
            {
                m_expectedRodSequence[pdcId] = startPsn;
            }
        }
        else
        {
            pdcId = existing->second;
            pdc = GetPdc(pdcId);
        }
    }
    else
    {
        pdc = GetPdc(pdcId);
    }
    if (!pdc)
    {
        return UetReceiveStatus::UNKNOWN_PDC;
    }
    if (pdc->GetState() != UetPdcState::ACTIVE)
    {
        return UetReceiveStatus::INACTIVE_PDC;
    }
    if (pdc->GetDeliveryMode() != deliveryMode)
    {
        return UetReceiveStatus::DELIVERY_MODE_MISMATCH;
    }

    UetSesStandardHeader sesHeader;
    copy->RemoveHeader(sesHeader);
    const auto opcode = sesHeader.GetOpcode();
    const bool supportedOpcode = opcode == UetSesOpcode::NO_OP || opcode == UetSesOpcode::SEND ||
                                 opcode == UetSesOpcode::WRITE || opcode == UetSesOpcode::ATOMIC ||
                                 opcode == UetSesOpcode::DEFERRABLE_SEND;
    if (!sesHeader.IsValid() || !supportedOpcode)
    {
        return UetReceiveStatus::INVALID_HEADER;
    }

    UetAtomicExtensionHeader atomicHeader;
    UetAtomicExtensionHeader* atomic = nullptr;
    if (opcode == UetSesOpcode::ATOMIC)
    {
        if (copy->GetSize() < UetAtomicExtensionHeader::SERIALIZED_SIZE ||
            copy->RemoveHeader(atomicHeader) != UetAtomicExtensionHeader::SERIALIZED_SIZE ||
            !atomicHeader.IsValid())
        {
            return UetReceiveStatus::INVALID_HEADER;
        }
        atomic = &atomicHeader;
    }
    if ((opcode == UetSesOpcode::NO_OP && copy->GetSize() != 0) ||
        (!sesHeader.IsStartOfMessage() && sesHeader.GetPayloadLength() != copy->GetSize()))
    {
        return UetReceiveStatus::INVALID_HEADER;
    }

    UetHeader header;
    header.SetPacketType(UetPacketType::DATA);
    header.SetDeliveryMode(deliveryMode);
    header.SetSourceEndpointId(route.GetSourceEndpointId());
    header.SetDestinationEndpointId(route.GetDestinationEndpointId());
    header.SetPdcId(pdcId);
    header.SetSequenceNumber(pdsHeader.GetPsn());
    header.SetMessageId(sesHeader.GetMessageId());
    header.SetPayloadLength(copy->GetSize());
    header.SetFragmentOffset(sesHeader.IsStartOfMessage() ? 0 : sesHeader.GetMessageOffset());
    header.SetPathId(route.GetPathId());
    header.SetFlag(UetHeaderFlag::START_OF_MESSAGE, sesHeader.IsStartOfMessage());
    header.SetFlag(UetHeaderFlag::END_OF_MESSAGE, sesHeader.IsEndOfMessage());
    header.SetFlag(UetHeaderFlag::RETRANSMISSION, (pdsHeader.GetFlags() & PDS_REQUEST_RETX) != 0);
    header.SetFlag(UetHeaderFlag::ECN, route.IsEcnMarked());
    if (!pdc->AcceptPacket(header))
    {
        return UetReceiveStatus::INVALID_HEADER;
    }

    const int32_t clearOffset = static_cast<int16_t>(pdsHeader.GetClearPsnOffset());
    ProcessClearPsn(pdcId, pdsHeader.GetPsn() + clearOffset);

    m_receivedByteCount[pdcId] += copy->GetSize();

    NotifyPacketRx(packet, pdcId, route.GetPathId());
    if (deliveryMode == UetDeliveryMode::RUD)
    {
        HandleRudData(pdc, header, sesHeader, atomic, copy);
    }
    else
    {
        HandleRodData(pdc, header, sesHeader, atomic, copy);
    }
    return UetReceiveStatus::ACCEPTED;
}

void
UetEndpoint::SetTransmitCallback(TransmitCallback callback)
{
    m_transmitCallback = callback;
}

bool
UetEndpoint::SendRudMessage(uint32_t remoteEndpointId,
                            uint32_t pdcId,
                            uint64_t messageId,
                            Ptr<const Packet> payload)
{
    return SendReliableMessage(remoteEndpointId, pdcId, messageId, payload, UetDeliveryMode::RUD);
}

bool
UetEndpoint::SendRodMessage(uint32_t remoteEndpointId,
                            uint32_t pdcId,
                            uint64_t messageId,
                            Ptr<const Packet> payload)
{
    return SendReliableMessage(remoteEndpointId, pdcId, messageId, payload, UetDeliveryMode::ROD);
}

bool
UetEndpoint::SendUudDatagram(uint32_t remoteEndpointId, Ptr<const Packet> payload)
{
    if (!payload || payload->GetSize() > m_payloadMtu ||
        payload->GetSize() > UetSesMediumHeader::MAX_REQUEST_LENGTH || m_transmitCallback.IsNull())
    {
        return false;
    }

    Ptr<Packet> datagram = payload->Copy();
    datagram->AddPacketTag(TimestampTag(Simulator::Now()));

    UetSesMediumHeader sesHeader;
    sesHeader.SetOpcode(UetSesOpcode::DATAGRAM_SEND);
    sesHeader.SetRequestLength(payload->GetSize());
    datagram->AddHeader(sesHeader);

    UetPdsHeader pdsHeader;
    pdsHeader.SetType(UetPdsType::UUD_REQUEST);
    pdsHeader.SetNextHeader(UetNextHeader::REQUEST_MEDIUM);
    datagram->AddHeader(pdsHeader);
    datagram->AddPacketTag(MakeSimulationTag(m_endpointId, remoteEndpointId, 0));

    NotifyPacketTx(datagram, 0, 0);
    m_transmitCallback(datagram);
    return true;
}

bool
UetEndpoint::SendReliableMessage(uint32_t remoteEndpointId,
                                 uint32_t pdcId,
                                 uint64_t messageId,
                                 Ptr<const Packet> payload,
                                 UetDeliveryMode deliveryMode)
{
    UetSesStandardHeader request;
    request.SetOpcode(UetSesOpcode::SEND);
    request.SetMessageId(messageId);
    return SendReliableTransaction(remoteEndpointId,
                                   pdcId,
                                   deliveryMode,
                                   request,
                                   payload,
                                   nullptr);
}

bool
UetEndpoint::SendSesTransaction(uint32_t remoteEndpointId,
                                uint32_t pdcId,
                                UetDeliveryMode deliveryMode,
                                const UetSesStandardHeader& request,
                                Ptr<const Packet> payload,
                                const UetAtomicExtensionHeader* atomic)
{
    return SendReliableTransaction(remoteEndpointId,
                                   pdcId,
                                   deliveryMode,
                                   request,
                                   payload,
                                   atomic);
}

bool
UetEndpoint::RequestPeerClear(uint32_t remoteEndpointId, uint32_t pdcId, uint32_t psn)
{
    return TransmitPdsControl(remoteEndpointId,
                              pdcId,
                              UetControlType::CLEAR_REQUEST,
                              psn,
                              false);
}

bool
UetEndpoint::ClosePdc(uint32_t remoteEndpointId, uint32_t pdcId)
{
    auto pdc = GetPdc(pdcId);
    if (!pdc || pdc->GetState() != UetPdcState::ACTIVE || GetOutstandingPacketCount(pdcId) != 0)
    {
        return false;
    }
    const uint32_t psn = m_nextTxSequence.contains(pdcId) ? m_nextTxSequence[pdcId]++ : 1;
    if (!TransmitPdsControl(remoteEndpointId,
                            pdcId,
                            UetControlType::CLOSE_COMMAND,
                            psn,
                            true))
    {
        return false;
    }
    return TransitionPdc(pdcId, UetPdcState::CLOSING);
}

Ptr<UetSesEngine>
UetEndpoint::GetSesEngine() const
{
    return m_sesEngine;
}

bool
UetEndpoint::SendReliableTransaction(uint32_t remoteEndpointId,
                                     uint32_t pdcId,
                                     UetDeliveryMode deliveryMode,
                                     const UetSesStandardHeader& request,
                                     Ptr<const Packet> payload,
                                     const UetAtomicExtensionHeader* atomic)
{
    auto pdc = GetPdc(pdcId);
    const uint64_t messageId = request.GetMessageId();
    const auto opcode = request.GetOpcode();
    const bool supportedOpcode = opcode == UetSesOpcode::NO_OP || opcode == UetSesOpcode::SEND ||
                                 opcode == UetSesOpcode::WRITE || opcode == UetSesOpcode::ATOMIC ||
                                 opcode == UetSesOpcode::DEFERRABLE_SEND;
    if (!pdc ||
        (pdc->GetState() != UetPdcState::ACTIVE && pdc->GetState() != UetPdcState::OPENING) ||
        pdc->GetDeliveryMode() != deliveryMode || !payload || !supportedOpcode || messageId == 0 ||
        messageId > UINT16_MAX || pdcId > UINT16_MAX || m_transmitCallback.IsNull())
    {
        return false;
    }
    if ((opcode == UetSesOpcode::NO_OP && payload->GetSize() != 0) ||
        (opcode == UetSesOpcode::ATOMIC && (!atomic || !atomic->IsValid())) ||
        (opcode != UetSesOpcode::ATOMIC && atomic != nullptr))
    {
        return false;
    }

    const uint32_t totalLength = payload->GetSize();
    uint32_t offset = 0;
    bool firstFragment = true;
    do
    {
        const uint32_t fragmentLength = std::min(m_payloadMtu, totalLength - offset);
        Ptr<Packet> fragment = payload->CreateFragment(offset, fragmentLength);
        fragment->AddPacketTag(TimestampTag(Simulator::Now()));

        auto nextSequenceIterator = m_nextTxSequence.find(pdcId);
        if (nextSequenceIterator == m_nextTxSequence.end())
        {
            uint32_t startSequence;
            if (pdc->HasConfiguredStartPsn())
            {
                startSequence = pdc->GetConfiguredStartPsn();
            }
            else if (pdc->GetState() == UetPdcState::OPENING)
            {
                uint32_t seed = (m_endpointId * 0x9e3779b9U) ^ (pdcId * 0x85ebca6bU);
                seed ^= seed >> 16;
                seed *= 0x7feb352dU;
                seed ^= seed >> 15;
                startSequence = seed;
            }
            else
            {
                startSequence = 1;
            }
            m_startTxSequence[pdcId] = startSequence;
            m_clearTxSequence[pdcId] = startSequence - 1;
            nextSequenceIterator = m_nextTxSequence.emplace(pdcId, startSequence).first;
        }
        const uint32_t sequenceNumber = nextSequenceIterator->second++;

        UetSesStandardHeader sesHeader = request;
        sesHeader.SetStartOfMessage(firstFragment);
        sesHeader.SetEndOfMessage(offset + fragmentLength == totalLength);
        sesHeader.SetMessageId(messageId);
        sesHeader.SetRequestLength(totalLength);
        sesHeader.SetPayloadLength(fragmentLength);
        sesHeader.SetMessageOffset(offset);
        if (atomic)
        {
            fragment->AddHeader(*atomic);
        }
        fragment->AddHeader(sesHeader);

        UetPdsHeader pdsHeader;
        pdsHeader.SetType(deliveryMode == UetDeliveryMode::RUD ? UetPdsType::RUD_REQUEST
                                                               : UetPdsType::ROD_REQUEST);
        pdsHeader.SetNextHeader(UetNextHeader::REQUEST_STANDARD);
        const bool establishing = pdc->GetState() == UetPdcState::OPENING;
        pdsHeader.SetFlags(PDS_REQUEST_ACK_REQUEST | (establishing ? PDS_REQUEST_SYN : 0));
        const int32_t clearOffset = static_cast<int32_t>(m_clearTxSequence[pdcId] - sequenceNumber);
        NS_ABORT_MSG_IF(clearOffset < INT16_MIN || clearOffset > INT16_MAX,
                        "CLEAR_PSN is outside the signed 16-bit Request-header window");
        pdsHeader.SetClearPsnOffset(static_cast<uint16_t>(static_cast<int16_t>(clearOffset)));
        pdsHeader.SetPsn(sequenceNumber);
        pdsHeader.SetSourcePdcId(pdcId);
        pdsHeader.SetDestinationPdcId(establishing
                                          ? (sequenceNumber - m_startTxSequence[pdcId]) & 0x0fff
                                          : pdc->GetRemotePdcId());
        fragment->AddHeader(pdsHeader);
        fragment->AddPacketTag(MakeSimulationTag(m_endpointId, remoteEndpointId, 0));

        OutstandingPacket outstanding;
        outstanding.packet = fragment->Copy();
        outstanding.wireBytes = fragment->GetSize();
        outstanding.currentRto = pdc->GetRetransmissionTimeout();
        m_outstandingPackets[pdcId].emplace(sequenceNumber, std::move(outstanding));
        EnqueueTrackedPacket(pdcId, sequenceNumber, false);

        firstFragment = false;
        offset += fragmentLength;
    } while (offset < totalLength);

    return true;
}

uint32_t
UetEndpoint::GetOutstandingPacketCount(uint32_t pdcId) const
{
    auto iterator = m_outstandingPackets.find(pdcId);
    return iterator == m_outstandingPackets.end() ? 0
                                                  : static_cast<uint32_t>(iterator->second.size());
}

uint32_t
UetEndpoint::GetRetainedResponseCount(uint32_t pdcId) const
{
    auto responses = m_retainedResponses.find(pdcId);
    return responses == m_retainedResponses.end() ? 0
                                                  : static_cast<uint32_t>(responses->second.size());
}

uint32_t
UetEndpoint::GetPeerClearPsn(uint32_t pdcId) const
{
    auto clearPsn = m_peerClearSequence.find(pdcId);
    return clearPsn == m_peerClearSequence.end() ? 0 : clearPsn->second;
}

uint32_t
UetEndpoint::GetCongestionWindow(uint32_t pdcId) const
{
    auto it = m_nscc.find(pdcId);
    return it == m_nscc.end() ? 0 : it->second->GetCongestionWindow();
}

bool
UetEndpoint::SetPdcLineRate(uint32_t pdcId, uint64_t lineRateBps)
{
    const auto it = m_nscc.find(pdcId);
    if (it == m_nscc.end() || lineRateBps == 0)
    {
        return false;
    }
    it->second->SetLineRateBps(lineRateBps);
    return true;
}

bool
UetEndpoint::SetPdcCongestionWindow(uint32_t pdcId, uint32_t bytes)
{
    const auto it = m_nscc.find(pdcId);
    if (it == m_nscc.end() || bytes == 0)
    {
        return false;
    }
    it->second->SetCongestionWindow(bytes);
    return true;
}

void
UetEndpoint::ConfigureJobScheduler(uint64_t lineRateBps)
{
    m_jobSchedulerEnabled = lineRateBps > 0;
    m_jobSchedulerLineRateBps = lineRateBps;
}

bool
UetEndpoint::AssignPdcToJob(uint32_t pdcId, uint32_t jobId, uint32_t weight)
{
    if (!m_jobSchedulerEnabled || !m_pdcs.contains(pdcId) || jobId == 0 || weight == 0)
    {
        return false;
    }
    m_pdcJobs[pdcId] = jobId;
    auto [it, inserted] = m_jobSchedules.try_emplace(jobId);
    auto& job = it->second;
    job.weight = weight;
    if (std::find(job.pdcs.begin(), job.pdcs.end(), pdcId) == job.pdcs.end())
    {
        job.pdcs.push_back(pdcId);
    }
    if (inserted)
    {
        m_jobOrder.push_back(jobId);
    }
    return true;
}

bool
UetEndpoint::GetSesResponse(uint16_t messageId, UetSesResponseHeader& response) const
{
    auto it = m_receivedSesResponses.find(messageId);
    if (it == m_receivedSesResponses.end())
    {
        return false;
    }
    response = it->second;
    return true;
}

void
UetEndpoint::EnqueueTrackedPacket(uint32_t pdcId,
                                  uint32_t sequenceNumber,
                                  bool retransmission)
{
    auto& queue = m_transmitQueues[pdcId];
    // Preserve PSN order for ROD Go-Back-N batches. A retransmission may be
    // prioritized by its caller only when no earlier queued packet exists.
    queue.emplace_back(sequenceNumber, retransmission);
    if (m_pdcJobs.contains(pdcId))
    {
        ScheduleJobDrain();
    }
    else
    {
        DrainTransmitQueue(pdcId);
    }
}

void
UetEndpoint::DrainTransmitQueue(uint32_t pdcId)
{
    if (m_pdcJobs.contains(pdcId))
    {
        ScheduleJobDrain();
        return;
    }
    auto queueIt = m_transmitQueues.find(pdcId);
    if (queueIt == m_transmitQueues.end() || queueIt->second.empty() ||
        m_drainingQueues.contains(pdcId))
    {
        return;
    }
    auto eventIt = m_pacingEvents.find(pdcId);
    if (eventIt != m_pacingEvents.end() && eventIt->second.IsPending())
    {
        return;
    }
    const Time nextSend = m_nextPacedSend.contains(pdcId) ? m_nextPacedSend[pdcId] : Seconds(0);
    if (Simulator::Now() < nextSend)
    {
        m_pacingEvents[pdcId] = Simulator::Schedule(nextSend - Simulator::Now(),
                                                    &UetEndpoint::DrainTransmitQueue,
                                                    this,
                                                    pdcId);
        return;
    }

    const auto [sequenceNumber, retransmission] = queueIt->second.front();
    auto packetsIt = m_outstandingPackets.find(pdcId);
    if (packetsIt == m_outstandingPackets.end() ||
        !packetsIt->second.contains(sequenceNumber))
    {
        queueIt->second.pop_front();
        DrainTransmitQueue(pdcId);
        return;
    }
    auto nsccIt = m_nscc.find(pdcId);
    const uint32_t bytes = packetsIt->second.at(sequenceNumber).wireBytes;
    if (!retransmission && nsccIt != m_nscc.end() && !nsccIt->second->CanSend(bytes))
    {
        return;
    }

    queueIt->second.pop_front();
    m_drainingQueues.insert(pdcId);
    TransmitTrackedPacket(pdcId, sequenceNumber, retransmission);
    m_drainingQueues.erase(pdcId);
    const Time pacingDelay = nsccIt == m_nscc.end() ? NanoSeconds(0)
                                                    : nsccIt->second->GetPacingDelay(bytes);
    m_nextPacedSend[pdcId] = Simulator::Now() + pacingDelay;
    auto remaining = m_transmitQueues.find(pdcId);
    if (remaining != m_transmitQueues.end() && !remaining->second.empty())
    {
        m_pacingEvents[pdcId] = Simulator::Schedule(pacingDelay,
                                                    &UetEndpoint::DrainTransmitQueue,
                                                    this,
                                                    pdcId);
    }
}

void
UetEndpoint::ScheduleJobDrain(Time delay)
{
    if (!m_jobSchedulerEnabled || m_jobOrder.empty() || m_jobSchedulerEvent.IsPending())
    {
        return;
    }
    if (Simulator::Now() < m_nextJobSchedulerSend)
    {
        delay = std::max(delay, m_nextJobSchedulerSend - Simulator::Now());
    }
    m_jobSchedulerEvent =
        Simulator::Schedule(delay, &UetEndpoint::DrainJobQueues, this);
}

void
UetEndpoint::DrainJobQueues()
{
    if (!m_jobSchedulerEnabled || m_jobOrder.empty())
    {
        return;
    }
    if (Simulator::Now() < m_nextJobSchedulerSend)
    {
        ScheduleJobDrain(m_nextJobSchedulerSend - Simulator::Now());
        return;
    }

    struct Candidate
    {
        uint32_t jobId;
        uint32_t pdcId;
        uint32_t sequenceNumber;
        bool retransmission;
        uint32_t bytes;
    };
    std::vector<Candidate> candidates;
    Time earliestReady = Time::Max();
    for (uint32_t jobId : m_jobOrder)
    {
        auto& job = m_jobSchedules.at(jobId);
        for (std::size_t pdcsTried = 0; pdcsTried < job.pdcs.size(); ++pdcsTried)
        {
            const uint32_t pdcId = job.pdcs[job.pdcCursor];
            job.pdcCursor = (job.pdcCursor + 1) % job.pdcs.size();
            auto queueIt = m_transmitQueues.find(pdcId);
            if (queueIt == m_transmitQueues.end())
            {
                continue;
            }
            while (!queueIt->second.empty())
            {
                const uint32_t sequenceNumber = queueIt->second.front().first;
                auto packetsIt = m_outstandingPackets.find(pdcId);
                if (packetsIt != m_outstandingPackets.end() &&
                    packetsIt->second.contains(sequenceNumber))
                {
                    break;
                }
                queueIt->second.pop_front();
            }
            if (queueIt->second.empty())
            {
                continue;
            }
            const Time ready = m_nextPacedSend.contains(pdcId) ? m_nextPacedSend[pdcId]
                                                               : Seconds(0);
            if (Simulator::Now() < ready)
            {
                earliestReady = std::min(earliestReady, ready);
                continue;
            }
            const auto [sequenceNumber, retransmission] = queueIt->second.front();
            auto packetsIt = m_outstandingPackets.find(pdcId);
            auto nsccIt = m_nscc.find(pdcId);
            const uint32_t bytes = packetsIt->second.at(sequenceNumber).wireBytes;
            if (!retransmission && nsccIt != m_nscc.end() && !nsccIt->second->CanSend(bytes))
            {
                continue;
            }
            candidates.push_back({jobId, pdcId, sequenceNumber, retransmission, bytes});
            break;
        }
    }
    if (!candidates.empty())
    {
        int64_t totalWeight = 0;
        Candidate* selected = nullptr;
        for (auto& candidate : candidates)
        {
            auto& job = m_jobSchedules.at(candidate.jobId);
            job.currentWeight += job.weight;
            totalWeight += job.weight;
            if (!selected || job.currentWeight > m_jobSchedules.at(selected->jobId).currentWeight)
            {
                selected = &candidate;
            }
        }
        auto& selectedJob = m_jobSchedules.at(selected->jobId);
        selectedJob.currentWeight -= totalWeight;
        auto& queue = m_transmitQueues.at(selected->pdcId);
        queue.pop_front();
        auto nsccIt = m_nscc.find(selected->pdcId);
        TransmitTrackedPacket(selected->pdcId,
                              selected->sequenceNumber,
                              selected->retransmission);
        const Time pdcDelay = nsccIt == m_nscc.end()
                                  ? NanoSeconds(0)
                                  : nsccIt->second->GetPacingDelay(selected->bytes);
        m_nextPacedSend[selected->pdcId] = Simulator::Now() + pdcDelay;
        const uint64_t serializationNs = std::max<uint64_t>(
            1,
            (static_cast<uint64_t>(selected->bytes) * 8ULL * 1000000000ULL) /
                m_jobSchedulerLineRateBps);
        m_nextJobSchedulerSend = Simulator::Now() + NanoSeconds(serializationNs);
        ScheduleJobDrain(NanoSeconds(serializationNs));
        return;
    }
    if (earliestReady != Time::Max())
    {
        ScheduleJobDrain(earliestReady - Simulator::Now());
    }
}

void
UetEndpoint::TransmitTrackedPacket(uint32_t pdcId, uint32_t sequenceNumber, bool retransmission)
{
    auto pdc = GetPdc(pdcId);
    auto pdcPackets = m_outstandingPackets.find(pdcId);
    if (!pdc || pdcPackets == m_outstandingPackets.end())
    {
        return;
    }
    auto packetIterator = pdcPackets->second.find(sequenceNumber);
    if (packetIterator == pdcPackets->second.end())
    {
        return;
    }

    auto& outstanding = packetIterator->second;
    outstanding.timeout.Cancel();
    Ptr<Packet> packet = outstanding.packet->Copy();
    if (retransmission)
    {
        if (outstanding.retransmissions >= m_maxRetransmissions)
        {
            TransitionPdc(pdcId, UetPdcState::ERROR);
            CancelPdcEvents(pdcId);
            return;
        }
        UetPdsHeader header;
        packet->RemoveHeader(header);
        uint8_t flags = header.GetFlags() | PDS_REQUEST_RETX;
        if (pdc->GetState() == UetPdcState::ACTIVE && (flags & PDS_REQUEST_SYN) != 0)
        {
            flags &= ~PDS_REQUEST_SYN;
            header.SetDestinationPdcId(pdc->GetRemotePdcId());
        }
        header.SetFlags(flags);
        packet->AddHeader(header);
        ++outstanding.retransmissions;
        outstanding.currentRto = std::min(outstanding.currentRto * 2, MilliSeconds(100));
        NotifyRetransmission(pdcId, sequenceNumber);
    }
    else
    {
        auto nscc = m_nscc.find(pdcId);
        if (nscc != m_nscc.end())
        {
            nscc->second->OnPacketSent(outstanding.wireBytes);
        }
    }
    outstanding.lastTransmitTime = Simulator::Now();
    outstanding.timeout = Simulator::Schedule(outstanding.currentRto,
                                              &UetEndpoint::HandleRetransmissionTimeout,
                                              this,
                                              pdcId,
                                              sequenceNumber);
    NotifyPacketTx(packet, pdcId, 0);
    m_transmitCallback(packet);
}

void
UetEndpoint::HandleRetransmissionTimeout(uint32_t pdcId, uint32_t sequenceNumber)
{
    if (GetOutstandingPacketCount(pdcId) == 0)
    {
        return;
    }
    NotifyTimeout(pdcId, sequenceNumber);
    ApplyNsccLoss(pdcId, sequenceNumber);
    EnqueueTrackedPacket(pdcId, sequenceNumber, true);
}

void
UetEndpoint::HandleRudData(Ptr<UetPdc> pdc,
                           const UetHeader& header,
                           const UetSesStandardHeader& sesHeader,
                           const UetAtomicExtensionHeader* atomic,
                           Ptr<const Packet> payload)
{
    auto& received = m_receivedSequences[pdc->GetPdcId()];
    const bool duplicate = received.contains(header.GetSequenceNumber());
    uint32_t& highest = m_highestReceivedSequence[pdc->GetPdcId()];

    if (!duplicate && PsnIsAfter(header.GetSequenceNumber(), highest + 1))
    {
        const uint32_t missing = highest + 1;
        if (!received.contains(missing))
        {
            UetHeader missingHeader = header;
            missingHeader.SetAcknowledgedSequenceNumber(missing);
            ScheduleControlPacket(missingHeader, UetPacketType::NACK);
        }
    }
    if (PsnIsAfter(header.GetSequenceNumber(), highest))
    {
        highest = header.GetSequenceNumber();
    }

    if (!duplicate)
    {
        received.insert(header.GetSequenceNumber());
        auto response = DeliverReliableFragment(pdc, header, sesHeader, atomic, payload);
        ScheduleControlPacket(header, UetPacketType::ACK, response);
        return;
    }
    auto retained = m_semanticResponses[pdc->GetPdcId()].find(header.GetSequenceNumber());
    ScheduleControlPacket(header,
                          UetPacketType::ACK,
                          retained == m_semanticResponses[pdc->GetPdcId()].end()
                              ? std::nullopt
                              : std::optional<UetSesResponseHeader>(retained->second));
}

void
UetEndpoint::HandleRodData(Ptr<UetPdc> pdc,
                           const UetHeader& header,
                           const UetSesStandardHeader& sesHeader,
                           const UetAtomicExtensionHeader* atomic,
                           Ptr<const Packet> payload)
{
    const uint32_t pdcId = pdc->GetPdcId();
    auto expectedIterator = m_expectedRodSequence.emplace(pdcId, 1).first;
    uint32_t& expectedSequence = expectedIterator->second;

    if (PsnIsBefore(header.GetSequenceNumber(), expectedSequence))
    {
        ScheduleControlPacket(header, UetPacketType::ACK);
        return;
    }
    if (PsnIsAfter(header.GetSequenceNumber(), expectedSequence))
    {
        UetHeader outOfOrderHeader = header;
        outOfOrderHeader.SetAcknowledgedSequenceNumber(expectedSequence);
        ScheduleControlPacket(outOfOrderHeader, UetPacketType::NACK);
        return;
    }

    auto response = DeliverReliableFragment(pdc, header, sesHeader, atomic, payload);
    ++expectedSequence;
    ScheduleControlPacket(header, UetPacketType::ACK, response);
}

UetReceiveStatus
UetEndpoint::HandleUudDatagram(Ptr<const Packet> packet,
                               const UetPdsHeader& header,
                               Ptr<Packet> payload)
{
    if (header.GetNextHeader() != UetNextHeader::REQUEST_MEDIUM ||
        payload->GetSize() < UetSesMediumHeader::SERIALIZED_SIZE)
    {
        return UetReceiveStatus::INVALID_HEADER;
    }

    UetSesMediumHeader sesHeader;
    payload->RemoveHeader(sesHeader);
    if (!sesHeader.IsValid() || sesHeader.GetOpcode() != UetSesOpcode::DATAGRAM_SEND ||
        sesHeader.GetRequestLength() != payload->GetSize() || payload->GetSize() > m_payloadMtu)
    {
        return UetReceiveStatus::INVALID_HEADER;
    }

    UetSimulationTag route;
    packet->PeekPacketTag(route);
    NotifyPacketRx(packet, 0, route.GetPathId());
    TimestampTag timestamp;
    const Time latency = payload->PeekPacketTag(timestamp)
                             ? Simulator::Now() - timestamp.GetTimestamp()
                             : NanoSeconds(0);
    NotifyMessageComplete(0, 0, payload->GetSize(), latency);
    return UetReceiveStatus::ACCEPTED;
}

UetReceiveStatus
UetEndpoint::HandlePdsControl(Ptr<const Packet> packet,
                              const UetPdsHeader& header,
                              const UetSimulationTag& route)
{
    const uint32_t pdcId = header.GetDestinationPdcId();
    auto pdc = GetPdc(pdcId);
    if (!pdc)
    {
        return UetReceiveStatus::UNKNOWN_PDC;
    }
    if (pdc->GetState() != UetPdcState::ACTIVE)
    {
        return UetReceiveStatus::INACTIVE_PDC;
    }
    NotifyPacketRx(packet, pdcId, route.GetPathId());
    switch (header.GetControlType())
    {
    case UetControlType::CLEAR_COMMAND:
        ProcessClearPsn(pdcId, header.GetPsn());
        break;
    case UetControlType::CLEAR_REQUEST:
        TransmitPdsControl(route.GetSourceEndpointId(),
                           pdcId,
                           UetControlType::CLEAR_COMMAND,
                           m_clearTxSequence[pdcId],
                           false);
        break;
    case UetControlType::ACK_REQUEST:
    case UetControlType::NOOP:
    case UetControlType::NEGOTIATION:
    {
        UetHeader response;
        response.SetPacketType(UetPacketType::ACK);
        response.SetDeliveryMode(pdc->GetDeliveryMode());
        response.SetSourceEndpointId(m_endpointId);
        response.SetDestinationEndpointId(route.GetSourceEndpointId());
        response.SetPdcId(pdcId);
        response.SetSequenceNumber(m_cumulativeAckSequence[pdcId]);
        response.SetAcknowledgedSequenceNumber(header.GetPsn());
        response.SetPathId(route.GetPathId());
        TransmitControlPacket(response);
        break;
    }
    case UetControlType::CLOSE_COMMAND:
    case UetControlType::CLOSE_REQUEST:
    {
        UetHeader response;
        response.SetPacketType(UetPacketType::ACK);
        response.SetDeliveryMode(pdc->GetDeliveryMode());
        response.SetSourceEndpointId(m_endpointId);
        response.SetDestinationEndpointId(route.GetSourceEndpointId());
        response.SetPdcId(pdcId);
        response.SetSequenceNumber(header.GetPsn());
        response.SetAcknowledgedSequenceNumber(header.GetPsn());
        response.SetPathId(route.GetPathId());
        TransmitControlPacket(response);
        TransitionPdc(pdcId, UetPdcState::CLOSING);
        TransitionPdc(pdcId, UetPdcState::CLOSED);
        break;
    }
    default:
        break;
    }
    return UetReceiveStatus::ACCEPTED;
}

bool
UetEndpoint::TransmitPdsControl(uint32_t remoteEndpointId,
                                uint32_t pdcId,
                                UetControlType controlType,
                                uint32_t value,
                                bool requestAck)
{
    auto pdc = GetPdc(pdcId);
    if (!pdc || pdc->GetRemotePdcId() == 0 || m_transmitCallback.IsNull())
    {
        return false;
    }
    Ptr<Packet> packet = Create<Packet>();
    UetPdsHeader header;
    header.SetType(UetPdsType::CONTROL);
    header.SetControlType(controlType);
    header.SetFlags(requestAck ? PDS_REQUEST_ACK_REQUEST : 0);
    header.SetPsn(value);
    header.SetSourcePdcId(pdcId);
    header.SetDestinationPdcId(pdc->GetRemotePdcId());
    packet->AddHeader(header);
    packet->AddPacketTag(MakeSimulationTag(m_endpointId, remoteEndpointId, 0));
    NotifyPacketTx(packet, pdcId, 0);
    m_transmitCallback(packet);
    return true;
}

std::optional<UetSesResponseHeader>
UetEndpoint::DeliverReliableFragment(Ptr<UetPdc> pdc,
                                     const UetHeader& header,
                                     const UetSesStandardHeader& sesHeader,
                                     const UetAtomicExtensionHeader* atomic,
                                     Ptr<const Packet> payload)
{
    const auto result = m_sesEngine->Execute(sesHeader, atomic, payload);
    auto& message = m_reassembly[pdc->GetPdcId()][header.GetMessageId()];
    if (message.returnCode == UetSesReturnCode::OK && result.returnCode != UetSesReturnCode::OK)
    {
        message.returnCode = result.returnCode;
    }
    message.modifiedLength += result.modifiedLength;
    message.deliveryComplete = message.deliveryComplete || result.deliveryComplete;
    message.riGeneration = sesHeader.GetRiGeneration();
    message.jobId = sesHeader.GetJobId();
    message.fragments.emplace(header.GetFragmentOffset(), header.GetPayloadLength());
    TimestampTag timestamp;
    if (message.fragments.size() == 1 && payload->PeekPacketTag(timestamp))
    {
        message.sendTime = timestamp.GetTimestamp();
    }
    if (header.HasFlag(UetHeaderFlag::END_OF_MESSAGE))
    {
        message.sawEnd = true;
        message.totalLength = header.GetFragmentOffset() + header.GetPayloadLength();
    }

    uint32_t contiguousBytes = 0;
    for (const auto& [fragmentOffset, fragmentLength] : message.fragments)
    {
        if (fragmentOffset != contiguousBytes)
        {
            break;
        }
        contiguousBytes += fragmentLength;
    }
    if (message.sawEnd && contiguousBytes == message.totalLength)
    {
        NotifyMessageComplete(pdc->GetPdcId(),
                              header.GetMessageId(),
                              message.totalLength,
                              Simulator::Now() - message.sendTime);
        UetSesResponseHeader response;
        response.SetOpcode(UetSesResponseOpcode::DEFAULT_RESPONSE);
        response.SetReturnCode(message.returnCode);
        response.SetMessageId(static_cast<uint16_t>(header.GetMessageId()));
        response.SetRiGeneration(message.riGeneration);
        response.SetJobId(message.jobId);
        response.SetModifiedLength(message.modifiedLength);
        m_semanticResponses[pdc->GetPdcId()][header.GetSequenceNumber()] = response;
        m_reassembly[pdc->GetPdcId()].erase(header.GetMessageId());
        return response;
    }
    return std::nullopt;
}

void
UetEndpoint::HandleAck(uint32_t pdcId, uint32_t sequenceNumber)
{
    NotifyAck(pdcId, sequenceNumber);
    ApplyNsccAck(pdcId, sequenceNumber, false, 0, 0, false);
    RecordAckReceipt(pdcId, sequenceNumber);
    m_rodGoBackNActive[pdcId] = false;
    auto pdcPackets = m_outstandingPackets.find(pdcId);
    if (pdcPackets == m_outstandingPackets.end())
    {
        return;
    }
    auto packet = pdcPackets->second.find(sequenceNumber);
    if (packet == pdcPackets->second.end())
    {
        return;
    }
    packet->second.timeout.Cancel();
    pdcPackets->second.erase(packet);
    DrainTransmitQueue(pdcId);
}

void
UetEndpoint::HandleAckCc(uint32_t pdcId, const UetPdsHeader& header)
{
    const int32_t ackOffset = static_cast<int16_t>(header.GetAckPsnOffset());
    const uint32_t ackPsn = header.GetCumulativeAckPsn() + ackOffset;
    NotifyAck(pdcId, ackPsn);
    RecordAckReceipt(pdcId, ackPsn);
    m_rodGoBackNActive[pdcId] = false;

    auto pdcPackets = m_outstandingPackets.find(pdcId);
    if (pdcPackets == m_outstandingPackets.end())
    {
        return;
    }

    std::set<uint32_t> acknowledged;
    acknowledged.insert(ackPsn);
    for (auto iterator = pdcPackets->second.begin(); iterator != pdcPackets->second.end();
         ++iterator)
    {
        if (!PsnIsAfter(iterator->first, header.GetCumulativeAckPsn()))
        {
            acknowledged.insert(iterator->first);
        }
    }

    const int32_t sackOffset = static_cast<int16_t>(header.GetSackPsnOffset());
    const uint32_t sackBase = header.GetCumulativeAckPsn() + sackOffset;
    for (uint32_t bit = 0; bit < 64; ++bit)
    {
        if ((header.GetSackBitmap() & (1ULL << bit)) != 0)
        {
            acknowledged.insert(sackBase + bit);
        }
    }

    for (uint32_t sequenceNumber : acknowledged)
    {
        auto packet = pdcPackets->second.find(sequenceNumber);
        if (packet != pdcPackets->second.end())
        {
            ApplyNsccAck(pdcId,
                         sequenceNumber,
                         (header.GetFlags() & 0x20) != 0,
                         header.GetNsccServiceTime(),
                         header.GetNsccReceiverCwndPending(),
                         header.GetNsccRestoreCwnd());
            packet->second.timeout.Cancel();
            pdcPackets->second.erase(packet);
        }
    }
    DrainTransmitQueue(pdcId);
}

void
UetEndpoint::ApplyNsccAck(uint32_t pdcId,
                          uint32_t sequenceNumber,
                          bool ecnMarked,
                          uint16_t serviceTime,
                          uint8_t receiverCwndPending,
                          bool restoreCwnd)
{
    auto nscc = m_nscc.find(pdcId);
    auto packets = m_outstandingPackets.find(pdcId);
    if (nscc == m_nscc.end() || packets == m_outstandingPackets.end())
    {
        return;
    }
    auto packet = packets->second.find(sequenceNumber);
    if (packet == packets->second.end())
    {
        return;
    }
    const uint32_t oldWindow = nscc->second->GetCongestionWindow();
    const Time measuredRtt = Simulator::Now() - packet->second.lastTransmitTime;
    nscc->second->OnAck(packet->second.wireBytes,
                        ecnMarked,
                        measuredRtt,
                        NanoSeconds(static_cast<uint64_t>(serviceTime) * 128),
                        receiverCwndPending,
                        restoreCwnd);
    const uint32_t newWindow = nscc->second->GetCongestionWindow();
    if (oldWindow != newWindow)
    {
        NotifyCongestionWindow(pdcId, oldWindow, newWindow);
    }
    if (ecnMarked)
    {
        NotifyEcnReceived(pdcId, 0);
    }
}

void
UetEndpoint::ApplyNsccLoss(uint32_t pdcId, uint32_t sequenceNumber)
{
    auto nscc = m_nscc.find(pdcId);
    auto packets = m_outstandingPackets.find(pdcId);
    if (nscc == m_nscc.end() || packets == m_outstandingPackets.end())
    {
        return;
    }
    auto packet = packets->second.find(sequenceNumber);
    if (packet == packets->second.end())
    {
        return;
    }
    const uint32_t oldWindow = nscc->second->GetCongestionWindow();
    nscc->second->OnLoss(packet->second.wireBytes);
    const uint32_t newWindow = nscc->second->GetCongestionWindow();
    if (oldWindow != newWindow)
    {
        NotifyCongestionWindow(pdcId, oldWindow, newWindow);
    }
}

void
UetEndpoint::RecordAckReceipt(uint32_t pdcId, uint32_t sequenceNumber)
{
    auto clearPsn = m_clearTxSequence.find(pdcId);
    if (clearPsn == m_clearTxSequence.end())
    {
        return;
    }
    auto& receivedAcks = m_receivedAckSequences[pdcId];
    receivedAcks.insert(sequenceNumber);
    while (receivedAcks.erase(clearPsn->second + 1) != 0)
    {
        ++clearPsn->second;
    }
}

void
UetEndpoint::ProcessClearPsn(uint32_t pdcId, uint32_t clearPsn)
{
    auto [peerClear, inserted] = m_peerClearSequence.emplace(pdcId, clearPsn);
    if (!inserted && PsnIsAfter(clearPsn, peerClear->second))
    {
        peerClear->second = clearPsn;
    }
    else if (!inserted)
    {
        clearPsn = peerClear->second;
    }

    auto responses = m_retainedResponses.find(pdcId);
    if (responses != m_retainedResponses.end())
    {
        for (auto response = responses->second.begin(); response != responses->second.end();)
        {
            if (!PsnIsAfter(*response, clearPsn))
            {
                response = responses->second.erase(response);
            }
            else
            {
                ++response;
            }
        }
    }
    auto semantic = m_semanticResponses.find(pdcId);
    if (semantic != m_semanticResponses.end())
    {
        for (auto response = semantic->second.begin(); response != semantic->second.end();)
        {
            if (!PsnIsAfter(response->first, clearPsn))
            {
                response = semantic->second.erase(response);
            }
            else
            {
                ++response;
            }
        }
    }
}

void
UetEndpoint::HandleNack(uint32_t pdcId, uint32_t sequenceNumber, UetDeliveryMode deliveryMode)
{
    NotifyNack(pdcId, sequenceNumber);
    ApplyNsccLoss(pdcId, sequenceNumber);
    if (deliveryMode == UetDeliveryMode::ROD)
    {
        if (m_rodGoBackNActive[pdcId])
        {
            return;
        }
        m_rodGoBackNActive[pdcId] = true;
        auto packets = m_outstandingPackets.find(pdcId);
        if (packets == m_outstandingPackets.end())
        {
            return;
        }
        std::vector<uint32_t> retransmitSequences;
        for (const auto& packet : packets->second)
        {
            const uint32_t outstandingSequence = packet.first;
            if (!PsnIsBefore(outstandingSequence, sequenceNumber))
            {
                retransmitSequences.push_back(outstandingSequence);
            }
        }
        std::sort(retransmitSequences.begin(),
                  retransmitSequences.end(),
                  [sequenceNumber](uint32_t lhs, uint32_t rhs) {
                      return PsnForwardDistance(sequenceNumber, lhs) <
                             PsnForwardDistance(sequenceNumber, rhs);
                  });
        for (uint32_t retransmitSequence : retransmitSequences)
        {
            EnqueueTrackedPacket(pdcId, retransmitSequence, true);
        }
        return;
    }
    EnqueueTrackedPacket(pdcId, sequenceNumber, true);
}

void
UetEndpoint::ScheduleControlPacket(const UetHeader& receivedHeader,
                                   UetPacketType packetType,
                                   std::optional<UetSesResponseHeader> sesResponse,
                                   uint8_t nackCode)
{
    if (m_transmitCallback.IsNull())
    {
        return;
    }
    UetHeader response;
    response.SetPacketType(packetType);
    response.SetDeliveryMode(receivedHeader.GetDeliveryMode());
    response.SetSourceEndpointId(m_endpointId);
    response.SetDestinationEndpointId(receivedHeader.GetSourceEndpointId());
    response.SetPdcId(receivedHeader.GetPdcId());
    if (packetType == UetPacketType::ACK)
    {
        m_retainedResponses[receivedHeader.GetPdcId()].insert(receivedHeader.GetSequenceNumber());
        uint32_t& cumulativeAck = m_cumulativeAckSequence[receivedHeader.GetPdcId()];
        if (receivedHeader.GetDeliveryMode() == UetDeliveryMode::RUD)
        {
            const auto& received = m_receivedSequences[receivedHeader.GetPdcId()];
            while (received.contains(cumulativeAck + 1))
            {
                ++cumulativeAck;
            }
        }
        else
        {
            const uint32_t expected = m_expectedRodSequence[receivedHeader.GetPdcId()];
            const uint32_t candidate = expected - 1;
            if (PsnIsAfter(candidate, cumulativeAck))
            {
                cumulativeAck = candidate;
            }
        }
        response.SetSequenceNumber(cumulativeAck);
    }
    else
    {
        response.SetSequenceNumber(receivedHeader.GetSequenceNumber());
    }
    response.SetAcknowledgedSequenceNumber(packetType == UetPacketType::ACK
                                               ? receivedHeader.GetSequenceNumber()
                                               : receivedHeader.GetAcknowledgedSequenceNumber());
    response.SetMessageId(receivedHeader.GetMessageId());
    response.SetPathId(receivedHeader.GetPathId());
    response.SetFlag(UetHeaderFlag::ECN, receivedHeader.HasFlag(UetHeaderFlag::ECN));
    Ptr<UetEndpoint> self = this;
    Simulator::Schedule(m_ackDelay,
                        &UetEndpoint::TransmitControlPacket,
                        self,
                        response,
                        sesResponse,
                        nackCode);
}

void
UetEndpoint::TransmitControlPacket(UetHeader header,
                                   std::optional<UetSesResponseHeader> sesResponse,
                                   uint8_t nackCode)
{
    if (m_transmitCallback.IsNull())
    {
        return;
    }
    Ptr<Packet> packet = Create<Packet>();
    UetPdsHeader pdsHeader;
    if (header.GetPacketType() == UetPacketType::ACK)
    {
        pdsHeader.SetType(UetPdsType::ACK_CC);
        pdsHeader.SetNextHeader(sesResponse ? UetNextHeader::RESPONSE : UetNextHeader::NONE);
        const int32_t ackOffset = static_cast<int32_t>(header.GetAcknowledgedSequenceNumber() -
                                                       header.GetSequenceNumber());
        pdsHeader.SetAckPsnOffset(static_cast<uint16_t>(static_cast<int16_t>(ackOffset)));
        pdsHeader.SetCumulativeAckPsn(header.GetSequenceNumber());
        pdsHeader.SetCcType(0); // CC_NSCC
        pdsHeader.SetFlags(header.HasFlag(UetHeaderFlag::ECN) ? 0x20 : 0);
        pdsHeader.SetMaximumPsnRange(128);

        const auto& received = m_receivedSequences[header.GetPdcId()];
        uint32_t sackBase = header.GetSequenceNumber();
        uint32_t firstSelectiveDistance = UINT32_MAX;
        for (uint32_t sequenceNumber : received)
        {
            if (PsnIsAfter(sequenceNumber, header.GetSequenceNumber()))
            {
                const uint32_t distance =
                    PsnForwardDistance(header.GetSequenceNumber(), sequenceNumber);
                if (distance < firstSelectiveDistance)
                {
                    firstSelectiveDistance = distance;
                    sackBase = sequenceNumber;
                }
            }
        }
        const int32_t sackOffset = static_cast<int32_t>(sackBase - header.GetSequenceNumber());
        pdsHeader.SetSackPsnOffset(static_cast<uint16_t>(static_cast<int16_t>(sackOffset)));
        uint64_t sackBitmap = 0;
        uint16_t outOfOrderCount = 0;
        for (uint32_t sequenceNumber : received)
        {
            if (PsnIsAfter(sequenceNumber, header.GetSequenceNumber()))
            {
                if (outOfOrderCount != UINT16_MAX)
                {
                    ++outOfOrderCount;
                }
                const uint32_t bit = PsnForwardDistance(sackBase, sequenceNumber);
                if (bit < 64)
                {
                    sackBitmap |= 1ULL << bit;
                }
            }
        }
        pdsHeader.SetSackBitmap(sackBitmap);

        const uint64_t serviceTimeNs = std::max<int64_t>(0, m_ackDelay.GetNanoSeconds());
        const uint16_t encodedServiceTime =
            static_cast<uint16_t>(std::min<uint64_t>(UINT16_MAX, serviceTimeNs / 128));
        const uint32_t receivedByteUnits = static_cast<uint32_t>(
            std::min<uint64_t>(0xffffff, (m_receivedByteCount[header.GetPdcId()] + 255) / 256));
        pdsHeader.SetNsccState(encodedServiceTime, false, 0, receivedByteUnits, outOfOrderCount);
        NotifyAck(header.GetPdcId(), header.GetAcknowledgedSequenceNumber());
    }
    else if (header.GetPacketType() == UetPacketType::NACK)
    {
        pdsHeader.SetType(UetPdsType::NACK);
        pdsHeader.SetNextHeader(UetNextHeader::NONE);
        pdsHeader.SetNackCode(nackCode != 0 ? nackCode
                                           : (header.GetDeliveryMode() == UetDeliveryMode::ROD
                                                  ? UET_ROD_OOO
                                                  : UET_PKT_NOT_RCVD));
        pdsHeader.SetNackPsn(header.GetSequenceNumber());
        pdsHeader.SetNackPayload(header.GetAcknowledgedSequenceNumber());
        NotifyNack(header.GetPdcId(), header.GetAcknowledgedSequenceNumber());
    }
    else
    {
        return;
    }
    auto pdc = GetPdc(header.GetPdcId());
    if (!pdc || pdc->GetRemotePdcId() == 0)
    {
        return;
    }
    pdsHeader.SetSourcePdcId(header.GetPdcId());
    pdsHeader.SetDestinationPdcId(pdc->GetRemotePdcId());
    if (sesResponse)
    {
        packet->AddHeader(*sesResponse);
    }
    packet->AddHeader(pdsHeader);
    packet->AddPacketTag(MakeSimulationTag(header.GetSourceEndpointId(),
                                           header.GetDestinationEndpointId(),
                                           header.GetPathId()));
    NotifyPacketTx(packet, header.GetPdcId(), header.GetPathId());
    m_transmitCallback(packet);
}

void
UetEndpoint::CancelPdcEvents(uint32_t pdcId)
{
    auto pacing = m_pacingEvents.find(pdcId);
    if (pacing != m_pacingEvents.end())
    {
        pacing->second.Cancel();
        m_pacingEvents.erase(pacing);
    }
    m_transmitQueues.erase(pdcId);
    m_nextPacedSend.erase(pdcId);
    m_drainingQueues.erase(pdcId);
    m_pdcJobs.erase(pdcId);
    auto packets = m_outstandingPackets.find(pdcId);
    if (packets == m_outstandingPackets.end())
    {
        return;
    }
    for (auto& [sequenceNumber, outstanding] : packets->second)
    {
        outstanding.timeout.Cancel();
    }
    m_outstandingPackets.erase(packets);
}

void
UetEndpoint::NotifyPacketTx(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId)
{
    m_packetTxTrace(packet, pdcId, pathId);
}

void
UetEndpoint::NotifyPacketRx(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId)
{
    m_packetRxTrace(packet, pdcId, pathId);
}

void
UetEndpoint::NotifyPdcStateChange(uint32_t pdcId, UetPdcState oldState, UetPdcState newState)
{
    m_pdcStateChangeTrace(pdcId, oldState, newState);
}

void
UetEndpoint::NotifyAck(uint32_t pdcId, uint32_t psn)
{
    m_ackTrace(pdcId, psn);
}

void
UetEndpoint::NotifyNack(uint32_t pdcId, uint32_t psn)
{
    m_nackTrace(pdcId, psn);
}

void
UetEndpoint::NotifyTimeout(uint32_t pdcId, uint32_t psn)
{
    m_timeoutTrace(pdcId, psn);
}

void
UetEndpoint::NotifyRetransmission(uint32_t pdcId, uint32_t psn)
{
    m_retransmissionTrace(pdcId, psn);
}

void
UetEndpoint::NotifyCongestionWindow(uint32_t pdcId, uint32_t oldBytes, uint32_t newBytes)
{
    m_congestionWindowTrace(pdcId, oldBytes, newBytes);
}

void
UetEndpoint::NotifyEcnReceived(uint32_t pdcId, uint32_t pathId)
{
    m_ecnReceivedTrace(pdcId, pathId);
}

void
UetEndpoint::NotifyPathSelected(uint32_t pdcId, uint32_t psn, uint32_t pathId)
{
    m_pathSelectedTrace(pdcId, psn, pathId);
}

void
UetEndpoint::NotifyPacketTrimmed(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId)
{
    m_packetTrimmedTrace(packet, pdcId, pathId);
}

void
UetEndpoint::NotifyReorderDepth(uint32_t pdcId, uint32_t oldDepth, uint32_t newDepth)
{
    m_reorderDepthTrace(pdcId, oldDepth, newDepth);
}

void
UetEndpoint::NotifyMessageComplete(uint32_t pdcId, uint64_t messageId, uint32_t bytes, Time latency)
{
    m_messageCompleteTrace(pdcId, messageId, bytes, latency);
}

} // namespace ns3
