/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_TYPES_H
#define UET_TYPES_H

#include <cstdint>

namespace ns3
{

/** UET profile implemented by this module baseline. */
enum class UetProfile : uint8_t
{
    AI_BASE = 0,
};

/** AI Base packet delivery services. */
enum class UetDeliveryMode : uint8_t
{
    RUD = 0,
    ROD = 1,
    UUD = 2,
};

/** Packet classes carried by the common simulation header. */
enum class UetPacketType : uint8_t
{
    DATA = 0,
    ACK = 1,
    NACK = 2,
    CONTROL = 3,
};

/** Bit positions in the common simulation header flags field. */
enum class UetHeaderFlag : uint8_t
{
    START_OF_MESSAGE = 1U << 0,
    END_OF_MESSAGE = 1U << 1,
    ECN = 1U << 2,
    TRIMMED = 1U << 3,
    RETRANSMISSION = 1U << 4,
};

/** Coarse PDC lifecycle states used by the trace contract. */
enum class UetPdcState : uint8_t
{
    CLOSED = 0,
    OPENING = 1,
    ACTIVE = 2,
    CLOSING = 3,
    ERROR = 4,
};

/** Result of endpoint validation and PDC dispatch for one received packet. */
enum class UetReceiveStatus : uint8_t
{
    ACCEPTED = 0,
    PACKET_TOO_SHORT = 1,
    INVALID_HEADER = 2,
    WRONG_ENDPOINT = 3,
    UNKNOWN_PDC = 4,
    INACTIVE_PDC = 5,
    DELIVERY_MODE_MISMATCH = 6,
};

} // namespace ns3

#endif // UET_TYPES_H
