/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/ai-transport-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mrc-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/roce-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/uet-module.h"
#include "ns3/veroce-module.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

class AiWorkload
{
  public:
    bool Run(uint32_t nodeCount,
             uint32_t messages,
             uint32_t payloadBytes,
             const std::string& transportName,
             const std::string& operationName,
             const std::string& pattern,
             bool ringInterleaved,
             const std::string& fabricType,
             const std::string& linkRate,
             uint64_t linkRateBps,
             uint32_t linkDelayNs,
             uint32_t queuePackets,
             bool reusePdc,
             uint32_t warmupBytes,
             uint32_t measurementStartUs,
             uint64_t startGapNs,
             uint32_t nsccBaseRttNs,
             uint32_t nsccTargetQueueDelayNs,
             uint32_t nsccInitialWindowBytes,
             bool autoScaleInitialWindow,
             uint32_t backgroundAllToAllGroups,
             uint32_t backgroundPayloadBytes,
             uint64_t backgroundStartOffsetNs,
             uint32_t ringJobWeight,
             uint32_t backgroundJobWeight,
             bool enableWorkConservingScheduler,
             bool enableEcn,
             bool enableTrimming,
             uint32_t ecnMinBytes,
             uint32_t ecnMaxBytes,
             uint32_t ecnQueueLimitBytes,
             const std::string& outputPrefix)
    {
        m_nodeCount = nodeCount;
        m_messagesPerPair = messages;
        m_payloadBytes = payloadBytes;
        m_transportName = transportName;
        m_operationName = operationName;
        m_operation = operationName == "send"    ? AiTransportOperation::SEND
                      : operationName == "write" ? AiTransportOperation::WRITE
                      : operationName == "read"  ? AiTransportOperation::READ
                                                 : AiTransportOperation::MESSAGE;
        m_pattern = pattern;
        m_ringInterleaved = ringInterleaved;
        m_fabricType = fabricType;
        m_linkRate = linkRate;
        m_linkRateBps = linkRateBps;
        m_linkDelayNs = linkDelayNs;
        m_queuePackets = (enableEcn || enableTrimming) ? 1 : queuePackets;
        m_reusePdc = reusePdc;
        m_warmupBytes = warmupBytes;
        m_measurementStartUs = measurementStartUs;
        m_startGapNs = startGapNs;
        m_nsccBaseRttNs = nsccBaseRttNs;
        m_nsccTargetQueueDelayNs = nsccTargetQueueDelayNs;
        m_nsccRequestedInitialWindowBytes = nsccInitialWindowBytes;
        m_autoScaleInitialWindow = autoScaleInitialWindow;
        m_backgroundAllToAllGroups = backgroundAllToAllGroups;
        m_backgroundPayloadBytes = backgroundPayloadBytes;
        m_backgroundStartOffsetNs = backgroundStartOffsetNs;
        m_ringJobWeight = ringJobWeight;
        m_backgroundJobWeight = backgroundJobWeight;
        m_workConservingScheduler = enableWorkConservingScheduler;
        uint32_t concurrentGroups = 1;
        if (pattern == "all-to-all" && !reusePdc)
        {
            concurrentGroups = messages;
        }
        else if (pattern == "ring-allreduce" && backgroundAllToAllGroups > 0)
        {
            concurrentGroups = 1 + backgroundAllToAllGroups;
        }
        m_nsccInitialWindowBytes =
            autoScaleInitialWindow
                ? std::max<uint32_t>(4096, nsccInitialWindowBytes / concurrentGroups)
                : nsccInitialWindowBytes;
        m_backgroundPdcInitialWindowBytes =
            autoScaleInitialWindow && pattern == "ring-allreduce" && backgroundAllToAllGroups > 0
                ? std::max<uint32_t>(4096, m_nsccInitialWindowBytes / (nodeCount - 1))
                : m_nsccInitialWindowBytes;
        if (!enableWorkConservingScheduler && pattern == "ring-allreduce" &&
            backgroundAllToAllGroups > 0 && ringJobWeight > 0 && backgroundJobWeight > 0)
        {
            const uint64_t totalWeight =
                ringJobWeight +
                static_cast<uint64_t>(backgroundJobWeight) * backgroundAllToAllGroups;
            m_ringPdcLineRateBps = linkRateBps * ringJobWeight / totalWeight;
            const uint64_t backgroundGroupRate = linkRateBps * backgroundJobWeight / totalWeight;
            m_backgroundPdcLineRateBps =
                std::max<uint64_t>(1, backgroundGroupRate / (nodeCount - 1));
        }
        m_enableEcn = enableEcn || enableTrimming;
        m_enableTrimming = enableTrimming;
        m_ecnMinBytes = ecnMinBytes;
        m_ecnMaxBytes = ecnMaxBytes;
        m_ecnQueueLimitBytes = ecnQueueLimitBytes;

        NodeContainer nodes;
        nodes.Create(nodeCount);
        InternetStackHelper internet;
        internet.Install(nodes);
        std::vector<Ipv4Address> endpointAddresses;
        std::vector<Ptr<NetDevice>> routerEgressDevices;
        std::vector<uint32_t> routerEgressLabels;
        if (fabricType == "switched")
        {
            NodeContainer router;
            router.Create(1);
            internet.Install(router);
            PointToPointHelper link;
            link.SetDeviceAttribute("DataRate", StringValue(linkRate));
            link.SetDeviceAttribute("Mtu", UintegerValue(9000));
            link.SetChannelAttribute("Delay", TimeValue(NanoSeconds(linkDelayNs)));
            link.SetQueue(
                "ns3::DropTailQueue",
                "MaxSize",
                StringValue(std::to_string((enableEcn || enableTrimming) ? 1 : queuePackets) +
                            "p"));
            for (uint32_t i = 0; i < nodeCount; ++i)
            {
                auto devices = link.Install(NodeContainer(nodes.Get(i), router.Get(0)));
                Ipv4AddressHelper subnet;
                std::ostringstream base;
                base << "10.42." << i << ".0";
                subnet.SetBase(base.str().c_str(), "255.255.255.252");
                const auto interfaces = subnet.Assign(devices);
                endpointAddresses.push_back(interfaces.GetAddress(0));
                routerEgressDevices.push_back(devices.Get(1));
                routerEgressLabels.push_back(i + 1);
            }
            Ipv4GlobalRoutingHelper::PopulateRoutingTables();
        }
        else if (fabricType == "leaf-spine")
        {
            NodeContainer fabricNodes;
            fabricNodes.Create(3);
            internet.Install(fabricNodes);
            PointToPointHelper link;
            link.SetDeviceAttribute("DataRate", StringValue(linkRate));
            link.SetDeviceAttribute("Mtu", UintegerValue(9000));
            link.SetChannelAttribute("Delay", TimeValue(NanoSeconds(linkDelayNs)));
            link.SetQueue(
                "ns3::DropTailQueue",
                "MaxSize",
                StringValue(std::to_string((enableEcn || enableTrimming) ? 1 : queuePackets) +
                            "p"));
            const uint32_t nodesPerLeaf = nodeCount / 2;
            for (uint32_t i = 0; i < nodeCount; ++i)
            {
                const uint32_t leaf = std::min(i / nodesPerLeaf, 1u);
                auto devices = link.Install(NodeContainer(nodes.Get(i), fabricNodes.Get(leaf)));
                Ipv4AddressHelper subnet;
                std::ostringstream base;
                base << "10.50." << i << ".0";
                subnet.SetBase(base.str().c_str(), "255.255.255.252");
                const auto interfaces = subnet.Assign(devices);
                endpointAddresses.push_back(interfaces.GetAddress(0));
                routerEgressDevices.push_back(devices.Get(1));
                routerEgressLabels.push_back(i + 1);
            }
            for (uint32_t leaf = 0; leaf < 2; ++leaf)
            {
                auto devices =
                    link.Install(NodeContainer(fabricNodes.Get(leaf), fabricNodes.Get(2)));
                Ipv4AddressHelper subnet;
                std::ostringstream base;
                base << "10.60." << leaf << ".0";
                subnet.SetBase(base.str().c_str(), "255.255.255.252");
                subnet.Assign(devices);
                routerEgressDevices.push_back(devices.Get(0));
                routerEgressLabels.push_back(1001 + leaf);
                routerEgressDevices.push_back(devices.Get(1));
                routerEgressLabels.push_back(2001 + leaf);
            }
            Ipv4GlobalRoutingHelper::PopulateRoutingTables();
        }
        else
        {
            CsmaHelper fabric;
            fabric.SetChannelAttribute("DataRate", StringValue(linkRate));
            fabric.SetChannelAttribute("Delay", TimeValue(NanoSeconds(linkDelayNs)));
            fabric.SetDeviceAttribute("Mtu", UintegerValue(9000));
            fabric.SetQueue("ns3::DropTailQueue",
                            "MaxSize",
                            StringValue(std::to_string(queuePackets) + "p"));
            auto devices = fabric.Install(nodes);
            Ipv4AddressHelper addresses;
            addresses.SetBase("10.42.0.0", "255.255.0.0");
            const auto interfaces = addresses.Assign(devices);
            for (uint32_t i = 0; i < nodeCount; ++i)
            {
                endpointAddresses.push_back(interfaces.GetAddress(i));
            }
        }

        if (enableEcn || enableTrimming)
        {
            TrafficControlHelper traffic;
            if (enableTrimming)
            {
                traffic.SetRootQueueDisc(
                    "ns3::VeRoceTrimQueueDisc",
                    "MarkThresholdBytes",
                    UintegerValue(ecnMinBytes),
                    "TrimThresholdBytes",
                    UintegerValue(ecnMaxBytes),
                    "MaxSize",
                    QueueSizeValue(QueueSize(std::to_string(ecnQueueLimitBytes) + "B")));
            }
            else
            {
                traffic.SetRootQueueDisc(
                    "ns3::RedQueueDisc",
                    "LinkBandwidth",
                    StringValue(linkRate),
                    "LinkDelay",
                    TimeValue(NanoSeconds(linkDelayNs)),
                    "MinTh",
                    DoubleValue(ecnMinBytes),
                    "MaxTh",
                    DoubleValue(ecnMaxBytes),
                    "MaxSize",
                    QueueSizeValue(QueueSize(std::to_string(ecnQueueLimitBytes) + "B")),
                    "MeanPktSize",
                    UintegerValue(4186),
                    "QW",
                    DoubleValue(1.0),
                    "UseEcn",
                    BooleanValue(true),
                    "UseHardDrop",
                    BooleanValue(false));
            }
            for (uint32_t i = 0; i < routerEgressDevices.size(); ++i)
            {
                traffic.Uninstall(routerEgressDevices[i]);
                auto queueDiscs = traffic.Install(routerEgressDevices[i]);
                auto queueDisc = queueDiscs.Get(0);
                m_bottleneckQueueDiscs.push_back(queueDisc);
                queueDisc->TraceConnectWithoutContext(
                    "BytesInQueue",
                    MakeBoundCallback(&AiWorkload::QueueBytesChanged, this, routerEgressLabels[i]));
            }
        }
        for (const auto& device : routerEgressDevices)
        {
            auto pointToPoint = DynamicCast<PointToPointNetDevice>(device);
            if (pointToPoint && pointToPoint->GetQueue())
            {
                pointToPoint->GetQueue()->TraceConnectWithoutContext(
                    "Drop",
                    MakeCallback(&AiWorkload::DeviceQueueDrop, this));
            }
        }

        const auto protocol = ParseAiTransportProtocol(transportName);
        AiTransportFactory transportFactory;
        transportFactory.Register(AiTransportProtocol::UEC, UetTransportAdapter::GetTypeId());
        transportFactory.Register(AiTransportProtocol::ROCEV2, RoceV2TransportAdapter::GetTypeId());
        transportFactory.Register(AiTransportProtocol::VEROCE, VeRoceTransportAdapter::GetTypeId());
        transportFactory.Register(AiTransportProtocol::MRC, MrcTransportAdapter::GetTypeId());
        if (protocol == AiTransportProtocol::UNKNOWN || !transportFactory.IsRegistered(protocol))
        {
            std::cerr << "Transport '" << transportName
                      << "' is known to the common framework but has no registered implementation"
                      << std::endl;
            return false;
        }
        m_transportName = ToString(protocol);
        m_endpoints.resize(nodeCount);
        for (uint32_t i = 0; i < nodeCount; ++i)
        {
            const uint64_t baseRttNs = 4ULL * linkDelayNs + 200ULL;
            const uint64_t bdpBytes = (linkRateBps * baseRttNs) / 8000000000ULL;
            AiTransportEndpointConfig endpointConfig;
            endpointConfig.endpointId = i + 1;
            endpointConfig.payloadMtuBytes = 4096;
            endpointConfig.maxRetransmissions = 16;
            endpointConfig.lineRateBps = linkRateBps;
            endpointConfig.initialWindowBytes = m_nsccInitialWindowBytes;
            endpointConfig.maximumWindowBytes = static_cast<uint32_t>(
                std::min<uint64_t>(std::numeric_limits<uint32_t>::max(),
                                   std::max<uint64_t>(65536, (3 * bdpBytes) / 2)));
            endpointConfig.baseRtt = NanoSeconds(nsccBaseRttNs);
            endpointConfig.targetQueueDelay = NanoSeconds(nsccTargetQueueDelayNs);
            endpointConfig.workConservingScheduler = enableWorkConservingScheduler;
            m_endpoints[i] = transportFactory.Create(protocol);
            if (!m_endpoints[i] || !m_endpoints[i]->Initialize(nodes.Get(i), endpointConfig))
            {
                return false;
            }
            m_endpoints[i]->TraceConnectWithoutContext(
                "MessageComplete",
                MakeBoundCallback(&AiWorkload::MessageCompleteSink, this, i + 1));
            m_endpoints[i]->TraceConnectWithoutContext(
                "Retransmission",
                MakeCallback(&AiWorkload::Retransmission, this));
            m_endpoints[i]->TraceConnectWithoutContext("Timeout",
                                                       MakeCallback(&AiWorkload::Timeout, this));
            m_endpoints[i]->TraceConnectWithoutContext("Nack",
                                                       MakeCallback(&AiWorkload::Nack, this));
            m_endpoints[i]->TraceConnectWithoutContext("EcnReceived",
                                                       MakeCallback(&AiWorkload::Ecn, this));
            m_endpoints[i]->TraceConnectWithoutContext("PacketTrimmed",
                                                       MakeCallback(&AiWorkload::Trimmed, this));
            m_endpoints[i]->TraceConnectWithoutContext(
                "CongestionWindow",
                MakeBoundCallback(&AiWorkload::CongestionWindowSink, this, i + 1));
            m_endpoints[i]->TraceConnectWithoutContext(
                "PayloadRx",
                MakeBoundCallback(&AiWorkload::ReceiverPayloadSink, this));
        }
        for (uint32_t i = 0; i < nodeCount; ++i)
        {
            for (uint32_t j = 0; j < nodeCount; ++j)
            {
                if (i != j)
                {
                    if (!m_endpoints[i]->AddPeer(j + 1, endpointAddresses[j].ConvertTo()))
                    {
                        return false;
                    }
                }
            }
        }

        uint64_t messageId = 1;
        if (pattern == "ring-allreduce")
        {
            m_ringChunkBytes = payloadBytes / nodeCount;
            m_ringStepsPerCollective = 2 * (nodeCount - 1);
            m_ringPdcIds.resize(nodeCount);
            m_ringTargetBySource.resize(nodeCount);
            std::vector<uint32_t> ringOrder;
            if (ringInterleaved)
            {
                for (uint32_t i = 0; i < nodeCount / 2; ++i)
                {
                    ringOrder.push_back(i);
                    ringOrder.push_back(i + nodeCount / 2);
                }
            }
            else
            {
                for (uint32_t i = 0; i < nodeCount; ++i)
                {
                    ringOrder.push_back(i);
                }
            }
            for (uint32_t i = 0; i < nodeCount; ++i)
            {
                m_ringTargetBySource[ringOrder[i]] = ringOrder[(i + 1) % nodeCount] + 1;
            }
            for (uint32_t source = 0; source < nodeCount; ++source)
            {
                const uint32_t pdcId = CreateConfiguredConnection(source,
                                                                  m_ringTargetBySource[source],
                                                                  0,
                                                                  0,
                                                                  m_ringPdcLineRateBps);
                if (pdcId == 0)
                {
                    return false;
                }
                m_ringPdcIds[source] = pdcId;
                if (enableWorkConservingScheduler)
                {
                    m_endpoints[source]->AssignConnectionToJob(pdcId, 1, ringJobWeight);
                }
                if (warmupBytes > 0)
                {
                    const uint64_t warmupId = messageId++;
                    m_warmupIds.insert(warmupId);
                    Simulator::Schedule(NanoSeconds(startGapNs * source),
                                        &AiWorkload::SubmitWarmup,
                                        this,
                                        source,
                                        m_ringTargetBySource[source],
                                        pdcId,
                                        warmupId,
                                        warmupBytes);
                }
                if (measurementStartUs > 0)
                {
                    Simulator::Schedule(MicroSeconds(measurementStartUs) - NanoSeconds(1),
                                        &AiWorkload::CaptureCongestionWindow,
                                        this,
                                        source,
                                        pdcId);
                }
            }
            m_ringMessageIds.resize(messages);
            m_ringCollectives.resize(messages);
            for (uint32_t collective = 0; collective < messages; ++collective)
            {
                m_ringMessageIds[collective].resize(m_ringStepsPerCollective);
                for (uint32_t step = 0; step < m_ringStepsPerCollective; ++step)
                {
                    for (uint32_t source = 0; source < nodeCount; ++source)
                    {
                        const uint64_t id = messageId++;
                        const uint32_t target = m_ringTargetBySource[source] - 1;
                        m_ringMessageIds[collective][step].push_back(id);
                        m_ringMessageLocation.emplace(id, std::make_pair(collective, step));
                        m_messageGroups[id] = collective + 1;
                        m_records.emplace(
                            id,
                            MessageRecord{id, source + 1, target + 1, m_ringChunkBytes, -1});
                    }
                }
            }
            Simulator::Schedule(MicroSeconds(measurementStartUs),
                                &AiWorkload::StartRingStep,
                                this,
                                0,
                                0);
            for (uint32_t group = 0; group < backgroundAllToAllGroups; ++group)
            {
                for (uint32_t source = 0; source < nodeCount; ++source)
                {
                    for (uint32_t target = 0; target < nodeCount; ++target)
                    {
                        if (source == target)
                        {
                            continue;
                        }
                        const uint32_t pdcId =
                            CreateConfiguredConnection(source,
                                                       target + 1,
                                                       group + 1,
                                                       m_backgroundPdcInitialWindowBytes,
                                                       m_backgroundPdcLineRateBps);
                        if (pdcId == 0)
                        {
                            return false;
                        }
                        if (enableWorkConservingScheduler)
                        {
                            m_endpoints[source]->AssignConnectionToJob(pdcId,
                                                                       2 + group,
                                                                       backgroundJobWeight);
                        }
                        const uint64_t id = messageId++;
                        const Time scheduled =
                            MicroSeconds(measurementStartUs) + NanoSeconds(backgroundStartOffsetNs);
                        m_records.emplace(id,
                                          MessageRecord{id,
                                                        source + 1,
                                                        target + 1,
                                                        backgroundPayloadBytes,
                                                        scheduled.GetNanoSeconds()});
                        m_messageGroups[id] = 1001 + group;
                        ++m_backgroundMessagesRemaining;
                        Simulator::Schedule(scheduled,
                                            &AiWorkload::SubmitMessage,
                                            this,
                                            source,
                                            target + 1,
                                            pdcId,
                                            id,
                                            backgroundPayloadBytes);
                    }
                }
            }
        }
        else
        {
            for (uint32_t source = 0; source < nodeCount; ++source)
            {
                for (uint32_t target = 0; target < nodeCount; ++target)
                {
                    const bool selected =
                        pattern == "all-to-all" ||
                        (pattern == "incast" && target == nodeCount - 1) ||
                        (pattern == "single" && source == 0 && target == nodeCount - 1);
                    if (source == target || !selected)
                    {
                        continue;
                    }
                    uint32_t reusedConnectionId = 0;
                    if (reusePdc)
                    {
                        reusedConnectionId = CreateConfiguredConnection(source, target + 1, 0);
                        if (reusedConnectionId == 0)
                        {
                            return false;
                        }
                        if (warmupBytes > 0)
                        {
                            const uint64_t warmupId = messageId++;
                            m_warmupIds.insert(warmupId);
                            Simulator::Schedule(NanoSeconds(startGapNs * source),
                                                &AiWorkload::SubmitWarmup,
                                                this,
                                                source,
                                                target + 1,
                                                reusedConnectionId,
                                                warmupId,
                                                warmupBytes);
                        }
                        if (measurementStartUs > 0)
                        {
                            const uint64_t captureNs = measurementStartUs * 1000ULL - 1;
                            Simulator::Schedule(NanoSeconds(captureNs),
                                                &AiWorkload::CaptureCongestionWindow,
                                                this,
                                                source,
                                                reusedConnectionId);
                        }
                    }
                    uint64_t previousMessageId = 0;
                    for (uint32_t occurrence = 0; occurrence < messages; ++occurrence)
                    {
                        const uint32_t pdcId =
                            reusePdc ? reusedConnectionId
                                     : CreateConfiguredConnection(source, target + 1, occurrence);
                        if (pdcId == 0)
                        {
                            return false;
                        }
                        const uint64_t id = messageId++;
                        const Time scheduled = NanoSeconds(
                            measurementStartUs * 1000ULL +
                            startGapNs * static_cast<uint64_t>(occurrence * nodeCount + source));
                        m_records.emplace(
                            id,
                            MessageRecord{id,
                                          source + 1,
                                          target + 1,
                                          payloadBytes,
                                          reusePdc && startGapNs == 0 && occurrence > 0
                                              ? -1
                                              : scheduled.GetNanoSeconds()});
                        m_messageGroups[id] = occurrence + 1;
                        if (reusePdc && startGapNs == 0 && occurrence > 0)
                        {
                            // Keep a long-lived PDC continuously busy without placing more than
                            // the signed 16-bit CLEAR_PSN window on the wire at once.
                            m_chainedSubmissions.emplace(
                                previousMessageId,
                                PendingSubmission{source, target + 1, pdcId, id, payloadBytes});
                        }
                        else
                        {
                            // Preserve a dense burst while avoiding artificial CSMA half-duplex
                            // collisions at exactly identical simulation timestamps.
                            Simulator::Schedule(scheduled,
                                                &AiWorkload::SubmitMessage,
                                                this,
                                                source,
                                                target + 1,
                                                pdcId,
                                                id,
                                                payloadBytes);
                        }
                        previousMessageId = id;
                    }
                }
            }
        }

        Simulator::Stop(MilliSeconds(100));
        Simulator::Run();
        CollectTransportCounters();
        CollectQueueDiscCounters();
        const Summary summary = CalculateSummary();
        const bool outputOk = WriteOutputs(outputPrefix, summary);
        PrintSummary(summary, outputPrefix);
        Simulator::Destroy();
        return summary.completed == summary.expected && outputOk;
    }

