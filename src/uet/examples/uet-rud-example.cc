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

class RudExample
{
  public:
    void Run()
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));

        auto senderPdc = m_sender->CreatePdc(UetDeliveryMode::RUD);
        auto receiverPdc = m_receiver->CreatePdc(UetDeliveryMode::RUD);
        Activate(m_sender, senderPdc->GetPdcId());
        Activate(m_receiver, receiverPdc->GetPdcId());

        m_sender->SetTransmitCallback(MakeCallback(&RudExample::FromSender, this));
        m_receiver->SetTransmitCallback(MakeCallback(&RudExample::FromReceiver, this));
        m_receiver->TraceConnectWithoutContext("MessageComplete",
                                               MakeCallback(&RudExample::MessageComplete, this));

        m_sender->SendRudMessage(2, senderPdc->GetPdcId(), 1, Create<Packet>(6000));
        Simulator::Run();
        Simulator::Destroy();
    }

    bool IsComplete() const
    {
        return m_complete;
    }

  private:
    void Activate(Ptr<UetEndpoint> endpoint, uint32_t pdcId)
    {
        endpoint->TransitionPdc(pdcId, UetPdcState::OPENING);
        endpoint->TransitionPdc(pdcId, UetPdcState::ACTIVE);
    }

    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void FromSender(Ptr<const Packet> packet)
    {
        UetPdsHeader header;
        auto copy = packet->Copy();
        copy->RemoveHeader(header);
        if (!m_dropped && header.GetType() == UetPdsType::RUD_REQUEST && header.GetPsn() == 1)
        {
            m_dropped = true;
            std::cout << "Dropping PSN 1 to demonstrate RUD recovery" << std::endl;
            return;
        }
        Simulator::Schedule(MicroSeconds(1),
                            &RudExample::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
    }

    void FromReceiver(Ptr<const Packet> packet)
    {
        Simulator::Schedule(MicroSeconds(1), &RudExample::Deliver, this, m_sender, packet->Copy());
    }

    void MessageComplete(uint32_t pdcId, uint64_t messageId, uint32_t bytes, Time latency)
    {
        m_complete = true;
        std::cout << "RUD message complete: pdc=" << pdcId << " message=" << messageId
                  << " bytes=" << bytes << " latency=" << latency.GetMicroSeconds() << " us"
                  << std::endl;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    bool m_dropped{false};
    bool m_complete{false};
};

int
main(int argc, char* argv[])
{
    CommandLine command(__FILE__);
    command.Parse(argc, argv);

    RudExample example;
    example.Run();
    return example.IsComplete() ? 0 : 1;
}
