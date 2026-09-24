/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "veroce-transport-adapter.h"

#include "veroce-trim-queue-disc.h"

#include "ns3/double.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(VeRoceTransportAdapter);

TypeId
VeRoceTransportAdapter::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::VeRoceTransportAdapter")
            .SetParent<AiTransportEndpoint>()
            .SetGroupName("VeRoce")
            .AddConstructor<VeRoceTransportAdapter>()
            .AddAttribute("PathMtu",
                          "Maximum IPv4 datagram size; packets are sent with DF set.",
                          UintegerValue(9000),
                          MakeUintegerAccessor(&VeRoceTransportAdapter::m_pathMtu),
                          MakeUintegerChecker<uint32_t>(576, 65535))
            .AddAttribute("PathCount",
                          "Number of UDP source-port entropy paths used for packet spreading.",
                          UintegerValue(4),
                          MakeUintegerAccessor(&VeRoceTransportAdapter::m_pathCount),
                          MakeUintegerChecker<uint32_t>(1, 64))
            .AddAttribute("ReceiveBitmapLength",
                          "Number of out-of-order PSNs retained by a receiver.",
                          UintegerValue(4096),
                          MakeUintegerAccessor(&VeRoceTransportAdapter::m_receiveBitmapLength),
                          MakeUintegerChecker<uint32_t>(128, 8388608))
            .AddAttribute("LazySackThreshold",
                          "OOO degree above which the receiver emits SACK instead of ACK.",
                          UintegerValue(4),
                          MakeUintegerAccessor(&VeRoceTransportAdapter::m_lazySackThreshold),
                          MakeUintegerChecker<uint32_t>(1, 127))
            .AddAttribute("FccGain",
                          "Estimator gain of the default rate-based FCC controller.",
                          DoubleValue(1.0 / 256.0),
                          MakeDoubleAccessor(&VeRoceTransportAdapter::m_fccGain),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("FccMinRateFactor",
                          "Minimum per-path rate as a fraction of its configured rate.",
                          DoubleValue(0.01),
                          MakeDoubleAccessor(&VeRoceTransportAdapter::m_fccMinRateFactor),
                          MakeDoubleChecker<double>(0.000001, 1.0))
            .AddAttribute("FccAdditiveIncreaseRate",
                          "Per-path additive rate-recovery step in bits per second.",
                          UintegerValue(5000000000ULL),
                          MakeUintegerAccessor(&VeRoceTransportAdapter::m_fccAdditiveIncreaseBps),
                          MakeUintegerChecker<uint64_t>(1))
            .AddAttribute("FccRecoveryPeriod",
                          "Interval between per-path FCC recovery steps.",
                          TimeValue(MicroSeconds(55)),
                          MakeTimeAccessor(&VeRoceTransportAdapter::m_fccRecoveryPeriod),
                          MakeTimeChecker(MicroSeconds(1)))
            .AddAttribute("CnpInterval",
                          "Minimum interval between path-specific CNP packets.",
                          TimeValue(MicroSeconds(50)),
                          MakeTimeAccessor(&VeRoceTransportAdapter::m_cnpInterval),
                          MakeTimeChecker(MicroSeconds(1)))
            .AddAttribute("RttProbeInterval",
                          "Interval between standalone RTT probes on each path.",
                          TimeValue(MicroSeconds(20)),
                          MakeTimeAccessor(&VeRoceTransportAdapter::m_rttProbeInterval),
                          MakeTimeChecker(MicroSeconds(1)))
            .AddAttribute("SlowPacketPsnThreshold",
                          "PSN lag that causes a receiver slow-path signal.",
                          UintegerValue(16),
                          MakeUintegerAccessor(&VeRoceTransportAdapter::m_slowPacketPsnThreshold),
                          MakeUintegerChecker<uint32_t>(1, 0x7fffff))
            .AddAttribute("SlowSignalThreshold",
                          "Signals required within a window to quarantine a path.",
                          UintegerValue(3),
                          MakeUintegerAccessor(&VeRoceTransportAdapter::m_slowSignalThreshold),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("SlowSignalWindow",
                          "Window over which slow-path signals are accumulated.",
                          TimeValue(MicroSeconds(100)),
                          MakeTimeAccessor(&VeRoceTransportAdapter::m_slowSignalWindow),
                          MakeTimeChecker(MicroSeconds(1)))
            .AddAttribute("SlowPathHoldDown",
                          "Time for which a detected slow path is avoided.",
                          TimeValue(MicroSeconds(200)),
                          MakeTimeAccessor(&VeRoceTransportAdapter::m_slowPathHoldDown),
                          MakeTimeChecker(MicroSeconds(1)))
            .AddAttribute("SlowRttFactor",
                          "RTT ratio to the best measured path that marks a path slow.",
                          DoubleValue(2.0),
                          MakeDoubleAccessor(&VeRoceTransportAdapter::m_slowRttFactor),
                          MakeDoubleChecker<double>(1.0));
    return tid;
}

VeRoceTransportAdapter::VeRoceTransportAdapter() = default;
VeRoceTransportAdapter::~VeRoceTransportAdapter() = default;

AiTransportProtocol
VeRoceTransportAdapter::GetProtocol() const
{
    return AiTransportProtocol::VEROCE;
}

AiTransportCapabilities
VeRoceTransportAdapter::GetCapabilities() const
{
    AiTransportCapabilities capabilities;
    capabilities.reliableUnordered = true;
    capabilities.reliableOrdered = false;
    capabilities.selectiveAcknowledgment = true;
    capabilities.packetSpraying = true;
    capabilities.perPathCongestionControl = true;
    capabilities.endpointTrimming = true;
    capabilities.jobScheduling = false;
    return capabilities;
}

bool
VeRoceTransportAdapter::Initialize(Ptr<Node> node, const AiTransportEndpointConfig& config)
{
    if (!node || m_receiveSocket || config.endpointId == 0 || config.lineRateBps == 0 ||
        config.payloadMtuBytes == 0 || m_pathCount == 0)
    {
        return false;
    }
    m_node = node;
    m_config = config;
    m_receiveSocket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
    if (!m_receiveSocket ||
        m_receiveSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), UDP_PORT)) != 0)
    {
        m_receiveSocket = nullptr;
        m_node = nullptr;
        return false;
    }
    m_receiveSocket->SetRecvCallback(MakeCallback(&VeRoceTransportAdapter::Receive, this));
    m_receiveSocket->SetIpRecvTos(true);
    for (uint32_t path = 0; path < m_pathCount; ++path)
    {
        Ptr<Socket> socket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
        if (!socket || socket->Bind() != 0)
        {
            DoDispose();
            return false;
        }
        m_pathSockets.push_back(socket);
    }
    return true;
}