  private:
    struct MessageRecord
    {
        uint64_t id{0};
        uint32_t source{0};
        uint32_t target{0};
        uint32_t bytes{0};
        int64_t scheduledNs{0};
        int64_t submittedNs{-1};
        int64_t completedNs{-1};
        int64_t latencyNs{-1};
        bool submitted{false};
        bool completed{false};
    };

    uint32_t CreateConfiguredConnection(uint32_t source,
                                        uint32_t target,
                                        uint32_t occurrence,
                                        uint32_t initialWindow = 0,
                                        uint64_t lineRateBps = 0)
    {
        AiTransportConnectionConfig config;
        config.remoteEndpointId = target;
        config.reliability = AiTransportReliability::RELIABLE_UNORDERED;
        config.initialWindowBytes = initialWindow;
        config.lineRateBps = lineRateBps;
        config.retransmissionTimeout = MicroSeconds(50 + source * 7 + occurrence * 3);
        return m_endpoints[source]->OpenConnection(config);
    }

    bool SubmitData(uint32_t source,
                    uint32_t target,
                    uint32_t connectionId,
                    uint64_t messageId,
                    uint32_t payloadBytes)
    {
        AiTransportRequest request;
        request.remoteEndpointId = target;
        request.connectionId = connectionId;
        request.messageId = messageId;
        request.operation = m_operation;
        request.reliability = AiTransportReliability::RELIABLE_UNORDERED;
        request.payload = Create<Packet>(payloadBytes);
        return m_endpoints[source]->Submit(request);
    }

