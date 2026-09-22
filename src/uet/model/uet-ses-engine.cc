/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-ses-engine.h"

#include "ns3/log.h"

#include <algorithm>
#include <limits>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UetSesEngine");
NS_OBJECT_ENSURE_REGISTERED(UetSesEngine);

TypeId
UetSesEngine::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UetSesEngine")
                            .SetParent<Object>()
                            .SetGroupName("Uet")
                            .AddConstructor<UetSesEngine>();
    return tid;
}

UetSesEngine::UetSesEngine() = default;
UetSesEngine::~UetSesEngine() = default;

uint32_t
UetSesEngine::RegisterBuffer(bool relative,
                             uint32_t jobId,
                             uint16_t pidOnFep,
                             uint16_t resourceIndex,
                             uint8_t generation,
                             uint64_t memoryKey,
                             uint32_t authorizedInitiator,
                             uint8_t access,
                             uint32_t size)
{
    if (jobId > 0xffffff || pidOnFep > 0x0fff || resourceIndex > 0x0fff || size == 0 ||
        access == 0)
    {
        return 0;
    }
    for (const auto& buffer : m_buffers)
    {
        if (buffer.relative == relative && buffer.jobId == (relative ? jobId : 0) &&
            buffer.pidOnFep == pidOnFep && buffer.resourceIndex == resourceIndex)
        {
            return 0;
        }
    }
    RegisteredBuffer buffer;
    buffer.handle = m_nextHandle++;
    buffer.relative = relative;
    buffer.jobId = relative ? jobId : 0;
    buffer.pidOnFep = pidOnFep;
    buffer.resourceIndex = resourceIndex;
    buffer.generation = generation;
    buffer.memoryKey = memoryKey;
    buffer.authorizedInitiator = authorizedInitiator;
    buffer.access = access;
    buffer.bytes.resize(size);
    m_buffers.push_back(std::move(buffer));
    return m_buffers.back().handle;
}

bool
UetSesEngine::DeregisterBuffer(uint32_t handle)
{
    auto it = std::find_if(m_buffers.begin(), m_buffers.end(), [handle](const auto& buffer) {
        return buffer.handle == handle;
    });
    if (it == m_buffers.end())
    {
        return false;
    }
    m_buffers.erase(it);
    return true;
}

bool
UetSesEngine::ReadBuffer(uint32_t handle,
                         uint32_t offset,
                         uint8_t* destination,
                         uint32_t length) const
{
    auto it = std::find_if(m_buffers.begin(), m_buffers.end(), [handle](const auto& buffer) {
        return buffer.handle == handle;
    });
    if (it == m_buffers.end() || !destination || offset > it->bytes.size() ||
        length > it->bytes.size() - offset)
    {
        return false;
    }
    std::copy_n(it->bytes.begin() + offset, length, destination);
    return true;
}

UetSesEngine::RegisteredBuffer*
UetSesEngine::Resolve(const UetSesStandardHeader& request, UetSesReturnCode& error)
{
    bool foundJob = !request.IsRelativeAddressing();
    bool foundPid = false;
    for (auto& buffer : m_buffers)
    {
        if (buffer.relative != request.IsRelativeAddressing())
        {
            continue;
        }
        if (buffer.relative && buffer.jobId != request.GetJobId())
        {
            continue;
        }
        foundJob = true;
        if (buffer.pidOnFep != request.GetPidOnFep())
        {
            continue;
        }
        foundPid = true;
        if (buffer.resourceIndex != request.GetResourceIndex())
        {
            continue;
        }
        if (buffer.generation != request.GetRiGeneration())
        {
            error = UetSesReturnCode::BAD_GENERATION;
            return nullptr;
        }
        if (buffer.authorizedInitiator != 0 &&
            buffer.authorizedInitiator != request.GetInitiator())
        {
            error = UetSesReturnCode::PERMISSION_VIOLATION;
            return nullptr;
        }
        return &buffer;
    }
    error = !foundJob ? UetSesReturnCode::BAD_JOB_ID
                      : (!foundPid ? UetSesReturnCode::BAD_PID : UetSesReturnCode::BAD_INDEX);
    return nullptr;
}