bool
VeRoceTransportAdapter::AddPeer(uint32_t endpointId, const Address& address)
{
    if (!m_receiveSocket || endpointId == 0 || !Ipv4Address::IsMatchingType(address))
    {
        return false;
    }
    m_peers[endpointId] = {Ipv4Address::ConvertFrom(address), UDP_PORT};
    return true;
}

uint32_t
VeRoceTransportAdapter::OpenConnection(const AiTransportConnectionConfig& config)
{
    if (!m_receiveSocket || config.remoteEndpointId == 0 ||
        !m_peers.contains(config.remoteEndpointId) ||
        config.reliability != AiTransportReliability::RELIABLE_UNORDERED)
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
    state.retransmissionTimeout = config.retransmissionTimeout;
    const uint64_t aggregateRate =
        config.lineRateBps == 0 ? m_config.lineRateBps : config.lineRateBps;
    const uint64_t pathRate = std::max<uint64_t>(1, aggregateRate / m_pathCount);
    state.paths.resize(m_pathCount);
    for (auto& path : state.paths)
    {
        path.lineRateBps = pathRate;
        path.currentRateBps = pathRate;
        path.targetRateBps = pathRate;
    }
    m_connections.emplace(connectionId, std::move(state));
    return connectionId;
}

bool
VeRoceTransportAdapter::Submit(const AiTransportRequest& request)
{
    auto connection = m_connections.find(request.connectionId);
    if (connection == m_connections.end() || !request.payload || request.payload->GetSize() == 0 ||
        request.remoteEndpointId != connection->second.remoteEndpointId ||
        request.reliability != AiTransportReliability::RELIABLE_UNORDERED ||
        (request.operation != AiTransportOperation::MESSAGE &&
         request.operation != AiTransportOperation::SEND &&
         request.operation != AiTransportOperation::WRITE &&
         request.operation != AiTransportOperation::READ))
    {
        return false;
    }
    auto& state = connection->second;
    if (state.messages.contains(request.messageId))
    {
        return false;
    }
    const uint32_t totalBytes = request.payload->GetSize();
    const bool read = request.operation == AiTransportOperation::READ;
    const uint32_t fragments =
        read ? 1 : (totalBytes + m_config.payloadMtuBytes - 1) / m_config.payloadMtuBytes;
    if (state.nextSequence + fragments > 0xffffff || state.nextMessageSequence > 0xffffff)
    {
        return false;
    }
    const uint32_t msn = state.nextMessageSequence++;
    state.messages.emplace(request.messageId, MessageState{totalBytes, fragments, read, false});
    uint32_t offset = 0;
    for (uint32_t fragment = 0; fragment < fragments; ++fragment)
    {
        const uint32_t bytes = read ? 0 : std::min(m_config.payloadMtuBytes, totalBytes - offset);
        PendingPacket pending;
        pending.payload = read ? Create<Packet>() : request.payload->CreateFragment(offset, bytes);
        pending.opcode = SelectOpcode(request.operation, fragment, fragments);
        pending.sequence = state.nextSequence++;
        pending.messageSequence = msn;
        pending.packetOrder = fragment;
        pending.payloadBytes = bytes;
        pending.readRequest = read;
        const bool write = IsRoceWriteOpcode(pending.opcode);
        pending.wireBytes =
            bytes + RoceBthHeader::SERIALIZED_SIZE + VeRoceMsnHeader::SERIALIZED_SIZE +
            ((write || read) ? RoceRethHeader::SERIALIZED_SIZE : VeRoceRqHeader::SERIALIZED_SIZE) +
            (IsRoceFirstOpcode(pending.opcode) ? 0 : VeRocePacketOffsetHeader::SERIALIZED_SIZE) +
            RoceInvariantCrcTrailer::SERIALIZED_SIZE;
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
    for (uint32_t pathId = 0; pathId < state.paths.size(); ++pathId)
    {
        if (!state.paths[pathId].probeEvent.IsPending())
        {
            const int64_t staggerNs = m_rttProbeInterval.GetNanoSeconds() *
                                      static_cast<int64_t>(pathId + 1) /
                                      static_cast<int64_t>(state.paths.size() + 1);
            state.paths[pathId].probeEvent =
                Simulator::Schedule(NanoSeconds(staggerNs),
                                    &VeRoceTransportAdapter::SendRttProbe,
                                    this,
                                    request.connectionId,
                                    pathId);
        }
    }
    TryTransmit(request.connectionId);
    return true;
}

uint32_t
VeRoceTransportAdapter::GetCongestionWindow(uint32_t connectionId) const
{
    const auto connection = m_connections.find(connectionId);
    return connection == m_connections.end() ? 0 : connection->second.congestionWindow;
}

bool
VeRoceTransportAdapter::SetCongestionWindow(uint32_t connectionId, uint32_t bytes)
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
VeRoceTransportAdapter::SetConnectionRate(uint32_t connectionId, uint64_t rateBps)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || rateBps == 0)
    {
        return false;
    }
    const uint64_t pathRate = std::max<uint64_t>(1, rateBps / connection->second.paths.size());
    for (auto& path : connection->second.paths)
    {
        path.lineRateBps = pathRate;
        path.currentRateBps = pathRate;
        path.targetRateBps = pathRate;
    }
    return true;
}

bool
VeRoceTransportAdapter::ConfigureJobScheduler(uint64_t)
{
    return false;
}

bool
VeRoceTransportAdapter::AssignConnectionToJob(uint32_t, uint32_t, uint32_t)
{
    return false;
}

AiTransportCounters
VeRoceTransportAdapter::GetCounters() const
{
    return m_counters;
}

RoceOpcode
VeRoceTransportAdapter::SelectOpcode(AiTransportOperation operation,
                                     uint32_t fragment,
                                     uint32_t fragments) const
{
    if (operation == AiTransportOperation::READ)
    {
        return RoceOpcode::RC_READ_REQUEST;
    }
    // The protocol-neutral MESSAGE operation maps to RDMA Write for the P2 profile.
    const bool write = operation != AiTransportOperation::SEND;
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

uint32_t
VeRoceTransportAdapter::SelectPath(ConnectionState& state)
{
    const uint32_t pathCount = state.paths.size();
    uint32_t earliestPath = state.nextPath % pathCount;
    Time earliestRelease = Time::Max();
    for (uint32_t attempt = 0; attempt < pathCount; ++attempt)
    {
        const uint32_t pathId = state.nextPath++ % pathCount;
        const auto& path = state.paths[pathId];
        if (path.slowUntil <= Simulator::Now())
        {
            return pathId;
        }
        if (path.slowUntil < earliestRelease)
        {
            earliestRelease = path.slowUntil;
            earliestPath = pathId;
        }
    }
    return earliestPath;
}

void
VeRoceTransportAdapter::TryTransmit(uint32_t connectionId)
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
        pending->second.pathId = SelectPath(state);
        auto& path = state.paths[pending->second.pathId];
        const Time sendAt = std::max(Simulator::Now(), path.nextSend);
        Simulator::Schedule(sendAt - Simulator::Now(),
                            &VeRoceTransportAdapter::TransmitSequence,
                            this,
                            connectionId,
                            sequence,
                            false);
        const uint64_t rate = std::max<uint64_t>(1, path.currentRateBps);
        const uint64_t serializationNs = std::max<uint64_t>(
            1,
            (static_cast<uint64_t>(pending->second.wireBytes) * 8ULL * 1000000000ULL + rate - 1) /
                rate);
        path.nextSend = sendAt + NanoSeconds(serializationNs);
    }
}

