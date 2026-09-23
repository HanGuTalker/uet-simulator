/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef AI_TRANSPORT_FACTORY_H
#define AI_TRANSPORT_FACTORY_H

#include "ai-transport-endpoint.h"

#include "ns3/type-id.h"

#include <map>
#include <vector>

namespace ns3
{

/** Per-experiment registry and creator for protocol adapters. */
class AiTransportFactory
{
  public:
    bool Register(AiTransportProtocol protocol, TypeId adapterType);
    bool IsRegistered(AiTransportProtocol protocol) const;
    Ptr<AiTransportEndpoint> Create(AiTransportProtocol protocol) const;
    std::vector<AiTransportProtocol> GetRegisteredProtocols() const;

  private:
    std::map<AiTransportProtocol, TypeId> m_adapters;
};

} // namespace ns3

#endif // AI_TRANSPORT_FACTORY_H
