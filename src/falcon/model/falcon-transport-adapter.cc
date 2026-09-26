/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "falcon-transport-adapter.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(FalconTransportAdapter);

TypeId
FalconTransportAdapter::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::FalconTransportAdapter")
            .SetParent<AiTransportEndpoint>()
            .SetGroupName("Falcon")
            .AddConstructor<FalconTransportAdapter>()
            .AddAttribute("PathMtu",
                          "Maximum IPv4 datagram size for the Falcon comparison model.",
                          UintegerValue(9000),
                          MakeUintegerAccessor(&FalconTransportAdapter::m_pathMtu),
                          MakeUintegerChecker<uint32_t>(576, 65535));
    return tid;
}

FalconTransportAdapter::FalconTransportAdapter() = default;
FalconTransportAdapter::~FalconTransportAdapter() = default;

AiTransportProtocol
FalconTransportAdapter::GetProtocol() const
{
    return AiTransportProtocol::FALCON;
}

AiTransportCapabilities
FalconTransportAdapter::GetCapabilities() const
{
    AiTransportCapabilities capabilities;
    capabilities.reliableUnordered = true;
    capabilities.selectiveAcknowledgment = true;
    return capabilities;
}

bool
FalconTransportAdapter::Initialize(Ptr<Node> node, const AiTransportEndpointConfig& config)
{
    if (!node || m_socket || config.endpointId == 0 || config.payloadMtuBytes == 0 ||
        config.payloadMtuBytes > std::numeric_limits<uint16_t>::max() || config.lineRateBps == 0)
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
    m_socket->SetRecvCallback(MakeCallback(&FalconTransportAdapter::Receive, this));
    return true;
}

bool
FalconTransportAdapter::AddPeer(uint32_t endpointId, const Address& address)
{
    if (!m_socket || endpointId == 0 || !Ipv4Address::IsMatchingType(address))
    {
        return false;
    }
    m_peers[endpointId] = {Ipv4Address::ConvertFrom(address), UDP_PORT};
    return true;
}