void
VeRoceTransportAdapter::TransmitSequence(uint32_t connectionId,
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
    if (retransmission)
    {
        pending->second.pathId = SelectPath(connection->second);
    }
    auto packet = pending->second.payload->Copy();
    if (!IsRoceFirstOpcode(pending->second.opcode))
    {
        VeRocePacketOffsetHeader poeth;
        poeth.SetPacketOrder(pending->second.packetOrder);
        packet->AddHeader(poeth);
    }
    if (IsRoceWriteOpcode(pending->second.opcode) || pending->second.readRequest)
    {
        RoceRethHeader reth;
        reth.SetVirtualAddress(pending->second.tag.GetMessageId() +
                               pending->second.packetOrder * m_config.payloadMtuBytes);
        reth.SetRemoteKey(connectionId);
        reth.SetDmaLength(pending->second.readRequest ? pending->second.tag.GetTotalMessageBytes()
                                                      : pending->second.payloadBytes);
        packet->AddHeader(reth);
    }
    else
    {
        VeRoceRqHeader rqeth;
        rqeth.SetReceiveQueueSequence(pending->second.messageSequence - 1);
        packet->AddHeader(rqeth);
    }
    VeRoceMsnHeader msneth;
    msneth.SetMessageSequence(pending->second.messageSequence);
    packet->AddHeader(msneth);
    RoceBthHeader bth;
    bth.SetOpcode(pending->second.opcode);
    bth.SetDestinationQp(connectionId);
    bth.SetPacketSequence(sequence);
    bth.SetAckRequest(true);
    bth.SetRetransmission(retransmission);
    packet->AddHeader(bth);
    if (retransmission)
    {
        NotifyRetransmission(connectionId, sequence);
    }
    SendWirePacket(packet, pending->second.tag, connectionId, sequence, pending->second.pathId);
    if (pending->second.timeout.IsPending())
    {
        pending->second.timeout.Cancel();
    }
    const Time timeout = connection->second.retransmissionTimeout *
                         (1ULL << std::min(pending->second.retransmissions, 6u));
    pending->second.timeout = Simulator::Schedule(timeout,
                                                  &VeRoceTransportAdapter::HandleTimeout,
                                                  this,
                                                  connectionId,
                                                  sequence);
}

bool
VeRoceTransportAdapter::SendWirePacket(Ptr<Packet> packet,
                                       RoceSimulationTag tag,
                                       uint32_t connectionId,
                                       uint32_t sequence,
                                       uint32_t pathId)
{
    if (!packet || m_pathSockets.empty())
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
    tag.SetPathId(pathId);
    packet->AddPacketTag(tag);
    SocketSetDontFragmentTag dontFragment;
    dontFragment.Enable();
    packet->AddPacketTag(dontFragment);
    SocketIpTosTag tos;
    tos.SetTos(tag.GetPayloadBytes() > 0 ? 0x22 : 0x02);
    packet->AddPacketTag(tos);
    const Address destination =
        InetSocketAddress(Ipv4Address::ConvertFrom(peer->second.address), peer->second.port);
    Ptr<Socket> socket = m_pathSockets[pathId % m_pathSockets.size()];
    if (socket->SendTo(packet, 0, destination) < 0)
    {
        return false;
    }
    ++m_counters.transmittedDatagrams;
    NotifyPacketTx(packet, connectionId, pathId);
    NotifyPathSelected(connectionId, sequence, pathId);
    return true;
}

