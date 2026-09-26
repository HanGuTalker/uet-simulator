/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/error-model.h"
#include "ns3/falcon-transport-adapter.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"

using namespace ns3;

class FalconDropFirstErrorModel : public ErrorModel
{
  private:
    bool DoCorrupt(Ptr<Packet>) override
    {
        if (!m_dropped)
        {
            m_dropped = true;
            return true;
        }
        return false;
    }

    void DoReset() override
    {
        m_dropped = false;
    }

    bool m_dropped{false};
};

class FalconAdapterLossRecoveryTestCase : public TestCase
{
  public:
    FalconAdapterLossRecoveryTestCase()
        : TestCase("Falcon adapter recovers a deterministically dropped first data packet")
    {
    }

  private:
    void Retransmitted(uint32_t, uint32_t)
    {
        ++m_retransmissions;
    }

    void Completed(uint32_t, uint64_t messageId, uint32_t bytes, Time)
    {
        if (messageId == 42 && bytes == 2048)
        {
            ++m_completions;
        }
    }

    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
        pointToPoint.SetChannelAttribute("Delay", TimeValue(MicroSeconds(1)));
        NetDeviceContainer devices = pointToPoint.Install(nodes);
        InternetStackHelper internet;
        internet.Install(nodes);
        Ipv4AddressHelper addresses;
        addresses.SetBase("10.250.0.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = addresses.Assign(devices);

        Ptr<FalconDropFirstErrorModel> dropFirst = CreateObject<FalconDropFirstErrorModel>();
        devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(dropFirst));

        Ptr<FalconTransportAdapter> sender = CreateObject<FalconTransportAdapter>();
        Ptr<FalconTransportAdapter> receiver = CreateObject<FalconTransportAdapter>();
        AiTransportEndpointConfig senderConfig;
        senderConfig.endpointId = 1;
        senderConfig.payloadMtuBytes = 1024;
        senderConfig.lineRateBps = 100000000000ULL;
        senderConfig.initialWindowBytes = 8192;
        senderConfig.maximumWindowBytes = 32768;
        senderConfig.baseRtt = MicroSeconds(4);
        senderConfig.targetQueueDelay = MicroSeconds(8);
        AiTransportEndpointConfig receiverConfig = senderConfig;
        receiverConfig.endpointId = 2;
        NS_TEST_EXPECT_MSG_EQ(sender->Initialize(nodes.Get(0), senderConfig), true, "sender init");
        NS_TEST_EXPECT_MSG_EQ(receiver->Initialize(nodes.Get(1), receiverConfig),
                              true,
                              "receiver init");
        NS_TEST_EXPECT_MSG_EQ(sender->AddPeer(2, interfaces.GetAddress(1)), true, "sender peer");
        NS_TEST_EXPECT_MSG_EQ(receiver->AddPeer(1, interfaces.GetAddress(0)),
                              true,
                              "receiver peer");

        AiTransportConnectionConfig connectionConfig;
        connectionConfig.remoteEndpointId = 2;
        connectionConfig.reliability = AiTransportReliability::RELIABLE_UNORDERED;
        connectionConfig.retransmissionTimeout = MicroSeconds(10);
        const uint32_t connectionId = sender->OpenConnection(connectionConfig);
        NS_TEST_EXPECT_MSG_NE(connectionId, 0, "connection open");
        sender->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&FalconAdapterLossRecoveryTestCase::Retransmitted, this));
        receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&FalconAdapterLossRecoveryTestCase::Completed, this));

        AiTransportRequest request;
        request.remoteEndpointId = 2;
        request.connectionId = connectionId;
        request.messageId = 42;
        request.operation = AiTransportOperation::MESSAGE;
        request.reliability = AiTransportReliability::RELIABLE_UNORDERED;
        request.payload = Create<Packet>(2048);
        NS_TEST_EXPECT_MSG_EQ(sender->Submit(request), true, "submit");
        Simulator::Stop(MilliSeconds(1));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_completions, 1, "message did not recover");
        NS_TEST_EXPECT_MSG_GT(m_retransmissions, 0, "loss did not trigger retransmission");
    }

    uint32_t m_retransmissions{0};
    uint32_t m_completions{0};
};

class FalconAdapterTestSuite : public TestSuite
{
  public:
    FalconAdapterTestSuite()
        : TestSuite("falcon-adapter", Type::SYSTEM)
    {
        AddTestCase(new FalconAdapterLossRecoveryTestCase, Duration::QUICK);
    }
};

static FalconAdapterTestSuite g_falconAdapterTestSuite;
