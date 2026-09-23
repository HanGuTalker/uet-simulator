/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "roce-v2-transport-adapter.h"

#include "ns3/double.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(RoceV2TransportAdapter);

TypeId
RoceV2TransportAdapter::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RoceV2TransportAdapter")
            .SetParent<AiTransportEndpoint>()
            .SetGroupName("Roce")
            .AddConstructor<RoceV2TransportAdapter>()
            .AddAttribute("PathMtu",
                          "Maximum IPv4 datagram size; RoCEv2 packets are sent with DF set.",
                          UintegerValue(9000),
                          MakeUintegerAccessor(&RoceV2TransportAdapter::m_pathMtu),
                          MakeUintegerChecker<uint32_t>(576, 65535))
            .AddAttribute("DcqcnG",
                          "DCQCN congestion-estimator gain.",
                          DoubleValue(1.0 / 256.0),
                          MakeDoubleAccessor(&RoceV2TransportAdapter::m_dcqcnG),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("DcqcnMinRateFactor",
                          "Minimum sending rate as a fraction of the configured line rate.",
                          DoubleValue(0.01),
                          MakeDoubleAccessor(&RoceV2TransportAdapter::m_dcqcnMinRateFactor),
                          MakeDoubleChecker<double>(0.000001, 1.0))
            .AddAttribute("DcqcnAdditiveIncreaseRate",
                          "DCQCN additive increase step in bits per second.",
                          UintegerValue(5000000000ULL),
                          MakeUintegerAccessor(&RoceV2TransportAdapter::m_dcqcnAdditiveIncreaseBps),
                          MakeUintegerChecker<uint64_t>(1))
            .AddAttribute("DcqcnRecoveryPeriod",
                          "Interval between DCQCN rate-recovery steps.",
                          TimeValue(MicroSeconds(55)),
                          MakeTimeAccessor(&RoceV2TransportAdapter::m_dcqcnRecoveryPeriod),
                          MakeTimeChecker(MicroSeconds(1)))
            .AddAttribute("CnpInterval",
                          "Minimum interval between CNP packets for a receiving flow.",
                          TimeValue(MicroSeconds(50)),
                          MakeTimeAccessor(&RoceV2TransportAdapter::m_cnpInterval),
                          MakeTimeChecker(MicroSeconds(1)));
    return tid;
}

RoceV2TransportAdapter::RoceV2TransportAdapter() = default;
RoceV2TransportAdapter::~RoceV2TransportAdapter() = default;

AiTransportProtocol
RoceV2TransportAdapter::GetProtocol() const
{
    return AiTransportProtocol::ROCEV2;
}

AiTransportCapabilities
RoceV2TransportAdapter::GetCapabilities() const
{
    AiTransportCapabilities capabilities;
    // RC provides ordered delivery, which also satisfies a workload requesting no ordering.
    capabilities.reliableUnordered = true;
    capabilities.reliableOrdered = true;
    capabilities.unreliableUnordered = false;
    capabilities.selectiveAcknowledgment = false;
    capabilities.packetSpraying = false;
    capabilities.perPathCongestionControl = false;
    capabilities.endpointTrimming = false;
    capabilities.jobScheduling = false;
    return capabilities;
}

bool
RoceV2TransportAdapter::Initialize(Ptr<Node> node, const AiTransportEndpointConfig& config)
{
    if (!node || m_socket || config.endpointId == 0 || config.lineRateBps == 0 ||
        config.payloadMtuBytes == 0)
    {
        return false;
    }
    m_node = node;
    m_config = config;
    m_socket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
    if (!m_socket || m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), UDP_PORT)) != 0)
    {
        m_socket = nullptr;
        m_node = nullptr;
        return false;
    }
    m_socket->SetRecvCallback(MakeCallback(&RoceV2TransportAdapter::Receive, this));
    m_socket->SetIpRecvTos(true);
    return true;
}

bool
RoceV2TransportAdapter::AddPeer(uint32_t endpointId, const Address& address)
{
    if (!m_socket || endpointId == 0 || !Ipv4Address::IsMatchingType(address))
    {
        return false;
    }
    m_peers[endpointId] = {Ipv4Address::ConvertFrom(address), UDP_PORT};
    return true;
}

