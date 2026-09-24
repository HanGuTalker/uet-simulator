/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/mrc-header.h"
#include "ns3/packet.h"
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

class MrcHeaderTestSuite : public TestSuite
{
  public:
    MrcHeaderTestSuite()
        : TestSuite("mrc-header", Type::UNIT)
    {
        AddTestCase(new MrcReliabilityHeaderTestCase, Duration::QUICK);
    }
};

static MrcHeaderTestSuite g_mrcHeaderTestSuite;