uint32_t
FalconTransportAdapter::OpenConnection(const AiTransportConnectionConfig& config)
{
    if (!m_socket || config.remoteEndpointId == 0 || !m_peers.contains(config.remoteEndpointId) ||
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
    state.rateBps = config.lineRateBps == 0 ? m_config.lineRateBps : config.lineRateBps;
    state.retransmissionTimeout = config.retransmissionTimeout;
    FalconSwiftConfig swiftConfig;
    swiftConfig.baseDelayTarget = m_config.targetQueueDelay;
    swiftConfig.maxFcwnd =
        std::max(1.0,
                 static_cast<double>(m_config.maximumWindowBytes) /
                     (m_config.payloadMtuBytes + FalconBaseHeader::SERIALIZED_SIZE +
                      FalconPushDataHeader::SERIALIZED_SIZE));
    swiftConfig.maxNcwnd = swiftConfig.maxFcwnd;
    swiftConfig.minRetransmissionTimeout = std::min(config.retransmissionTimeout, m_config.baseRtt);
    state.swift = FalconSwift(swiftConfig);
    const double initialPackets =
        std::max(1.0,
                 static_cast<double>(state.congestionWindow) /
                     (m_config.payloadMtuBytes + FalconBaseHeader::SERIALIZED_SIZE +
                      FalconPushDataHeader::SERIALIZED_SIZE));
    state.swift.Initialize(initialPackets, initialPackets, m_config.baseRtt);
    m_connections.emplace(connectionId, std::move(state));
    return connectionId;
}

bool
FalconTransportAdapter::Submit(const AiTransportRequest& request)
{
    auto found = m_connections.find(request.connectionId);
    if (found == m_connections.end() || !request.payload || request.payload->GetSize() == 0 ||
        request.remoteEndpointId != found->second.remoteEndpointId ||
        request.reliability != AiTransportReliability::RELIABLE_UNORDERED ||
        (request.operation != AiTransportOperation::MESSAGE &&
         request.operation != AiTransportOperation::SEND &&
         request.operation != AiTransportOperation::WRITE))
    {
        return false;
    }
    auto& state = found->second;
    const uint32_t totalBytes = request.payload->GetSize();
    const uint32_t fragments =
        (totalBytes + m_config.payloadMtuBytes - 1) / m_config.payloadMtuBytes;
    uint32_t offset = 0;
    for (uint32_t fragment = 0; fragment < fragments; ++fragment)
    {
        const uint32_t bytes = std::min(m_config.payloadMtuBytes, totalBytes - offset);
        const uint32_t psn = state.reliability.AllocatePsn(FalconReliabilityWindow::DATA);
        PendingPacket pending;
        pending.payload = request.payload->CreateFragment(offset, bytes);
        pending.psn = psn;
        pending.wireBytes =
            bytes + FalconBaseHeader::SERIALIZED_SIZE + FalconPushDataHeader::SERIALIZED_SIZE;
        pending.tag.SetSourceEndpointId(m_config.endpointId);
        pending.tag.SetDestinationEndpointId(request.remoteEndpointId);
        pending.tag.SetConnectionId(request.connectionId);
        pending.tag.SetMessageId(request.messageId);
        pending.tag.SetPayloadBytes(bytes);
        pending.tag.SetTotalMessageBytes(totalBytes);
        pending.tag.SetSubmittedTimeNs(Simulator::Now().GetNanoSeconds());
        pending.tag.SetFragment(fragment);
        pending.tag.SetFragmentCount(fragments);
        state.reliability.TrackTransmitted(FalconReliabilityWindow::DATA, psn);
        state.pending.emplace(psn, std::move(pending));
        state.transmitQueue.push_back(psn);
        offset += bytes;
    }
    TryTransmit(request.connectionId);
    return true;
}

uint32_t
FalconTransportAdapter::GetCongestionWindow(uint32_t connectionId) const
{
    const auto found = m_connections.find(connectionId);
    return found == m_connections.end() ? 0 : GetSwiftWindowBytes(found->second);
}

bool
FalconTransportAdapter::SetCongestionWindow(uint32_t connectionId, uint32_t bytes)
{
    auto found = m_connections.find(connectionId);
    if (found == m_connections.end() || bytes == 0)
    {
        return false;
    }
    const uint32_t old = GetSwiftWindowBytes(found->second);
    found->second.congestionWindow = bytes;
    const uint32_t current = GetSwiftWindowBytes(found->second);
    if (old != current)
    {
        NotifyCongestionWindow(connectionId, old, current);
    }
    TryTransmit(connectionId);
    return true;
}

bool
FalconTransportAdapter::SetConnectionRate(uint32_t connectionId, uint64_t rateBps)
{
    auto found = m_connections.find(connectionId);
    if (found == m_connections.end() || rateBps == 0)
    {
        return false;
    }
    found->second.rateBps = rateBps;
    return true;
}

bool
FalconTransportAdapter::ConfigureJobScheduler(uint64_t)
{
    return false;
}

bool
FalconTransportAdapter::AssignConnectionToJob(uint32_t, uint32_t, uint32_t)
{
    return false;
}

AiTransportCounters
FalconTransportAdapter::GetCounters() const
{
    return m_counters;
}

void
FalconTransportAdapter::TryTransmit(uint32_t connectionId)
{
    auto found = m_connections.find(connectionId);
    if (found == m_connections.end())
    {
        return;
    }
    auto& state = found->second;
    const uint32_t swiftPackets =
        std::max(1u, static_cast<uint32_t>(state.swift.GetEffectiveWindow()));
    while (!state.transmitQueue.empty())
    {
        const uint32_t psn = state.transmitQueue.front();
        auto pending = state.pending.find(psn);
        if (pending == state.pending.end())
        {
            state.transmitQueue.pop_front();
            continue;
        }
        if (state.inflightBytes + pending->second.wireBytes > state.congestionWindow ||
            state.inflightPackets >= swiftPackets)
        {
            break;
        }
        state.transmitQueue.pop_front();
        state.inflightBytes += pending->second.wireBytes;
        ++state.inflightPackets;
        pending->second.sent = true;
        const Time sendAt = std::max(Simulator::Now(), state.nextSend);
        Simulator::Schedule(sendAt - Simulator::Now(),
                            &FalconTransportAdapter::Transmit,
                            this,
                            connectionId,
                            psn,
                            false);
        const uint64_t rate = std::max<uint64_t>(1, state.rateBps);
        uint64_t serializationNs = std::max<uint64_t>(
            1,
            (static_cast<uint64_t>(pending->second.wireBytes) * 8000000000ULL + rate - 1) / rate);
        serializationNs = std::max<uint64_t>(
            serializationNs,
            std::max<int64_t>(0, state.swift.GetInterPacketGap().GetNanoSeconds()));
        state.nextSend = sendAt + NanoSeconds(serializationNs);
    }
}

void
FalconTransportAdapter::Transmit(uint32_t connectionId, uint32_t psn, bool retransmission)
{
    auto found = m_connections.find(connectionId);
    if (found == m_connections.end())
    {
        return;
    }
    auto pending = found->second.pending.find(psn);
    if (pending == found->second.pending.end())
    {
        return;
    }
    Ptr<Packet> packet = pending->second.payload->Copy();
    FalconPushDataHeader suffix;
    suffix.SetRequestLength(static_cast<uint16_t>(pending->second.tag.GetPayloadBytes()));
    packet->AddHeader(suffix);
    FalconBaseHeader base;
    base.SetDestinationConnectionId(connectionId);
    base.SetDestinationFunction(pending->second.tag.GetDestinationEndpointId());
    base.SetPacketType(FalconPacketType::PUSH_DATA);
    base.SetAckRequest(true);
    base.SetPacketSequenceNumber(psn);
    base.SetRequestSequenceNumber(pending->second.tag.GetMessageId() & 0xffffffff);
    packet->AddHeader(base);
    if (retransmission)
    {
        NotifyRetransmission(connectionId, psn);
    }
    FalconSimulationTag outgoing = pending->second.tag;
    outgoing.SetPacketTxTimeNs(Simulator::Now().GetNanoSeconds());
    SendWire(packet, outgoing, connectionId);
    pending->second.timeout.Cancel();
    const Time baseTimeout = std::max(found->second.retransmissionTimeout,
                                      found->second.swift.GetRetransmissionTimeout());
    const Time timeout = baseTimeout * (1ULL << std::min(pending->second.retransmissions, 6u));
    pending->second.timeout = Simulator::Schedule(timeout,
                                                  &FalconTransportAdapter::HandleTimeout,
                                                  this,
                                                  connectionId,
                                                  psn);
}

bool
FalconTransportAdapter::SendWire(Ptr<Packet> packet,
                                 const FalconSimulationTag& tag,
                                 uint32_t connectionId)
{
    const auto peer = m_peers.find(tag.GetDestinationEndpointId());
    if (!packet || !m_socket || peer == m_peers.end())
    {
        return false;
    }
    if (packet->GetSize() + 28 > m_pathMtu)
    {
        ++m_counters.mtuDrops;
        return false;
    }
    packet->AddPacketTag(tag);
    SocketSetDontFragmentTag dontFragment;
    dontFragment.Enable();
    packet->AddPacketTag(dontFragment);
    SocketIpTosTag tos;
    tos.SetTos(0x02);
    packet->AddPacketTag(tos);
    const Address destination =
        InetSocketAddress(Ipv4Address::ConvertFrom(peer->second.address), peer->second.port);
    if (m_socket->SendTo(packet, 0, destination) < 0)
    {
        return false;
    }
    ++m_counters.transmittedDatagrams;
    NotifyPacketTx(packet, connectionId, 0);
    NotifyPathSelected(connectionId, 0, 0);
    return true;
}

void
FalconTransportAdapter::Receive(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        FalconSimulationTag tag;
        uint8_t prefix[8]{};
        if (!packet->PeekPacketTag(tag) || packet->GetSize() < 8 ||
            packet->CopyData(prefix, sizeof(prefix)) != sizeof(prefix))
        {
            ++m_counters.integrityDrops;
            continue;
        }
        const FalconPacketType type = static_cast<FalconPacketType>((prefix[7] >> 1) & 0xf);
        ++m_counters.receivedDatagrams;
        NotifyPacketRx(packet, tag.GetConnectionId(), 0);
        if (type == FalconPacketType::PUSH_DATA)
        {
            ReceiveData(packet, tag);
        }
        else
        {
            ReceiveControl(packet, tag, type);
        }
    }
}

