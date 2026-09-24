/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef AI_TRANSPORT_TYPES_H
#define AI_TRANSPORT_TYPES_H

#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"

#include <cstdint>
#include <string>

namespace ns3
{

/** Transport implementations supported by the comparison framework. */
enum class AiTransportProtocol : uint8_t
{
    UEC = 0,
    VEROCE = 1,
    MRC = 2,
    FALCON = 3,
    METAROCE = 4,
    ROCEV2 = 5,
    UNKNOWN = 255,
};

/** Protocol-neutral reliability behavior requested by a workload. */
enum class AiTransportReliability : uint8_t
{
    RELIABLE_UNORDERED = 0,
    RELIABLE_ORDERED = 1,
    UNRELIABLE_UNORDERED = 2,
};

/**
 * Workload-level operation.
 *
 * MESSAGE intentionally describes data movement rather than a wire verb. Each protocol adapter
 * maps it to the operation used by its benchmark profile and reports that mapping in its model
 * documentation.
 */
enum class AiTransportOperation : uint8_t
{
    MESSAGE = 0,
    WRITE = 1,
    WRITE_IMMEDIATE = 2,
    SEND = 3,
    READ = 4,
    ATOMIC_COMPARE_SWAP = 5,
    ATOMIC_FETCH_ADD = 6,
};

/** Common endpoint configuration supplied by an experiment. */
struct AiTransportEndpointConfig
{
    uint32_t endpointId{0};
    uint32_t payloadMtuBytes{4096};
    uint32_t maxRetransmissions{16};
    uint64_t lineRateBps{100000000000ULL};
    uint32_t initialWindowBytes{65536};
    uint32_t maximumWindowBytes{225000};
    Time baseRtt{MicroSeconds(12)};
    Time targetQueueDelay{MicroSeconds(12)};
    bool workConservingScheduler{false};
};

/** Common connection configuration supplied by an experiment. */
struct AiTransportConnectionConfig
{
    uint32_t remoteEndpointId{0};
    AiTransportReliability reliability{AiTransportReliability::RELIABLE_UNORDERED};
    uint32_t initialWindowBytes{0};
    uint64_t lineRateBps{0};
    Time retransmissionTimeout{MicroSeconds(50)};
};

/** One workload request submitted through the common interface. */
struct AiTransportRequest
{
    uint32_t remoteEndpointId{0};
    uint32_t connectionId{0};
    uint64_t messageId{0};
    AiTransportOperation operation{AiTransportOperation::MESSAGE};
    AiTransportReliability reliability{AiTransportReliability::RELIABLE_UNORDERED};
    Ptr<const Packet> payload;
};

/** Features exposed by a protocol adapter, based on implemented behavior rather than intent. */
struct AiTransportCapabilities
{
    bool reliableUnordered{false};
    bool reliableOrdered{false};
    bool unreliableUnordered{false};
    bool selectiveAcknowledgment{false};
    bool packetSpraying{false};
    bool perPathCongestionControl{false};
    bool endpointTrimming{false};
    bool jobScheduling{false};
};

/** Common wire-binding counters. */
struct AiTransportCounters
{
    uint64_t transmittedDatagrams{0};
    uint64_t receivedDatagrams{0};
    uint64_t mtuDrops{0};
    uint64_t integrityDrops{0};
    uint64_t selectiveAcknowledgments{0};
    uint64_t fastRetransmissions{0};
    uint64_t rttProbes{0};
    uint64_t slowPathSignals{0};
};

std::string ToString(AiTransportProtocol protocol);
AiTransportProtocol ParseAiTransportProtocol(const std::string& name);

} // namespace ns3

#endif // AI_TRANSPORT_TYPES_H
