/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_SES_ENGINE_H
#define UET_SES_ENGINE_H

#include "uet-ses-header.h"

#include "ns3/object.h"
#include "ns3/packet.h"

#include <cstdint>
#include <vector>

namespace ns3
{

/** Access capabilities associated with one registered SES buffer. */
enum UetSesAccess : uint8_t
{
    UET_SES_ACCESS_SEND = 1U << 0,
    UET_SES_ACCESS_WRITE = 1U << 1,
    UET_SES_ACCESS_ATOMIC = 1U << 2,
};

struct UetSesExecutionResult
{
    UetSesReturnCode returnCode{UetSesReturnCode::OK};
    uint32_t modifiedLength{0};
    bool deliveryComplete{false};
    uint64_t immediateData{0};
};

/**
 * Executable AI Base semantic sublayer.
 *
 * The engine models relative/absolute address resolution, generation checks,
 * memory-key and initiator authorization, SEND receive buffers, WRITE/WRITE
 * Immediate, and non-fetching integral atomic operations.
 */
class UetSesEngine : public Object
{
  public:
    static TypeId GetTypeId();

    UetSesEngine();
    ~UetSesEngine() override;

    uint32_t RegisterBuffer(bool relative,
                            uint32_t jobId,
                            uint16_t pidOnFep,
                            uint16_t resourceIndex,
                            uint8_t generation,
                            uint64_t memoryKey,
                            uint32_t authorizedInitiator,
                            uint8_t access,
                            uint32_t size);
    bool DeregisterBuffer(uint32_t handle);
    bool ReadBuffer(uint32_t handle, uint32_t offset, uint8_t* destination, uint32_t length) const;

    UetSesExecutionResult Execute(const UetSesStandardHeader& request,
                                  const UetAtomicExtensionHeader* atomic,
                                  Ptr<const Packet> payload);

  private:
    struct RegisteredBuffer
    {
        uint32_t handle{0};
        bool relative{true};
        uint32_t jobId{0};
        uint16_t pidOnFep{0};
        uint16_t resourceIndex{0};
        uint8_t generation{0};
        uint64_t memoryKey{0};
        uint32_t authorizedInitiator{0};
        uint8_t access{0};
        std::vector<uint8_t> bytes;
    };

    RegisteredBuffer* Resolve(const UetSesStandardHeader& request, UetSesReturnCode& error);
    static uint64_t ReadNetworkInteger(const uint8_t* bytes, uint32_t size);
    static void WriteNetworkInteger(uint8_t* bytes, uint32_t size, uint64_t value);
    UetSesReturnCode ExecuteAtomic(RegisteredBuffer& buffer,
                                   uint64_t offset,
                                   const UetAtomicExtensionHeader& atomic,
                                   const uint8_t* payload,
                                   uint32_t length);

    uint32_t m_nextHandle{1};
    std::vector<RegisteredBuffer> m_buffers;
};

} // namespace ns3

#endif // UET_SES_ENGINE_H
