/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mrc-header.h"
#include "ns3/mrc-transport-adapter.h"
#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/roce-v2-header.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"

using namespace ns3;

class MrcReliabilityHeaderTestCase : public TestCase
{
  public:
    MrcReliabilityHeaderTestCase()
        : TestCase("MRC SETH, CC_STATE, NETH and PETH preserve their wire fields")
    {
    }

  private:
    void DoRun() override
    {
        MrcImmediateHeader immediate;
        immediate.SetImmediateData(0xdeadbeef);
        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(immediate);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), 4, "Immediate Data wire size changed");
        MrcImmediateHeader decodedImmediate;
        packet->RemoveHeader(decodedImmediate);
        NS_TEST_EXPECT_MSG_EQ(decodedImmediate.GetImmediateData(),
                              0xdeadbeef,
                              "Immediate Data changed");

        RoceAethHeader invalidRequest;
        invalidRequest.SetSyndrome(RoceAethHeader::INVALID_REQUEST_NAK_SYNDROME);
        packet = Create<Packet>();
        packet->AddHeader(invalidRequest);
        RoceAethHeader decodedAeth;
        packet->RemoveHeader(decodedAeth);
        NS_TEST_EXPECT_MSG_EQ(decodedAeth.GetSyndrome(),
                              RoceAethHeader::INVALID_REQUEST_NAK_SYNDROME,
                              "Invalid-request transport NAK syndrome changed");

        MrcErthHeader endpointRequest;
        endpointRequest.SetOperation(MrcEndpointOperation::PORT_STATUS_UPDATE);
        endpointRequest.SetPortStatusMask(0xa5a55a5a);
        endpointRequest.SetTimestamp(0x1234);
        packet = Create<Packet>();
        packet->AddHeader(endpointRequest);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), 16, "ERTH wire size changed");
        MrcErthHeader decodedRequest;
        packet->RemoveHeader(decodedRequest);
        NS_TEST_EXPECT_MSG_EQ(decodedRequest.GetPortStatusMask(),
                              0xa5a55a5a,
                              "ERTH port mask changed");
        NS_TEST_EXPECT_MSG_EQ(decodedRequest.GetTimestamp(), 0x1234, "ERTH timestamp changed");

        MrcEethHeader endpointResponse;
        endpointResponse.SetOperation(MrcEndpointOperation::EV_PROBE);
        endpointResponse.SetTimestamp(0x5678);
        packet = Create<Packet>();
        packet->AddHeader(endpointResponse);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), 36, "EETH wire size changed");
        MrcEethHeader decodedResponse;
        packet->RemoveHeader(decodedResponse);
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(decodedResponse.GetOperation()),
                              static_cast<uint8_t>(MrcEndpointOperation::EV_PROBE),
                              "EETH operation changed");
        NS_TEST_EXPECT_MSG_EQ(decodedResponse.GetTimestamp(), 0x5678, "EETH timestamp changed");

        MrcCcStateHeader cc;
        cc.SetTimestamp(0x1234);
        cc.SetOutOfOrderCount(0x2345);
        cc.SetRestoreWindow(true);
        cc.SetReceiverWindowPenalty(0x45);
        cc.SetReceivedBytesUnits(0xabcdef);
        MrcSethHeader sack;
        sack.SetCongestionMark(2);
        sack.SetProbeResponse(true);
        sack.SetAcknowledgedPsnOffset(-17);
        sack.SetEntropy(0x12345678);
        sack.SetSourcePdcId(0x1122);
        sack.SetDestinationPdcId(0x3344);
        sack.SetCumulativeAck(0xabcdef);
        sack.SetCcType(0);
        sack.SetMaximumPsnRange(32);
        sack.SetSackOffset(7);
        sack.SetBitmap(0x8040201008040201ULL);
        packet = Create<Packet>();
        packet->AddHeader(cc);
        packet->AddHeader(sack);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), 36, "SETH plus CC_STATE wire size changed");
        MrcSethHeader decodedSack;
        MrcCcStateHeader decodedCc;
        packet->RemoveHeader(decodedSack);
        packet->RemoveHeader(decodedCc);
        NS_TEST_EXPECT_MSG_EQ(decodedSack.GetCongestionMark(), 2, "SETH M changed");
        NS_TEST_EXPECT_MSG_EQ(decodedSack.IsProbeResponse(), true, "SETH PR changed");
        NS_TEST_EXPECT_MSG_EQ(decodedSack.GetAcknowledgedPsnOffset(), -17, "ACK offset changed");
        NS_TEST_EXPECT_MSG_EQ(decodedSack.GetEntropy(), 0x12345678, "Entropy changed");
        NS_TEST_EXPECT_MSG_EQ(decodedSack.GetSourcePdcId(), 0x1122, "Source PDC changed");
        NS_TEST_EXPECT_MSG_EQ(decodedSack.GetDestinationPdcId(), 0x3344, "Dest PDC changed");
        NS_TEST_EXPECT_MSG_EQ(decodedSack.GetCumulativeAck(), 0xabcdef, "CACK changed");
        NS_TEST_EXPECT_MSG_EQ(decodedSack.GetMaximumPsnRange(), 32, "MPR changed");
        NS_TEST_EXPECT_MSG_EQ(decodedSack.GetSackOffset(), 7, "SACK offset changed");
        NS_TEST_EXPECT_MSG_EQ(decodedSack.GetBitmap(),
                              0x8040201008040201ULL,
                              "SACK bitmap changed");
        NS_TEST_EXPECT_MSG_EQ(decodedCc.GetTimestamp(), 0x1234, "CC timestamp changed");
        NS_TEST_EXPECT_MSG_EQ(decodedCc.GetOutOfOrderCount(), 0x2345, "OOO count changed");
        NS_TEST_EXPECT_MSG_EQ(decodedCc.GetRestoreWindow(), true, "Restore flag changed");
        NS_TEST_EXPECT_MSG_EQ(decodedCc.GetReceiverWindowPenalty(), 0x45, "Penalty changed");
        NS_TEST_EXPECT_MSG_EQ(decodedCc.GetReceivedBytesUnits(), 0xabcdef, "Byte clock changed");

        MrcNethHeader nack;
        nack.SetReason(MrcNackReason::PSN_OUT_OF_RANGE);
        nack.SetEntropy(0xfedcba98);
        nack.SetSourcePdcId(1);
        nack.SetDestinationPdcId(2);
        nack.SetNackPsn(0x654321);
        nack.SetTimestamp(0x5678);
        packet = Create<Packet>();
        packet->AddHeader(nack);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), 20, "NETH wire size changed");
        MrcNethHeader decodedNack;
        packet->RemoveHeader(decodedNack);
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(decodedNack.GetReason()),
                              static_cast<uint8_t>(MrcNackReason::PSN_OUT_OF_RANGE),
                              "NACK reason changed");
        NS_TEST_EXPECT_MSG_EQ(decodedNack.GetEntropy(), 0xfedcba98, "NACK entropy changed");
        NS_TEST_EXPECT_MSG_EQ(decodedNack.GetNackPsn(), 0x654321, "NACK PSN changed");
        NS_TEST_EXPECT_MSG_EQ(decodedNack.GetTimestamp(), 0x5678, "NACK timestamp changed");

        MrcPethHeader probe;
        probe.SetProbeId(0x7788);
        probe.SetSourcePdcId(3);
        probe.SetDestinationPdcId(4);
        probe.SetTimestamp(0x9abc);
        packet = Create<Packet>();
        packet->AddHeader(probe);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), 16, "PETH wire size changed");
        MrcPethHeader decodedProbe;
        packet->RemoveHeader(decodedProbe);
        NS_TEST_EXPECT_MSG_EQ(decodedProbe.GetProbeId(), 0x7788, "Probe ID changed");
        NS_TEST_EXPECT_MSG_EQ(decodedProbe.GetTimestamp(), 0x9abc, "Probe timestamp changed");
    }
};

