/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_UDP_TRANSPORT_H
#define UET_UDP_TRANSPORT_H

#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <cstdint>
#include <unordered_map>

namespace ns3
{

class Node;
class Packet;
class Socket;
class UetEndpoint;

/** IPv4- or IPv6/UDP network binding for a UET endpoint. */
class UetUdpTransport : public Object
{
  public:
    static constexpr uint16_t UET_UDP_PORT = 4793;

    static TypeId GetTypeId();
    UetUdpTransport();
    ~UetUdpTransport() override;

    bool Bind(Ptr<Node> node, Ptr<UetEndpoint> endpoint, uint16_t port = UET_UDP_PORT);
    bool Bind6(Ptr<Node> node, Ptr<UetEndpoint> endpoint, uint16_t port = UET_UDP_PORT);
    void AddPeer(uint32_t endpointId, Ipv4Address address, uint16_t port = UET_UDP_PORT);
    void AddPeer(uint32_t endpointId, Ipv6Address address, uint16_t port = UET_UDP_PORT);
    uint64_t GetTransmittedDatagrams() const;
    uint64_t GetReceivedDatagrams() const;
    uint64_t GetMtuDrops() const;
    uint64_t GetCrcDrops() const;

  private:
    void DoDispose() override;
    void Transmit(Ptr<const Packet> packet);
    void Receive(Ptr<Socket> socket);

    Ptr<Node> m_node;
    Ptr<UetEndpoint> m_endpoint;
    Ptr<Socket> m_socket;
    uint32_t m_pathMtu{9000};
    struct Peer
    {
        Address address;
        uint16_t port;
        bool ipv6;
    };
    std::unordered_map<uint32_t, Peer> m_peers;
    uint64_t m_transmittedDatagrams{0};
    uint64_t m_receivedDatagrams{0};
    uint64_t m_mtuDrops{0};
    uint64_t m_crcDrops{0};
};

} // namespace ns3

#endif // UET_UDP_TRANSPORT_H