    void SubmitWarmup(uint32_t source,
                      uint32_t target,
                      uint32_t pdcId,
                      uint64_t messageId,
                      uint32_t payloadBytes)
    {
        ++m_warmupsAttempted;
        if (SubmitData(source, target, pdcId, messageId, payloadBytes))
        {
            ++m_warmupsSubmitted;
        }
    }

    void CaptureCongestionWindow(uint32_t source, uint32_t pdcId)
    {
        m_measurementStartCwndBytes += m_endpoints[source]->GetCongestionWindow(pdcId);
        ++m_measurementStartCwndSamples;
    }

    struct Summary
    {
        uint32_t expected{0};
        uint32_t submitted{0};
        uint32_t completed{0};
        uint64_t completedBytes{0};
        int64_t firstSubmitNs{0};
        int64_t lastCompletionNs{0};
        int64_t makespanNs{0};
        double completionRate{0};
        double goodputBps{0};
        double meanLatencyNs{0};
        int64_t p50LatencyNs{0};
        int64_t p95LatencyNs{0};
        int64_t p99LatencyNs{0};
        int64_t maxLatencyNs{0};
    };

    struct PendingSubmission
    {
        uint32_t source{0};
        uint32_t target{0};
        uint32_t pdcId{0};
        uint64_t messageId{0};
        uint32_t payloadBytes{0};
    };

