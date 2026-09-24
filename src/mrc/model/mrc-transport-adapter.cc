/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "mrc-transport-adapter.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <climits>
#include <utility>

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(MrcTransportAdapter);

TypeId
MrcTransportAdapter::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::MrcTransportAdapter")
            .SetParent<AiTransportEndpoint>()
            .SetGroupName("Mrc")
            .AddConstructor<MrcTransportAdapter>()
            .AddAttribute("PathMtu",
                          "Maximum IPv4 datagram size; MRC packets are sent with DF set.",
                          UintegerValue(9000),
                          MakeUintegerAccessor(&MrcTransportAdapter::m_pathMtu),
                          MakeUintegerChecker<uint32_t>(576, 65535))
            .AddAttribute("PathCount",
                          "Number of UDP source-port entropy paths used for MRC spraying.",
                          UintegerValue(4),
                          MakeUintegerAccessor(&MrcTransportAdapter::m_pathCount),
                          MakeUintegerChecker<uint32_t>(1, 64))
            .AddAttribute("ReceiveBitmapLength",
                          "Number of out-of-order PSNs tracked at the responder.",
                          UintegerValue(4096),
                          MakeUintegerAccessor(&MrcTransportAdapter::m_receiveBitmapLength),
                          MakeUintegerChecker<uint32_t>(128, 32640))
            .AddAttribute("TestDropDataSequenceOnce",
                          "Fault-injection PSN dropped once at each responder; zero disables it.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&MrcTransportAdapter::m_testDropDataSequenceOnce),
                          MakeUintegerChecker<uint32_t>(0, 0xffffff));
    return tid;
}

MrcTransportAdapter::MrcTransportAdapter() = default;
MrcTransportAdapter::~MrcTransportAdapter() = default;

AiTransportProtocol
MrcTransportAdapter::GetProtocol() const
{
    return AiTransportProtocol::MRC;
}

AiTransportCapabilities
MrcTransportAdapter::GetCapabilities() const
{
    AiTransportCapabilities capabilities;
    capabilities.reliableUnordered = true;
    capabilities.selectiveAcknowledgment = true;
    capabilities.packetSpraying = true;
    return capabilities;
}

bool
MrcTransportAdapter::Initialize(Ptr<Node> node, const AiTransportEndpointConfig& config)
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
    m_receiveSocket->SetRecvCallback(MakeCallback(&MrcTransportAdapter::Receive, this));
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
MrcTransportAdapter::AddPeer(uint32_t endpointId, const Address& address)
{
    if (!m_receiveSocket || endpointId == 0 || !Ipv4Address::IsMatchingType(address))
    {
        return false;
    }
    m_peers[endpointId] = {Ipv4Address::ConvertFrom(address), UDP_PORT};
    return true;
}

uint32_t
MrcTransportAdapter::OpenConnection(const AiTransportConnectionConfig& config)
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
        path.rateBps = pathRate;
    }
    state.nscc.Initialize(m_config.payloadMtuBytes,
                          state.congestionWindow,
                          m_config.maximumWindowBytes,
                          m_config.baseRtt,
                          m_config.targetQueueDelay);
    m_connections.emplace(connectionId, std::move(state));
    return connectionId;
}

MrcOpcode
MrcTransportAdapter::SelectOpcode(uint32_t fragment, uint32_t fragments) const
{
    if (fragments == 1)
    {
        return MrcOpcode::WRITE_ONLY;
    }
    if (fragment == 0)
    {
        return MrcOpcode::WRITE_FIRST;
    }
    return fragment + 1 == fragments ? MrcOpcode::WRITE_LAST : MrcOpcode::WRITE_MIDDLE;
}

