/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uet-helper.h"

#include "ns3/node.h"
#include "ns3/uet-endpoint.h"

namespace ns3
{

UetHelper::UetHelper()
{
    m_endpointFactory.SetTypeId(UetEndpoint::GetTypeId());
}

void
UetHelper::SetAttribute(const std::string& name, const AttributeValue& value)
{
    m_endpointFactory.Set(name, value);
}

Ptr<UetEndpoint>
UetHelper::Install(Ptr<Node> node) const
{
    auto endpoint = m_endpointFactory.Create<UetEndpoint>();
    node->AggregateObject(endpoint);
    return endpoint;
}

std::vector<Ptr<UetEndpoint>>
UetHelper::Install(const NodeContainer& nodes) const
{
    std::vector<Ptr<UetEndpoint>> endpoints;
    endpoints.reserve(nodes.GetN());
    for (auto iterator = nodes.Begin(); iterator != nodes.End(); ++iterator)
    {
        endpoints.push_back(Install(*iterator));
    }
    return endpoints;
}

} // namespace ns3