uint32_t
RoceV2TransportAdapter::OpenConnection(const AiTransportConnectionConfig& config)
{
    if (!m_socket || config.remoteEndpointId == 0 || !m_peers.contains(config.remoteEndpointId) ||
        config.reliability == AiTransportReliability::UNRELIABLE_UNORDERED)
    {
        return 0;
    }
    const uint32_t connectionId = m_nextConnectionId++;
    if (connectionId > 0xffffff)
    {
        return 0;
    }
    ConnectionState state;
    state.remoteEndpointId = config.remoteEndpointId;
    state.congestionWindow =
        config.initialWindowBytes == 0 ? m_config.initialWindowBytes : config.initialWindowBytes;
    state.lineRateBps = config.lineRateBps == 0 ? m_config.lineRateBps : config.lineRateBps;
    state.currentRateBps = state.lineRateBps;
    state.targetRateBps = state.lineRateBps;
    state.retransmissionTimeout = config.retransmissionTimeout;
    m_connections.emplace(connectionId, std::move(state));
    return connectionId;
}

bool
RoceV2TransportAdapter::Submit(const AiTransportRequest& request)
{
    auto connection = m_connections.find(request.connectionId);
    if (connection == m_connections.end() || !request.payload || request.payload->GetSize() == 0 ||
        request.remoteEndpointId != connection->second.remoteEndpointId ||
        request.reliability == AiTransportReliability::UNRELIABLE_UNORDERED ||
        (request.operation != AiTransportOperation::MESSAGE &&
         request.operation != AiTransportOperation::SEND &&
         request.operation != AiTransportOperation::WRITE))
    {
        return false;
    }

    auto& state = connection->second;
    if (state.messages.contains(request.messageId))
    {
        return false;
    }
    const uint32_t totalBytes = request.payload->GetSize();
    const uint32_t fragments =
        (totalBytes + m_config.payloadMtuBytes - 1) / m_config.payloadMtuBytes;
    if (state.nextSequence + fragments > 0xffffff)
    {
        return false;
    }
    state.messages.emplace(request.messageId,
                           MessageState{totalBytes, fragments, Simulator::Now()});
    uint32_t offset = 0;
    for (uint32_t fragment = 0; fragment < fragments; ++fragment)
    {
        const uint32_t bytes = std::min(m_config.payloadMtuBytes, totalBytes - offset);
        PendingPacket pending;
        pending.payload = request.payload->CreateFragment(offset, bytes);
        pending.opcode = SelectOpcode(request.operation, fragment, fragments);
        pending.sequence = state.nextSequence++;
        pending.payloadBytes = bytes;
        pending.wireBytes =
            bytes + RoceBthHeader::SERIALIZED_SIZE + RoceInvariantCrcTrailer::SERIALIZED_SIZE +
            ((IsRoceWriteOpcode(pending.opcode) && IsRoceFirstOpcode(pending.opcode))
                 ? RoceRethHeader::SERIALIZED_SIZE
                 : 0);
        pending.tag.SetSourceEndpointId(m_config.endpointId);
        pending.tag.SetDestinationEndpointId(request.remoteEndpointId);
        pending.tag.SetConnectionId(request.connectionId);
        pending.tag.SetMessageId(request.messageId);
        pending.tag.SetPayloadBytes(bytes);
        pending.tag.SetTotalMessageBytes(totalBytes);
        pending.tag.SetSubmittedTimeNs(Simulator::Now().GetNanoSeconds());
        const uint32_t sequence = pending.sequence;
        state.pending.emplace(sequence, std::move(pending));
        state.transmitQueue.push_back(sequence);
        offset += bytes;
    }
    TryTransmit(request.connectionId);
    return true;
}

uint32_t
RoceV2TransportAdapter::GetCongestionWindow(uint32_t connectionId) const
{
    const auto connection = m_connections.find(connectionId);
    return connection == m_connections.end() ? 0 : connection->second.congestionWindow;
}