    struct RingCollectiveRecord
    {
        int64_t submittedNs{-1};
        int64_t completedNs{-1};
    };

    struct CwndSample
    {
        int64_t timeNs{0};
        uint32_t source{0};
        uint32_t pdcId{0};
        uint32_t oldBytes{0};
        uint32_t newBytes{0};
    };

    struct QueueSample
    {
        int64_t timeNs{0};
        uint32_t egressEndpoint{0};
        uint32_t oldBytes{0};
        uint32_t newBytes{0};
    };

    void SubmitMessage(uint32_t source,
                       uint32_t target,
                       uint32_t pdcId,
                       uint64_t messageId,
                       uint32_t payloadBytes)
    {
        auto& record = m_records.at(messageId);
        if (record.scheduledNs < 0)
        {
            record.scheduledNs = Simulator::Now().GetNanoSeconds();
        }
        record.submittedNs = Simulator::Now().GetNanoSeconds();
        record.submitted = SubmitData(source, target, pdcId, messageId, payloadBytes);
    }

    void StartRingStep(uint32_t collective, uint32_t step)
    {
        if (collective == 0 && step == 0 && m_workConservingScheduler)
        {
            for (uint32_t source = 0; source < m_ringPdcIds.size(); ++source)
            {
                m_endpoints[source]->SetCongestionWindow(m_ringPdcIds[source],
                                                         m_nsccInitialWindowBytes);
            }
        }
        m_ringCurrentCollective = collective;
        m_ringCurrentStep = step;
        m_ringStepCompletions = 0;
        if (step == 0)
        {
            m_ringCollectives[collective].submittedNs = Simulator::Now().GetNanoSeconds();
        }
        const auto& ids = m_ringMessageIds[collective][step];
        for (uint32_t source = 0; source < m_nodeCount; ++source)
        {
            auto& record = m_records.at(ids[source]);
            record.scheduledNs = Simulator::Now().GetNanoSeconds();
            SubmitMessage(source,
                          m_ringTargetBySource[source],
                          m_ringPdcIds[source],
                          ids[source],
                          m_ringChunkBytes);
        }
    }

    void RingMessageComplete(uint64_t messageId)
    {
        const auto location = m_ringMessageLocation.find(messageId);
        if (location == m_ringMessageLocation.end() ||
            location->second.first != m_ringCurrentCollective ||
            location->second.second != m_ringCurrentStep)
        {
            return;
        }
        ++m_ringStepCompletions;
        if (m_ringStepCompletions != m_nodeCount)
        {
            return;
        }
        if (m_ringCurrentStep + 1 < m_ringStepsPerCollective)
        {
            Simulator::ScheduleNow(&AiWorkload::StartRingStep,
                                   this,
                                   m_ringCurrentCollective,
                                   m_ringCurrentStep + 1);
        }
        else
        {
            m_ringCollectives[m_ringCurrentCollective].completedNs =
                Simulator::Now().GetNanoSeconds();
            if (m_ringCurrentCollective + 1 < m_ringCollectives.size())
            {
                Simulator::ScheduleNow(&AiWorkload::StartRingStep,
                                       this,
                                       m_ringCurrentCollective + 1,
                                       0);
            }
        }
    }

    void MessageComplete(uint32_t receiver,
                         uint32_t,
                         uint64_t messageId,
                         uint32_t bytes,
                         Time latency)
    {
        if (m_warmupIds.erase(messageId) > 0)
        {
            ++m_warmupsCompleted;
            return;
        }
        auto found = m_records.find(messageId);
        if (found == m_records.end() ||
            (m_operation == AiTransportOperation::READ ? found->second.source
                                                       : found->second.target) != receiver ||
            found->second.completed)
        {
            return;
        }
        found->second.completed = true;
        found->second.bytes = bytes;
        found->second.completedNs = Simulator::Now().GetNanoSeconds();
        found->second.latencyNs = latency.GetNanoSeconds();
        const auto group = m_messageGroups.find(messageId);
        if (group != m_messageGroups.end() && group->second >= 1001 &&
            m_backgroundMessagesRemaining > 0)
        {
            --m_backgroundMessagesRemaining;
            if (m_backgroundMessagesRemaining == 0 && m_ringPdcLineRateBps > 0)
            {
                for (uint32_t source = 0; source < m_ringPdcIds.size(); ++source)
                {
                    m_endpoints[source]->SetConnectionRate(m_ringPdcIds[source], m_linkRateBps);
                }
                m_jobRateReleasedNs = Simulator::Now().GetNanoSeconds();
            }
        }
        if (m_ringMessageLocation.contains(messageId))
        {
            RingMessageComplete(messageId);
            return;
        }
        auto next = m_chainedSubmissions.find(messageId);
        if (next != m_chainedSubmissions.end())
        {
            const auto submission = next->second;
            m_chainedSubmissions.erase(next);
            Simulator::ScheduleNow(&AiWorkload::SubmitMessage,
                                   this,
                                   submission.source,
                                   submission.target,
                                   submission.pdcId,
                                   submission.messageId,
                                   submission.payloadBytes);
        }
    }

    static void MessageCompleteSink(AiWorkload* workload,
                                    uint32_t receiver,
                                    uint32_t pdcId,
                                    uint64_t messageId,
                                    uint32_t bytes,
                                    Time latency)
    {
        workload->MessageComplete(receiver, pdcId, messageId, bytes, latency);
    }

    void Retransmission(uint32_t, uint32_t)
    {
        ++m_retransmissions;
    }

    void Timeout(uint32_t, uint32_t)
    {
        ++m_timeouts;
    }

    void Nack(uint32_t, uint32_t)
    {
        ++m_nacks;
    }

    void Ecn(uint32_t, uint32_t)
    {
        ++m_ecnMarks;
    }

    void Trimmed(Ptr<const Packet>, uint32_t, uint32_t)
    {
        ++m_trimmedPackets;
    }