void
VeRoceTransportAdapter::Receive(Ptr<Socket> socket)
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
        const bool hasTos = packet->PeekPacketTag(tos);
        const bool congestionExperienced = hasTos && (tos.GetTos() & 0x03) == 0x03;
        const bool packetTrimmed =
            hasTos && (tos.GetTos() >> 2) == VeRoceTrimQueueDisc::TRIMMED_DSCP;
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
        NotifyPacketRx(wireImage, tag.GetConnectionId(), tag.GetPathId());
        const RoceOpcode opcode = bth.GetOpcode();
        if (IsRoceDataOpcode(opcode))
        {
            const bool readRequest = IsRoceReadRequestOpcode(opcode);
            const bool readResponse = IsRoceReadResponseOpcode(opcode);
            VeRoceMsnHeader msneth;
            if (packet->RemoveHeader(msneth) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            if (packetTrimmed)
            {
                NotifyPacketTrimmed(wireImage, tag.GetConnectionId(), tag.GetPathId());
                SendPacketDropNak(tag,
                                  bth.GetPacketSequence(),
                                  msneth.GetMessageSequence(),
                                  readResponse);
                continue;
            }
            if (readResponse)
            {
                RoceAethHeader aeth;
                if (packet->RemoveHeader(aeth) == 0)
                {
                    ++m_counters.integrityDrops;
                    continue;
                }
            }
            else if (IsRoceWriteOpcode(opcode) || readRequest)
            {
                RoceRethHeader reth;
                if (packet->RemoveHeader(reth) == 0)
                {
                    ++m_counters.integrityDrops;
                    continue;
                }
            }
            else
            {
                VeRoceRqHeader rqeth;
                if (packet->RemoveHeader(rqeth) == 0)
                {
                    ++m_counters.integrityDrops;
                    continue;
                }
            }
            uint32_t packetOrder = 0;
            if (!IsRoceFirstOpcode(opcode))
            {
                VeRocePacketOffsetHeader poeth;
                if (packet->RemoveHeader(poeth) == 0)
                {
                    ++m_counters.integrityDrops;
                    continue;
                }
                packetOrder = poeth.GetPacketOrder();
            }
            const uint64_t key = ReceiverKey(tag.GetSourceEndpointId(), tag.GetConnectionId());
            auto& state = readResponse ? m_responseReceivers[key] : m_receivers[key];
            const uint32_t sequence = bth.GetPacketSequence();
            const uint32_t previousHighest = state.highestPsn;
            if (sequence > state.acknowledgedPsn + m_receiveBitmapLength)
            {
                continue;
            }
            bool newPacket = false;
            if (sequence > state.acknowledgedPsn)
            {
                newPacket = state.receivedPsns.insert(sequence).second;
                state.highestPsn = std::max(state.highestPsn, sequence);
            }
            if (newPacket)
            {
                if (readRequest)
                {
                    auto& message = state.messages[msneth.GetMessageSequence()];
                    message.messageId = tag.GetMessageId();
                    message.totalBytes = tag.GetTotalMessageBytes();
                    message.submittedTimeNs = tag.GetSubmittedTimeNs();
                    message.completed = true;
                    while (true)
                    {
                        auto next = state.messages.find(state.acknowledgedMsn + 1);
                        if (next == state.messages.end() || !next->second.completed)
                        {
                            break;
                        }
                        ++state.acknowledgedMsn;
                        state.messages.erase(next);
                    }
                    GenerateReadResponse(tag, msneth.GetMessageSequence());
                }
                else
                {
                    NotifyPayloadRx(tag.GetSourceEndpointId(), tag.GetPayloadBytes());
                    const bool completed = CompleteReceivedMessage(tag,
                                                                   msneth.GetMessageSequence(),
                                                                   packetOrder,
                                                                   IsRoceLastOpcode(opcode),
                                                                   state);
                    if (readResponse && completed)
                    {
                        auto connection = m_connections.find(tag.GetConnectionId());
                        if (connection != m_connections.end())
                        {
                            auto message = connection->second.messages.find(tag.GetMessageId());
                            if (message != connection->second.messages.end())
                            {
                                message->second.responseComplete = true;
                                if (message->second.remainingPackets == 0)
                                {
                                    connection->second.messages.erase(message);
                                }
                            }
                        }
                    }
                }
                if (!readRequest && !readResponse && previousHighest > sequence &&
                    previousHighest - sequence >= m_slowPacketPsnThreshold)
                {
                    SendSlowPathSignal(tag, sequence);
                }
            }
            while (state.receivedPsns.erase(state.acknowledgedPsn + 1) > 0)
            {
                ++state.acknowledgedPsn;
            }
            const uint32_t oldDepth = state.reorderDepth;
            state.reorderDepth = state.highestPsn > state.acknowledgedPsn
                                     ? state.highestPsn - state.acknowledgedPsn
                                     : 0;
            if (oldDepth != state.reorderDepth)
            {
                NotifyReorderDepth(tag.GetConnectionId(), oldDepth, state.reorderDepth);
            }
            if (readResponse)
            {
                SendResponseAcknowledgment(tag, state, state.reorderDepth > m_lazySackThreshold);
            }
            else
            {
                SendAcknowledgment(tag, state, state.reorderDepth > m_lazySackThreshold);
            }
            if (congestionExperienced && !readResponse)
            {
                SendCnp(tag);
            }
        }
        else if (opcode == RoceOpcode::RC_ACK || opcode == RoceOpcode::RC_SACK)
        {
            RoceAethHeader aeth;
            VeRocePacketOffsetHeader poeth;
            if (packet->RemoveHeader(aeth) == 0 || packet->RemoveHeader(poeth) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            if (aeth.GetSyndrome() == RoceAethHeader::SEQUENCE_NAK_SYNDROME)
            {
                ProcessPacketDropNak(tag.GetConnectionId(), bth.GetPacketSequence());
            }
            else if (opcode == RoceOpcode::RC_SACK)
            {
                VeRoceSackHeader sack;
                if (packet->RemoveHeader(sack) == 0)
                {
                    ++m_counters.integrityDrops;
                    continue;
                }
                ProcessSack(tag.GetConnectionId(), bth.GetPacketSequence(), sack);
            }
            else
            {
                ProcessAck(tag.GetConnectionId(), bth.GetPacketSequence());
            }
        }
        else if (opcode == RoceOpcode::RC_ACK_RSP || opcode == RoceOpcode::RC_SACK_RSP)
        {
            RoceAethHeader aeth;
            VeRocePacketOffsetHeader poeth;
            if (packet->RemoveHeader(aeth) == 0 || packet->RemoveHeader(poeth) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            const uint64_t responseKey =
                ReceiverKey(tag.GetSourceEndpointId(), tag.GetConnectionId());
            if (aeth.GetSyndrome() == RoceAethHeader::SEQUENCE_NAK_SYNDROME)
            {
                ProcessResponsePacketDropNak(responseKey, bth.GetPacketSequence());
            }
            else if (opcode == RoceOpcode::RC_SACK_RSP)
            {
                VeRoceSackHeader sack;
                if (packet->RemoveHeader(sack) == 0)
                {
                    ++m_counters.integrityDrops;
                    continue;
                }
                ProcessResponseSack(responseKey, bth.GetPacketSequence(), sack);
            }
            else
            {
                ProcessResponseAck(responseKey, bth.GetPacketSequence());
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
            ProcessCnp(tag.GetConnectionId(), tag.GetPathId());
        }
        else if (opcode == RoceOpcode::RTT_REQUEST)
        {
            VeRoceRttHeader request;
            if (packet->RemoveHeader(request) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            SendRttResponse(tag, request);
        }
        else if (opcode == RoceOpcode::RTT_RESPONSE)
        {
            VeRoceRttHeader response;
            if (packet->RemoveHeader(response) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            ProcessRttResponse(tag.GetConnectionId(), tag.GetPathId(), response);
        }
        else if (opcode == RoceOpcode::SLOW_PATH)
        {
            ProcessSlowPathSignal(tag.GetConnectionId(), tag.GetPathId());
        }
    }
}

bool
VeRoceTransportAdapter::CompleteReceivedMessage(const RoceSimulationTag& tag,
                                                uint32_t messageSequence,
                                                uint32_t packetOrder,
                                                bool lastPacket,
                                                ReceiverState& state)
{
    auto& message = state.messages[messageSequence];
    if (message.messageId == 0)
    {
        message.messageId = tag.GetMessageId();
        message.totalBytes = tag.GetTotalMessageBytes();
        message.submittedTimeNs = tag.GetSubmittedTimeNs();
    }
    message.packetOrders.insert(packetOrder);
    if (lastPacket)
    {
        message.lastSeen = true;
        message.lastPacketOrder = packetOrder;
    }
    bool completedNow = false;
    if (!message.completed && message.lastSeen &&
        message.packetOrders.size() == static_cast<size_t>(message.lastPacketOrder + 1))
    {
        message.completed = true;
        completedNow = true;
        NotifyMessageComplete(tag.GetConnectionId(),
                              message.messageId,
                              message.totalBytes,
                              Simulator::Now() - NanoSeconds(message.submittedTimeNs));
    }
    while (true)
    {
        auto next = state.messages.find(state.acknowledgedMsn + 1);
        if (next == state.messages.end() || !next->second.completed)
        {
            break;
        }
        ++state.acknowledgedMsn;
        state.messages.erase(next);
    }
    return completedNow;
}

void
VeRoceTransportAdapter::SendAcknowledgment(const RoceSimulationTag& received,
                                           const ReceiverState& state,
                                           bool selective)
{
    Ptr<Packet> packet = Create<Packet>();
    if (selective)
    {
        ++m_counters.selectiveAcknowledgments;
        VeRoceSackHeader sack;
        sack.SetBitmapStartingPsn(state.acknowledgedPsn);
        const uint32_t span = std::min<uint32_t>(VeRoceSackHeader::MAX_BITMAP_BITS,
                                                 state.highestPsn >= state.acknowledgedPsn
                                                     ? state.highestPsn - state.acknowledgedPsn + 1
                                                     : 1);
        sack.SetBitmapValidLength(static_cast<uint8_t>(span));
        sack.SetReceived(0);
        for (uint32_t offset = 1; offset < span; ++offset)
        {
            sack.SetReceived(offset, state.receivedPsns.contains(state.acknowledgedPsn + offset));
        }
        packet->AddHeader(sack);
    }
    VeRocePacketOffsetHeader poeth;
    poeth.SetPacketOrder(VeRocePacketOffsetHeader::UNAVAILABLE);
    packet->AddHeader(poeth);
    RoceAethHeader aeth;
    aeth.SetSyndrome(RoceAethHeader::ACK_SYNDROME);
    aeth.SetMessageSequence(state.acknowledgedMsn);
    packet->AddHeader(aeth);
    RoceBthHeader bth;
    bth.SetOpcode(selective ? RoceOpcode::RC_SACK : RoceOpcode::RC_ACK);
    bth.SetDestinationQp(received.GetConnectionId());
    bth.SetPacketSequence(state.acknowledgedPsn);
    packet->AddHeader(bth);
    RoceSimulationTag response;
    response.SetSourceEndpointId(m_config.endpointId);
    response.SetDestinationEndpointId(received.GetSourceEndpointId());
    response.SetConnectionId(received.GetConnectionId());
    response.SetMessageId(received.GetMessageId());
    SendWirePacket(packet,
                   response,
                   received.GetConnectionId(),
                   state.acknowledgedPsn,
                   received.GetPathId());
}

void
VeRoceTransportAdapter::SendResponseAcknowledgment(const RoceSimulationTag& received,
                                                   const ReceiverState& state,
                                                   bool selective)
{
    Ptr<Packet> packet = Create<Packet>();
    if (selective)
    {
        ++m_counters.selectiveAcknowledgments;
        VeRoceSackHeader sack;
        sack.SetBitmapStartingPsn(state.acknowledgedPsn);
        const uint32_t span = std::min<uint32_t>(VeRoceSackHeader::MAX_BITMAP_BITS,
                                                 state.highestPsn >= state.acknowledgedPsn
                                                     ? state.highestPsn - state.acknowledgedPsn + 1
                                                     : 1);
        sack.SetBitmapValidLength(static_cast<uint8_t>(span));
        sack.SetReceived(0);
        for (uint32_t offset = 1; offset < span; ++offset)
        {
            sack.SetReceived(offset, state.receivedPsns.contains(state.acknowledgedPsn + offset));
        }
        packet->AddHeader(sack);
    }
    VeRocePacketOffsetHeader poeth;
    poeth.SetPacketOrder(VeRocePacketOffsetHeader::UNAVAILABLE);
    packet->AddHeader(poeth);
    RoceAethHeader aeth;
    aeth.SetSyndrome(RoceAethHeader::ACK_SYNDROME);
    aeth.SetMessageSequence(state.acknowledgedMsn);
    packet->AddHeader(aeth);
    RoceBthHeader bth;
    bth.SetOpcode(selective ? RoceOpcode::RC_SACK_RSP : RoceOpcode::RC_ACK_RSP);
    bth.SetDestinationQp(received.GetConnectionId());
    bth.SetPacketSequence(state.acknowledgedPsn);
    packet->AddHeader(bth);
    RoceSimulationTag response;
    response.SetSourceEndpointId(m_config.endpointId);
    response.SetDestinationEndpointId(received.GetSourceEndpointId());
    response.SetConnectionId(received.GetConnectionId());
    response.SetMessageId(received.GetMessageId());
    SendWirePacket(packet,
                   response,
                   received.GetConnectionId(),
                   state.acknowledgedPsn,
                   received.GetPathId());
}

void
VeRoceTransportAdapter::SendCnp(const RoceSimulationTag& received)
{
    const uint64_t key =
        ReceiverKey(received.GetSourceEndpointId(), received.GetConnectionId()) * 67ULL +
        received.GetPathId();
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
    SendWirePacket(packet, response, received.GetConnectionId(), 0, received.GetPathId());
}

void
VeRoceTransportAdapter::SendPacketDropNak(const RoceSimulationTag& received,
                                          uint32_t packetSequence,
                                          uint32_t messageSequence,
                                          bool responseSpace)
{
    Ptr<Packet> packet = Create<Packet>();
    VeRocePacketOffsetHeader poeth;
    poeth.SetPacketOrder(VeRocePacketOffsetHeader::UNAVAILABLE);
    packet->AddHeader(poeth);
    RoceAethHeader aeth;
    aeth.SetSyndrome(RoceAethHeader::SEQUENCE_NAK_SYNDROME);
    aeth.SetMessageSequence(messageSequence);
    packet->AddHeader(aeth);
    RoceBthHeader bth;
    bth.SetOpcode(responseSpace ? RoceOpcode::RC_ACK_RSP : RoceOpcode::RC_ACK);
    bth.SetDestinationQp(received.GetConnectionId());
    bth.SetPacketSequence(packetSequence);
    packet->AddHeader(bth);
    RoceSimulationTag response;
    response.SetSourceEndpointId(m_config.endpointId);
    response.SetDestinationEndpointId(received.GetSourceEndpointId());
    response.SetConnectionId(received.GetConnectionId());
    response.SetMessageId(received.GetMessageId());
    SendWirePacket(packet,
                   response,
                   received.GetConnectionId(),
                   packetSequence,
                   received.GetPathId());
}

void
VeRoceTransportAdapter::GenerateReadResponse(const RoceSimulationTag& request,
                                             uint32_t requestMessageSequence)
{
    const uint64_t responseKey =
        ReceiverKey(request.GetSourceEndpointId(), request.GetConnectionId());
    auto [found, inserted] = m_responseConnections.try_emplace(responseKey);
    auto& state = found->second;
    if (inserted)
    {
        state.remoteEndpointId = request.GetSourceEndpointId();
        state.nextSend.resize(m_pathCount, Simulator::Now());
    }
    const uint32_t totalBytes = request.GetTotalMessageBytes();
    const uint32_t fragments =
        (totalBytes + m_config.payloadMtuBytes - 1) / m_config.payloadMtuBytes;
    if (fragments == 0 || state.nextSequence + fragments > 0xffffff ||
        state.nextMessageSequence > 0xffffff)
    {
        return;
    }
    const uint32_t responseMsn = state.nextMessageSequence++;
    uint32_t offset = 0;
    const uint64_t pathRate = std::max<uint64_t>(1, m_config.lineRateBps / m_pathCount);
    for (uint32_t fragment = 0; fragment < fragments; ++fragment)
    {
        const uint32_t bytes = std::min(m_config.payloadMtuBytes, totalBytes - offset);
        ResponsePendingPacket pending;
        pending.payload = Create<Packet>(bytes);
        pending.opcode = fragments == 1
                             ? RoceOpcode::RC_READ_RESPONSE_ONLY
                             : (fragment == 0 ? RoceOpcode::RC_READ_RESPONSE_FIRST
                                              : (fragment + 1 == fragments
                                                     ? RoceOpcode::RC_READ_RESPONSE_LAST
                                                     : RoceOpcode::RC_READ_RESPONSE_MIDDLE));
        pending.sequence = state.nextSequence++;
        pending.responseMessageSequence = responseMsn;
        pending.requestMessageSequence = requestMessageSequence;
        pending.packetOrder = fragment;
        pending.pathId = state.nextPath++ % m_pathCount;
        pending.tag.SetSourceEndpointId(m_config.endpointId);
        pending.tag.SetDestinationEndpointId(request.GetSourceEndpointId());
        pending.tag.SetConnectionId(request.GetConnectionId());
        pending.tag.SetMessageId(request.GetMessageId());
        pending.tag.SetPayloadBytes(bytes);
        pending.tag.SetTotalMessageBytes(totalBytes);
        pending.tag.SetSubmittedTimeNs(request.GetSubmittedTimeNs());
        const uint32_t sequence = pending.sequence;
        state.pending.emplace(sequence, std::move(pending));
        const uint32_t wireBytes = bytes + RoceBthHeader::SERIALIZED_SIZE +
                                   VeRoceMsnHeader::SERIALIZED_SIZE +
                                   RoceAethHeader::SERIALIZED_SIZE +
                                   (fragment == 0 ? 0 : VeRocePacketOffsetHeader::SERIALIZED_SIZE) +
                                   RoceInvariantCrcTrailer::SERIALIZED_SIZE;
        const Time sendAt =
            std::max(Simulator::Now(), state.nextSend[state.pending.at(sequence).pathId]);
        Simulator::Schedule(sendAt - Simulator::Now(),
                            &VeRoceTransportAdapter::TransmitReadResponse,
                            this,
                            responseKey,
                            sequence,
                            false);
        const uint64_t serializationNs = std::max<uint64_t>(
            1,
            (static_cast<uint64_t>(wireBytes) * 8ULL * 1000000000ULL + pathRate - 1) / pathRate);
        state.nextSend[state.pending.at(sequence).pathId] = sendAt + NanoSeconds(serializationNs);
        offset += bytes;
    }
}

void
VeRoceTransportAdapter::TransmitReadResponse(uint64_t responseKey,
                                             uint32_t sequence,
                                             bool retransmission)
{
    auto response = m_responseConnections.find(responseKey);
    if (response == m_responseConnections.end())
    {
        return;
    }
    auto pending = response->second.pending.find(sequence);
    if (pending == response->second.pending.end())
    {
        return;
    }
    if (retransmission)
    {
        pending->second.pathId = response->second.nextPath++ % m_pathCount;
    }
    Ptr<Packet> packet = pending->second.payload->Copy();
    if (!IsRoceFirstOpcode(pending->second.opcode))
    {
        VeRocePacketOffsetHeader poeth;
        poeth.SetPacketOrder(pending->second.packetOrder);
        packet->AddHeader(poeth);
    }
    RoceAethHeader aeth;
    aeth.SetSyndrome(RoceAethHeader::ACK_SYNDROME);
    aeth.SetMessageSequence(pending->second.requestMessageSequence);
    packet->AddHeader(aeth);
    VeRoceMsnHeader msneth;
    msneth.SetMessageSequence(pending->second.requestMessageSequence);
    packet->AddHeader(msneth);
    RoceBthHeader bth;
    bth.SetOpcode(pending->second.opcode);
    bth.SetDestinationQp(pending->second.tag.GetConnectionId());
    bth.SetPacketSequence(sequence);
    bth.SetAckRequest(true);
    bth.SetRetransmission(retransmission);
    packet->AddHeader(bth);
    if (retransmission)
    {
        NotifyRetransmission(pending->second.tag.GetConnectionId(), sequence);
    }
    SendWirePacket(packet,
                   pending->second.tag,
                   pending->second.tag.GetConnectionId(),
                   sequence,
                   pending->second.pathId);
    pending->second.timeout.Cancel();
    const Time timeout = response->second.retransmissionTimeout *
                         (1ULL << std::min(pending->second.retransmissions, 6u));
    pending->second.timeout = Simulator::Schedule(timeout,
                                                  &VeRoceTransportAdapter::HandleResponseTimeout,
                                                  this,
                                                  responseKey,
                                                  sequence);
}

void
VeRoceTransportAdapter::HandleResponseTimeout(uint64_t responseKey, uint32_t sequence)
{
    auto response = m_responseConnections.find(responseKey);
    if (response == m_responseConnections.end())
    {
        return;
    }
    auto pending = response->second.pending.find(sequence);
    if (pending == response->second.pending.end() ||
        pending->second.retransmissions >= m_config.maxRetransmissions)
    {
        return;
    }
    NotifyTimeout(pending->second.tag.GetConnectionId(), sequence);
    ++pending->second.retransmissions;
    TransmitReadResponse(responseKey, sequence, true);
}

void
VeRoceTransportAdapter::ProcessResponseAck(uint64_t responseKey, uint32_t acknowledgedPsn)
{
    auto response = m_responseConnections.find(responseKey);
    if (response == m_responseConnections.end())
    {
        return;
    }
    auto& state = response->second;
    for (auto pending = state.pending.begin();
         pending != state.pending.end() && pending->first <= acknowledgedPsn;)
    {
        pending->second.timeout.Cancel();
        pending = state.pending.erase(pending);
    }
}

void
VeRoceTransportAdapter::ProcessResponseSack(uint64_t responseKey,
                                            uint32_t acknowledgedPsn,
                                            const VeRoceSackHeader& sack)
{
    ProcessResponseAck(responseKey, acknowledgedPsn);
    auto response = m_responseConnections.find(responseKey);
    if (response == m_responseConnections.end())
    {
        return;
    }
    auto& state = response->second;
    if (state.retransmitFrontierUpdated.IsZero() ||
        Simulator::Now() - state.retransmitFrontierUpdated >= state.retransmissionTimeout)
    {
        state.retransmitFrontier = acknowledgedPsn;
    }
    const uint32_t start = sack.GetBitmapStartingPsn();
    const uint32_t length = sack.GetBitmapValidLength();
    for (uint32_t offset = 0; offset < length; ++offset)
    {
        const uint32_t sequence = start + offset;
        if (sequence <= acknowledgedPsn || sequence <= state.retransmitFrontier ||
            sack.IsReceived(offset))
        {
            continue;
        }
        auto pending = state.pending.find(sequence);
        if (pending != state.pending.end() &&
            pending->second.retransmissions < m_config.maxRetransmissions)
        {
            ++pending->second.retransmissions;
            ++m_counters.fastRetransmissions;
            TransmitReadResponse(responseKey, sequence, true);
        }
    }
    if (length > 0)
    {
        state.retransmitFrontier = std::max(state.retransmitFrontier, start + length - 1);
        state.retransmitFrontierUpdated = Simulator::Now();
    }
}

void
VeRoceTransportAdapter::ProcessResponsePacketDropNak(uint64_t responseKey, uint32_t packetSequence)
{
    auto response = m_responseConnections.find(responseKey);
    if (response == m_responseConnections.end())
    {
        return;
    }
    auto pending = response->second.pending.find(packetSequence);
    if (pending == response->second.pending.end() ||
        pending->second.retransmissions >= m_config.maxRetransmissions)
    {
        return;
    }
    NotifyNack(pending->second.tag.GetConnectionId(), packetSequence);
    ++pending->second.retransmissions;
    ++m_counters.fastRetransmissions;
    TransmitReadResponse(responseKey, packetSequence, true);
}

void
VeRoceTransportAdapter::ProcessAck(uint32_t connectionId, uint32_t acknowledgedPsn)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto& state = connection->second;
    for (auto pending = state.pending.begin();
         pending != state.pending.end() && pending->first <= acknowledgedPsn;)
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
            --message->second.remainingPackets == 0 &&
            (!message->second.read || message->second.responseComplete))
        {
            state.messages.erase(message);
        }
        pending = state.pending.erase(pending);
    }
    TryTransmit(connectionId);
}

void
VeRoceTransportAdapter::ProcessPacketDropNak(uint32_t connectionId, uint32_t packetSequence)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto pending = connection->second.pending.find(packetSequence);
    if (pending == connection->second.pending.end() || !pending->second.sent ||
        pending->second.retransmissions >= m_config.maxRetransmissions)
    {
        return;
    }
    NotifyNack(connectionId, packetSequence);
    ++pending->second.retransmissions;
    ++m_counters.fastRetransmissions;
    TransmitSequence(connectionId, packetSequence, true);
}

