/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UET_HELPER_H
#define UET_HELPER_H

#include "ns3/attribute.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ptr.h"

#include <string>
#include <vector>

namespace ns3
{

class Node;
class UetEndpoint;

/** @ingroup uet Helper for aggregating AI Base endpoints onto nodes. */
class UetHelper
{
  public:
    UetHelper();

    void SetAttribute(const std::string& name, const AttributeValue& value);
    Ptr<UetEndpoint> Install(Ptr<Node> node) const;
    std::vector<Ptr<UetEndpoint>> Install(const NodeContainer& nodes) const;

  private:
    ObjectFactory m_endpointFactory;
};

} // namespace ns3

#endif // UET_HELPER_H
