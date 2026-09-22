/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-udp-transport.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/uet-endpoint.h"
#include "ns3/uet-crc-trailer.h"
#include "ns3/uet-simulation-tag.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(UetUdpTransport);

TypeId
UetUdpTransport::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::UetUdpTransport")
            .SetParent<Object>()
            .SetGroupName("Uet")
            .AddConstructor<UetUdpTransport>()
            .AddAttribute("PathMtu",
                          "Maximum IPv4 datagram size; UET packets are sent with DF set.",
                          UintegerValue(9000),
                          MakeUintegerAccessor(&UetUdpTransport::m_pathMtu),
                          MakeUintegerChecker<uint32_t>(576, 65535));
    return tid;
}

UetUdpTransport::UetUdpTransport() = default;
UetUdpTransport::~UetUdpTransport() = default;

bool
UetUdpTransport::Bind(Ptr<Node> node, Ptr<UetEndpoint> endpoint, uint16_t port)
{
    if (!node || !endpoint || m_socket)
    {
        return false;
    }
    m_node = node;
    m_endpoint = endpoint;
    m_socket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
    if (!m_socket || m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port)) != 0)
    {
        m_socket = nullptr;
        return false;
    }
    m_socket->SetRecvCallback(MakeCallback(&UetUdpTransport::Receive, this));
    m_socket->SetIpRecvTos(true);
    m_endpoint->SetTransmitCallback(MakeCallback(&UetUdpTransport::Transmit, this));
    return true;
}

bool
UetUdpTransport::Bind6(Ptr<Node> node, Ptr<UetEndpoint> endpoint, uint16_t port)
{
    if (!node || !endpoint || m_socket)
    {
        return false;
    }
    m_node = node;
    m_endpoint = endpoint;
    m_socket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
    if (!m_socket || m_socket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), port)) != 0)
    {
        m_socket = nullptr;
        return false;
    }
    m_socket->SetRecvCallback(MakeCallback(&UetUdpTransport::Receive, this));
    m_socket->SetIpv6RecvTclass(true);
    m_endpoint->SetTransmitCallback(MakeCallback(&UetUdpTransport::Transmit, this));
    return true;
}

void
UetUdpTransport::AddPeer(uint32_t endpointId, Ipv4Address address, uint16_t port)
{
    m_peers[endpointId] = {address, port, false};
}

void
UetUdpTransport::AddPeer(uint32_t endpointId, Ipv6Address address, uint16_t port)
{
    m_peers[endpointId] = {address, port, true};
}

void
UetUdpTransport::Transmit(Ptr<const Packet> packet)
{
    if (!packet || !m_socket)
    {
        return;
    }
    UetSimulationTag route;
    if (!packet->PeekPacketTag(route))
    {
        return;
    }
    auto peer = m_peers.find(route.GetDestinationEndpointId());
    if (peer == m_peers.end())
    {
        return;
    }
    const uint32_t networkOverhead = peer->second.ipv6 ? 48 : 28;
    // IP and UDP headers are included in the path MTU. AI Base UET datagrams are not fragmented.
    if (packet->GetSize() + UetCrcTrailer::SERIALIZED_SIZE + networkOverhead > m_pathMtu)
    {
        ++m_mtuDrops;
        return;
    }
    Ptr<Packet> datagram = packet->Copy();
    UetCrcTrailer crc;
    crc.SetCrc(UetCrcTrailer::Calculate(datagram));
    datagram->AddTrailer(crc);
    Address destination;
    if (peer->second.ipv6)
    {
        SocketIpv6TclassTag tclass;
        tclass.SetTclass(0x02); // ECN ECT(0)
        datagram->AddPacketTag(tclass);
        destination = Inet6SocketAddress(Ipv6Address::ConvertFrom(peer->second.address),
                                         peer->second.port);
    }
    else
    {
        SocketSetDontFragmentTag dontFragment;
        dontFragment.Enable();
        datagram->AddPacketTag(dontFragment);
        SocketIpTosTag tos;
        tos.SetTos(0x02); // ECN ECT(0)
        datagram->AddPacketTag(tos);
        destination = InetSocketAddress(Ipv4Address::ConvertFrom(peer->second.address),
                                        peer->second.port);
    }
    if (m_socket->SendTo(datagram, 0, destination) >= 0)
    {
        ++m_transmittedDatagrams;
    }
}

void
UetUdpTransport::Receive(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        SocketIpTosTag tos;
        SocketIpv6TclassTag tclass;
        UetSimulationTag route;
        packet->PeekPacketTag(route);
        if (packet->PeekPacketTag(tos) && packet->RemovePacketTag(route))
        {
            route.SetEcnMarked((tos.GetTos() & 0x03) == 0x03);
            packet->AddPacketTag(route);
        }
        else if (packet->PeekPacketTag(tclass) && packet->RemovePacketTag(route))
        {
            route.SetEcnMarked((tclass.GetTclass() & 0x03) == 0x03);
            packet->AddPacketTag(route);
        }
        if (!route.IsTrimmed())
        {
            if (packet->GetSize() < UetCrcTrailer::SERIALIZED_SIZE)
            {
                ++m_crcDrops;
                continue;
            }
            UetCrcTrailer crc;
            packet->RemoveTrailer(crc);
            if (crc.GetCrc() != UetCrcTrailer::Calculate(packet))
            {
                ++m_crcDrops;
                continue;
            }
        }
        ++m_receivedDatagrams;
        m_endpoint->ReceivePacket(packet);
    }
}

uint64_t UetUdpTransport::GetTransmittedDatagrams() const { return m_transmittedDatagrams; }
uint64_t UetUdpTransport::GetReceivedDatagrams() const { return m_receivedDatagrams; }
uint64_t UetUdpTransport::GetMtuDrops() const { return m_mtuDrops; }
uint64_t UetUdpTransport::GetCrcDrops() const { return m_crcDrops; }

void
UetUdpTransport::DoDispose()
{
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    if (m_endpoint)
    {
        m_endpoint->SetTransmitCallback(UetEndpoint::TransmitCallback());
        m_endpoint = nullptr;
    }
    m_node = nullptr;
    m_peers.clear();
    Object::DoDispose();
}

} // namespace ns3