    static void CongestionWindowSink(AiWorkload* workload,
                                     uint32_t source,
                                     uint32_t pdcId,
                                     uint32_t oldBytes,
                                     uint32_t newBytes)
    {
        workload->m_cwndSamples.push_back(
            {Simulator::Now().GetNanoSeconds(), source, pdcId, oldBytes, newBytes});
    }

    static void QueueBytesChanged(AiWorkload* workload,
                                  uint32_t egressEndpoint,
                                  uint32_t oldBytes,
                                  uint32_t newBytes)
    {
        workload->m_peakQueueBytes = std::max(workload->m_peakQueueBytes, newBytes);
        workload->m_peakQueueBytesByEgress[egressEndpoint] =
            std::max(workload->m_peakQueueBytesByEgress[egressEndpoint], newBytes);
        workload->m_queueSamples.push_back(
            {Simulator::Now().GetNanoSeconds(), egressEndpoint, oldBytes, newBytes});
    }

    static void ReceiverPayloadSink(AiWorkload* workload,
                                    uint32_t sourceEndpointId,
                                    uint32_t payloadBytes)
    {
        const int64_t nowNs = Simulator::Now().GetNanoSeconds();
        const int64_t binNs = (nowNs / workload->m_throughputBinNs) * workload->m_throughputBinNs;
        workload->m_rxPayloadBins[sourceEndpointId][binNs] += payloadBytes;
    }

    void DeviceQueueDrop(Ptr<const Packet>)
    {
        ++m_deviceQueueDrops;
    }

    void CollectTransportCounters()
    {
        for (const auto& endpoint : m_endpoints)
        {
            const auto counters = endpoint->GetCounters();
            m_txDatagrams += counters.transmittedDatagrams;
            m_rxDatagrams += counters.receivedDatagrams;
            m_mtuDrops += counters.mtuDrops;
            m_crcDrops += counters.integrityDrops;
            m_selectiveAcknowledgments += counters.selectiveAcknowledgments;
            m_fastRetransmissions += counters.fastRetransmissions;
            m_rttProbes += counters.rttProbes;
            m_slowPathSignals += counters.slowPathSignals;
        }
    }

    void CollectQueueDiscCounters()
    {
        for (const auto& queueDisc : m_bottleneckQueueDiscs)
        {
            const auto stats = queueDisc->GetStats();
            m_queueMarkedPackets += stats.nTotalMarkedPackets;
            m_queueMarkedBytes += stats.nTotalMarkedBytes;
            m_queueDroppedPackets += stats.nTotalDroppedPackets;
            m_queueDroppedBytes += stats.nTotalDroppedBytes;
        }
    }

    static int64_t Percentile(const std::vector<int64_t>& sorted, double percentile)
    {
        if (sorted.empty())
        {
            return 0;
        }
        const std::size_t index =
            static_cast<std::size_t>(std::ceil(percentile * static_cast<double>(sorted.size()))) -
            1;
        return sorted[std::min(index, sorted.size() - 1)];
    }

    Summary CalculateSummary() const
    {
        Summary summary;
        summary.expected = static_cast<uint32_t>(m_records.size());
        std::vector<int64_t> latencies;
        bool haveFirstSubmit = false;
        for (const auto& [id, record] : m_records)
        {
            (void)id;
            if (record.submitted)
            {
                ++summary.submitted;
                summary.firstSubmitNs = haveFirstSubmit
                                            ? std::min(summary.firstSubmitNs, record.submittedNs)
                                            : record.submittedNs;
                haveFirstSubmit = true;
            }
            if (record.completed)
            {
                ++summary.completed;
                summary.completedBytes += record.bytes;
                summary.lastCompletionNs = std::max(summary.lastCompletionNs, record.completedNs);
                latencies.push_back(record.latencyNs);
            }
        }
        summary.completionRate =
            summary.expected == 0 ? 0.0 : static_cast<double>(summary.completed) / summary.expected;
        summary.makespanNs =
            summary.completed == 0 ? 0 : summary.lastCompletionNs - summary.firstSubmitNs;
        if (summary.makespanNs > 0)
        {
            summary.goodputBps = static_cast<double>(summary.completedBytes) * 8.0 * 1e9 /
                                 static_cast<double>(summary.makespanNs);
        }
        if (!latencies.empty())
        {
            std::sort(latencies.begin(), latencies.end());
            summary.meanLatencyNs =
                static_cast<double>(
                    std::accumulate(latencies.begin(), latencies.end(), int64_t{0})) /
                latencies.size();
            summary.p50LatencyNs = Percentile(latencies, 0.50);
            summary.p95LatencyNs = Percentile(latencies, 0.95);
            summary.p99LatencyNs = Percentile(latencies, 0.99);
            summary.maxLatencyNs = latencies.back();
        }
        return summary;
    }

