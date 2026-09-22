/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/uet-module.h"

#include <iostream>

using namespace ns3;

int
main(int argc, char* argv[])
{
    CommandLine command(__FILE__);
    command.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    UetHelper helper;
    const auto endpoints = helper.Install(nodes);

    std::cout << "Installed " << endpoints.size() << " UET AI Base endpoints" << std::endl;
    Simulator::Destroy();
    return endpoints.size() == 2 ? 0 : 1;
}