void
VeRoceTransportAdapter::ProcessSack(uint32_t connectionId,
                                    uint32_t acknowledgedPsn,
                                    const VeRoceSackHeader& sack)
{
    ProcessAck(connectionId, acknowledgedPsn);
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto& state = connection->second;
    if (state.retransmitFrontierUpdated.IsZero() ||
        Simulator::Now() - state.retransmitFrontierUpdated >= state.retransmissionTimeout)
    {
        state.retransmitFrontier = acknowledgedPsn;
    }
    const uint32_t start = sack.GetBitmapStartingPsn();
    const uint32_t length = sack.GetBitmapValidLength();
    for (uint32_t offset = 0; offset < length; ++offset)
    {
        const uint32_t sequence = start + offset;
        if (sequence <= acknowledgedPsn || sequence <= state.retransmitFrontier ||
            sack.IsReceived(offset))
        {
            continue;
        }
        auto pending = state.pending.find(sequence);
        if (pending != state.pending.end() && pending->second.sent &&
            pending->second.retransmissions < m_config.maxRetransmissions)
        {
            ++pending->second.retransmissions;
            ++m_counters.fastRetransmissions;
            TransmitSequence(connectionId, sequence, true);
        }
    }
    if (length > 0)
    {
        state.retransmitFrontier = std::max(state.retransmitFrontier, start + length - 1);
        state.retransmitFrontierUpdated = Simulator::Now();
    }
}