bool
RoceV2TransportAdapter::SetCongestionWindow(uint32_t connectionId, uint32_t bytes)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || bytes == 0)
    {
        return false;
    }
    const uint32_t oldBytes = connection->second.congestionWindow;
    connection->second.congestionWindow = bytes;
    if (oldBytes != bytes)
    {
        NotifyCongestionWindow(connectionId, oldBytes, bytes);
    }
    TryTransmit(connectionId);
    return true;
}

bool
RoceV2TransportAdapter::SetConnectionRate(uint32_t connectionId, uint64_t rateBps)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || rateBps == 0)
    {
        return false;
    }
    connection->second.lineRateBps = rateBps;
    connection->second.currentRateBps = rateBps;
    connection->second.targetRateBps = rateBps;
    return true;
}

bool
RoceV2TransportAdapter::ConfigureJobScheduler(uint64_t)
{
    return false;
}

bool
RoceV2TransportAdapter::AssignConnectionToJob(uint32_t, uint32_t, uint32_t)
{
    return false;
}

AiTransportCounters
RoceV2TransportAdapter::GetCounters() const
{
    return m_counters;
}

RoceOpcode
RoceV2TransportAdapter::SelectOpcode(AiTransportOperation operation,
                                     uint32_t fragment,
                                     uint32_t fragments) const
{
    const bool write = operation == AiTransportOperation::WRITE;
    if (fragments == 1)
    {
        return write ? RoceOpcode::RC_WRITE_ONLY : RoceOpcode::RC_SEND_ONLY;
    }
    if (fragment == 0)
    {
        return write ? RoceOpcode::RC_WRITE_FIRST : RoceOpcode::RC_SEND_FIRST;
    }
    if (fragment + 1 == fragments)
    {
        return write ? RoceOpcode::RC_WRITE_LAST : RoceOpcode::RC_SEND_LAST;
    }
    return write ? RoceOpcode::RC_WRITE_MIDDLE : RoceOpcode::RC_SEND_MIDDLE;
}

void
RoceV2TransportAdapter::TryTransmit(uint32_t connectionId)
{
    auto found = m_connections.find(connectionId);
    if (found == m_connections.end())
    {
        return;
    }
    auto& state = found->second;
    while (!state.transmitQueue.empty())
    {
        const uint32_t sequence = state.transmitQueue.front();
        auto pending = state.pending.find(sequence);
        if (pending == state.pending.end())
        {
            state.transmitQueue.pop_front();
            continue;
        }
        if (state.inflightBytes + pending->second.wireBytes > state.congestionWindow)
        {
            break;
        }
        state.transmitQueue.pop_front();
        state.inflightBytes += pending->second.wireBytes;
        pending->second.sent = true;
        const Time sendAt = std::max(Simulator::Now(), state.nextSend);
        Simulator::Schedule(sendAt - Simulator::Now(),
                            &RoceV2TransportAdapter::TransmitSequence,
                            this,
                            connectionId,
                            sequence,
                            false);
        const uint64_t rate = std::max<uint64_t>(1, state.currentRateBps);
        const uint64_t serializationNs = std::max<uint64_t>(
            1,
            (static_cast<uint64_t>(pending->second.wireBytes) * 8ULL * 1000000000ULL + rate - 1) /
                rate);
        state.nextSend = sendAt + NanoSeconds(serializationNs);
    }
}

void
RoceV2TransportAdapter::TransmitSequence(uint32_t connectionId,
                                         uint32_t sequence,
                                         bool retransmission)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto pending = connection->second.pending.find(sequence);
    if (pending == connection->second.pending.end())
    {
        return;
    }
    auto packet = pending->second.payload->Copy();
    if (IsRoceWriteOpcode(pending->second.opcode) && IsRoceFirstOpcode(pending->second.opcode))
    {
        RoceRethHeader reth;
        reth.SetVirtualAddress(pending->second.tag.GetMessageId());
        reth.SetRemoteKey(connectionId);
        reth.SetDmaLength(pending->second.tag.GetTotalMessageBytes());
        packet->AddHeader(reth);
    }
    RoceBthHeader bth;
    bth.SetOpcode(pending->second.opcode);
    bth.SetDestinationQp(connectionId);
    bth.SetPacketSequence(sequence);
    bth.SetAckRequest(true);
    packet->AddHeader(bth);
    if (retransmission)
    {
        NotifyRetransmission(connectionId, sequence);
    }
    SendWirePacket(packet, pending->second.tag, connectionId);
    if (pending->second.timeout.IsPending())
    {
        pending->second.timeout.Cancel();
    }
    const Time timeout = connection->second.retransmissionTimeout *
                         (1ULL << std::min(pending->second.retransmissions, 6u));
    pending->second.timeout = Simulator::Schedule(timeout,
                                                  &RoceV2TransportAdapter::HandleTimeout,
                                                  this,
                                                  connectionId,
                                                  sequence);
}