class MrcEndpointOperationsTestCase : public TestCase
{
  public:
    MrcEndpointOperationsTestCase()
        : TestCase("MRC EV Probe and Port Status Update complete without QP state")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        PointToPointHelper links;
        links.SetDeviceAttribute("DataRate", StringValue("800Gbps"));
        links.SetChannelAttribute("Delay", TimeValue(MicroSeconds(1)));
        const NetDeviceContainer devices = links.Install(nodes);
        InternetStackHelper internet;
        internet.Install(nodes);
        Ipv4AddressHelper addresses;
        addresses.SetBase("10.99.0.0", "255.255.255.0");
        const Ipv4InterfaceContainer interfaces = addresses.Assign(devices);

        Ptr<MrcTransportAdapter> first = CreateObject<MrcTransportAdapter>();
        Ptr<MrcTransportAdapter> second = CreateObject<MrcTransportAdapter>();
        AiTransportEndpointConfig firstConfig;
        firstConfig.endpointId = 1;
        firstConfig.lineRateBps = 800000000000ULL;
        AiTransportEndpointConfig secondConfig = firstConfig;
        secondConfig.endpointId = 2;
        NS_TEST_ASSERT_MSG_EQ(first->Initialize(nodes.Get(0), firstConfig), true, "first init");
        NS_TEST_ASSERT_MSG_EQ(second->Initialize(nodes.Get(1), secondConfig), true, "second init");
        NS_TEST_ASSERT_MSG_EQ(first->AddPeer(2, interfaces.GetAddress(1)), true, "first peer");
        NS_TEST_ASSERT_MSG_EQ(second->AddPeer(1, interfaces.GetAddress(0)), true, "second peer");
        first->SetAttribute("EndpointResponseTimeout", TimeValue(MicroSeconds(10)));
        uint32_t observedPortMask = 0;

        Simulator::Schedule(NanoSeconds(1), [first]() { first->SendEvProbe(2, 3); });
        Simulator::Schedule(NanoSeconds(1),
                            [first]() { first->SendPortStatusUpdate(2, 0xa5, 1); });
        Simulator::Schedule(MicroSeconds(5),
                            [second, &observedPortMask]() {
                                observedPortMask = second->GetPeerPortStatusMask(1);
                                second->Dispose();
                            });
        Simulator::Schedule(MicroSeconds(6), [first]() { first->SendEvProbe(2, 3); });
        Simulator::Stop(MicroSeconds(20));
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(first->IsPathReachable(2, 3), false, "EV timeout did not fail path");
        NS_TEST_EXPECT_MSG_GT(first->GetPathRtt(2, 3).GetNanoSeconds(), 0, "EV RTT is missing");
        NS_TEST_EXPECT_MSG_EQ(observedPortMask, 0xa5, "Port mask changed");
        Simulator::Destroy();
    }
};

class MrcHeaderTestSuite : public TestSuite
{
  public:
    MrcHeaderTestSuite()
        : TestSuite("mrc-header", Type::UNIT)
    {
        AddTestCase(new MrcReliabilityHeaderTestCase, Duration::QUICK);
        AddTestCase(new MrcEndpointOperationsTestCase, Duration::QUICK);
    }
};

static MrcHeaderTestSuite g_mrcHeaderTestSuite;