bool
MrcTransportAdapter::Submit(const AiTransportRequest& request)
{
    auto connection = m_connections.find(request.connectionId);
    if (connection == m_connections.end() || !request.payload || request.payload->GetSize() == 0 ||
        request.remoteEndpointId != connection->second.remoteEndpointId ||
        request.reliability != AiTransportReliability::RELIABLE_UNORDERED ||
        (request.operation != AiTransportOperation::MESSAGE &&
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
    if (state.nextSequence + fragments > 0xffffff || state.nextMessageSequence > 0xffff)
    {
        return false;
    }
    const uint16_t msn = static_cast<uint16_t>(state.nextMessageSequence++);
    state.messages.emplace(request.messageId, MessageState{fragments});
    uint32_t offset = 0;
    for (uint32_t fragment = 0; fragment < fragments; ++fragment)
    {
        const uint32_t bytes = std::min(m_config.payloadMtuBytes, totalBytes - offset);
        PendingPacket pending;
        pending.payload = request.payload->CreateFragment(offset, bytes);
        pending.opcode = SelectOpcode(fragment, fragments);
        pending.sequence = state.nextSequence++;
        pending.messageSequence = msn;
        pending.packetOrder = fragment;
        pending.payloadBytes = bytes;
        pending.wireBytes = bytes + RoceBthHeader::SERIALIZED_SIZE +
                            MrcMethHeader::SERIALIZED_SIZE + MrcTimestampHeader::SERIALIZED_SIZE +
                            RoceRethHeader::SERIALIZED_SIZE +
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
    TryTransmit(request.connectionId);
    return true;
}

uint32_t
MrcTransportAdapter::GetCongestionWindow(uint32_t connectionId) const
{
    const auto connection = m_connections.find(connectionId);
    return connection == m_connections.end() ? 0 : connection->second.congestionWindow;
}

bool
MrcTransportAdapter::SetCongestionWindow(uint32_t connectionId, uint32_t bytes)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || bytes == 0)
    {
        return false;
    }
    const uint32_t oldBytes = connection->second.congestionWindow;
    connection->second.nscc.SetCongestionWindow(bytes);
    connection->second.congestionWindow = connection->second.nscc.GetCongestionWindow();
    if (oldBytes != bytes)
    {
        NotifyCongestionWindow(connectionId, oldBytes, bytes);
    }
    TryTransmit(connectionId);
    return true;
}

bool
MrcTransportAdapter::SetConnectionRate(uint32_t connectionId, uint64_t rateBps)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end() || rateBps == 0)
    {
        return false;
    }
    const uint64_t pathRate = std::max<uint64_t>(1, rateBps / connection->second.paths.size());
    for (auto& path : connection->second.paths)
    {
        path.rateBps = pathRate;
    }
    return true;
}

bool
MrcTransportAdapter::ConfigureJobScheduler(uint64_t)
{
    return false;
}

bool
MrcTransportAdapter::AssignConnectionToJob(uint32_t, uint32_t, uint32_t)
{
    return false;
}

AiTransportCounters
MrcTransportAdapter::GetCounters() const
{
    return m_counters;
}

void
MrcTransportAdapter::TryTransmit(uint32_t connectionId)
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
        if (state.inflightBytes != 0 &&
            state.inflightBytes + pending->second.wireBytes > state.congestionWindow)
        {
            break;
        }
        state.transmitQueue.pop_front();
        state.inflightBytes += pending->second.wireBytes;
        pending->second.sent = true;
        pending->second.pathId = state.nextPath++ % state.paths.size();
        auto& path = state.paths[pending->second.pathId];
        const Time sendAt = std::max(Simulator::Now(), path.nextSend);
        Simulator::Schedule(sendAt - Simulator::Now(),
                            &MrcTransportAdapter::TransmitSequence,
                            this,
                            connectionId,
                            sequence,
                            false);
        const uint64_t serializationNs = std::max<uint64_t>(
            1,
            (static_cast<uint64_t>(pending->second.wireBytes) * 8ULL * 1000000000ULL +
             path.rateBps - 1) /
                path.rateBps);
        path.nextSend = sendAt + NanoSeconds(serializationNs);
    }
}

