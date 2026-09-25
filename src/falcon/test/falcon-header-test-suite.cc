/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/falcon-header.h"
#include "ns3/packet.h"
#include "ns3/test.h"

#include <array>
#include <cstdint>

using namespace ns3;

class FalconHeaderTestCase : public TestCase
{
  public:
    FalconHeaderTestCase()
        : TestCase("Falcon 1.0 base and transaction suffix headers preserve their wire fields")
    {
    }

  private:
    void DoRun() override
    {
        FalconBaseHeader base;
        base.SetDestinationConnectionId(0xabcdef);
        base.SetDestinationFunction(0x123456);
        base.SetProtocolType(FalconProtocolType::RDMA);
        base.SetPacketType(FalconPacketType::PUSH_DATA);
        base.SetAckRequest(true);
        base.SetReceiverDataWindowBase(0x01020304);
        base.SetReceiverRequestWindowBase(0x11121314);
        base.SetPacketSequenceNumber(0x21222324);
        base.SetRequestSequenceNumber(0x31323334);

        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(base);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), FalconBaseHeader::SERIALIZED_SIZE, "base size");
        std::array<uint8_t, FalconBaseHeader::SERIALIZED_SIZE> bytes{};
        packet->CopyData(bytes.data(), bytes.size());
        const std::array<uint8_t, FalconBaseHeader::SERIALIZED_SIZE> expected = {
            0x01, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x4b,
            0x01, 0x02, 0x03, 0x04, 0x11, 0x12, 0x13, 0x14,
            0x21, 0x22, 0x23, 0x24, 0x31, 0x32, 0x33, 0x34,
        };
        for (uint32_t i = 0; i < expected.size(); ++i)
        {
            NS_TEST_EXPECT_MSG_EQ(bytes[i], expected[i], "unexpected base-header octet " << i);
        }

        FalconBaseHeader decoded;
        packet->RemoveHeader(decoded);
        NS_TEST_EXPECT_MSG_EQ(decoded.GetVersion(), FalconBaseHeader::VERSION_1, "version");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetDestinationConnectionId(), 0xabcdef, "CID");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetDestinationFunction(), 0x123456, "function");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(decoded.GetProtocolType()),
                              static_cast<uint8_t>(FalconProtocolType::RDMA),
                              "protocol type");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(decoded.GetPacketType()),
                              static_cast<uint8_t>(FalconPacketType::PUSH_DATA),
                              "packet type");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetAckRequest(), true, "AR");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetReceiverDataWindowBase(), 0x01020304, "DBPSN");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetReceiverRequestWindowBase(), 0x11121314, "RBPSN");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetPacketSequenceNumber(), 0x21222324, "PSN");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetRequestSequenceNumber(), 0x31323334, "RSN");

        FalconPushDataHeader push;
        push.SetRequestLength(4096);
        packet->AddHeader(push);
        FalconPushDataHeader decodedPush;
        packet->RemoveHeader(decodedPush);
        NS_TEST_EXPECT_MSG_EQ(decodedPush.GetRequestLength(), 4096, "push request length");

        FalconPullRequestHeader pull;
        pull.SetRequestLength(8192);
        packet->AddHeader(pull);
        FalconPullRequestHeader decodedPull;
        packet->RemoveHeader(decodedPull);
        NS_TEST_EXPECT_MSG_EQ(decodedPull.GetRequestLength(), 8192, "pull request length");
        NS_TEST_EXPECT_MSG_EQ(decodedPull.HasValidReservedField(), true, "pull reserved field");
    }
};

class FalconHeaderTestSuite : public TestSuite
{
  public:
    FalconHeaderTestSuite()
        : TestSuite("falcon-header", Type::UNIT)
    {
        AddTestCase(new FalconHeaderTestCase, Duration::QUICK);
    }
};

static FalconHeaderTestSuite g_falconHeaderTestSuite;
