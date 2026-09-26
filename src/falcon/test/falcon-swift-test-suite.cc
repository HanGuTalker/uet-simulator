/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/falcon-swift.h"
#include "ns3/test.h"

using namespace ns3;

class FalconSwiftTestCase : public TestCase
{
  public:
    FalconSwiftTestCase()
        : TestCase("Falcon Swift applies delay AI/MD, RTT guards and retransmit collapse")
    {
    }

  private:
    void DoRun() override
    {
        FalconSwiftConfig config;
        config.minFcwnd = 0.25;
        config.maxFcwnd = 64.0;
        config.minNcwnd = 1.0;
        config.maxNcwnd = 64.0;
        config.baseDelayTarget = MicroSeconds(10);
        config.rttSmoothingAlpha = 0.0;
        config.delaySmoothingAlpha = 0.0;
        config.retransmitLimit = 3;
        FalconSwift swift(config);
        swift.Initialize(16.0, 16.0, MicroSeconds(20));

        swift.ProcessAck(MicroSeconds(20), MicroSeconds(20), MicroSeconds(5), 16, 0);
        NS_TEST_EXPECT_MSG_EQ_TOL(swift.GetFabricWindow(), 17.0, 0.0001, "AI result");

        swift.ProcessAck(MicroSeconds(40), MicroSeconds(20), MicroSeconds(20), 1, 0);
        const double decreased = swift.GetFabricWindow();
        NS_TEST_EXPECT_MSG_LT(decreased, 17.0, "delay did not reduce fcwnd");
        swift.ProcessAck(MicroSeconds(41), MicroSeconds(20), MicroSeconds(20), 1, 0);
        NS_TEST_EXPECT_MSG_EQ_TOL(swift.GetFabricWindow(),
                                  decreased,
                                  0.0001,
                                  "RTT guard allowed repeated decrease");

        swift.ProcessRetransmit(MicroSeconds(61));
        NS_TEST_EXPECT_MSG_LT(swift.GetFabricWindow(),
                              decreased,
                              "retransmit did not reduce fcwnd");
        swift.ProcessRetransmit(MicroSeconds(62));
        swift.ProcessRetransmit(MicroSeconds(63));
        NS_TEST_EXPECT_MSG_EQ_TOL(swift.GetFabricWindow(), 0.25, 0.0001, "limit did not collapse");
        NS_TEST_EXPECT_MSG_GT(swift.GetInterPacketGap(), MicroSeconds(20), "pacing not enabled");
        NS_TEST_EXPECT_MSG_EQ(swift.GetRetransmissionTimeout(), MicroSeconds(40), "Swift RTO");
    }
};

class FalconSwiftTestSuite : public TestSuite
{
  public:
    FalconSwiftTestSuite()
        : TestSuite("falcon-swift", Type::UNIT)
    {
        AddTestCase(new FalconSwiftTestCase, Duration::QUICK);
    }
};

static FalconSwiftTestSuite g_falconSwiftTestSuite;