    bool WriteOutputs(const std::string& outputPrefix, const Summary& summary) const
    {
        const std::string base = outputPrefix + "-" + m_pattern;
        std::ofstream messages(base + "-messages.csv");
        std::ofstream summaries(base + "-summary.csv");
        std::ofstream json(base + "-summary.json");
        std::ofstream cwnd(base + "-cwnd.csv");
        std::ofstream queue(base + "-queue.csv");
        std::ofstream throughput(base + "-throughput.csv");
        std::ofstream collectives(base + "-collectives.csv");
        if (!messages || !summaries || !json || !cwnd || !queue || !throughput || !collectives)
        {
            std::cerr << "Unable to create structured output with prefix " << base << std::endl;
            return false;
        }

        cwnd << "time_ns,source,pdc_id,old_cwnd_bytes,new_cwnd_bytes\n";
        for (const auto& sample : m_cwndSamples)
        {
            cwnd << sample.timeNs << ',' << sample.source << ',' << sample.pdcId << ','
                 << sample.oldBytes << ',' << sample.newBytes << '\n';
        }
        queue << "time_ns,egress_endpoint,old_queue_bytes,new_queue_bytes\n";
        for (const auto& sample : m_queueSamples)
        {
            queue << sample.timeNs << ',' << sample.egressEndpoint << ',' << sample.oldBytes << ','
                  << sample.newBytes << '\n';
        }
        throughput << "bin_start_ns,source,payload_bytes,goodput_bps\n";
        for (const auto& [source, bins] : m_rxPayloadBins)
        {
            for (const auto& [binStartNs, bytes] : bins)
            {
                const double goodputBps =
                    static_cast<double>(bytes) * 8.0 * 1e9 / static_cast<double>(m_throughputBinNs);
                throughput << binStartNs << ',' << source << ',' << bytes << ','
                           << std::setprecision(12) << goodputBps << '\n';
            }
        }

        messages << "protocol,operation,pattern,node_count,group_id,message_id,source,target,bytes,"
                    "scheduled_"
                    "ns,submitted,"
                    "submitted_ns,completed,completed_ns,latency_ns\n";
        for (const auto& [id, record] : m_records)
        {
            const auto group = m_messageGroups.find(id);
            const uint32_t groupId = group == m_messageGroups.end() ? 0 : group->second;
            messages << m_transportName << ',' << m_operationName << ',' << m_pattern << ','
                     << m_nodeCount << ',' << groupId << ',' << id << ',' << record.source << ','
                     << record.target << ',' << record.bytes << ',' << record.scheduledNs << ','
                     << (record.submitted ? 1 : 0) << ',' << record.submittedNs << ','
                     << (record.completed ? 1 : 0) << ',' << record.completedNs << ','
                     << record.latencyNs << '\n';
        }

        collectives << "protocol,pattern,collective_id,tensor_bytes,chunk_bytes,steps,submitted_ns,"
                       "completed_ns,latency_ns\n";
        for (uint32_t i = 0; i < m_ringCollectives.size(); ++i)
        {
            const auto& collective = m_ringCollectives[i];
            const int64_t latency = collective.completedNs < 0 || collective.submittedNs < 0
                                        ? -1
                                        : collective.completedNs - collective.submittedNs;
            collectives << m_transportName << ',' << m_pattern << ',' << i + 1 << ','
                        << m_payloadBytes << ',' << m_ringChunkBytes << ','
                        << m_ringStepsPerCollective << ',' << collective.submittedNs << ','
                        << collective.completedNs << ',' << latency << '\n';
        }

        const std::string header =
            "protocol,operation,pattern,fabric,link_rate,link_rate_bps,link_delay_ns,queue_packets,"
            "node_"
            "count,"
            "messages_per_pair,payload_bytes,reuse_pdc,warmup_bytes,warmups_attempted,"
            "warmups_submitted,warmups_completed,measurement_start_us,start_gap_ns,background_"
            "start_offset_ns,"
            "nscc_base_rtt_ns,nscc_target_queue_delay_ns,nscc_initial_window_bytes,"
            "nscc_requested_initial_window_bytes,auto_scale_initial_window,measurement_start_cwnd_"
            "bytes,"
            "ecn_enabled,trimming_enabled,ecn_min_bytes,ecn_max_bytes,ecn_queue_limit_bytes,peak_"
            "queue_bytes,"
            "queue_marked_packets,queue_marked_bytes,queue_dropped_packets,queue_dropped_bytes,"
            "expected,submitted,completed,"
            "completion_rate,completed_payload_bytes,first_submit_ns,last_completion_ns,makespan_"
            "ns,"
            "goodput_bps,mean_latency_ns,p50_latency_ns,p95_latency_ns,p99_latency_ns,"
            "max_latency_ns,retransmissions,timeouts,nacks,ecn_marks,trimmed_packets,sack_packets,"
            "fast_retransmissions,rtt_probes,slow_path_signals,tx_datagrams,"
            "rx_datagrams,mtu_drops,crc_drops,device_queue_drops\n";
        summaries << header << m_transportName << ',' << m_operationName << ',' << m_pattern << ','
                  << m_fabricType << ',' << m_linkRate << ',' << m_linkRateBps << ','
                  << m_linkDelayNs << ',' << m_queuePackets << ',' << m_nodeCount << ','
                  << m_messagesPerPair << ',' << m_payloadBytes << ',' << (m_reusePdc ? 1 : 0)
                  << ',' << m_warmupBytes << ',' << m_warmupsAttempted << ',' << m_warmupsSubmitted
                  << ',' << m_warmupsCompleted << ',' << m_measurementStartUs << ',' << m_startGapNs
                  << ',' << m_backgroundStartOffsetNs << ',' << m_nsccBaseRttNs << ','
                  << m_nsccTargetQueueDelayNs << ',' << m_nsccInitialWindowBytes << ','
                  << m_nsccRequestedInitialWindowBytes << ',' << (m_autoScaleInitialWindow ? 1 : 0)
                  << ',' << MeanMeasurementStartCwnd() << ',' << (m_enableEcn ? 1 : 0) << ','
                  << (m_enableTrimming ? 1 : 0) << ',' << m_ecnMinBytes << ',' << m_ecnMaxBytes
                  << ',' << m_ecnQueueLimitBytes << ',' << m_peakQueueBytes << ','
                  << m_queueMarkedPackets << ',' << m_queueMarkedBytes << ','
                  << m_queueDroppedPackets << ',' << m_queueDroppedBytes << ',' << summary.expected
                  << ',' << summary.submitted << ',' << summary.completed << ','
                  << std::setprecision(10) << summary.completionRate << ','
                  << summary.completedBytes << ',' << summary.firstSubmitNs << ','
                  << summary.lastCompletionNs << ',' << summary.makespanNs << ','
                  << summary.goodputBps << ',' << summary.meanLatencyNs << ','
                  << summary.p50LatencyNs << ',' << summary.p95LatencyNs << ','
                  << summary.p99LatencyNs << ',' << summary.maxLatencyNs << ',' << m_retransmissions
                  << ',' << m_timeouts << ',' << m_nacks << ',' << m_ecnMarks << ','
                  << m_trimmedPackets << ',' << m_selectiveAcknowledgments << ','
                  << m_fastRetransmissions << ',' << m_rttProbes << ',' << m_slowPathSignals << ','
                  << m_txDatagrams << ',' << m_rxDatagrams << ',' << m_mtuDrops << ',' << m_crcDrops
                  << ',' << m_deviceQueueDrops << '\n';

        json << std::fixed << std::setprecision(3) << "{\n"
             << "  \"schema_version\": 4,\n"
             << "  \"protocol\": \"" << m_transportName << "\",\n"
             << "  \"operation\": \"" << m_operationName << "\",\n"
             << "  \"pattern\": \"" << m_pattern << "\",\n"
             << "  \"fabric\": \"" << m_fabricType << "\",\n"
             << "  \"link_rate\": \"" << m_linkRate << "\",\n"
             << "  \"link_rate_bps\": " << m_linkRateBps << ",\n"
             << "  \"link_delay_ns\": " << m_linkDelayNs << ",\n"
             << "  \"queue_packets\": " << m_queuePackets << ",\n"
             << "  \"node_count\": " << m_nodeCount << ",\n"
             << "  \"messages_per_pair\": " << m_messagesPerPair << ",\n"
             << "  \"payload_bytes\": " << m_payloadBytes << ",\n"
             << "  \"ring_allreduce\": {\"collectives\": " << m_ringCollectives.size()
             << ", \"chunk_bytes\": " << m_ringChunkBytes
             << ", \"steps_per_collective\": " << m_ringStepsPerCollective
             << ", \"interleaved\": " << (m_ringInterleaved ? "true" : "false") << "},\n"
             << "  \"background_all_to_all\": {\"groups\": " << m_backgroundAllToAllGroups
             << ", \"payload_bytes\": " << m_backgroundPayloadBytes
             << ", \"pdc_initial_window_bytes\": " << m_backgroundPdcInitialWindowBytes
             << ", \"start_offset_ns\": " << m_backgroundStartOffsetNs << "},\n"
             << "  \"job_rate_scheduler\": {\"enabled\": "
             << ((m_ringPdcLineRateBps > 0 || m_workConservingScheduler) ? "true" : "false")
             << ", \"work_conserving\": " << (m_workConservingScheduler ? "true" : "false")
             << ", \"ring_weight\": " << m_ringJobWeight
             << ", \"background_weight\": " << m_backgroundJobWeight
             << ", \"ring_pdc_rate_bps\": " << m_ringPdcLineRateBps
             << ", \"background_pdc_rate_bps\": " << m_backgroundPdcLineRateBps
             << ", \"released_to_line_rate_ns\": " << m_jobRateReleasedNs << "},\n"
             << "  \"steady_state\": {\"reuse_pdc\": " << (m_reusePdc ? "true" : "false")
             << ", \"warmup_bytes\": " << m_warmupBytes
             << ", \"warmups_attempted\": " << m_warmupsAttempted
             << ", \"warmups_submitted\": " << m_warmupsSubmitted
             << ", \"warmups_completed\": " << m_warmupsCompleted
             << ", \"measurement_start_us\": " << m_measurementStartUs
             << ", \"start_gap_ns\": " << m_startGapNs
             << ", \"measurement_start_cwnd_bytes\": " << MeanMeasurementStartCwnd() << "},\n"
             << "  \"nscc\": {\"base_rtt_ns\": " << m_nsccBaseRttNs
             << ", \"target_queue_delay_ns\": " << m_nsccTargetQueueDelayNs
             << ", \"initial_window_bytes\": " << m_nsccInitialWindowBytes
             << ", \"requested_initial_window_bytes\": " << m_nsccRequestedInitialWindowBytes
             << ", \"auto_scaled_initial_window\": "
             << (m_autoScaleInitialWindow ? "true" : "false") << "},\n"
             << "  \"ecn_queue\": {\"enabled\": " << (m_enableEcn ? "true" : "false")
             << ", \"trimming_enabled\": " << (m_enableTrimming ? "true" : "false")
             << ", \"min_bytes\": " << m_ecnMinBytes << ", \"max_bytes\": " << m_ecnMaxBytes
             << ", \"limit_bytes\": " << m_ecnQueueLimitBytes
             << ", \"peak_bytes\": " << m_peakQueueBytes
             << ", \"marked_packets\": " << m_queueMarkedPackets
             << ", \"marked_bytes\": " << m_queueMarkedBytes
             << ", \"dropped_packets\": " << m_queueDroppedPackets
             << ", \"dropped_bytes\": " << m_queueDroppedBytes << "},\n"
             << "  \"messages\": {\"expected\": " << summary.expected
             << ", \"submitted\": " << summary.submitted << ", \"completed\": " << summary.completed
             << ", \"completion_rate\": " << summary.completionRate << "},\n"
             << "  \"timing_ns\": {\"first_submit\": " << summary.firstSubmitNs
             << ", \"last_completion\": " << summary.lastCompletionNs
             << ", \"makespan\": " << summary.makespanNs << "},\n"
             << "  \"latency_ns\": {\"mean\": " << summary.meanLatencyNs
             << ", \"p50\": " << summary.p50LatencyNs << ", \"p95\": " << summary.p95LatencyNs
             << ", \"p99\": " << summary.p99LatencyNs << ", \"max\": " << summary.maxLatencyNs
             << "},\n"
             << "  \"completed_payload_bytes\": " << summary.completedBytes << ",\n"
             << "  \"goodput_bps\": " << summary.goodputBps << ",\n"
             << "  \"protocol_events\": {\"retransmissions\": " << m_retransmissions
             << ", \"timeouts\": " << m_timeouts << ", \"nacks\": " << m_nacks
             << ", \"ecn_marks\": " << m_ecnMarks << ", \"trimmed_packets\": " << m_trimmedPackets
             << ", \"sack_packets\": " << m_selectiveAcknowledgments
             << ", \"fast_retransmissions\": " << m_fastRetransmissions
             << ", \"rtt_probes\": " << m_rttProbes
             << ", \"slow_path_signals\": " << m_slowPathSignals << "},\n"
             << "  \"transport\": {\"tx_datagrams\": " << m_txDatagrams
             << ", \"rx_datagrams\": " << m_rxDatagrams << ", \"mtu_drops\": " << m_mtuDrops
             << ", \"crc_drops\": " << m_crcDrops << "},\n"
             << "  \"device_queue_drops\": " << m_deviceQueueDrops << "\n"
             << "}\n";
        return true;
    }

