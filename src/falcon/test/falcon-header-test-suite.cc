/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/falcon-control-header.h"
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
        : TestCase("Falcon 1.1 data and reliability headers preserve their wire fields")
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
            0x10, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x4b,
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
        NS_TEST_EXPECT_MSG_EQ(decoded.HasValidReservedField(), true, "base reserved field");
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
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), FalconPushDataHeader::SERIALIZED_SIZE, "push size");
        std::array<uint8_t, FalconPushDataHeader::SERIALIZED_SIZE> pushBytes{};
        packet->CopyData(pushBytes.data(), pushBytes.size());
        const std::array<uint8_t, FalconPushDataHeader::SERIALIZED_SIZE> expectedPush = {
            0x00,
            0x00,
            0x10,
            0x00,
        };
        for (uint32_t i = 0; i < expectedPush.size(); ++i)
        {
            NS_TEST_EXPECT_MSG_EQ(pushBytes[i], expectedPush[i], "unexpected Push octet " << i);
        }
        FalconPushDataHeader decodedPush;
        packet->RemoveHeader(decodedPush);
        NS_TEST_EXPECT_MSG_EQ(decodedPush.GetRequestLength(), 4096, "push request length");
        NS_TEST_EXPECT_MSG_EQ(decodedPush.HasValidReservedField(), true, "push reserved field");

        FalconPullRequestHeader pull;
        pull.SetRequestLength(8192);
        packet->AddHeader(pull);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), FalconPullRequestHeader::SERIALIZED_SIZE,
                              "pull size");
        std::array<uint8_t, FalconPullRequestHeader::SERIALIZED_SIZE> pullBytes{};
        packet->CopyData(pullBytes.data(), pullBytes.size());
        const std::array<uint8_t, FalconPullRequestHeader::SERIALIZED_SIZE> expectedPull = {
            0x00,
            0x00,
            0x20,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
        };
        for (uint32_t i = 0; i < expectedPull.size(); ++i)
        {
            NS_TEST_EXPECT_MSG_EQ(pullBytes[i], expectedPull[i], "unexpected Pull octet " << i);
        }
        FalconPullRequestHeader decodedPull;
        packet->RemoveHeader(decodedPull);
        NS_TEST_EXPECT_MSG_EQ(decodedPull.GetRequestLength(), 8192, "pull request length");
        NS_TEST_EXPECT_MSG_EQ(decodedPull.HasValidReservedField(), true, "pull reserved field");

        FalconBackHeader back;
        back.SetConnectionId(0xabcdef);
        back.SetReceiverDataWindowBase(0x01020304);
        back.SetReceiverRequestWindowBase(0x11121314);
        back.SetTimestamp1(0x21222324);
        back.SetTimestamp2(0x31323334);
        back.SetCongestionMetadata(0x4142434445464748ULL);
        packet->AddHeader(back);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), FalconBackHeader::SERIALIZED_SIZE, "BACK size");
        std::array<uint8_t, FalconBackHeader::SERIALIZED_SIZE> backBytes{};
        packet->CopyData(backBytes.data(), backBytes.size());
        const std::array<uint8_t, FalconBackHeader::SERIALIZED_SIZE> expectedBack = {
            0x10, 0xab, 0xcd, 0xef, 0x00, 0x00, 0x00, 0x12,
            0x01, 0x02, 0x03, 0x04, 0x11, 0x12, 0x13, 0x14,
            0x21, 0x22, 0x23, 0x24, 0x31, 0x32, 0x33, 0x34,
            0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        };
        for (uint32_t i = 0; i < expectedBack.size(); ++i)
        {
            NS_TEST_EXPECT_MSG_EQ(backBytes[i], expectedBack[i], "unexpected BACK octet " << i);
        }
        FalconBackHeader decodedBack;
        packet->RemoveHeader(decodedBack);
        NS_TEST_EXPECT_MSG_EQ(decodedBack.GetConnectionId(), 0xabcdef, "BACK CID");
        NS_TEST_EXPECT_MSG_EQ(decodedBack.HasValidReservedFields(), true, "BACK reserved fields");
        NS_TEST_EXPECT_MSG_EQ(decodedBack.GetCongestionMetadata(),
                              0x4142434445464748ULL,
                              "BACK congestion metadata");

        FalconEackHeader eack;
        eack.SetBack(back);
        eack.SetDataAckBitmap({0x0102030405060708ULL, 0x1112131415161718ULL});
        eack.SetDataReceivedBitmap({0x2122232425262728ULL, 0x3132333435363738ULL});
        eack.SetRequestBitmap(0x4142434445464748ULL);
        packet->AddHeader(eack);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), FalconEackHeader::SERIALIZED_SIZE, "EACK size");
        std::array<uint8_t, FalconEackHeader::SERIALIZED_SIZE> eackBytes{};
        packet->CopyData(eackBytes.data(), eackBytes.size());
        NS_TEST_EXPECT_MSG_EQ(eackBytes[7], 0x14, "EACK packet type octet");
        NS_TEST_EXPECT_MSG_EQ(eackBytes[32], 0x01, "EACK ACK bitmap first octet");
        NS_TEST_EXPECT_MSG_EQ(eackBytes[47], 0x18, "EACK ACK bitmap last octet");
        NS_TEST_EXPECT_MSG_EQ(eackBytes[64], 0x41, "EACK request bitmap first octet");
        NS_TEST_EXPECT_MSG_EQ(eackBytes[71], 0x48, "EACK request bitmap last octet");
        FalconEackHeader decodedEack;
        packet->RemoveHeader(decodedEack);
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(decodedEack.GetBack().GetPacketType()),
                              static_cast<uint8_t>(FalconPacketType::EACK),
                              "EACK type");
        NS_TEST_EXPECT_MSG_EQ(decodedEack.GetDataAckBitmap()[0],
                              0x0102030405060708ULL,
                              "EACK ACK bitmap high");
        NS_TEST_EXPECT_MSG_EQ(decodedEack.GetDataReceivedBitmap()[1],
                              0x3132333435363738ULL,
                              "EACK RX bitmap low");
        NS_TEST_EXPECT_MSG_EQ(decodedEack.GetRequestBitmap(),
                              0x4142434445464748ULL,
                              "EACK request bitmap");

        FalconNackHeader nack;
        nack.SetConnectionId(0x123456);
        nack.SetReceiverDataWindowBase(0x01020304);
        nack.SetReceiverRequestWindowBase(0x11121314);
        nack.SetNackPacketSequenceNumber(0x21222324);
        nack.SetTimestamp1(0x31323334);
        nack.SetTimestamp2(0x41424344);
        nack.SetCongestionMetadata(0x5152535455565758ULL);
        nack.SetNackCode(FalconNackCode::RECEIVER_NOT_READY);
        nack.SetRnrTimeout(0x1a);
        nack.SetRequestWindow(true);
        nack.SetUlpNackCode(0xa5);
        packet->AddHeader(nack);
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(), FalconNackHeader::SERIALIZED_SIZE, "NACK size");
        std::array<uint8_t, FalconNackHeader::SERIALIZED_SIZE> nackBytes{};
        packet->CopyData(nackBytes.data(), nackBytes.size());
        NS_TEST_EXPECT_MSG_EQ(nackBytes[0], 0x10, "NACK version octet");
        NS_TEST_EXPECT_MSG_EQ(nackBytes[7], 0x10, "NACK packet type octet");
        NS_TEST_EXPECT_MSG_EQ(nackBytes[36], 0x02, "NACK code octet");
        NS_TEST_EXPECT_MSG_EQ(nackBytes[37], 0x1a, "NACK timeout octet");
        NS_TEST_EXPECT_MSG_EQ(nackBytes[38], 0x80, "NACK window octet");
        NS_TEST_EXPECT_MSG_EQ(nackBytes[39], 0xa5, "NACK ULP code octet");
        FalconNackHeader decodedNack;
        packet->RemoveHeader(decodedNack);
        NS_TEST_EXPECT_MSG_EQ(decodedNack.GetConnectionId(), 0x123456, "NACK CID");
        NS_TEST_EXPECT_MSG_EQ(decodedNack.GetNackPacketSequenceNumber(), 0x21222324, "NACK PSN");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(decodedNack.GetNackCode()),
                              static_cast<uint8_t>(FalconNackCode::RECEIVER_NOT_READY),
                              "NACK code");
        NS_TEST_EXPECT_MSG_EQ(decodedNack.GetRnrTimeout(), 0x1a, "NACK RNR timeout");
        NS_TEST_EXPECT_MSG_EQ(decodedNack.IsRequestWindow(), true, "NACK window");
        NS_TEST_EXPECT_MSG_EQ(decodedNack.GetUlpNackCode(), 0xa5, "NACK ULP code");
        NS_TEST_EXPECT_MSG_EQ(decodedNack.HasValidReservedFields(), true, "NACK reserved fields");
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