UetSesExecutionResult
UetSesEngine::Execute(const UetSesStandardHeader& request,
                      const UetAtomicExtensionHeader* atomic,
                      Ptr<const Packet> payload)
{
    UetSesExecutionResult result;
    result.deliveryComplete = request.IsDeliveryComplete();
    result.immediateData = request.HasHeaderData() ? request.GetHeaderData() : 0;
    const uint32_t length = payload ? payload->GetSize() : 0;

    if (request.GetOpcode() == UetSesOpcode::NO_OP)
    {
        result.returnCode = length == 0 && request.IsStartOfMessage() && request.IsEndOfMessage()
                                ? UetSesReturnCode::OK
                                : UetSesReturnCode::UNSUPPORTED_SIZE;
        return result;
    }

    UetSesReturnCode resolutionError = UetSesReturnCode::NO_MATCH;
    RegisteredBuffer* buffer = Resolve(request, resolutionError);
    if (!buffer)
    {
        result.returnCode = request.GetOpcode() == UetSesOpcode::DEFERRABLE_SEND
                                ? UetSesReturnCode::NO_MATCH
                                : resolutionError;
        return result;
    }

    uint8_t requiredAccess = 0;
    switch (request.GetOpcode())
    {
    case UetSesOpcode::SEND:
    case UetSesOpcode::DEFERRABLE_SEND:
        requiredAccess = UET_SES_ACCESS_SEND;
        break;
    case UetSesOpcode::WRITE:
        requiredAccess = UET_SES_ACCESS_WRITE;
        break;
    case UetSesOpcode::ATOMIC:
        requiredAccess = UET_SES_ACCESS_ATOMIC;
        break;
    default:
        result.returnCode = UetSesReturnCode::UNSUPPORTED_OPERATION;
        return result;
    }
    if ((buffer->access & requiredAccess) == 0)
    {
        result.returnCode = UetSesReturnCode::OPERATION_VIOLATION;
        return result;
    }
    if ((request.GetOpcode() == UetSesOpcode::WRITE || request.GetOpcode() == UetSesOpcode::ATOMIC) &&
        buffer->memoryKey != request.GetMemoryKey())
    {
        result.returnCode = UetSesReturnCode::PERMISSION_VIOLATION;
        return result;
    }

    const uint64_t offset = request.GetBufferOffset() + request.GetMessageOffset();
    if (offset > buffer->bytes.size() || length > buffer->bytes.size() - offset)
    {
        result.returnCode = UetSesReturnCode::UNSUPPORTED_SIZE;
        return result;
    }
    std::vector<uint8_t> bytes(length);
    if (length != 0)
    {
        payload->CopyData(bytes.data(), length);
    }
    if (request.GetOpcode() == UetSesOpcode::ATOMIC)
    {
        if (!atomic || !atomic->IsValid())
        {
            result.returnCode = UetSesReturnCode::AMO_UNSUPPORTED_OPERATION;
            return result;
        }
        result.returnCode = ExecuteAtomic(*buffer, offset, *atomic, bytes.data(), length);
    }
    else
    {
        std::copy(bytes.begin(), bytes.end(), buffer->bytes.begin() + offset);
        result.returnCode = UetSesReturnCode::OK;
    }
    result.modifiedLength = result.returnCode == UetSesReturnCode::OK ? length : 0;
    return result;
}

uint64_t
UetSesEngine::ReadNetworkInteger(const uint8_t* bytes, uint32_t size)
{
    uint64_t value = 0;
    for (uint32_t i = 0; i < size; ++i)
    {
        value = (value << 8) | bytes[i];
    }
    return value;
}

void
UetSesEngine::WriteNetworkInteger(uint8_t* bytes, uint32_t size, uint64_t value)
{
    for (uint32_t i = 0; i < size; ++i)
    {
        bytes[size - i - 1] = value & 0xff;
        value >>= 8;
    }
}

UetSesReturnCode
UetSesEngine::ExecuteAtomic(RegisteredBuffer& buffer,
                            uint64_t offset,
                            const UetAtomicExtensionHeader& atomic,
                            const uint8_t* payload,
                            uint32_t length)
{
    const uint32_t elementSize = atomic.GetDatatypeSize();
    if (elementSize == 0)
    {
        return UetSesReturnCode::AMO_UNSUPPORTED_DATATYPE;
    }
    if (length == 0 || length % elementSize != 0)
    {
        return UetSesReturnCode::AMO_UNSUPPORTED_SIZE;
    }
    if (offset % elementSize != 0)
    {
        return UetSesReturnCode::AMO_UNALIGNED;
    }
    const uint64_t mask = elementSize == 8 ? std::numeric_limits<uint64_t>::max()
                                           : ((1ULL << (elementSize * 8)) - 1);
    const bool signedDatatype = atomic.GetAtomicDatatype() >= UetAtomicDatatype::INT8;
    const auto asSigned = [elementSize](uint64_t value) {
        if (elementSize == 8)
        {
            return static_cast<int64_t>(value);
        }
        const uint64_t sign = 1ULL << (elementSize * 8 - 1);
        return static_cast<int64_t>((value ^ sign) - sign);
    };
    for (uint32_t pos = 0; pos < length; pos += elementSize)
    {
        const uint64_t source = ReadNetworkInteger(payload + pos, elementSize);
        const uint64_t target = ReadNetworkInteger(buffer.bytes.data() + offset + pos, elementSize);
        uint64_t value = target;
        switch (atomic.GetAtomicOpcode())
        {
        case UetAtomicOpcode::WRITE:
            value = source;
            break;
        case UetAtomicOpcode::SUM:
            value = target + source;
            break;
        case UetAtomicOpcode::BOR:
            value = target | source;
            break;
        case UetAtomicOpcode::BAND:
            value = target & source;
            break;
        case UetAtomicOpcode::BXOR:
            value = target ^ source;
            break;
        case UetAtomicOpcode::MIN:
            value = signedDatatype
                        ? static_cast<uint64_t>(std::min(asSigned(target), asSigned(source)))
                        : std::min(target, source);
            break;
        case UetAtomicOpcode::MAX:
            value = signedDatatype
                        ? static_cast<uint64_t>(std::max(asSigned(target), asSigned(source)))
                        : std::max(target, source);
            break;
        default:
            return UetSesReturnCode::AMO_UNSUPPORTED_OPERATION;
        }
        WriteNetworkInteger(buffer.bytes.data() + offset + pos, elementSize, value & mask);
    }
    return UetSesReturnCode::OK;
}

} // namespace ns3