void
MrcTransportAdapter::TransmitSequence(uint32_t connectionId, uint32_t sequence, bool retransmission)
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
        pending->second.pathId = connection->second.nextPath++ % connection->second.paths.size();
    }
    Ptr<Packet> packet = pending->second.payload->Copy();
    RoceRethHeader reth;
    reth.SetVirtualAddress(pending->second.tag.GetMessageId() +
                           pending->second.packetOrder * m_config.payloadMtuBytes);
    reth.SetRemoteKey(connectionId);
    reth.SetDmaLength(pending->second.tag.GetTotalMessageBytes());
    packet->AddHeader(reth);
    MrcTimestampHeader tseth;
    tseth.SetTimestamp(static_cast<uint16_t>((Simulator::Now().GetNanoSeconds() / 128) & 0xffff));
    packet->AddHeader(tseth);
    MrcMethHeader meth;
    meth.SetMessageSequence(pending->second.messageSequence);
    packet->AddHeader(meth);
    RoceBthHeader bth;
    bth.SetOpcode(static_cast<RoceOpcode>(pending->second.opcode));
    bth.SetDestinationQp(connectionId);
    bth.SetPacketSequence(sequence);
    bth.SetAckRequest(true);
    bth.SetRetransmission(retransmission);
    bth.SetTimestampHeader(true);
    packet->AddHeader(bth);
    if (retransmission)
    {
        NotifyRetransmission(connectionId, sequence);
    }
    SendWirePacket(packet, pending->second.tag, connectionId, sequence, pending->second.pathId);
    pending->second.timeout.Cancel();
    const Time timeout = connection->second.retransmissionTimeout *
                         (1ULL << std::min(pending->second.retransmissions, 6u));
    pending->second.timeout = Simulator::Schedule(timeout,
                                                  &MrcTransportAdapter::HandleTimeout,
                                                  this,
                                                  connectionId,
                                                  sequence);
}

bool
MrcTransportAdapter::SendWirePacket(Ptr<Packet> packet,
                                    RoceSimulationTag tag,
                                    uint32_t connectionId,
                                    uint32_t sequence,
                                    uint32_t pathId)
{
    const auto peer = m_peers.find(tag.GetDestinationEndpointId());
    if (!packet || peer == m_peers.end() || m_pathSockets.empty())
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
    if (m_pathSockets[pathId % m_pathSockets.size()]->SendTo(packet, 0, destination) < 0)
    {
        return false;
    }
    ++m_counters.transmittedDatagrams;
    NotifyPacketTx(packet, connectionId, pathId);
    NotifyPathSelected(connectionId, sequence, pathId);
    return true;
}

