/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/falcon-reliability.h"
#include "ns3/test.h"

#include <cstdint>

using namespace ns3;

class FalconReliabilityTestCase : public TestCase
{
  public:
    FalconReliabilityTestCase()
        : TestCase("Falcon dual reliability windows generate and consume BACK/EACK/NACK state")
    {
    }

  private:
    void DoRun() override
    {
        FalconReliabilityManager sender(100, 200);
        FalconReliabilityManager receiver(100, 200);

        for (uint32_t i = 0; i < 4; ++i)
        {
            const uint32_t psn = sender.AllocatePsn(FalconReliabilityWindow::DATA);
            sender.TrackTransmitted(FalconReliabilityWindow::DATA, psn);
        }
        for (uint32_t i = 0; i < 3; ++i)
        {
            const uint32_t psn = sender.AllocatePsn(FalconReliabilityWindow::REQUEST);
            sender.TrackTransmitted(FalconReliabilityWindow::REQUEST, psn);
        }

        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(receiver.ReceiveData(100)),
                              static_cast<uint8_t>(FalconReceiveResult::ACCEPTED),
                              "first data packet rejected");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(receiver.ReceiveData(102)),
                              static_cast<uint8_t>(FalconReceiveResult::ACCEPTED),
                              "out-of-order data packet rejected");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(receiver.ReceiveData(102)),
                              static_cast<uint8_t>(FalconReceiveResult::DUPLICATE),
                              "duplicate data packet not detected");
        NS_TEST_EXPECT_MSG_EQ(receiver.AcknowledgeData(100), true, "data ULP ACK rejected");
        NS_TEST_EXPECT_MSG_EQ(receiver.GetReceiverDataWindowBase(), 101, "data base did not move");

        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(receiver.ReceiveRequest(201)),
                              static_cast<uint8_t>(FalconReceiveResult::ACCEPTED),
                              "out-of-order request rejected");
        NS_TEST_EXPECT_MSG_EQ(receiver.GetReceiverRequestWindowBase(),
                              200,
                              "request base moved across a hole");
        FalconEackHeader requestEack = receiver.BuildEack(7);
        NS_TEST_EXPECT_MSG_EQ((requestEack.GetRequestBitmap() & uint64_t{2}) != 0,
                              true,
                              "EACK did not report request PSN 201");
        const std::vector<uint32_t> requestSelective = sender.ProcessEack(requestEack);
        NS_TEST_EXPECT_MSG_EQ(requestSelective.size(), 2, "combined EACK completion count");
        NS_TEST_EXPECT_MSG_EQ(requestSelective[0], 100, "wrong cumulatively ACKed data PSN");
        NS_TEST_EXPECT_MSG_EQ(requestSelective[1], 201, "wrong selectively ACKed request PSN");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(receiver.ReceiveRequest(200)),
                              static_cast<uint8_t>(FalconReceiveResult::ACCEPTED),
                              "request at window base rejected");
        NS_TEST_EXPECT_MSG_EQ(receiver.GetReceiverRequestWindowBase(),
                              202,
                              "request base did not consume contiguous packets");

        FalconEackHeader eack = receiver.BuildEack(7);
        NS_TEST_EXPECT_MSG_EQ(eack.GetBack().GetReceiverDataWindowBase(), 101, "EACK data base");
        NS_TEST_EXPECT_MSG_EQ(eack.GetBack().GetReceiverRequestWindowBase(),
                              202,
                              "EACK request base");
        NS_TEST_EXPECT_MSG_EQ((eack.GetDataReceivedBitmap()[0] & uint64_t{2}) != 0,
                              true,
                              "EACK did not report received data PSN 102");

        const std::vector<uint32_t> eacknowledged = sender.ProcessEack(eack);
        NS_TEST_EXPECT_MSG_EQ(eacknowledged.size(), 1, "wrong cumulative ACK count");
        NS_TEST_EXPECT_MSG_EQ(eacknowledged[0], 200, "wrong cumulatively ACKed request PSN");
        NS_TEST_EXPECT_MSG_EQ(sender.IsOutstanding(FalconReliabilityWindow::DATA, 100),
                              false,
                              "cumulatively ACKed data remains outstanding");
        NS_TEST_EXPECT_MSG_EQ(sender.IsOutstanding(FalconReliabilityWindow::REQUEST, 200),
                              false,
                              "cumulatively ACKed request remains outstanding");
        NS_TEST_EXPECT_MSG_EQ(sender.IsPeerReceivedData(102),
                              true,
                              "data receive bitmap was not retained");
        NS_TEST_EXPECT_MSG_EQ(sender.IsOutstanding(FalconReliabilityWindow::DATA, 102),
                              true,
                              "received-but-not-ULP-ACKed data was retired");

        NS_TEST_EXPECT_MSG_EQ(receiver.AcknowledgeData(102), true, "selective data ACK rejected");
        eack = receiver.BuildEack(7);
        const std::vector<uint32_t> selective = sender.ProcessEack(eack);
        NS_TEST_EXPECT_MSG_EQ(selective.size(), 1, "selective ACK count");
        NS_TEST_EXPECT_MSG_EQ(selective[0], 102, "wrong selectively ACKed PSN");

        FalconNackHeader nack;
        nack.SetConnectionId(7);
        nack.SetNackPacketSequenceNumber(103);
        uint32_t retransmit = 0;
        NS_TEST_EXPECT_MSG_EQ(sender.ProcessNack(nack, retransmit), true, "valid NACK ignored");
        NS_TEST_EXPECT_MSG_EQ(retransmit, 103, "wrong NACK retransmission PSN");
        nack.SetRequestWindow(true);
        NS_TEST_EXPECT_MSG_EQ(sender.ProcessNack(nack, retransmit),
                              false,
                              "NACK matched the wrong reliability window");

        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(receiver.ReceiveData(229)),
                              static_cast<uint8_t>(FalconReceiveResult::OUT_OF_WINDOW),
                              "packet beyond 128-bit data window accepted");
    }
};

class FalconReliabilityTestSuite : public TestSuite
{
  public:
    FalconReliabilityTestSuite()
        : TestSuite("falcon-reliability", Type::UNIT)
    {
        AddTestCase(new FalconReliabilityTestCase, Duration::QUICK);
    }
};

static FalconReliabilityTestSuite g_falconReliabilityTestSuite;