uint64_t
FalconTransportAdapter::ReceiverKey(uint32_t endpointId, uint32_t connectionId) const
{
    return (static_cast<uint64_t>(endpointId) << 32) | connectionId;
}

void
FalconTransportAdapter::ReceiveData(Ptr<Packet> packet, const FalconSimulationTag& tag)
{
    if (packet->GetSize() <
        FalconBaseHeader::SERIALIZED_SIZE + FalconPushDataHeader::SERIALIZED_SIZE)
    {
        ++m_counters.integrityDrops;
        return;
    }
    FalconBaseHeader base;
    FalconPushDataHeader suffix;
    packet->RemoveHeader(base);
    packet->RemoveHeader(suffix);
    if (!base.HasValidReservedField() || !suffix.HasValidReservedField() ||
        suffix.GetRequestLength() != tag.GetPayloadBytes() ||
        packet->GetSize() != tag.GetPayloadBytes())
    {
        ++m_counters.integrityDrops;
        return;
    }
    const uint64_t key = ReceiverKey(tag.GetSourceEndpointId(), tag.GetConnectionId());
    auto [receiver, inserted] = m_receivers.try_emplace(key, 0, 0);
    (void)inserted;
    const uint32_t psn = base.GetPacketSequenceNumber();
    const FalconReceiveResult result = receiver->second.ReceiveData(psn);
    if (result == FalconReceiveResult::OUT_OF_WINDOW)
    {
        SendNack(tag, psn);
        return;
    }
    if (result == FalconReceiveResult::ACCEPTED)
    {
        receiver->second.AcknowledgeData(psn);
        NotifyPayloadRx(tag.GetSourceEndpointId(), tag.GetPayloadBytes());
        auto& message = m_receiveMessages[key][tag.GetMessageId()];
        message.totalBytes = tag.GetTotalMessageBytes();
        message.fragmentCount = tag.GetFragmentCount();
        message.submittedTimeNs = tag.GetSubmittedTimeNs();
        message.fragments.insert(tag.GetFragment());
        if (message.fragmentCount != 0 && message.fragments.size() == message.fragmentCount)
        {
            NotifyMessageComplete(tag.GetConnectionId(),
                                  tag.GetMessageId(),
                                  message.totalBytes,
                                  Simulator::Now() - NanoSeconds(message.submittedTimeNs));
            m_receiveMessages[key].erase(tag.GetMessageId());
        }
    }
    SendEack(tag, receiver->second);
}