void
MrcTransportAdapter::Receive(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        RoceSimulationTag tag;
        SocketIpTosTag tos;
        const bool congestionExperienced =
            packet->PeekPacketTag(tos) && (tos.GetTos() & 0x03) == 0x03;
        if (!packet->PeekPacketTag(tag) ||
            packet->GetSize() <
                RoceBthHeader::SERIALIZED_SIZE + RoceInvariantCrcTrailer::SERIALIZED_SIZE)
        {
            ++m_counters.integrityDrops;
            continue;
        }
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
        const auto opcode = static_cast<MrcOpcode>(bth.GetOpcode());
        if (IsMrcWriteOpcode(opcode))
        {
            MrcMethHeader meth;
            MrcTimestampHeader tseth;
            RoceRethHeader reth;
            if (packet->RemoveHeader(meth) == 0 || !bth.HasTimestampHeader() ||
                packet->RemoveHeader(tseth) == 0 || packet->RemoveHeader(reth) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            const uint64_t key = ReceiverKey(tag.GetSourceEndpointId(), tag.GetConnectionId());
            auto& state = m_receivers[key];
            const uint32_t sequence = bth.GetPacketSequence();
            if (!m_testDropConsumed && m_testDropDataSequenceOnce != 0 &&
                sequence == m_testDropDataSequenceOnce)
            {
                m_testDropConsumed = true;
                continue;
            }
            if (sequence > state.cumulativeAck + m_receiveBitmapLength)
            {
                SendReliabilityNack(tag,
                                    sequence,
                                    tseth.GetTimestamp(),
                                    MrcNackReason::PSN_OUT_OF_RANGE);
                continue;
            }
            bool newPacket = false;
            if (sequence > state.cumulativeAck)
            {
                newPacket = state.receivedPsns.insert(sequence).second;
            }
            if (newPacket)
            {
                state.highestPsn = std::max(state.highestPsn, sequence);
                state.receivedBytes += tag.GetPayloadBytes() + 40;
                NotifyPayloadRx(tag.GetSourceEndpointId(), tag.GetPayloadBytes());
                auto& message = state.messages[meth.GetMessageSequence()];
                if (message.messageId == 0)
                {
                    message.messageId = tag.GetMessageId();
                    message.totalBytes = tag.GetTotalMessageBytes();
                    message.submittedTimeNs = tag.GetSubmittedTimeNs();
                }
                const uint64_t baseAddress = tag.GetMessageId();
                const uint32_t packetOrder =
                    reth.GetVirtualAddress() >= baseAddress
                        ? static_cast<uint32_t>((reth.GetVirtualAddress() - baseAddress) /
                                                m_config.payloadMtuBytes)
                        : 0;
                message.packetOrders.insert(packetOrder);
                if (IsMrcLastOpcode(opcode))
                {
                    message.lastSeen = true;
                    message.lastPacketOrder = packetOrder;
                }
                if (!message.completed && message.lastSeen &&
                    message.packetOrders.size() == static_cast<size_t>(message.lastPacketOrder + 1))
                {
                    message.completed = true;
                    NotifyMessageComplete(tag.GetConnectionId(),
                                          message.messageId,
                                          message.totalBytes,
                                          Simulator::Now() - NanoSeconds(message.submittedTimeNs));
                }
            }
            while (state.receivedPsns.erase(state.cumulativeAck + 1) > 0)
            {
                ++state.cumulativeAck;
            }
            SendReliabilitySack(tag, state, sequence, tseth.GetTimestamp(), congestionExperienced);
            SendTransportAck(tag, state.cumulativeAck);
        }
        else if (opcode == MrcOpcode::TRANSPORT_ACK)
        {
            RoceAethHeader aeth;
            if (packet->RemoveHeader(aeth) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            ProcessAck(tag.GetConnectionId(), bth.GetPacketSequence());
        }
        else if (opcode == MrcOpcode::RELIABILITY_SACK)
        {
            MrcSethHeader sack;
            MrcCcStateHeader ccState;
            if (packet->RemoveHeader(sack) == 0 || packet->RemoveHeader(ccState) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            ++m_counters.selectiveAcknowledgments;
            ProcessSack(tag.GetConnectionId(), sack, ccState);
        }
        else if (opcode == MrcOpcode::RELIABILITY_NACK)
        {
            MrcNethHeader nack;
            if (packet->RemoveHeader(nack) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            ProcessNack(tag.GetConnectionId(), nack);
        }
        else if (opcode == MrcOpcode::RELIABILITY_PROBE)
        {
            MrcPethHeader probe;
            if (packet->RemoveHeader(probe) == 0)
            {
                ++m_counters.integrityDrops;
                continue;
            }
            const uint64_t key = ReceiverKey(tag.GetSourceEndpointId(), tag.GetConnectionId());
            const auto state = m_receivers.find(key);
            const ReceiverState empty;
            SendReliabilitySack(tag,
                                state == m_receivers.end() ? empty : state->second,
                                0,
                                probe.GetTimestamp(),
                                congestionExperienced,
                                true,
                                probe.GetProbeId());
        }
    }
}

void
MrcTransportAdapter::SendTransportAck(const RoceSimulationTag& received, uint32_t cumulativeAck)
{
    Ptr<Packet> packet = Create<Packet>();
    RoceAethHeader aeth;
    aeth.SetSyndrome(RoceAethHeader::ACK_SYNDROME);
    packet->AddHeader(aeth);
    RoceBthHeader bth;
    bth.SetOpcode(static_cast<RoceOpcode>(MrcOpcode::TRANSPORT_ACK));
    bth.SetDestinationQp(received.GetConnectionId());
    bth.SetPacketSequence(cumulativeAck);
    packet->AddHeader(bth);
    RoceSimulationTag response;
    response.SetSourceEndpointId(m_config.endpointId);
    response.SetDestinationEndpointId(received.GetSourceEndpointId());
    response.SetConnectionId(received.GetConnectionId());
    response.SetMessageId(received.GetMessageId());
    SendWirePacket(packet,
                   response,
                   received.GetConnectionId(),
                   cumulativeAck,
                   received.GetPathId());
}

void
MrcTransportAdapter::SendReliabilitySack(const RoceSimulationTag& received,
                                         const ReceiverState& state,
                                         uint32_t acknowledgedPsn,
                                         uint16_t timestamp,
                                         bool congestionExperienced,
                                         bool probeResponse,
                                         uint16_t probeId)
{
    Ptr<Packet> packet = Create<Packet>();
    MrcCcStateHeader ccState;
    ccState.SetTimestamp(timestamp);
    ccState.SetOutOfOrderCount(
        static_cast<uint16_t>(std::min<size_t>(state.receivedPsns.size(), 0x7fff)));
    ccState.SetReceivedBytesUnits(
        static_cast<uint32_t>(std::min<uint64_t>((state.receivedBytes + 255) / 256, 0xffffff)));
    packet->AddHeader(ccState);

    MrcSethHeader sack;
    sack.SetCongestionMark(congestionExperienced ? 1 : 0);
    sack.SetProbeResponse(probeResponse);
    const int64_t ackOffset =
        probeResponse ? probeId : static_cast<int64_t>(acknowledgedPsn) - state.cumulativeAck;
    sack.SetAcknowledgedPsnOffset(
        static_cast<int16_t>(std::clamp<int64_t>(ackOffset, INT16_MIN, INT16_MAX)));
    sack.SetEntropy(received.GetPathId());
    sack.SetSourcePdcId(static_cast<uint16_t>(received.GetConnectionId()));
    sack.SetDestinationPdcId(static_cast<uint16_t>(received.GetConnectionId()));
    sack.SetCumulativeAck(state.cumulativeAck);
    sack.SetCcType(0);
    sack.SetMaximumPsnRange(
        static_cast<uint8_t>(std::min<uint32_t>(m_receiveBitmapLength / 128, 0xff)));
    sack.SetSackOffset(1);
    uint64_t bitmap = 0;
    for (uint32_t offset = 0; offset < MrcSethHeader::BITMAP_BITS; ++offset)
    {
        if (state.receivedPsns.contains(state.cumulativeAck + 1 + offset))
        {
            bitmap |= 1ULL << offset;
        }
    }
    sack.SetBitmap(bitmap);
    packet->AddHeader(sack);

    RoceBthHeader bth;
    bth.SetOpcode(static_cast<RoceOpcode>(MrcOpcode::RELIABILITY_SACK));
    bth.SetDestinationQp(received.GetConnectionId());
    bth.SetPacketSequence(state.cumulativeAck);
    packet->AddHeader(bth);
    RoceSimulationTag response;
    response.SetSourceEndpointId(m_config.endpointId);
    response.SetDestinationEndpointId(received.GetSourceEndpointId());
    response.SetConnectionId(received.GetConnectionId());
    response.SetMessageId(received.GetMessageId());
    SendWirePacket(packet,
                   response,
                   received.GetConnectionId(),
                   state.cumulativeAck,
                   received.GetPathId());
}

void
MrcTransportAdapter::SendReliabilityNack(const RoceSimulationTag& received,
                                         uint32_t packetSequence,
                                         uint16_t timestamp,
                                         MrcNackReason reason)
{
    Ptr<Packet> packet = Create<Packet>();
    MrcNethHeader nack;
    nack.SetReason(reason);
    nack.SetEntropy(received.GetPathId());
    nack.SetSourcePdcId(static_cast<uint16_t>(received.GetConnectionId()));
    nack.SetDestinationPdcId(static_cast<uint16_t>(received.GetConnectionId()));
    nack.SetNackPsn(packetSequence);
    nack.SetTimestamp(timestamp);
    packet->AddHeader(nack);
    RoceBthHeader bth;
    bth.SetOpcode(static_cast<RoceOpcode>(MrcOpcode::RELIABILITY_NACK));
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
MrcTransportAdapter::ProcessAck(uint32_t connectionId, uint32_t cumulativeAck)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto& state = connection->second;
    for (auto pending = state.pending.begin();
         pending != state.pending.end() && pending->first <= cumulativeAck;)
    {
        if (!pending->second.sent)
        {
            ++pending;
            continue;
        }
        if (!pending->second.reliabilityAcknowledged)
        {
            pending->second.timeout.Cancel();
            state.inflightBytes = state.inflightBytes > pending->second.wireBytes
                                      ? state.inflightBytes - pending->second.wireBytes
                                      : 0;
        }
        const uint64_t messageId = pending->second.tag.GetMessageId();
        auto message = state.messages.find(messageId);
        if (message != state.messages.end() && message->second.remainingPackets > 0 &&
            --message->second.remainingPackets == 0)
        {
            state.messages.erase(message);
        }
        pending = state.pending.erase(pending);
    }
    TryTransmit(connectionId);
}

void
MrcTransportAdapter::MarkReliabilityAcknowledged(ConnectionState& state, uint32_t sequence)
{
    auto pending = state.pending.find(sequence);
    if (pending == state.pending.end() || !pending->second.sent ||
        pending->second.reliabilityAcknowledged)
    {
        return;
    }
    pending->second.reliabilityAcknowledged = true;
    pending->second.timeout.Cancel();
    state.inflightBytes = state.inflightBytes > pending->second.wireBytes
                              ? state.inflightBytes - pending->second.wireBytes
                              : 0;
}

void
MrcTransportAdapter::ProcessSack(uint32_t connectionId,
                                 const MrcSethHeader& sack,
                                 const MrcCcStateHeader& ccState)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto& state = connection->second;
    const uint32_t receivedUnits = ccState.GetReceivedBytesUnits();
    const uint32_t deltaUnits = (receivedUnits - state.previousReceivedByteUnits) & 0xffffff;
    const bool forwardProgress = deltaUnits != 0 && deltaUnits < 0x800000;
    const uint32_t newlyReceivedBytes = forwardProgress ? deltaUnits << 8 : 0;
    if (forwardProgress)
    {
        state.previousReceivedByteUnits = receivedUnits;
    }
    const uint16_t nowTimestamp =
        static_cast<uint16_t>((Simulator::Now().GetNanoSeconds() / 128) & 0xffff);
    const uint16_t elapsedTicks = nowTimestamp - ccState.GetTimestamp();
    const Time measuredRtt = NanoSeconds(std::max<uint64_t>(128, elapsedTicks * 128ULL));
    const bool ecnMarked = sack.GetCongestionMark() != 0;
    const uint32_t oldWindow = state.congestionWindow;
    state.congestionWindow = state.nscc.OnAck(newlyReceivedBytes, ecnMarked, measuredRtt);
    if (ecnMarked)
    {
        NotifyEcnReceived(connectionId, sack.GetEntropy() % state.paths.size());
    }
    if (oldWindow != state.congestionWindow)
    {
        NotifyCongestionWindow(connectionId, oldWindow, state.congestionWindow);
    }
    const uint32_t cumulativeAck = sack.GetCumulativeAck();
    bool inferredLoss = false;
    for (auto& [sequence, pending] : state.pending)
    {
        if (sequence <= cumulativeAck && pending.sent)
        {
            MarkReliabilityAcknowledged(state, sequence);
        }
    }

    const int64_t bitmapBaseSigned = static_cast<int64_t>(cumulativeAck) + sack.GetSackOffset();
    if (bitmapBaseSigned < 0 || bitmapBaseSigned > 0xffffff)
    {
        return;
    }
    const uint32_t bitmapBase = static_cast<uint32_t>(bitmapBaseSigned);
    uint32_t highestReceived = cumulativeAck;
    for (uint32_t offset = 0; offset < MrcSethHeader::BITMAP_BITS; ++offset)
    {
        if (!sack.IsReceived(offset))
        {
            continue;
        }
        const uint32_t sequence = bitmapBase + offset;
        MarkReliabilityAcknowledged(state, sequence);
        highestReceived = std::max(highestReceived, sequence);
    }
    if (!sack.IsProbeResponse())
    {
        const int64_t acknowledged =
            static_cast<int64_t>(cumulativeAck) + sack.GetAcknowledgedPsnOffset();
        if (acknowledged >= 0 && acknowledged <= 0xffffff)
        {
            MarkReliabilityAcknowledged(state, static_cast<uint32_t>(acknowledged));
            highestReceived = std::max(highestReceived, static_cast<uint32_t>(acknowledged));
        }
    }

    const uint32_t reorderTolerancePackets =
        std::max<uint32_t>(3, m_config.maximumWindowBytes / m_config.payloadMtuBytes);
    for (auto& [sequence, pending] : state.pending)
    {
        if (sequence <= cumulativeAck || sequence > highestReceived || !pending.sent ||
            pending.reliabilityAcknowledged || pending.fastRetransmitted ||
            pending.retransmissions >= m_config.maxRetransmissions ||
            highestReceived - sequence < reorderTolerancePackets)
        {
            continue;
        }
        pending.fastRetransmitted = true;
        ++pending.retransmissions;
        ++m_counters.fastRetransmissions;
        inferredLoss = true;
        TransmitSequence(connectionId, sequence, true);
    }
    if (inferredLoss)
    {
        const uint32_t beforeLoss = state.congestionWindow;
        state.congestionWindow = state.nscc.OnLoss();
        if (beforeLoss != state.congestionWindow)
        {
            NotifyCongestionWindow(connectionId, beforeLoss, state.congestionWindow);
        }
    }
    TryTransmit(connectionId);
}

void
MrcTransportAdapter::ProcessNack(uint32_t connectionId, const MrcNethHeader& nack)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    auto pending = connection->second.pending.find(nack.GetNackPsn());
    if (pending == connection->second.pending.end() || !pending->second.sent ||
        pending->second.reliabilityAcknowledged ||
        pending->second.retransmissions >= m_config.maxRetransmissions)
    {
        return;
    }
    NotifyNack(connectionId, nack.GetNackPsn());
    if (nack.GetReason() == MrcNackReason::UNEXPECTED_EVENT)
    {
        return;
    }
    ++pending->second.retransmissions;
    ++m_counters.fastRetransmissions;
    const uint32_t oldWindow = connection->second.congestionWindow;
    connection->second.congestionWindow = connection->second.nscc.OnLoss();
    if (oldWindow != connection->second.congestionWindow)
    {
        NotifyCongestionWindow(connectionId, oldWindow, connection->second.congestionWindow);
    }
    TransmitSequence(connectionId, nack.GetNackPsn(), true);
}

void
MrcTransportAdapter::SendReliabilityProbe(uint32_t connectionId, uint32_t pathId)
{
    auto connection = m_connections.find(connectionId);
    if (connection == m_connections.end())
    {
        return;
    }
    const uint16_t probeId = static_cast<uint16_t>(connection->second.nextSequence);
    Ptr<Packet> packet = Create<Packet>();
    MrcPethHeader probe;
    probe.SetProbeId(probeId);
    probe.SetSourcePdcId(static_cast<uint16_t>(connectionId));
    probe.SetDestinationPdcId(static_cast<uint16_t>(connectionId));
    probe.SetTimestamp(static_cast<uint16_t>((Simulator::Now().GetNanoSeconds() / 128) & 0xffff));
    packet->AddHeader(probe);
    RoceBthHeader bth;
    bth.SetOpcode(static_cast<RoceOpcode>(MrcOpcode::RELIABILITY_PROBE));
    bth.SetDestinationQp(connectionId);
    bth.SetPacketSequence(probeId);
    packet->AddHeader(bth);
    RoceSimulationTag tag;
    tag.SetSourceEndpointId(m_config.endpointId);
    tag.SetDestinationEndpointId(connection->second.remoteEndpointId);
    tag.SetConnectionId(connectionId);
    SendWirePacket(packet, tag, connectionId, probeId, pathId);
    ++m_counters.rttProbes;
}

void
MrcTransportAdapter::HandleTimeout(uint32_t connectionId, uint32_t sequence)
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
    SendReliabilityProbe(connectionId, pending->second.pathId);
    ++pending->second.retransmissions;
    pending->second.fastRetransmitted = false;
    const uint32_t oldWindow = connection->second.congestionWindow;
    connection->second.congestionWindow = connection->second.nscc.OnLoss();
    if (oldWindow != connection->second.congestionWindow)
    {
        NotifyCongestionWindow(connectionId, oldWindow, connection->second.congestionWindow);
    }
    TransmitSequence(connectionId, sequence, true);
}

uint64_t
MrcTransportAdapter::ReceiverKey(uint32_t sourceEndpointId, uint32_t connectionId) const
{
    return (static_cast<uint64_t>(sourceEndpointId) << 32) | connectionId;
}

void
MrcTransportAdapter::DoDispose()
{
    for (auto& [connectionId, state] : m_connections)
    {
        (void)connectionId;
        for (auto& [sequence, pending] : state.pending)
        {
            (void)sequence;
            pending.timeout.Cancel();
        }
    }
    m_connections.clear();
    m_receivers.clear();
    m_peers.clear();
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