void
VeRoceTransportAdapter::HandleTimeout(uint32_t connectionId, uint32_t sequence)
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
VeRoceTransportAdapter::ProcessCnp(uint32_t connectionId, uint32_t pathId)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || connection->second.paths.empty())
    {
        return;
    }
    pathId %= connection->second.paths.size();
    auto& path = connection->second.paths[pathId];
    NotifyEcnReceived(connectionId, pathId);
    path.alpha = (1.0 - m_fccGain) * path.alpha + m_fccGain;
    path.targetRateBps = path.currentRateBps;
    const double reduction = std::max(0.0, 1.0 - path.alpha / 2.0);
    const uint64_t minimumRate =
        std::max<uint64_t>(1, static_cast<uint64_t>(path.lineRateBps * m_fccMinRateFactor));
    path.currentRateBps =
        std::max(minimumRate, static_cast<uint64_t>(path.currentRateBps * reduction));
    path.recoveryStage = 0;
    path.recoveryEvent.Cancel();
    path.recoveryEvent = Simulator::Schedule(m_fccRecoveryPeriod,
                                             &VeRoceTransportAdapter::RecoverPathRate,
                                             this,
                                             connectionId,
                                             pathId);
}

void
VeRoceTransportAdapter::RecoverPathRate(uint32_t connectionId, uint32_t pathId)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || pathId >= connection->second.paths.size())
    {
        return;
    }
    auto& path = connection->second.paths[pathId];
    path.alpha *= 1.0 - m_fccGain;
    if (path.recoveryStage >= 5)
    {
        path.targetRateBps =
            std::min(path.lineRateBps, path.targetRateBps + m_fccAdditiveIncreaseBps);
    }
    path.currentRateBps =
        std::min(path.lineRateBps, (path.currentRateBps + path.targetRateBps) / 2);
    ++path.recoveryStage;
    if (path.currentRateBps < path.lineRateBps || path.alpha > 0.000001)
    {
        path.recoveryEvent = Simulator::Schedule(m_fccRecoveryPeriod,
                                                 &VeRoceTransportAdapter::RecoverPathRate,
                                                 this,
                                                 connectionId,
                                                 pathId);
    }
}