bool
RoceV2TransportAdapter::SendWirePacket(Ptr<Packet> packet,
                                       const RoceSimulationTag& tag,
                                       uint32_t connectionId,
                                       uint32_t pathId)
{
    if (!packet || !m_socket)
    {
        return false;
    }
    const auto peer = m_peers.find(tag.GetDestinationEndpointId());
    if (peer == m_peers.end())
    {
        return false;
    }
    if (packet->GetSize() + RoceInvariantCrcTrailer::SERIALIZED_SIZE + 28 > m_pathMtu)
    {
        ++m_counters.mtuDrops;
        return false;
    }
    RoceInvariantCrcTrailer crc;
    crc.SetCrc(RoceInvariantCrcTrailer::Calculate(packet));
    packet->AddTrailer(crc);
    packet->AddPacketTag(tag);
    SocketSetDontFragmentTag dontFragment;
    dontFragment.Enable();
    packet->AddPacketTag(dontFragment);
    SocketIpTosTag tos;
    tos.SetTos(0x02); // ECN ECT(0)
    packet->AddPacketTag(tos);
    const Address destination =
        InetSocketAddress(Ipv4Address::ConvertFrom(peer->second.address), peer->second.port);
    if (m_socket->SendTo(packet, 0, destination) < 0)
    {
        return false;
    }
    ++m_counters.transmittedDatagrams;
    NotifyPacketTx(packet, connectionId, pathId);
    NotifyPathSelected(connectionId, 0, pathId);
    return true;
}