    void PrintSummary(const Summary& summary, const std::string& outputPrefix) const
    {
        std::cout << std::fixed << std::setprecision(3) << m_transportName << '/' << m_pattern
                  << ": completed=" << summary.completed << '/' << summary.expected
                  << " completion=" << summary.completionRate * 100.0 << "%"
                  << " goodput=" << summary.goodputBps / 1e9 << " Gbps"
                  << " latency_us(mean/p50/p95/p99/max)=" << summary.meanLatencyNs / 1e3 << '/'
                  << summary.p50LatencyNs / 1e3 << '/' << summary.p95LatencyNs / 1e3 << '/'
                  << summary.p99LatencyNs / 1e3 << '/' << summary.maxLatencyNs / 1e3
                  << " retransmissions=" << m_retransmissions << " timeouts=" << m_timeouts
                  << " nacks=" << m_nacks << std::endl;
        if (m_reusePdc)
        {
            std::cout << "steady-state: warmups=" << m_warmupsCompleted << '/' << m_warmupsSubmitted
                      << " measurement_start_cwnd=" << MeanMeasurementStartCwnd() << " bytes"
                      << std::endl;
        }
        std::cout << "outputs: " << outputPrefix << '-' << m_pattern
                  << "-{messages.csv,collectives.csv,summary.csv,summary.json,cwnd.csv,queue.csv,"
                     "throughput.csv}"
                  << std::endl;
    }

    uint32_t m_nodeCount{0};
    uint32_t m_messagesPerPair{0};
    uint32_t m_payloadBytes{0};
    std::string m_transportName{"uec"};
    std::string m_operationName{"message"};
    AiTransportOperation m_operation{AiTransportOperation::MESSAGE};
    std::string m_pattern;
    std::string m_fabricType;
    std::string m_linkRate;
    uint64_t m_linkRateBps{0};
    uint32_t m_linkDelayNs{0};
    uint32_t m_queuePackets{0};
    bool m_reusePdc{false};
    uint32_t m_warmupBytes{0};
    uint32_t m_measurementStartUs{0};
    uint64_t m_startGapNs{10000};
    uint32_t m_nsccBaseRttNs{12000};
    uint32_t m_nsccTargetQueueDelayNs{12000};
    uint32_t m_nsccInitialWindowBytes{65536};
    uint32_t m_nsccRequestedInitialWindowBytes{65536};
    bool m_autoScaleInitialWindow{false};
    uint32_t m_backgroundAllToAllGroups{0};
    uint32_t m_backgroundPayloadBytes{0};
    uint64_t m_backgroundStartOffsetNs{0};
    uint32_t m_backgroundPdcInitialWindowBytes{65536};
    uint32_t m_ringJobWeight{0};
    uint32_t m_backgroundJobWeight{0};
    uint64_t m_ringPdcLineRateBps{0};
    uint64_t m_backgroundPdcLineRateBps{0};
    bool m_workConservingScheduler{false};
    uint32_t m_backgroundMessagesRemaining{0};
    int64_t m_jobRateReleasedNs{-1};
    bool m_enableEcn{false};
    bool m_enableTrimming{false};
    uint32_t m_ecnMinBytes{204800};
    uint32_t m_ecnMaxBytes{409600};
    uint32_t m_ecnQueueLimitBytes{2097152};
    std::vector<Ptr<QueueDisc>> m_bottleneckQueueDiscs;
    std::vector<Ptr<AiTransportEndpoint>> m_endpoints;
    std::map<uint64_t, MessageRecord> m_records;
    std::map<uint64_t, uint32_t> m_messageGroups;
    std::map<uint64_t, PendingSubmission> m_chainedSubmissions;
    std::vector<uint32_t> m_ringPdcIds;
    std::vector<uint32_t> m_ringTargetBySource;
    std::vector<std::vector<std::vector<uint64_t>>> m_ringMessageIds;
    std::map<uint64_t, std::pair<uint32_t, uint32_t>> m_ringMessageLocation;
    std::vector<RingCollectiveRecord> m_ringCollectives;
    uint32_t m_ringChunkBytes{0};
    uint32_t m_ringStepsPerCollective{0};
    uint32_t m_ringCurrentCollective{0};
    uint32_t m_ringCurrentStep{0};
    uint32_t m_ringStepCompletions{0};
    bool m_ringInterleaved{false};
    std::set<uint64_t> m_warmupIds;
    std::vector<CwndSample> m_cwndSamples;
    std::vector<QueueSample> m_queueSamples;
    int64_t m_throughputBinNs{1000};
    std::map<uint32_t, std::map<int64_t, uint64_t>> m_rxPayloadBins;
    uint32_t m_warmupsAttempted{0};
    uint32_t m_warmupsSubmitted{0};
    uint32_t m_warmupsCompleted{0};
    uint64_t m_measurementStartCwndBytes{0};
    uint32_t m_measurementStartCwndSamples{0};
    uint64_t m_retransmissions{0};
    uint64_t m_timeouts{0};
    uint64_t m_nacks{0};
    uint64_t m_ecnMarks{0};
    uint64_t m_trimmedPackets{0};
    uint64_t m_selectiveAcknowledgments{0};
    uint64_t m_fastRetransmissions{0};
    uint64_t m_rttProbes{0};
    uint64_t m_slowPathSignals{0};
    uint64_t m_txDatagrams{0};
    uint64_t m_rxDatagrams{0};
    uint64_t m_mtuDrops{0};
    uint64_t m_crcDrops{0};
    uint64_t m_deviceQueueDrops{0};
    uint32_t m_peakQueueBytes{0};
    std::map<uint32_t, uint32_t> m_peakQueueBytesByEgress;
    uint32_t m_queueMarkedPackets{0};
    uint64_t m_queueMarkedBytes{0};
    uint32_t m_queueDroppedPackets{0};
    uint64_t m_queueDroppedBytes{0};

    uint64_t MeanMeasurementStartCwnd() const
    {
        return m_measurementStartCwndSamples == 0
                   ? 0
                   : m_measurementStartCwndBytes / m_measurementStartCwndSamples;
    }
};