void
VeRoceTransportAdapter::SendRttProbe(uint32_t connectionId, uint32_t pathId)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || pathId >= connection->second.paths.size() ||
        connection->second.pending.empty())
    {
        return;
    }
    Ptr<Packet> packet = Create<Packet>();
    VeRoceRttHeader rtt;
    rtt.SetContextId((connectionId << 6) | (pathId & 0x3f));
    rtt.SetTimestamp(0, static_cast<uint32_t>(Simulator::Now().GetNanoSeconds()));
    packet->AddHeader(rtt);
    RoceBthHeader bth;
    bth.SetOpcode(RoceOpcode::RTT_REQUEST);
    bth.SetDestinationQp(connectionId);
    packet->AddHeader(bth);
    RoceSimulationTag tag;
    tag.SetSourceEndpointId(m_config.endpointId);
    tag.SetDestinationEndpointId(connection->second.remoteEndpointId);
    tag.SetConnectionId(connectionId);
    ++m_counters.rttProbes;
    SendWirePacket(packet, tag, connectionId, 0, pathId);
    connection->second.paths[pathId].probeEvent =
        Simulator::Schedule(m_rttProbeInterval,
                            &VeRoceTransportAdapter::SendRttProbe,
                            this,
                            connectionId,
                            pathId);
}