void
RoceV2TransportAdapter::Receive(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        RoceSimulationTag tag;
        if (!packet->PeekPacketTag(tag) ||
            packet->GetSize() <
                RoceBthHeader::SERIALIZED_SIZE + RoceInvariantCrcTrailer::SERIALIZED_SIZE)
        {
            ++m_counters.integrityDrops;
            continue;
        }
        SocketIpTosTag tos;
        const bool congestionExperienced =
            packet->PeekPacketTag(tos) && (tos.GetTos() & 0x03) == 0x03;
        RoceInvariantCrcTrailer receivedCrc;
        packet->RemoveTrailer(receivedCrc);
        if (receivedCrc.GetCrc() != RoceInvariantCrcTrailer::Calculate(packet))
        {
            ++m_counters.integrityDrops;
            continue;
        }
        ++m_counters.receivedDatagrams;
        Ptr<Packet> wireImage = packet->Copy();
        RoceBthHeader bth;
        if (packet->RemoveHeader(bth) == 0)
        {
            ++m_counters.integrityDrops;
            continue;
        }
        NotifyPacketRx(wireImage, tag.GetConnectionId(), 0);

        const RoceOpcode opcode = bth.GetOpcode();
        if (IsRoceDataOpcode(opcode))
        {
            if (IsRoceWriteOpcode(opcode) && IsRoceFirstOpcode(opcode))
            {
                RoceRethHeader reth;
                if (packet->RemoveHeader(reth) == 0)
                {
                    ++m_counters.integrityDrops;
                    continue;
                }
            }
            const uint64_t key = ReceiverKey(tag.GetSourceEndpointId(), tag.GetConnectionId());
            uint32_t& expected = m_expectedReceiveSequence[key];
            if (expected == 0)
            {
                expected = 1;
            }
            const uint32_t sequence = bth.GetPacketSequence();
            if (sequence == expected)
            {
                ++expected;
                NotifyPayloadRx(tag.GetSourceEndpointId(), tag.GetPayloadBytes());
                if (IsRoceLastOpcode(opcode))
                {
                    NotifyMessageComplete(tag.GetConnectionId(),
                                          tag.GetMessageId(),
                                          tag.GetTotalMessageBytes(),
                                          Simulator::Now() - NanoSeconds(tag.GetSubmittedTimeNs()));
                }
                SendAck(tag, sequence, false);
            }
            else if (sequence < expected)
            {
                SendAck(tag, sequence, false);
            }
            else
            {
                NotifyReorderDepth(tag.GetConnectionId(), 0, sequence - expected);
                SendAck(tag, expected, true);
            }
            if (congestionExperienced)
            {
                SendCnp(tag);
            }
        }
        else if (opcode == RoceOpcode::RC_ACK)
        {
            RoceAethHeader aeth;
            if (packet->RemoveHeader(aeth) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            if (aeth.GetSyndrome() == RoceAethHeader::SEQUENCE_NAK_SYNDROME)
            {
                ProcessNack(tag.GetConnectionId(), bth.GetPacketSequence());
            }
            else
            {
                ProcessAck(tag.GetConnectionId(), bth.GetPacketSequence());
            }
        }
        else if (opcode == RoceOpcode::CNP)
        {
            RoceCnpHeader cnp;
            if (packet->RemoveHeader(cnp) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            ProcessCnp(tag.GetConnectionId());
        }
    }
}

void
RoceV2TransportAdapter::SendAck(const RoceSimulationTag& received, uint32_t sequence, bool nack)
{
    Ptr<Packet> packet = Create<Packet>();
    RoceAethHeader aeth;
    aeth.SetSyndrome(nack ? RoceAethHeader::SEQUENCE_NAK_SYNDROME : RoceAethHeader::ACK_SYNDROME);
    aeth.SetMessageSequence(received.GetMessageId() & 0xffffff);
    packet->AddHeader(aeth);
    RoceBthHeader bth;
    bth.SetOpcode(RoceOpcode::RC_ACK);
    bth.SetDestinationQp(received.GetConnectionId());
    bth.SetPacketSequence(sequence);
    packet->AddHeader(bth);
    RoceSimulationTag response;
    response.SetSourceEndpointId(m_config.endpointId);
    response.SetDestinationEndpointId(received.GetSourceEndpointId());
    response.SetConnectionId(received.GetConnectionId());
    response.SetMessageId(received.GetMessageId());
    SendWirePacket(packet, response, received.GetConnectionId());
}

void
RoceV2TransportAdapter::SendCnp(const RoceSimulationTag& received)
{
    const uint64_t key = ReceiverKey(received.GetSourceEndpointId(), received.GetConnectionId());
    const auto last = m_lastCnp.find(key);
    if (last != m_lastCnp.end() && Simulator::Now() - last->second < m_cnpInterval)
    {
        return;
    }
    m_lastCnp[key] = Simulator::Now();
    Ptr<Packet> packet = Create<Packet>();
    RoceCnpHeader cnp;
    cnp.SetSourceQp(received.GetConnectionId());
    packet->AddHeader(cnp);
    RoceBthHeader bth;
    bth.SetOpcode(RoceOpcode::CNP);
    bth.SetDestinationQp(received.GetConnectionId());
    packet->AddHeader(bth);
    RoceSimulationTag response;
    response.SetSourceEndpointId(m_config.endpointId);
    response.SetDestinationEndpointId(received.GetSourceEndpointId());
    response.SetConnectionId(received.GetConnectionId());
    response.SetMessageId(received.GetMessageId());
    SendWirePacket(packet, response, received.GetConnectionId());
}

void
RoceV2TransportAdapter::ProcessAck(uint32_t connectionId, uint32_t sequence)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto& state = connection->second;
    std::vector<uint64_t> completed;
    for (auto pending = state.pending.begin();
         pending != state.pending.end() && pending->first <= sequence;)
    {
        if (!pending->second.sent)
        {
            ++pending;
            continue;
        }
        pending->second.timeout.Cancel();
        state.inflightBytes = state.inflightBytes > pending->second.wireBytes
                                  ? state.inflightBytes - pending->second.wireBytes
                                  : 0;
        const uint64_t messageId = pending->second.tag.GetMessageId();
        auto message = state.messages.find(messageId);
        if (message != state.messages.end() && message->second.remainingPackets > 0 &&
            --message->second.remainingPackets == 0)
        {
            completed.push_back(messageId);
        }
        pending = state.pending.erase(pending);
    }
    for (uint64_t messageId : completed)
    {
        const auto message = state.messages.find(messageId);
        if (message != state.messages.end())
        {
            state.messages.erase(message);
        }
    }
    TryTransmit(connectionId);
}