int
main(int argc, char* argv[])
{
    uint32_t nodes = 4;
    uint32_t messages = 4;
    uint32_t payloadBytes = 4096;
    std::string transport = "uec";
    std::string operation = "message";
    std::string pattern = "incast";
    bool ringInterleaved = false;
    std::string fabric = "switched";
    std::string linkRate = "400Gbps";
    uint32_t linkDelayNs = 1000;
    uint32_t queuePackets = 10000;
    bool reusePdc = false;
    uint32_t warmupBytes = 0;
    uint32_t measurementStartUs = 0;
    uint64_t startGapNs = 10000;
    uint32_t nsccBaseRttNs = 12000;
    uint32_t nsccTargetQueueDelayNs = 12000;
    uint32_t nsccInitialWindowBytes = 65536;
    bool autoScaleInitialWindow = false;
    uint32_t backgroundAllToAllGroups = 0;
    uint32_t backgroundPayloadBytes = 1048576;
    uint64_t backgroundStartOffsetNs = 0;
    uint32_t ringJobWeight = 0;
    uint32_t backgroundJobWeight = 0;
    bool enableWorkConservingScheduler = false;
    bool enableEcn = false;
    bool enableTrimming = false;
    uint32_t ecnMinBytes = 204800;
    uint32_t ecnMaxBytes = 409600;
    uint32_t ecnQueueLimitBytes = 2097152;
    std::string outputPrefix = "uet-ai-workload";
    CommandLine command(__FILE__);
    command.AddValue("nodes", "Number of endpoints (at least two)", nodes);
    command.AddValue("messages", "Messages per pair, or collectives for ring-allreduce", messages);
    command.AddValue("payloadBytes",
                     "Bytes per message, or tensor bytes per AllReduce rank",
                     payloadBytes);
    command.AddValue("transport", "uec, veroce, mrc, falcon, metaroce, or rocev2", transport);
    command.AddValue("operation", "message, send, write, or read", operation);
    command.AddValue("pattern", "single, incast, all-to-all, or ring-allreduce", pattern);
    command.AddValue("ringInterleaved",
                     "Alternate first-half and second-half ranks in the AllReduce ring",
                     ringInterleaved);
    command.AddValue("fabric", "switched star, two-leaf leaf-spine, or shared csma", fabric);
    command.AddValue("linkRate", "Endpoint and fabric link data rate", linkRate);
    command.AddValue("linkDelayNs", "One-way propagation delay per link in ns", linkDelayNs);
    command.AddValue("queuePackets", "DropTail capacity per device in packets", queuePackets);
    command.AddValue("reusePdc", "Reuse one RUD PDC per communicating pair", reusePdc);
    command.AddValue("warmupBytes", "Unmeasured warm-up message bytes per reused PDC", warmupBytes);
    command.AddValue("measurementStartUs", "Measured traffic start time in us", measurementStartUs);
    command.AddValue("startGapNs", "Gap between measured message submissions in ns", startGapNs);
    command.AddValue("nsccBaseRttNs", "NSCC base RTT estimate in ns", nsccBaseRttNs);
    command.AddValue("nsccTargetQueueDelayNs",
                     "NSCC target queue delay in ns",
                     nsccTargetQueueDelayNs);
    command.AddValue("nsccInitialWindowBytes",
                     "Initial NSCC congestion window per PDC in bytes",
                     nsccInitialWindowBytes);
    command.AddValue("autoScaleInitialWindow",
                     "Divide the initial PDC window by concurrent All-to-All groups",
                     autoScaleInitialWindow);
    command.AddValue("backgroundAllToAllGroups",
                     "Concurrent All-to-All groups added to a Ring AllReduce workload",
                     backgroundAllToAllGroups);
    command.AddValue("backgroundPayloadBytes",
                     "Message bytes per pair in each background All-to-All group",
                     backgroundPayloadBytes);
    command.AddValue("backgroundStartOffsetNs",
                     "Delay background All-to-All submission after measured Ring start in ns",
                     backgroundStartOffsetNs);
    command.AddValue("ringJobWeight",
                     "Ring job weight; zero disables job-level rate scheduling",
                     ringJobWeight);
    command.AddValue("backgroundJobWeight",
                     "Weight of each background All-to-All job",
                     backgroundJobWeight);
    command.AddValue("enableWorkConservingScheduler",
                     "Use endpoint weighted round-robin across job queues",
                     enableWorkConservingScheduler);
    command.AddValue("enableEcn",
                     "Install RED/ECN on the switched receiver-facing link",
                     enableEcn);
    command.AddValue("enableTrimming",
                     "Install the veRoCE ECN/packet-trimming queue discipline",
                     enableTrimming);
    command.AddValue("ecnMinBytes", "RED minimum ECN threshold in bytes", ecnMinBytes);
    command.AddValue("ecnMaxBytes", "RED maximum ECN threshold in bytes", ecnMaxBytes);
    command.AddValue("ecnQueueLimitBytes", "RED hard queue limit in bytes", ecnQueueLimitBytes);
    command.AddValue("outputPrefix",
                     "Prefix for per-message CSV, summary CSV, and JSON",
                     outputPrefix);
    command.Parse(argc, argv);
    if (nodes < 2 || nodes > 250 || messages == 0 ||
        ParseAiTransportProtocol(transport) == AiTransportProtocol::UNKNOWN ||
        (operation != "message" && operation != "send" && operation != "write" &&
         operation != "read") ||
        (operation == "read" &&
         ParseAiTransportProtocol(transport) != AiTransportProtocol::VEROCE) ||
        (pattern != "single" && pattern != "incast" && pattern != "all-to-all" &&
         pattern != "ring-allreduce") ||
        (pattern == "ring-allreduce" && payloadBytes % nodes != 0) ||
        (ringInterleaved && (pattern != "ring-allreduce" || nodes % 2 != 0)) ||
        (backgroundAllToAllGroups > 0 &&
         (pattern != "ring-allreduce" || backgroundPayloadBytes == 0)) ||
        (backgroundStartOffsetNs > 0 && backgroundAllToAllGroups == 0) ||
        ((ringJobWeight == 0) != (backgroundJobWeight == 0)) ||
        (ringJobWeight > 0 && backgroundAllToAllGroups == 0) ||
        (enableWorkConservingScheduler && ringJobWeight == 0) ||
        (fabric != "switched" && fabric != "leaf-spine" && fabric != "csma") ||
        (fabric == "leaf-spine" && (nodes < 4 || nodes % 2 != 0)) || linkDelayNs == 0 ||
        queuePackets == 0 || nsccBaseRttNs < 128 || nsccTargetQueueDelayNs < 128 ||
        nsccInitialWindowBytes == 0 ||
        ((enableEcn || enableTrimming) &&
         (fabric == "csma" || ecnMinBytes == 0 || ecnMinBytes >= ecnMaxBytes ||
          ecnMaxBytes >= ecnQueueLimitBytes)) ||
        (enableTrimming && ParseAiTransportProtocol(transport) != AiTransportProtocol::VEROCE) ||
        (warmupBytes > 0 && (!reusePdc || measurementStartUs == 0)))
    {
        return 2;
    }
    AiWorkload workload;
    const uint64_t linkRateBps = DataRate(linkRate).GetBitRate();
    return workload.Run(nodes,
                        messages,
                        payloadBytes,
                        transport,
                        operation,
                        pattern,
                        ringInterleaved,
                        fabric,
                        linkRate,
                        linkRateBps,
                        linkDelayNs,
                        queuePackets,
                        reusePdc,
                        warmupBytes,
                        measurementStartUs,
                        startGapNs,
                        nsccBaseRttNs,
                        nsccTargetQueueDelayNs,
                        nsccInitialWindowBytes,
                        autoScaleInitialWindow,
                        backgroundAllToAllGroups,
                        backgroundPayloadBytes,
                        backgroundStartOffsetNs,
                        ringJobWeight,
                        backgroundJobWeight,
                        enableWorkConservingScheduler,
                        enableEcn,
                        enableTrimming,
                        ecnMinBytes,
                        ecnMaxBytes,
                        ecnQueueLimitBytes,
                        outputPrefix)
               ? 0
               : 1;
}