void
VeRoceTransportAdapter::SendRttResponse(const RoceSimulationTag& received,
                                        const VeRoceRttHeader& request)
{
    Ptr<Packet> packet = Create<Packet>();
    VeRoceRttHeader response = request;
    const uint32_t now = static_cast<uint32_t>(Simulator::Now().GetNanoSeconds());
    response.SetTimestamp(1, now);
    response.SetTimestamp(2, now);
    response.SetTimestamp(3, 0);
    packet->AddHeader(response);
    RoceBthHeader bth;
    bth.SetOpcode(RoceOpcode::RTT_RESPONSE);
    bth.SetDestinationQp(received.GetConnectionId());
    packet->AddHeader(bth);
    RoceSimulationTag tag;
    tag.SetSourceEndpointId(m_config.endpointId);
    tag.SetDestinationEndpointId(received.GetSourceEndpointId());
    tag.SetConnectionId(received.GetConnectionId());
    SendWirePacket(packet, tag, received.GetConnectionId(), 0, received.GetPathId());
}

void
VeRoceTransportAdapter::ProcessRttResponse(uint32_t connectionId,
                                           uint32_t pathId,
                                           const VeRoceRttHeader& response)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || pathId >= connection->second.paths.size())
    {
        return;
    }
    const uint32_t rx2 = static_cast<uint32_t>(Simulator::Now().GetNanoSeconds());
    const uint32_t total = rx2 - response.GetTimestamp(0);
    const uint32_t host = response.GetTimestamp(2) - response.GetTimestamp(1);
    const uint64_t networkRtt = total >= host ? total - host : total;
    auto& path = connection->second.paths[pathId];
    path.smoothedRttNs =
        path.rttSamples == 0 ? networkRtt : (7 * path.smoothedRttNs + networkRtt) / 8;
    ++path.rttSamples;
    uint64_t minimumRtt = std::numeric_limits<uint64_t>::max();
    bool allMeasured = true;
    for (const auto& candidate : connection->second.paths)
    {
        allMeasured = allMeasured && candidate.rttSamples > 0;
        if (candidate.rttSamples > 0)
        {
            minimumRtt = std::min(minimumRtt, candidate.smoothedRttNs);
        }
    }
    if (allMeasured && path.rttSamples >= 2 && minimumRtt > 0 &&
        static_cast<double>(path.smoothedRttNs) > m_slowRttFactor * minimumRtt)
    {
        path.slowUntil = std::max(path.slowUntil, Simulator::Now() + m_slowPathHoldDown);
    }
}

void
VeRoceTransportAdapter::SendSlowPathSignal(const RoceSimulationTag& received,
                                           uint32_t packetSequence)
{
    Ptr<Packet> packet = Create<Packet>();
    RoceBthHeader bth;
    bth.SetOpcode(RoceOpcode::SLOW_PATH);
    bth.SetDestinationQp(received.GetConnectionId());
    bth.SetPacketSequence(packetSequence);
    packet->AddHeader(bth);
    RoceSimulationTag tag;
    tag.SetSourceEndpointId(m_config.endpointId);
    tag.SetDestinationEndpointId(received.GetSourceEndpointId());
    tag.SetConnectionId(received.GetConnectionId());
    ++m_counters.slowPathSignals;
    SendWirePacket(packet, tag, received.GetConnectionId(), packetSequence, received.GetPathId());
}

void
VeRoceTransportAdapter::ProcessSlowPathSignal(uint32_t connectionId, uint32_t pathId)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || pathId >= connection->second.paths.size())
    {
        return;
    }
    auto& path = connection->second.paths[pathId];
    if (path.signalWindowStart.IsZero() ||
        Simulator::Now() - path.signalWindowStart > m_slowSignalWindow)
    {
        path.signalWindowStart = Simulator::Now();
        path.slowSignals = 0;
    }
    if (++path.slowSignals >= m_slowSignalThreshold)
    {
        path.slowUntil = std::max(path.slowUntil, Simulator::Now() + m_slowPathHoldDown);
        path.slowSignals = 0;
        path.signalWindowStart = Simulator::Now();
    }
}

uint64_t
VeRoceTransportAdapter::ReceiverKey(uint32_t sourceEndpointId, uint32_t connectionId) const
{
    return (static_cast<uint64_t>(sourceEndpointId) << 32) | connectionId;
}

void
VeRoceTransportAdapter::DoDispose()
{
    for (auto& [connectionId, state] : m_connections)
    {
        (void)connectionId;
        for (auto& path : state.paths)
        {
            path.recoveryEvent.Cancel();
            path.probeEvent.Cancel();
        }
        for (auto& [sequence, pending] : state.pending)
        {
            (void)sequence;
            pending.timeout.Cancel();
        }
    }
    for (auto& [responseKey, state] : m_responseConnections)
    {
        (void)responseKey;
        for (auto& [sequence, pending] : state.pending)
        {
            (void)sequence;
            pending.timeout.Cancel();
        }
    }
    m_connections.clear();
    m_receivers.clear();
    m_responseReceivers.clear();
    m_responseConnections.clear();
    m_peers.clear();
    m_lastCnp.clear();
    for (auto& socket : m_pathSockets)
    {
        socket->Close();
    }
    m_pathSockets.clear();
    if (m_receiveSocket)
    {
        m_receiveSocket->Close();
        m_receiveSocket = nullptr;
    }
    m_node = nullptr;
    AiTransportEndpoint::DoDispose();
}

} // namespace ns3