void
RoceV2TransportAdapter::ProcessNack(uint32_t connectionId, uint32_t expectedSequence)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    NotifyNack(connectionId, expectedSequence);
    for (auto& [sequence, pending] : connection->second.pending)
    {
        if (sequence >= expectedSequence && pending.sent &&
            pending.retransmissions < m_config.maxRetransmissions)
        {
            ++pending.retransmissions;
            TransmitSequence(connectionId, sequence, true);
        }
    }
}

void
RoceV2TransportAdapter::HandleTimeout(uint32_t connectionId, uint32_t sequence)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto pending = connection->second.pending.find(sequence);
    if (pending == connection->second.pending.end() ||
        pending->second.retransmissions >= m_config.maxRetransmissions)
    {
        return;
    }
    NotifyTimeout(connectionId, sequence);
    ++pending->second.retransmissions;
    TransmitSequence(connectionId, sequence, true);
}

void
RoceV2TransportAdapter::ProcessCnp(uint32_t connectionId)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto& state = connection->second;
    NotifyEcnReceived(connectionId, 0);
    state.dcqcnAlpha = (1.0 - m_dcqcnG) * state.dcqcnAlpha + m_dcqcnG;
    state.targetRateBps = state.currentRateBps;
    const double reduction = std::max(0.0, 1.0 - state.dcqcnAlpha / 2.0);
    const uint64_t minimumRate =
        std::max<uint64_t>(1, static_cast<uint64_t>(state.lineRateBps * m_dcqcnMinRateFactor));
    state.currentRateBps =
        std::max(minimumRate, static_cast<uint64_t>(state.currentRateBps * reduction));
    state.recoveryStage = 0;
    if (state.recoveryEvent.IsPending())
    {
        state.recoveryEvent.Cancel();
    }
    state.recoveryEvent = Simulator::Schedule(m_dcqcnRecoveryPeriod,
                                              &RoceV2TransportAdapter::RecoverDcqcnRate,
                                              this,
                                              connectionId);
}

void
RoceV2TransportAdapter::RecoverDcqcnRate(uint32_t connectionId)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto& state = connection->second;
    state.dcqcnAlpha *= 1.0 - m_dcqcnG;
    if (state.recoveryStage >= 5)
    {
        state.targetRateBps =
            std::min(state.lineRateBps, state.targetRateBps + m_dcqcnAdditiveIncreaseBps);
    }
    state.currentRateBps =
        std::min(state.lineRateBps, (state.currentRateBps + state.targetRateBps) / 2);
    ++state.recoveryStage;
    if (state.currentRateBps < state.lineRateBps || state.dcqcnAlpha > 0.000001)
    {
        state.recoveryEvent = Simulator::Schedule(m_dcqcnRecoveryPeriod,
                                                  &RoceV2TransportAdapter::RecoverDcqcnRate,
                                                  this,
                                                  connectionId);
    }
}

uint64_t
RoceV2TransportAdapter::ReceiverKey(uint32_t sourceEndpointId, uint32_t connectionId) const
{
    return (static_cast<uint64_t>(sourceEndpointId) << 32) | connectionId;
}

void
RoceV2TransportAdapter::DoDispose()
{
    for (auto& [connectionId, state] : m_connections)
    {
        (void)connectionId;
        state.recoveryEvent.Cancel();
        for (auto& [sequence, pending] : state.pending)
        {
            (void)sequence;
            pending.timeout.Cancel();
        }
    }
    m_connections.clear();
    m_peers.clear();
    m_expectedReceiveSequence.clear();
    m_lastCnp.clear();
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    m_node = nullptr;
    AiTransportEndpoint::DoDispose();
}

} // namespace ns3
