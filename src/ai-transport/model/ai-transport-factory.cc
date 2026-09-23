/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ai-transport-factory.h"

#include "ns3/object-factory.h"

namespace ns3
{

bool
AiTransportFactory::Register(AiTransportProtocol protocol, TypeId adapterType)
{
    if (protocol == AiTransportProtocol::UNKNOWN ||
        !adapterType.IsChildOf(AiTransportEndpoint::GetTypeId()))
    {
        return false;
    }
    return m_adapters.emplace(protocol, adapterType).second;
}

bool
AiTransportFactory::IsRegistered(AiTransportProtocol protocol) const
{
    return m_adapters.contains(protocol);
}

Ptr<AiTransportEndpoint>
AiTransportFactory::Create(AiTransportProtocol protocol) const
{
    const auto adapter = m_adapters.find(protocol);
    if (adapter == m_adapters.end())
    {
        return nullptr;
    }
    ObjectFactory factory;
    factory.SetTypeId(adapter->second);
    return factory.Create<AiTransportEndpoint>();
}

std::vector<AiTransportProtocol>
AiTransportFactory::GetRegisteredProtocols() const
{
    std::vector<AiTransportProtocol> protocols;
    protocols.reserve(m_adapters.size());
    for (const auto& [protocol, unused] : m_adapters)
    {
        (void)unused;
        protocols.push_back(protocol);
    }
    return protocols;
}

} // namespace ns3