void
FalconTransportAdapter::SendEack(const FalconSimulationTag& received,
                                 FalconReliabilityManager& reliability)
{
    const uint64_t receiveTimeNs = Simulator::Now().GetNanoSeconds();
    constexpr uint64_t TIMESTAMP_UNIT_PS = 131072;
    const uint32_t timestamp1 = static_cast<uint32_t>(
        (received.GetPacketTxTimeNs() * 1000ULL / TIMESTAMP_UNIT_PS) & 0xffffffff);
    const uint32_t timestamp2 =
        static_cast<uint32_t>((receiveTimeNs * 1000ULL / TIMESTAMP_UNIT_PS) & 0xffffffff);
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(reliability.BuildEack(received.GetConnectionId(), timestamp1, timestamp2));
    FalconSimulationTag response;
    response.SetSourceEndpointId(m_config.endpointId);
    response.SetDestinationEndpointId(received.GetSourceEndpointId());
    response.SetConnectionId(received.GetConnectionId());
    response.SetMessageId(received.GetMessageId());
    response.SetPacketTxTimeNs(received.GetPacketTxTimeNs());
    response.SetPacketRxTimeNs(receiveTimeNs);
    response.SetAckTxTimeNs(Simulator::Now().GetNanoSeconds());
    SendWire(packet, response, received.GetConnectionId());
}

