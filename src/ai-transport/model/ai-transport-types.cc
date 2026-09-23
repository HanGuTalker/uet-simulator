/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ai-transport-types.h"

#include <algorithm>
#include <cctype>

namespace ns3
{

std::string
ToString(AiTransportProtocol protocol)
{
    switch (protocol)
    {
    case AiTransportProtocol::UEC:
        return "uec";
    case AiTransportProtocol::VEROCE:
        return "veroce";
    case AiTransportProtocol::MRC:
        return "mrc";
    case AiTransportProtocol::FALCON:
        return "falcon";
    case AiTransportProtocol::METAROCE:
        return "metaroce";
    case AiTransportProtocol::ROCEV2:
        return "rocev2";
    case AiTransportProtocol::UNKNOWN:
        return "unknown";
    }
    return "unknown";
}

AiTransportProtocol
ParseAiTransportProtocol(const std::string& name)
{
    std::string normalized = name;
    std::transform(normalized.begin(),
                   normalized.end(),
                   normalized.begin(),
                   [](unsigned char character) { return std::tolower(character); });
    normalized.erase(std::remove_if(normalized.begin(),
                                    normalized.end(),
                                    [](unsigned char character) {
                                        return character == '-' || character == '_' ||
                                               std::isspace(character);
                                    }),
                     normalized.end());
    if (normalized == "uec" || normalized == "uet")
    {
        return AiTransportProtocol::UEC;
    }
    if (normalized == "veroce")
    {
        return AiTransportProtocol::VEROCE;
    }
    if (normalized == "mrc")
    {
        return AiTransportProtocol::MRC;
    }
    if (normalized == "falcon")
    {
        return AiTransportProtocol::FALCON;
    }
    if (normalized == "metaroce")
    {
        return AiTransportProtocol::METAROCE;
    }
    if (normalized == "rocev2" || normalized == "roce")
    {
        return AiTransportProtocol::ROCEV2;
    }
    return AiTransportProtocol::UNKNOWN;
}

} // namespace ns3