void
FalconTransportAdapter::SendNack(const FalconSimulationTag& received, uint32_t psn)
{
    Ptr<Packet> packet = Create<Packet>();
    FalconNackHeader nack;
    nack.SetConnectionId(received.GetConnectionId());
    nack.SetNackPacketSequenceNumber(psn);
    nack.SetNackCode(FalconNackCode::RESOURCE_EXHAUSTION);
    packet->AddHeader(nack);
    FalconSimulationTag response;
    response.SetSourceEndpointId(m_config.endpointId);
    response.SetDestinationEndpointId(received.GetSourceEndpointId());
    response.SetConnectionId(received.GetConnectionId());
    response.SetMessageId(received.GetMessageId());
    SendWire(packet, response, received.GetConnectionId());
}

void
FalconTransportAdapter::ReceiveControl(Ptr<Packet> packet,
                                       const FalconSimulationTag& tag,
                                       FalconPacketType type)
{
    auto found = m_connections.find(tag.GetConnectionId());
    if (found == m_connections.end())
    {
        ++m_counters.integrityDrops;
        return;
    }
    if (type == FalconPacketType::EACK && packet->GetSize() >= FalconEackHeader::SERIALIZED_SIZE)
    {
        FalconEackHeader eack;
        packet->RemoveHeader(eack);
        ++m_counters.selectiveAcknowledgments;
        const std::vector<uint32_t> acknowledged = found->second.reliability.ProcessEack(eack);
        const uint64_t nowNs = Simulator::Now().GetNanoSeconds();
        if (tag.GetPacketTxTimeNs() <= nowNs && tag.GetPacketRxTimeNs() <= tag.GetAckTxTimeNs())
        {
            const Time rtt = NanoSeconds(nowNs - tag.GetPacketTxTimeNs());
            const Time remoteResidence =
                NanoSeconds(tag.GetAckTxTimeNs() - tag.GetPacketRxTimeNs());
            const Time fabricDelay = rtt > remoteResidence ? rtt - remoteResidence : NanoSeconds(0);
            const uint32_t oldWindow = GetSwiftWindowBytes(found->second);
            found->second.swift.ProcessAck(Simulator::Now(),
                                           rtt,
                                           fabricDelay,
                                           acknowledged.size(),
                                           0);
            NotifySwiftWindowChange(tag.GetConnectionId(), oldWindow);
        }
        RetireAcknowledged(tag.GetConnectionId(), acknowledged);
    }
    else if (type == FalconPacketType::BACK &&
             packet->GetSize() >= FalconBackHeader::SERIALIZED_SIZE)
    {
        FalconBackHeader back;
        packet->RemoveHeader(back);
        RetireAcknowledged(tag.GetConnectionId(), found->second.reliability.ProcessBack(back));
    }
    else if (type == FalconPacketType::NACK &&
             packet->GetSize() >= FalconNackHeader::SERIALIZED_SIZE)
    {
        FalconNackHeader nack;
        packet->RemoveHeader(nack);
        uint32_t psn = 0;
        if (found->second.reliability.ProcessNack(nack, psn))
        {
            NotifyNack(tag.GetConnectionId(), psn);
            auto pending = found->second.pending.find(psn);
            if (pending != found->second.pending.end() &&
                pending->second.retransmissions < m_config.maxRetransmissions)
            {
                const uint32_t oldWindow = GetSwiftWindowBytes(found->second);
                if (nack.GetNackCode() == FalconNackCode::RESOURCE_EXHAUSTION)
                {
                    found->second.swift.ProcessResourceExhaustionNack(Simulator::Now(),
                                                                      m_config.baseRtt,
                                                                      m_config.baseRtt,
                                                                      0,
                                                                      31);
                }
                else
                {
                    found->second.swift.ProcessRetransmit(Simulator::Now());
                }
                NotifySwiftWindowChange(tag.GetConnectionId(), oldWindow);
                ++pending->second.retransmissions;
                ++m_counters.fastRetransmissions;
                Transmit(tag.GetConnectionId(), psn, true);
            }
        }
    }
    else
    {
        ++m_counters.integrityDrops;
    }
}

void
FalconTransportAdapter::RetireAcknowledged(uint32_t connectionId,
                                           const std::vector<uint32_t>& acknowledged)
{
    auto& state = m_connections.at(connectionId);
    for (uint32_t psn : acknowledged)
    {
        auto pending = state.pending.find(psn);
        if (pending == state.pending.end())
        {
            continue;
        }
        pending->second.timeout.Cancel();
        state.inflightBytes = state.inflightBytes > pending->second.wireBytes
                                  ? state.inflightBytes - pending->second.wireBytes
                                  : 0;
        if (state.inflightPackets > 0)
        {
            --state.inflightPackets;
        }
        state.pending.erase(pending);
    }
    TryTransmit(connectionId);
}

void
FalconTransportAdapter::HandleTimeout(uint32_t connectionId, uint32_t psn)
{
    auto found = m_connections.find(connectionId);
    if (found == m_connections.end())
    {
        return;
    }
    auto pending = found->second.pending.find(psn);
    if (pending == found->second.pending.end() ||
        pending->second.retransmissions >= m_config.maxRetransmissions)
    {
        return;
    }
    NotifyTimeout(connectionId, psn);
    const uint32_t oldWindow = GetSwiftWindowBytes(found->second);
    found->second.swift.ProcessRetransmit(Simulator::Now());
    NotifySwiftWindowChange(connectionId, oldWindow);
    ++pending->second.retransmissions;
    Transmit(connectionId, psn, true);
}

uint32_t
FalconTransportAdapter::GetSwiftWindowBytes(const ConnectionState& state) const
{
    const double packets = std::max(1.0, state.swift.GetEffectiveWindow());
    const uint64_t bytes = static_cast<uint64_t>(packets) *
                           (m_config.payloadMtuBytes + FalconBaseHeader::SERIALIZED_SIZE +
                            FalconPushDataHeader::SERIALIZED_SIZE);
    return static_cast<uint32_t>(std::min<uint64_t>(state.congestionWindow, bytes));
}

void
FalconTransportAdapter::NotifySwiftWindowChange(uint32_t connectionId, uint32_t oldWindowBytes)
{
    const auto found = m_connections.find(connectionId);
    if (found == m_connections.end())
    {
        return;
    }
    const uint32_t newWindowBytes = GetSwiftWindowBytes(found->second);
    if (oldWindowBytes != newWindowBytes)
    {
        NotifyCongestionWindow(connectionId, oldWindowBytes, newWindowBytes);
    }
}

void
FalconTransportAdapter::DoDispose()
{
    for (auto& [unusedConnection, state] : m_connections)
    {
        (void)unusedConnection;
        for (auto& [unusedPsn, pending] : state.pending)
        {
            (void)unusedPsn;
            pending.timeout.Cancel();
        }
    }
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    m_node = nullptr;
    AiTransportEndpoint::DoDispose();
}

} // namespace ns3
