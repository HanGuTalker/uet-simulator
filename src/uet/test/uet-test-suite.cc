/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/boolean.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/uet-endpoint.h"
#include "ns3/uet-crc-trailer.h"
#include "ns3/uet-header.h"
#include "ns3/uet-helper.h"
#include "ns3/uet-nscc.h"
#include "ns3/uet-pdc.h"
#include "ns3/uet-pds-header.h"
#include "ns3/uet-ses-engine.h"
#include "ns3/uet-ses-header.h"
#include "ns3/uet-simulation-tag.h"
#include "ns3/uet-udp-transport.h"
#include "ns3/uinteger.h"

#include <array>
#include <set>
#include <unordered_map>
#include <vector>

using namespace ns3;

class UetWireHeaderLayoutTestCase : public TestCase
{
  public:
    UetWireHeaderLayoutTestCase()
        : TestCase("UEC PDS and SES headers match fixed network-order wire vectors")
    {
    }

  private:
    template <std::size_t N>
    void ExpectBytes(Ptr<Packet> packet,
                     const std::array<uint8_t, N>& expected,
                     const std::string& reason)
    {
        NS_TEST_ASSERT_MSG_EQ(packet->GetSize(), N, reason << ": size differs");
        std::array<uint8_t, N> actual{};
        packet->CopyData(actual.data(), actual.size());
        for (std::size_t index = 0; index < N; ++index)
        {
            NS_TEST_EXPECT_MSG_EQ(+actual[index],
                                  +expected[index],
                                  reason << ": byte " << index << " differs");
        }
    }

    void CheckUudPds()
    {
        UetPdsHeader header;
        header.SetType(UetPdsType::UUD_REQUEST);
        header.SetNextHeader(UetNextHeader::REQUEST_MEDIUM);
        auto packet = Create<Packet>();
        packet->AddHeader(header);
        ExpectBytes(packet, std::array<uint8_t, 4>{0x31, 0x00, 0x00, 0x00}, "UUD PDS header");
    }

    void CheckReliableRequestPds()
    {
        UetPdsHeader header;
        header.SetType(UetPdsType::RUD_REQUEST);
        header.SetNextHeader(UetNextHeader::REQUEST_STANDARD);
        header.SetFlags(0x18); // retx and ack-request
        header.SetClearPsnOffset(0x1234);
        header.SetPsn(0x01020304);
        header.SetSourcePdcId(0x1122);
        header.SetDestinationPdcId(0x3344);
        auto packet = Create<Packet>();
        packet->AddHeader(header);
        ExpectBytes(
            packet,
            std::array<uint8_t,
                       12>{0x11, 0x98, 0x12, 0x34, 0x01, 0x02, 0x03, 0x04, 0x11, 0x22, 0x33, 0x44},
            "RUD Request PDS header");
    }

    void CheckAckPds()
    {
        UetPdsHeader header;
        header.SetType(UetPdsType::ACK);
        header.SetNextHeader(UetNextHeader::NONE);
        header.SetAckPsnOffset(2);
        header.SetCumulativeAckPsn(0x10203040);
        header.SetSourcePdcId(0x0102);
        header.SetDestinationPdcId(0x0304);
        auto packet = Create<Packet>();
        packet->AddHeader(header);
        ExpectBytes(
            packet,
            std::array<uint8_t,
                       12>{0x38, 0x00, 0x00, 0x02, 0x10, 0x20, 0x30, 0x40, 0x01, 0x02, 0x03, 0x04},
            "ACK PDS header");
    }

    void CheckNackPds()
    {
        UetPdsHeader header;
        header.SetType(UetPdsType::NACK);
        header.SetNextHeader(UetNextHeader::NONE);
        header.SetNackCode(0x0d); // UET_ROD_OOO
        header.SetNackPsn(0x01020304);
        header.SetSourcePdcId(0x1122);
        header.SetDestinationPdcId(0x3344);
        header.SetNackPayload(0x05060708);
        auto packet = Create<Packet>();
        packet->AddHeader(header);
        ExpectBytes(packet,
                    std::array<uint8_t, 16>{0x50,
                                            0x00,
                                            0x0d,
                                            0x00,
                                            0x01,
                                            0x02,
                                            0x03,
                                            0x04,
                                            0x11,
                                            0x22,
                                            0x33,
                                            0x44,
                                            0x05,
                                            0x06,
                                            0x07,
                                            0x08},
                    "NACK PDS header");
    }

    void CheckAckCcPds()
    {
        UetPdsHeader header;
        header.SetType(UetPdsType::ACK_CC);
        header.SetNextHeader(UetNextHeader::NONE);
        header.SetFlags(0x20); // Associated Request was ECN marked
        header.SetAckPsnOffset(0xfffe);
        header.SetCumulativeAckPsn(0x10203040);
        header.SetSourcePdcId(0x0102);
        header.SetDestinationPdcId(0x0304);
        header.SetCcType(0); // CC_NSCC
        header.SetMaximumPsnRange(0x80);
        header.SetSackPsnOffset(0xfff0);
        header.SetSackBitmap(0x8000000000000001ULL);
        header.SetNsccState(0x1234, true, 0x12, 0x345678, 0x9abc);
        auto packet = Create<Packet>();
        packet->AddHeader(header);
        ExpectBytes(packet,
                    std::array<uint8_t, 32>{0x40, 0x20, 0xff, 0xfe, 0x10, 0x20, 0x30, 0x40,
                                            0x01, 0x02, 0x03, 0x04, 0x00, 0x80, 0xff, 0xf0,
                                            0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
                                            0x12, 0x34, 0x92, 0x34, 0x56, 0x78, 0x9a, 0xbc},
                    "ACK_CC PDS header");

        UetPdsHeader decoded;
        packet->RemoveHeader(decoded);
        NS_TEST_EXPECT_MSG_EQ(decoded.GetNsccServiceTime(), 0x1234, "NSCC service time changed");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetNsccRestoreCwnd(), true, "NSCC restore flag changed");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetNsccReceiverCwndPending(),
                              0x12,
                              "NSCC receiver congestion changed");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetNsccReceivedBytes(),
                              0x345678,
                              "NSCC received byte count changed");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetNsccOutOfOrderCount(),
                              0x9abc,
                              "NSCC out-of-order count changed");
    }

    void CheckControlPds()
    {
        UetPdsHeader header;
        header.SetType(UetPdsType::CONTROL);
        header.SetControlType(UetControlType::NEGOTIATION);
        header.SetFlags(0x2c); // ROD, ACK request, and SYN
        header.SetProbeOpaque(0x1234);
        header.SetPsn(0x01020304);
        header.SetSourcePdcId(0x1122);
        header.SetDestinationPdcId(0x3344);
        auto packet = Create<Packet>();
        packet->AddHeader(header);
        ExpectBytes(
            packet,
            std::array<uint8_t,
                       12>{0x5c, 0xac, 0x12, 0x34, 0x01, 0x02, 0x03, 0x04, 0x11, 0x22, 0x33, 0x44},
            "Negotiation Control PDS header");

        UetPdsHeader decoded;
        packet->RemoveHeader(decoded);
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(decoded.GetControlType()),
                              static_cast<uint8_t>(UetControlType::NEGOTIATION),
                              "Control type did not survive decoding");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetProbeOpaque(),
                              0x1234,
                              "Probe opaque did not survive decoding");
    }

    void CheckNegotiationPayload()
    {
        UetNegotiationOnOffHeader header;
        header.SetFeatureMask(UetNegotiationOnOffHeader::SYN_RETX_TRANSFER);
        auto packet = Create<Packet>();
        packet->AddHeader(header);
        ExpectBytes(packet,
                    std::array<uint8_t, 8>{0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
                    "PDS_NEG_ON_OFF payload");

        UetNegotiationOnOffHeader decoded;
        packet->RemoveHeader(decoded);
        NS_TEST_EXPECT_MSG_EQ(decoded.IsValid(), true, "Negotiation payload failed validation");
        NS_TEST_EXPECT_MSG_EQ(decoded.GetFeatureMask(),
                              UetNegotiationOnOffHeader::SYN_RETX_TRANSFER,
                              "Negotiation mask did not survive decoding");
    }

    void CheckSesHeaders()
    {
        UetSesMediumHeader medium;
        medium.SetOpcode(UetSesOpcode::DATAGRAM_SEND);
        medium.SetRequestLength(128);
        auto mediumPacket = Create<Packet>();
        mediumPacket->AddHeader(medium);
        std::array<uint8_t, 32> mediumExpected{};
        mediumExpected[0] = 0x07;
        mediumExpected[1] = 0x03;
        mediumExpected[2] = 0x00;
        mediumExpected[3] = 0x80;
        ExpectBytes(mediumPacket, mediumExpected, "UUD medium SES header");

        UetSesStandardHeader standard;
        standard.SetOpcode(UetSesOpcode::SEND);
        standard.SetStartOfMessage(true);
        standard.SetEndOfMessage(false);
        standard.SetMessageId(0x1234);
        standard.SetRequestLength(6000);
        auto standardPacket = Create<Packet>();
        standardPacket->AddHeader(standard);
        std::array<uint8_t, 44> standardExpected{};
        standardExpected[0] = 0x05;
        standardExpected[1] = 0x01;
        standardExpected[2] = 0x12;
        standardExpected[3] = 0x34;
        standardExpected[42] = 0x17;
        standardExpected[43] = 0x70;
        ExpectBytes(standardPacket, standardExpected, "standard SES first-fragment header");

        UetSesStandardHeader addressed;
        addressed.SetOpcode(UetSesOpcode::WRITE);
        addressed.SetStartOfMessage(true);
        addressed.SetEndOfMessage(true);
        addressed.SetDeliveryComplete(true);
        addressed.SetRelativeAddressing(true);
        addressed.SetHeaderDataPresent(true);
        addressed.SetMessageId(0x0102);
        addressed.SetRiGeneration(0x12);
        addressed.SetJobId(0x345678);
        addressed.SetPidOnFep(0x0abc);
        addressed.SetResourceIndex(0x0def);
        addressed.SetBufferOffset(0x0102030405060708ULL);
        addressed.SetInitiator(0x11223344);
        addressed.SetMemoryKey(0x1020304050607080ULL);
        addressed.SetHeaderData(0xa1a2a3a4a5a6a7a8ULL);
        addressed.SetRequestLength(8);
        auto addressedPacket = Create<Packet>();
        addressedPacket->AddHeader(addressed);
        std::array<uint8_t, 44> addressedExpected{
            0x01, 0x2f, 0x01, 0x02, 0x12, 0x34, 0x56, 0x78, 0x0a, 0xbc, 0x0d,
            0xef, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x11, 0x22,
            0x33, 0x44, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0xa1,
            0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0x00, 0x00, 0x00, 0x08};
        ExpectBytes(addressedPacket, addressedExpected, "addressed standard SES header");

        UetAtomicExtensionHeader atomic;
        atomic.SetAtomicOpcode(UetAtomicOpcode::SUM);
        atomic.SetAtomicDatatype(UetAtomicDatatype::UINT64);
        atomic.SetSemanticControl(0x5a);
        auto atomicPacket = Create<Packet>();
        atomicPacket->AddHeader(atomic);
        ExpectBytes(atomicPacket,
                    std::array<uint8_t, 4>{0x01, 0x03, 0x5a, 0x00},
                    "atomic extension header");

        UetSesResponseHeader response;
        response.SetList(1);
        response.SetOpcode(UetSesResponseOpcode::RESPONSE);
        response.SetReturnCode(UetSesReturnCode::BAD_GENERATION);
        response.SetMessageId(0x1234);
        response.SetRiGeneration(0x56);
        response.SetJobId(0x789abc);
        response.SetModifiedLength(0x01020304);
        auto responsePacket = Create<Packet>();
        responsePacket->AddHeader(response);
        ExpectBytes(responsePacket,
                    std::array<uint8_t, 16>{0x41, 0x02, 0x12, 0x34, 0x56, 0x78,
                                            0x9a, 0xbc, 0x01, 0x02, 0x03, 0x04,
                                            0x00, 0x00, 0x00, 0x00},
                    "standard SES response header");
    }

    void CheckReservedFieldValidation()
    {
        std::array<uint8_t, 12> pdsBytes{};
        pdsBytes[0] = 0x11;
        pdsBytes[1] = 0xc0; // RUD Request with a reserved flag set
        auto pdsPacket = Create<Packet>(pdsBytes.data(), pdsBytes.size());
        UetPdsHeader pdsHeader;
        pdsPacket->RemoveHeader(pdsHeader);
        NS_TEST_EXPECT_MSG_EQ(pdsHeader.IsValid(), false, "Reserved PDS flag was accepted");

        std::array<uint8_t, 44> standardBytes{};
        standardBytes[0] = 0x85; // top reserved SES bit plus UET_SEND
        standardBytes[1] = 0x01;
        standardBytes[2] = 0x00;
        standardBytes[3] = 0x01;
        auto standardPacket = Create<Packet>(standardBytes.data(), standardBytes.size());
        UetSesStandardHeader standardHeader;
        standardPacket->RemoveHeader(standardHeader);
        NS_TEST_EXPECT_MSG_EQ(standardHeader.IsValid(),
                              false,
                              "Reserved standard SES bit was accepted");

        std::array<uint8_t, 32> mediumBytes{};
        mediumBytes[0] = 0x07;
        mediumBytes[3] = 0x01; // SOM and EOM are both clear
        auto mediumPacket = Create<Packet>(mediumBytes.data(), mediumBytes.size());
        UetSesMediumHeader mediumHeader;
        mediumPacket->RemoveHeader(mediumHeader);
        NS_TEST_EXPECT_MSG_EQ(mediumHeader.IsValid(),
                              false,
                              "Medium SES header without SOM/EOM was accepted");
    }

    void CheckCrc32c()
    {
        const std::array<uint8_t, 9> check{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
        auto packet = Create<Packet>(check.data(), check.size());
        NS_TEST_EXPECT_MSG_EQ(UetCrcTrailer::Calculate(packet),
                              0xe3069283U,
                              "CRC32C did not match the Castagnoli check vector");
        UetCrcTrailer trailer;
        trailer.SetCrc(UetCrcTrailer::Calculate(packet));
        packet->AddTrailer(trailer);
        UetCrcTrailer decoded;
        packet->RemoveTrailer(decoded);
        NS_TEST_EXPECT_MSG_EQ(decoded.GetCrc(), 0xe3069283U, "CRC trailer wire round trip failed");
    }

    void DoRun() override
    {
        CheckUudPds();
        CheckReliableRequestPds();
        CheckAckPds();
        CheckAckCcPds();
        CheckNackPds();
        CheckControlPds();
        CheckNegotiationPayload();
        CheckSesHeaders();
        CheckReservedFieldValidation();
        CheckCrc32c();
    }
};

class UetSesEngineTestCase : public TestCase
{
  public:
    UetSesEngineTestCase()
        : TestCase("AI Base SES executes addressing, authorization, SEND, WRITE IMM, and atomic")
    {
    }

  private:
    static UetSesStandardHeader Request(UetSesOpcode opcode,
                                        uint16_t messageId,
                                        uint64_t key,
                                        uint64_t offset = 0)
    {
        UetSesStandardHeader request;
        request.SetOpcode(opcode);
        request.SetMessageId(messageId);
        request.SetStartOfMessage(true);
        request.SetEndOfMessage(true);
        request.SetRelativeAddressing(true);
        request.SetRiGeneration(7);
        request.SetJobId(0x123456);
        request.SetPidOnFep(0x234);
        request.SetResourceIndex(0x345);
        request.SetBufferOffset(offset);
        request.SetInitiator(0x55667788);
        request.SetMemoryKey(key);
        return request;
    }

    void DoRun() override
    {
        auto engine = CreateObject<UetSesEngine>();
        const uint64_t key = 0x1122334455667788ULL;
        const uint32_t handle = engine->RegisterBuffer(true,
                                                       0x123456,
                                                       0x234,
                                                       0x345,
                                                       7,
                                                       key,
                                                       0x55667788,
                                                       UET_SES_ACCESS_SEND | UET_SES_ACCESS_WRITE |
                                                           UET_SES_ACCESS_ATOMIC,
                                                       64);
        NS_TEST_ASSERT_MSG_NE(handle, 0, "SES buffer registration failed");

        auto noOp = Request(UetSesOpcode::NO_OP, 1, 0);
        auto result = engine->Execute(noOp, nullptr, Create<Packet>());
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(result.returnCode),
                              static_cast<uint8_t>(UetSesReturnCode::OK),
                              "Valid NO_OP failed");

        const std::array<uint8_t, 8> writeBytes{0, 0, 0, 0, 0, 0, 0, 5};
        auto write = Request(UetSesOpcode::WRITE, 2, key, 8);
        write.SetHeaderDataPresent(true);
        write.SetHeaderData(0xdeadbeef);
        write.SetDeliveryComplete(true);
        result = engine->Execute(write,
                                 nullptr,
                                 Create<Packet>(writeBytes.data(), writeBytes.size()));
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(result.returnCode),
                              static_cast<uint8_t>(UetSesReturnCode::OK),
                              "WRITE Immediate failed");
        NS_TEST_EXPECT_MSG_EQ(result.deliveryComplete, true, "GO completion was not requested");
        NS_TEST_EXPECT_MSG_EQ(result.immediateData, 0xdeadbeef, "Immediate data changed");

        UetAtomicExtensionHeader atomic;
        atomic.SetAtomicOpcode(UetAtomicOpcode::SUM);
        atomic.SetAtomicDatatype(UetAtomicDatatype::UINT64);
        auto atomicRequest = Request(UetSesOpcode::ATOMIC, 3, key, 8);
        result = engine->Execute(atomicRequest,
                                 &atomic,
                                 Create<Packet>(writeBytes.data(), writeBytes.size()));
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(result.returnCode),
                              static_cast<uint8_t>(UetSesReturnCode::OK),
                              "Non-fetching atomic SUM failed");
        std::array<uint8_t, 8> actual{};
        NS_TEST_ASSERT_MSG_EQ(engine->ReadBuffer(handle, 8, actual.data(), actual.size()),
                              true,
                              "Could not read registered SES buffer");
        const std::array<uint8_t, 8> expected{0, 0, 0, 0, 0, 0, 0, 10};
        NS_TEST_EXPECT_MSG_EQ(actual == expected, true, "Atomic SUM produced the wrong value");

        auto absolute = Request(UetSesOpcode::WRITE, 4, key, 0);
        absolute.SetRelativeAddressing(false);
        const uint32_t absoluteHandle = engine->RegisterBuffer(false,
                                                               0,
                                                               0x234,
                                                               0x345,
                                                               7,
                                                               key,
                                                               0x55667788,
                                                               UET_SES_ACCESS_WRITE,
                                                               16);
        NS_TEST_ASSERT_MSG_NE(absoluteHandle, 0, "Absolute SES buffer registration failed");
        result = engine->Execute(absolute,
                                 nullptr,
                                 Create<Packet>(writeBytes.data(), writeBytes.size()));
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(result.returnCode),
                              static_cast<uint8_t>(UetSesReturnCode::OK),
                              "Absolute addressing failed");

        auto denied = Request(UetSesOpcode::WRITE, 5, key ^ 1, 0);
        result = engine->Execute(denied,
                                 nullptr,
                                 Create<Packet>(writeBytes.data(), writeBytes.size()));
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(result.returnCode),
                              static_cast<uint8_t>(UetSesReturnCode::PERMISSION_VIOLATION),
                              "Invalid memory key was accepted");

        auto deferred = Request(UetSesOpcode::DEFERRABLE_SEND, 6, 0, 0);
        deferred.SetResourceIndex(0x346);
        result = engine->Execute(deferred, nullptr, Create<Packet>(4));
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(result.returnCode),
                              static_cast<uint8_t>(UetSesReturnCode::NO_MATCH),
                              "Deferrable Send-as-Send did not report NO_MATCH");
        NS_TEST_EXPECT_MSG_EQ(result.modifiedLength, 0, "NO_MATCH modified length was nonzero");
    }
};

class UetNsccAlgorithmTestCase : public TestCase
{
  public:
    UetNsccAlgorithmTestCase()
        : TestCase("NSCC gates inflight data and reacts to delay, ECN, loss, and receiver limits")
    {
    }

  private:
    void DoRun() override
    {
        auto nscc = CreateObject<UetNscc>();
        nscc->Initialize(4096, 16384);
        nscc->SetLineRateBps(400000000000ULL);
        NS_TEST_EXPECT_MSG_EQ(nscc->GetLineRateBps(),
                              400000000000ULL,
                              "Runtime NSCC line-rate update was not retained");
        nscc->SetCongestionWindow(8192);
        NS_TEST_EXPECT_MSG_EQ(nscc->GetCongestionWindow(),
                              8192,
                              "Runtime NSCC congestion-window update was not retained");
        nscc->SetCongestionWindow(16384);
        NS_TEST_EXPECT_MSG_EQ(nscc->CanSend(4096), true, "Initial NSCC window rejected data");
        for (uint32_t i = 0; i < 4; ++i)
        {
            nscc->OnPacketSent(4096);
        }
        NS_TEST_EXPECT_MSG_EQ(nscc->CanSend(1), false, "NSCC exceeded its congestion window");
        const uint32_t beforeIncrease = nscc->GetCongestionWindow();
        for (uint32_t i = 0; i < 4; ++i)
        {
            nscc->OnAck(4096, false, MicroSeconds(12), NanoSeconds(0), 0, false);
        }
        NS_TEST_EXPECT_MSG_GT(nscc->GetCongestionWindow(),
                              beforeIncrease,
                              "Uncongested ACKs did not increase cwnd");
        const uint32_t beforeEcn = nscc->GetCongestionWindow();
        nscc->OnPacketSent(4096);
        nscc->OnAck(4096, true, MicroSeconds(30), NanoSeconds(0), 0, false);
        NS_TEST_EXPECT_MSG_LT(nscc->GetCongestionWindow(),
                              beforeEcn,
                              "ECN with excess queue delay did not reduce cwnd");
        const uint32_t beforeReceiverLimit = nscc->GetCongestionWindow();
        nscc->OnAck(0, false, MicroSeconds(12), NanoSeconds(0), 120, false);
        NS_TEST_EXPECT_MSG_EQ(nscc->GetCongestionWindow() <= beforeReceiverLimit,
                              true,
                              "Receiver cwnd penalty increased the source window");
        const uint32_t limited = nscc->GetCongestionWindow();
        nscc->OnAck(0, false, MicroSeconds(12), NanoSeconds(0), 0, true);
        NS_TEST_EXPECT_MSG_EQ(nscc->GetCongestionWindow() >= limited,
                              true,
                              "Restore cwnd was ignored");
        const uint32_t beforeLoss = nscc->GetCongestionWindow();
        nscc->OnLoss(0);
        NS_TEST_EXPECT_MSG_LT(nscc->GetCongestionWindow(), beforeLoss, "Loss did not reduce cwnd");
        NS_TEST_EXPECT_MSG_GT(nscc->GetPacingDelay(4096), NanoSeconds(0), "Pacing was disabled");
    }
};

class UetEndpointDefaultsTestCase : public TestCase
{
  public:
    UetEndpointDefaultsTestCase()
        : TestCase("AI Base endpoint exposes the pinned default capabilities")
    {
    }

  private:
    void DoRun() override
    {
        auto endpoint = CreateObject<UetEndpoint>();
        UintegerValue mtu;
        BooleanValue trimming;
        BooleanValue spraying;
        endpoint->GetAttribute("PayloadMtu", mtu);
        endpoint->GetAttribute("TrimmingSupported", trimming);
        endpoint->GetAttribute("PacketSprayingEnabled", spraying);

        NS_TEST_EXPECT_MSG_EQ(mtu.Get(), 4096, "Unexpected default payload MTU");
        NS_TEST_EXPECT_MSG_EQ(trimming.Get(), true, "AI Base must support trimmed packets");
        NS_TEST_EXPECT_MSG_EQ(spraying.Get(), true, "Packet spraying should be enabled by default");

        auto pdc = endpoint->CreatePdc(UetDeliveryMode::ROD, 32768, 100000000000ULL);
        NS_TEST_ASSERT_MSG_NE(pdc, nullptr, "Could not create PDC for scheduler configuration");
        NS_TEST_EXPECT_MSG_EQ(endpoint->SetPdcCongestionWindow(pdc->GetPdcId(), 24576),
                              true,
                              "Endpoint rejected a valid PDC congestion-window update");
        NS_TEST_EXPECT_MSG_EQ(endpoint->GetCongestionWindow(pdc->GetPdcId()),
                              24576,
                              "Endpoint did not retain the PDC congestion-window update");
        endpoint->ConfigureJobScheduler(800000000000ULL);
        NS_TEST_EXPECT_MSG_EQ(endpoint->AssignPdcToJob(pdc->GetPdcId(), 1, 2),
                              true,
                              "Endpoint rejected a valid job-scheduler assignment");
        NS_TEST_EXPECT_MSG_EQ(endpoint->AssignPdcToJob(pdc->GetPdcId(), 0, 2),
                              false,
                              "Endpoint accepted reserved job identifier zero");
    }
};

class UetTraceContractTestCase : public TestCase
{
  public:
    UetTraceContractTestCase()
        : TestCase("Endpoint emits the stable path-selection trace payload")
    {
    }

  private:
    void PathSelected(uint32_t pdcId, uint32_t psn, uint32_t pathId)
    {
        m_called = true;
        m_pdcId = pdcId;
        m_psn = psn;
        m_pathId = pathId;
    }

    void DoRun() override
    {
        auto endpoint = CreateObject<UetEndpoint>();
        endpoint->TraceConnectWithoutContext(
            "PathSelected",
            MakeCallback(&UetTraceContractTestCase::PathSelected, this));

        endpoint->NotifyPathSelected(7, 19, 3);

        NS_TEST_EXPECT_MSG_EQ(m_called, true, "PathSelected callback was not invoked");
        NS_TEST_EXPECT_MSG_EQ(m_pdcId, 7, "PDC ID changed in trace delivery");
        NS_TEST_EXPECT_MSG_EQ(m_psn, 19, "PSN changed in trace delivery");
        NS_TEST_EXPECT_MSG_EQ(m_pathId, 3, "Path ID changed in trace delivery");
    }

    bool m_called{false};
    uint32_t m_pdcId{0};
    uint32_t m_psn{0};
    uint32_t m_pathId{0};
};

class UetPdcLifecycleTestCase : public TestCase
{
  public:
    UetPdcLifecycleTestCase()
        : TestCase("Endpoint manages PDC allocation and legal lifecycle transitions")
    {
    }

  private:
    void StateChanged(uint32_t pdcId, UetPdcState oldState, UetPdcState newState)
    {
        ++m_transitionCount;
        m_lastPdcId = pdcId;
        m_lastOldState = oldState;
        m_lastNewState = newState;
    }

    void DoRun() override
    {
        auto endpoint = CreateObject<UetEndpoint>();
        endpoint->SetAttribute("MaxPdcCount", UintegerValue(1));
        endpoint->SetAttribute("NsccInitialWindow", UintegerValue(16384));
        endpoint->TraceConnectWithoutContext(
            "PdcStateChange",
            MakeCallback(&UetPdcLifecycleTestCase::StateChanged, this));

        auto pdc = endpoint->CreatePdc(UetDeliveryMode::ROD);
        NS_TEST_ASSERT_MSG_NE(pdc, nullptr, "First PDC allocation failed");
        NS_TEST_EXPECT_MSG_EQ(endpoint->GetPdcCount(), 1, "PDC was not inserted into the table");
        NS_TEST_EXPECT_MSG_EQ(endpoint->GetPdc(pdc->GetPdcId()) == pdc,
                              true,
                              "PDC lookup returned a different object");
        NS_TEST_EXPECT_MSG_EQ(endpoint->GetCongestionWindow(pdc->GetPdcId()),
                              16384,
                              "Endpoint initial NSCC window was not applied at PDC creation");
        auto overrideEndpoint = CreateObject<UetEndpoint>();
        auto overridePdc = overrideEndpoint->CreatePdc(UetDeliveryMode::RUD, 8192);
        NS_TEST_ASSERT_MSG_NE(overridePdc, nullptr, "PDC allocation with window override failed");
        NS_TEST_EXPECT_MSG_EQ(overrideEndpoint->GetCongestionWindow(overridePdc->GetPdcId()),
                              8192,
                              "Per-PDC initial window override was not applied before NSCC initialization");
        NS_TEST_EXPECT_MSG_EQ(endpoint->CreatePdc(UetDeliveryMode::RUD) == nullptr,
                              true,
                              "MaxPdcCount was not enforced");

        NS_TEST_EXPECT_MSG_EQ(endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::OPENING),
                              true,
                              "CLOSED to OPENING was rejected");
        NS_TEST_EXPECT_MSG_EQ(endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::ACTIVE),
                              true,
                              "OPENING to ACTIVE was rejected");
        NS_TEST_EXPECT_MSG_EQ(endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::CLOSED),
                              false,
                              "Illegal ACTIVE to CLOSED transition was accepted");
        NS_TEST_EXPECT_MSG_EQ(endpoint->RemovePdc(pdc->GetPdcId()),
                              false,
                              "An active PDC was removed");
        NS_TEST_EXPECT_MSG_EQ(endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::CLOSING),
                              true,
                              "ACTIVE to CLOSING was rejected");
        NS_TEST_EXPECT_MSG_EQ(endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::CLOSED),
                              true,
                              "CLOSING to CLOSED was rejected");

        NS_TEST_EXPECT_MSG_EQ(m_transitionCount, 4, "Trace count includes a rejected transition");
        NS_TEST_EXPECT_MSG_EQ(m_lastPdcId, pdc->GetPdcId(), "Trace reported the wrong PDC");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(m_lastOldState),
                              static_cast<uint32_t>(UetPdcState::CLOSING),
                              "Trace reported the wrong old state");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(m_lastNewState),
                              static_cast<uint32_t>(UetPdcState::CLOSED),
                              "Trace reported the wrong new state");
        NS_TEST_EXPECT_MSG_EQ(endpoint->RemovePdc(pdc->GetPdcId()),
                              true,
                              "Closed PDC was not removed");
        NS_TEST_EXPECT_MSG_EQ(endpoint->GetPdcCount(), 0, "PDC table retained a removed entry");
    }

    uint32_t m_transitionCount{0};
    uint32_t m_lastPdcId{0};
    UetPdcState m_lastOldState{UetPdcState::CLOSED};
    UetPdcState m_lastNewState{UetPdcState::CLOSED};
};

class UetPacketDispatchTestCase : public TestCase
{
  public:
    UetPacketDispatchTestCase()
        : TestCase("Endpoint validates and dispatches packets to the selected active PDC")
    {
    }

  private:
    Ptr<Packet> MakePacket(uint32_t destination,
                           uint32_t pdcId,
                           UetDeliveryMode mode,
                           uint32_t sequenceNumber = 99)
    {
        auto packet = Create<Packet>(128);
        UetSesStandardHeader sesHeader;
        sesHeader.SetOpcode(UetSesOpcode::SEND);
        sesHeader.SetStartOfMessage(true);
        sesHeader.SetEndOfMessage(true);
        sesHeader.SetMessageId(1234);
        sesHeader.SetRequestLength(128);
        packet->AddHeader(sesHeader);
        UetPdsHeader pdsHeader;
        pdsHeader.SetType(mode == UetDeliveryMode::RUD ? UetPdsType::RUD_REQUEST
                                                       : UetPdsType::ROD_REQUEST);
        pdsHeader.SetNextHeader(UetNextHeader::REQUEST_STANDARD);
        pdsHeader.SetPsn(sequenceNumber);
        pdsHeader.SetSourcePdcId(pdcId);
        pdsHeader.SetDestinationPdcId(pdcId);
        packet->AddHeader(pdsHeader);
        UetSimulationTag route;
        route.SetSourceEndpointId(11);
        route.SetDestinationEndpointId(destination);
        route.SetPathId(3);
        packet->AddPacketTag(route);
        return packet;
    }

    void PacketReceived(Ptr<const Packet> packet, uint32_t pdcId, uint32_t pathId)
    {
        ++m_receiveTraceCount;
        m_receivedPacketSize = packet->GetSize();
        m_receivedPdcId = pdcId;
        m_receivedPathId = pathId;
    }

    void ExpectStatus(UetReceiveStatus actual, UetReceiveStatus expected, const std::string& reason)
    {
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(actual),
                              static_cast<uint32_t>(expected),
                              reason);
    }

    void DoRun() override
    {
        auto endpoint = CreateObject<UetEndpoint>();
        endpoint->SetAttribute("EndpointId", UintegerValue(22));
        endpoint->TraceConnectWithoutContext(
            "PacketRx",
            MakeCallback(&UetPacketDispatchTestCase::PacketReceived, this));
        auto pdc = endpoint->CreatePdc(UetDeliveryMode::ROD);

        ExpectStatus(endpoint->ReceivePacket(Create<Packet>(2)),
                     UetReceiveStatus::PACKET_TOO_SHORT,
                     "Short packet was accepted");
        ExpectStatus(endpoint->ReceivePacket(MakePacket(22, pdc->GetPdcId(), UetDeliveryMode::ROD)),
                     UetReceiveStatus::INACTIVE_PDC,
                     "Closed PDC accepted a packet");

        endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::OPENING);
        endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::ACTIVE);
        auto accepted = MakePacket(22, pdc->GetPdcId(), UetDeliveryMode::ROD);
        ExpectStatus(endpoint->ReceivePacket(accepted),
                     UetReceiveStatus::ACCEPTED,
                     "Valid packet was rejected");
        NS_TEST_EXPECT_MSG_EQ(pdc->GetReceivedPacketCount(), 1, "PDC did not record the packet");
        NS_TEST_EXPECT_MSG_EQ(pdc->GetLastReceivedSequenceNumber(), 99, "PDC recorded wrong PSN");
        NS_TEST_EXPECT_MSG_EQ(pdc->GetLastReceivedMessageId(), 1234, "PDC recorded wrong message");
        NS_TEST_EXPECT_MSG_EQ(m_receiveTraceCount, 1, "PacketRx trace count is incorrect");
        NS_TEST_EXPECT_MSG_EQ(m_receivedPacketSize,
                              accepted->GetSize(),
                              "Trace packet size changed");
        NS_TEST_EXPECT_MSG_EQ(m_receivedPdcId, pdc->GetPdcId(), "Trace PDC ID changed");
        NS_TEST_EXPECT_MSG_EQ(m_receivedPathId, 3, "Trace path ID changed");

        ExpectStatus(endpoint->ReceivePacket(MakePacket(23, pdc->GetPdcId(), UetDeliveryMode::ROD)),
                     UetReceiveStatus::WRONG_ENDPOINT,
                     "Packet for another endpoint was accepted");
        ExpectStatus(endpoint->ReceivePacket(MakePacket(22, 9999, UetDeliveryMode::ROD)),
                     UetReceiveStatus::UNKNOWN_PDC,
                     "Packet for an unknown PDC was accepted");
        ExpectStatus(endpoint->ReceivePacket(MakePacket(22, pdc->GetPdcId(), UetDeliveryMode::RUD)),
                     UetReceiveStatus::DELIVERY_MODE_MISMATCH,
                     "Packet with the wrong delivery mode was accepted");
        NS_TEST_EXPECT_MSG_EQ(pdc->GetReceivedPacketCount(),
                              1,
                              "Rejected packets changed the PDC receive count");
        NS_TEST_EXPECT_MSG_EQ(m_receiveTraceCount, 1, "Rejected packet emitted PacketRx");
    }

    uint32_t m_receiveTraceCount{0};
    uint32_t m_receivedPacketSize{0};
    uint32_t m_receivedPdcId{0};
    uint32_t m_receivedPathId{0};
};

class UetPdcEstablishmentTestCase : public TestCase
{
  public:
    UetPdcEstablishmentTestCase()
        : TestCase("PDC SYN establishes independently allocated source and target PDCIDs")
    {
    }

  private:
    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader pds;
        copy->RemoveHeader(pds);
        if (pds.GetType() == UetPdsType::RUD_REQUEST)
        {
            UetSesStandardHeader ses;
            copy->RemoveHeader(ses);
            if (ses.GetMessageId() == 41)
            {
                m_sawSyn = (pds.GetFlags() & 0x04) != 0;
                m_synSourcePdcId = pds.GetSourcePdcId();
                m_synOverloadedDestination = pds.GetDestinationPdcId();
            }
            else if (ses.GetMessageId() == 42)
            {
                m_sawEstablishedRequest = (pds.GetFlags() & 0x04) == 0;
                m_establishedDestinationPdcId = pds.GetDestinationPdcId();
            }
        }
        Simulator::Schedule(MicroSeconds(1),
                            &UetPdcEstablishmentTestCase::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader pds;
        copy->RemoveHeader(pds);
        if ((pds.GetType() == UetPdsType::ACK || pds.GetType() == UetPdsType::ACK_CC) &&
            !m_sawEstablishmentAck)
        {
            m_sawEstablishmentAck = true;
            m_ackSourcePdcId = pds.GetSourcePdcId();
            m_ackDestinationPdcId = pds.GetDestinationPdcId();
        }
        Simulator::Schedule(MicroSeconds(1),
                            &UetPdcEstablishmentTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    void SendSecondMessage()
    {
        m_secondSendAccepted = m_sender->SendRudMessage(2, m_sourcePdcId, 42, Create<Packet>(64));
    }

    void MessageComplete(uint32_t, uint64_t, uint32_t, Time)
    {
        ++m_completionCount;
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));

        auto sourcePdc = m_sender->CreatePdc(UetDeliveryMode::RUD);
        m_sourcePdcId = sourcePdc->GetPdcId();
        m_sender->TransitionPdc(m_sourcePdcId, UetPdcState::OPENING);
        auto occupiedTargetId = m_receiver->CreatePdc(UetDeliveryMode::ROD);
        NS_TEST_ASSERT_MSG_EQ(occupiedTargetId->GetPdcId(),
                              m_sourcePdcId,
                              "Test precondition did not occupy the matching target PDCID");

        m_sender->SetTransmitCallback(
            MakeCallback(&UetPdcEstablishmentTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(
            MakeCallback(&UetPdcEstablishmentTestCase::ReceiverTransmit, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetPdcEstablishmentTestCase::MessageComplete, this));

        NS_TEST_EXPECT_MSG_EQ(m_sender->SendRudMessage(2, m_sourcePdcId, 41, Create<Packet>(64)),
                              true,
                              "Opening PDC rejected its SYN request");
        Simulator::Schedule(MicroSeconds(5), &UetPdcEstablishmentTestCase::SendSecondMessage, this);
        Simulator::Run();

        auto targetPdc = m_receiver->GetPdc(m_ackSourcePdcId);
        NS_TEST_EXPECT_MSG_EQ(m_sawSyn, true, "Initial request did not set SYN");
        NS_TEST_EXPECT_MSG_EQ(m_synSourcePdcId, m_sourcePdcId, "SYN used wrong SPDCID");
        NS_TEST_EXPECT_MSG_EQ(m_synOverloadedDestination,
                              0,
                              "First SYN did not encode PSN_OFFSET zero");
        NS_TEST_EXPECT_MSG_EQ(m_sawEstablishmentAck, true, "Target did not return an ACK");
        NS_TEST_EXPECT_MSG_NE(m_ackSourcePdcId,
                              m_sourcePdcId,
                              "Target did not allocate an independent TPDCID");
        NS_TEST_EXPECT_MSG_EQ(m_ackDestinationPdcId,
                              m_sourcePdcId,
                              "Establishment ACK used the wrong DPDCID");
        NS_TEST_ASSERT_MSG_NE(targetPdc, nullptr, "Target PDC was not allocated");
        NS_TEST_EXPECT_MSG_EQ(targetPdc->GetRemotePdcId(),
                              m_sourcePdcId,
                              "Target did not retain the initiator PDCID");
        NS_TEST_EXPECT_MSG_EQ(sourcePdc->GetRemotePdcId(),
                              m_ackSourcePdcId,
                              "Initiator did not learn the target PDCID");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(sourcePdc->GetState()),
                              static_cast<uint32_t>(UetPdcState::ACTIVE),
                              "Initiator PDC did not become active");
        NS_TEST_EXPECT_MSG_EQ(m_secondSendAccepted, true, "Established PDC rejected later traffic");
        NS_TEST_EXPECT_MSG_EQ(m_sawEstablishedRequest,
                              true,
                              "Post-establishment request retained SYN");
        NS_TEST_EXPECT_MSG_EQ(m_establishedDestinationPdcId,
                              m_ackSourcePdcId,
                              "Post-establishment request used the wrong DPDCID");
        NS_TEST_EXPECT_MSG_EQ(m_completionCount, 2, "Target did not complete both messages");

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    uint32_t m_sourcePdcId{0};
    bool m_sawSyn{false};
    uint16_t m_synSourcePdcId{0};
    uint16_t m_synOverloadedDestination{0};
    bool m_sawEstablishmentAck{false};
    uint16_t m_ackSourcePdcId{0};
    uint16_t m_ackDestinationPdcId{0};
    bool m_secondSendAccepted{false};
    bool m_sawEstablishedRequest{false};
    uint16_t m_establishedDestinationPdcId{0};
    uint32_t m_completionCount{0};
};

class UetRudRecoveryTestCase : public TestCase
{
  public:
    enum class RecoveryMode
    {
        NACK,
        TIMEOUT,
    };

    explicit UetRudRecoveryTestCase(RecoveryMode mode)
        : TestCase(mode == RecoveryMode::NACK
                       ? "RUD recovers a missing fragment through NACK"
                       : "RUD recovers a single missing packet through timeout"),
          m_mode(mode)
    {
    }

  private:
    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        UetPdsHeader header;
        Ptr<Packet> copy = packet->Copy();
        copy->RemoveHeader(header);
        if (header.GetType() == UetPdsType::RUD_REQUEST && header.GetPsn() == 1 &&
            !m_droppedFirstData)
        {
            NS_TEST_EXPECT_MSG_EQ(packet->GetSize(),
                                  UetPdsHeader::REQUEST_SIZE +
                                      UetSesStandardHeader::SERIALIZED_SIZE +
                                      (m_mode == RecoveryMode::NACK ? 4096 : 128),
                                  "RUD first packet has the wrong wire size");
            m_droppedFirstData = true;
            return;
        }
        Simulator::Schedule(MicroSeconds(1),
                            &UetRudRecoveryTestCase::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        Simulator::Schedule(MicroSeconds(1),
                            &UetRudRecoveryTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    void MessageComplete(uint32_t pdcId, uint64_t messageId, uint32_t bytes, Time latency)
    {
        ++m_completionCount;
        m_completedPdcId = pdcId;
        m_completedMessageId = messageId;
        m_completedBytes = bytes;
        m_completionLatency = latency;
    }

    void Retransmission(uint32_t pdcId, uint32_t sequenceNumber)
    {
        ++m_retransmissionCount;
        m_retransmittedPdcId = pdcId;
        m_retransmittedSequence = sequenceNumber;
    }

    void Timeout(uint32_t, uint32_t)
    {
        ++m_timeoutCount;
    }

    void Nack(uint32_t, uint32_t)
    {
        ++m_nackCount;
    }

    void Activate(Ptr<UetEndpoint> endpoint, uint32_t pdcId)
    {
        endpoint->TransitionPdc(pdcId, UetPdcState::OPENING);
        endpoint->TransitionPdc(pdcId, UetPdcState::ACTIVE);
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));

        auto senderPdc = m_sender->CreatePdc(UetDeliveryMode::RUD);
        auto receiverPdc = m_receiver->CreatePdc(UetDeliveryMode::RUD);
        NS_TEST_ASSERT_MSG_EQ(senderPdc->GetPdcId(),
                              receiverPdc->GetPdcId(),
                              "Test endpoints did not allocate matching PDC IDs");
        const Time retransmissionTimeout =
            m_mode == RecoveryMode::NACK ? MicroSeconds(20) : MicroSeconds(5);
        senderPdc->SetAttribute("RetransmissionTimeout", TimeValue(retransmissionTimeout));
        Activate(m_sender, senderPdc->GetPdcId());
        Activate(m_receiver, receiverPdc->GetPdcId());

        m_sender->SetTransmitCallback(MakeCallback(&UetRudRecoveryTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(
            MakeCallback(&UetRudRecoveryTestCase::ReceiverTransmit, this));
        m_sender->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&UetRudRecoveryTestCase::Retransmission, this));
        m_sender->TraceConnectWithoutContext("Timeout",
                                             MakeCallback(&UetRudRecoveryTestCase::Timeout, this));
        m_sender->TraceConnectWithoutContext("Nack",
                                             MakeCallback(&UetRudRecoveryTestCase::Nack, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetRudRecoveryTestCase::MessageComplete, this));

        const uint32_t messageSize = m_mode == RecoveryMode::NACK ? 6000 : 128;
        NS_TEST_EXPECT_MSG_EQ(
            m_sender->SendRudMessage(2, senderPdc->GetPdcId(), 77, Create<Packet>(messageSize)),
            true,
            "RUD message submission failed");
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(m_droppedFirstData, true, "Test did not inject the intended loss");
        NS_TEST_EXPECT_MSG_EQ(m_completionCount, 1, "Message did not complete exactly once");
        NS_TEST_EXPECT_MSG_EQ(m_completedPdcId, senderPdc->GetPdcId(), "Completed PDC changed");
        NS_TEST_EXPECT_MSG_EQ(m_completedMessageId, 77, "Completed message ID changed");
        NS_TEST_EXPECT_MSG_EQ(m_completedBytes, messageSize, "Completed byte count changed");
        NS_TEST_EXPECT_MSG_GT(m_completionLatency, NanoSeconds(0), "Completion latency is zero");
        NS_TEST_EXPECT_MSG_EQ(m_retransmissionCount, 1, "Unexpected retransmission count");
        NS_TEST_EXPECT_MSG_EQ(m_retransmittedPdcId,
                              senderPdc->GetPdcId(),
                              "Retransmitted PDC changed");
        NS_TEST_EXPECT_MSG_EQ(m_retransmittedSequence, 1, "Wrong packet was retransmitted");
        NS_TEST_EXPECT_MSG_EQ(m_sender->GetOutstandingPacketCount(senderPdc->GetPdcId()),
                              0,
                              "ACK processing left an outstanding packet");
        if (m_mode == RecoveryMode::NACK)
        {
            NS_TEST_EXPECT_MSG_EQ(m_nackCount, 1, "NACK recovery did not process one NACK");
            NS_TEST_EXPECT_MSG_EQ(m_timeoutCount, 0, "NACK recovery unexpectedly timed out");
        }
        else
        {
            NS_TEST_EXPECT_MSG_EQ(m_nackCount, 0, "Single-packet recovery unexpectedly used NACK");
            NS_TEST_EXPECT_MSG_EQ(m_timeoutCount, 1, "Timeout recovery did not expire once");
        }

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    RecoveryMode m_mode;
    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    bool m_droppedFirstData{false};
    uint32_t m_completionCount{0};
    uint32_t m_completedPdcId{0};
    uint64_t m_completedMessageId{0};
    uint32_t m_completedBytes{0};
    Time m_completionLatency{Seconds(0)};
    uint32_t m_retransmissionCount{0};
    uint32_t m_retransmittedPdcId{0};
    uint32_t m_retransmittedSequence{0};
    uint32_t m_timeoutCount{0};
    uint32_t m_nackCount{0};
};

class UetLostSynRetransmissionTestCase : public TestCase
{
  public:
    UetLostSynRetransmissionTestCase()
        : TestCase("PDC establishment survives loss of the first SYN Request")
    {
    }

  private:
    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader header;
        copy->RemoveHeader(header);
        if (header.GetType() != UetPdsType::RUD_REQUEST)
        {
            return;
        }

        ++m_requestTransmissions;
        if (m_requestTransmissions == 1)
        {
            m_firstRequestWasSyn = (header.GetFlags() & 0x04) != 0;
            return;
        }

        m_retransmissionWasSyn = (header.GetFlags() & 0x04) != 0;
        m_retransmissionFlagSet = (header.GetFlags() & 0x10) != 0;
        Simulator::Schedule(NanoSeconds(100),
                            &UetLostSynRetransmissionTestCase::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        Simulator::Schedule(NanoSeconds(100),
                            &UetLostSynRetransmissionTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    void MessageComplete(uint32_t, uint64_t, uint32_t, Time)
    {
        ++m_completionCount;
    }

    void Retransmission(uint32_t pdcId, uint32_t)
    {
        NS_TEST_EXPECT_MSG_EQ(pdcId, m_sourcePdcId, "SYN retry used the wrong PDC");
        ++m_retransmissionCount;
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));

        auto sourcePdc = m_sender->CreatePdc(UetDeliveryMode::RUD);
        m_sourcePdcId = sourcePdc->GetPdcId();
        sourcePdc->SetAttribute("RetransmissionTimeout", TimeValue(MicroSeconds(5)));
        m_sender->TransitionPdc(m_sourcePdcId, UetPdcState::OPENING);

        m_sender->SetTransmitCallback(
            MakeCallback(&UetLostSynRetransmissionTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(
            MakeCallback(&UetLostSynRetransmissionTestCase::ReceiverTransmit, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetLostSynRetransmissionTestCase::MessageComplete, this));
        m_sender->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&UetLostSynRetransmissionTestCase::Retransmission, this));

        NS_TEST_ASSERT_MSG_EQ(m_sender->SendRudMessage(2, m_sourcePdcId, 51, Create<Packet>(64)),
                              true,
                              "Opening PDC rejected the SYN Request");
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(m_requestTransmissions, 2, "Lost SYN was not retried exactly once");
        NS_TEST_EXPECT_MSG_EQ(m_retransmissionCount, 1, "SYN retry trace count changed");
        NS_TEST_EXPECT_MSG_EQ(m_firstRequestWasSyn, true, "Initial Request did not carry SYN");
        NS_TEST_EXPECT_MSG_EQ(m_retransmissionWasSyn,
                              true,
                              "Retry stopped carrying SYN before establishment");
        NS_TEST_EXPECT_MSG_EQ(m_retransmissionFlagSet,
                              true,
                              "Retried SYN did not carry the retransmission flag");
        NS_TEST_EXPECT_MSG_EQ(m_receiver->GetPdcCount(),
                              1,
                              "Retried SYN allocated more than one target PDC");
        NS_TEST_EXPECT_MSG_EQ(m_completionCount,
                              1,
                              "Retried SYN completed the message incorrectly");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(sourcePdc->GetState()),
                              static_cast<uint32_t>(UetPdcState::ACTIVE),
                              "SYN retry did not activate the initiating PDC");
        NS_TEST_EXPECT_MSG_EQ(m_sender->GetOutstandingPacketCount(m_sourcePdcId),
                              0,
                              "SYN retry retained acknowledged state");

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    uint32_t m_sourcePdcId{0};
    uint32_t m_requestTransmissions{0};
    uint32_t m_retransmissionCount{0};
    uint32_t m_completionCount{0};
    bool m_firstRequestWasSyn{false};
    bool m_retransmissionWasSyn{false};
    bool m_retransmissionFlagSet{false};
};

class UetRudRetryLimitTestCase : public TestCase
{
  public:
    UetRudRetryLimitTestCase()
        : TestCase("RUD retry limit terminates a permanently failed transfer")
    {
    }

  private:
    void DropPacket(Ptr<const Packet>)
    {
        ++m_transmitCount;
    }

    void Timeout(uint32_t, uint32_t)
    {
        ++m_timeoutCount;
    }

    void Retransmission(uint32_t, uint32_t)
    {
        ++m_retransmissionCount;
    }

    void DoRun() override
    {
        auto endpoint = CreateObject<UetEndpoint>();
        endpoint->SetAttribute("EndpointId", UintegerValue(1));
        endpoint->SetAttribute("MaxRetransmissions", UintegerValue(2));
        auto pdc = endpoint->CreatePdc(UetDeliveryMode::RUD);
        pdc->SetAttribute("RetransmissionTimeout", TimeValue(MicroSeconds(1)));
        endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::OPENING);
        endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::ACTIVE);
        endpoint->SetTransmitCallback(MakeCallback(&UetRudRetryLimitTestCase::DropPacket, this));
        endpoint->TraceConnectWithoutContext(
            "Timeout",
            MakeCallback(&UetRudRetryLimitTestCase::Timeout, this));
        endpoint->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&UetRudRetryLimitTestCase::Retransmission, this));

        NS_TEST_EXPECT_MSG_EQ(endpoint->SendRudMessage(2, pdc->GetPdcId(), 88, Create<Packet>(1)),
                              true,
                              "Permanently lost message was not submitted");
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(m_transmitCount, 3, "Unexpected number of wire transmissions");
        NS_TEST_EXPECT_MSG_EQ(m_retransmissionCount, 2, "Retry limit was not enforced");
        NS_TEST_EXPECT_MSG_EQ(m_timeoutCount, 3, "Unexpected timeout count at retry limit");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(pdc->GetState()),
                              static_cast<uint32_t>(UetPdcState::ERROR),
                              "Exhausted PDC did not enter ERROR");
        NS_TEST_EXPECT_MSG_EQ(endpoint->GetOutstandingPacketCount(pdc->GetPdcId()),
                              0,
                              "Failed PDC retained retransmission state");
        Simulator::Destroy();
    }

    uint32_t m_transmitCount{0};
    uint32_t m_timeoutCount{0};
    uint32_t m_retransmissionCount{0};
};

class UetAckCcSackProcessingTestCase : public TestCase
{
  public:
    UetAckCcSackProcessingTestCase()
        : TestCase("ACK_CC CACK and SACK release exactly the acknowledged retransmit state")
    {
    }

  private:
    void DropPacket(Ptr<const Packet>)
    {
    }

    void DoRun() override
    {
        auto sender = CreateObject<UetEndpoint>();
        sender->SetAttribute("EndpointId", UintegerValue(1));
        auto pdc = sender->CreatePdc(UetDeliveryMode::RUD);
        sender->TransitionPdc(pdc->GetPdcId(), UetPdcState::OPENING);
        sender->TransitionPdc(pdc->GetPdcId(), UetPdcState::ACTIVE);
        sender->SetTransmitCallback(
            MakeCallback(&UetAckCcSackProcessingTestCase::DropPacket, this));

        NS_TEST_ASSERT_MSG_EQ(sender->SendRudMessage(2, pdc->GetPdcId(), 1, Create<Packet>(1)),
                              true,
                              "First test Request was rejected");
        NS_TEST_ASSERT_MSG_EQ(sender->SendRudMessage(2, pdc->GetPdcId(), 2, Create<Packet>(1)),
                              true,
                              "Second test Request was rejected");
        NS_TEST_ASSERT_MSG_EQ(sender->SendRudMessage(2, pdc->GetPdcId(), 3, Create<Packet>(1)),
                              true,
                              "Third test Request was rejected");
        NS_TEST_ASSERT_MSG_EQ(sender->GetOutstandingPacketCount(pdc->GetPdcId()),
                              3,
                              "Test setup did not retain three Requests");

        UetPdsHeader ack;
        ack.SetType(UetPdsType::ACK_CC);
        ack.SetNextHeader(UetNextHeader::NONE);
        ack.SetCumulativeAckPsn(1);
        ack.SetAckPsnOffset(0);
        ack.SetSourcePdcId(pdc->GetPdcId());
        ack.SetDestinationPdcId(pdc->GetPdcId());
        ack.SetCcType(0);
        ack.SetMaximumPsnRange(128);
        ack.SetSackPsnOffset(1);      // SACK base is PSN 2
        ack.SetSackBitmap(1ULL << 1); // PSN 3 received; PSN 2 remains outstanding
        ack.SetNsccState(0, false, 0, 1, 1);
        auto packet = Create<Packet>();
        packet->AddHeader(ack);
        UetSimulationTag route;
        route.SetSourceEndpointId(2);
        route.SetDestinationEndpointId(1);
        packet->AddPacketTag(route);

        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(sender->ReceivePacket(packet)),
                              static_cast<uint32_t>(UetReceiveStatus::ACCEPTED),
                              "Valid ACK_CC was rejected");
        NS_TEST_EXPECT_MSG_EQ(sender->GetOutstandingPacketCount(pdc->GetPdcId()),
                              1,
                              "CACK/SACK did not leave exactly one unacknowledged Request");

        Simulator::Destroy();
    }
};

class UetRudMixedLossStressTestCase : public TestCase
{
  public:
    UetRudMixedLossStressTestCase()
        : TestCase("RUD completes a batch under deterministic data loss, ACK loss, and reordering")
    {
    }

  private:
    static constexpr uint32_t MESSAGE_COUNT = 40;

    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader header;
        copy->RemoveHeader(header);
        if (header.GetType() != UetPdsType::RUD_REQUEST)
        {
            return;
        }
        const uint32_t attempt = ++m_dataAttempts[header.GetPsn()];
        if (header.GetPsn() % 7 == 0 && attempt == 1)
        {
            ++m_droppedData;
            return;
        }
        const Time delay = NanoSeconds(200 + (header.GetPsn() % 5) * 150);
        Simulator::Schedule(delay,
                            &UetRudMixedLossStressTestCase::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader header;
        copy->RemoveHeader(header);
        if (header.GetType() == UetPdsType::ACK_CC)
        {
            const uint32_t ackPsn =
                header.GetCumulativeAckPsn() + static_cast<int16_t>(header.GetAckPsnOffset());
            const uint32_t attempt = ++m_ackAttempts[ackPsn];
            if (ackPsn % 11 == 0 && attempt == 1)
            {
                ++m_droppedAcks;
                return;
            }
        }
        Simulator::Schedule(NanoSeconds(200),
                            &UetRudMixedLossStressTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    void MessageComplete(uint32_t, uint64_t messageId, uint32_t bytes, Time)
    {
        NS_TEST_EXPECT_MSG_EQ(bytes, 64, "Stress message payload length changed");
        const bool firstCompletion = m_completedMessages.insert(messageId).second;
        NS_TEST_EXPECT_MSG_EQ(firstCompletion, true, "Stress message completed more than once");
    }

    void Retransmission(uint32_t, uint32_t)
    {
        ++m_retransmissions;
    }

    void Activate(Ptr<UetEndpoint> endpoint, uint32_t pdcId)
    {
        endpoint->TransitionPdc(pdcId, UetPdcState::OPENING);
        endpoint->TransitionPdc(pdcId, UetPdcState::ACTIVE);
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));
        m_sender->SetAttribute("MaxRetransmissions", UintegerValue(32));

        auto senderPdc = m_sender->CreatePdc(UetDeliveryMode::RUD);
        auto receiverPdc = m_receiver->CreatePdc(UetDeliveryMode::RUD);
        senderPdc->SetAttribute("RetransmissionTimeout", TimeValue(MicroSeconds(100)));
        Activate(m_sender, senderPdc->GetPdcId());
        Activate(m_receiver, receiverPdc->GetPdcId());

        m_sender->SetTransmitCallback(
            MakeCallback(&UetRudMixedLossStressTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(
            MakeCallback(&UetRudMixedLossStressTestCase::ReceiverTransmit, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetRudMixedLossStressTestCase::MessageComplete, this));
        m_sender->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&UetRudMixedLossStressTestCase::Retransmission, this));

        for (uint32_t messageId = 1; messageId <= MESSAGE_COUNT; ++messageId)
        {
            NS_TEST_ASSERT_MSG_EQ(
                m_sender->SendRudMessage(2, senderPdc->GetPdcId(), messageId, Create<Packet>(64)),
                true,
                "Stress message submission failed");
        }
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(m_droppedData, 5, "Stress test did not inject all data losses");
        NS_TEST_EXPECT_MSG_EQ(m_droppedAcks, 3, "Stress test did not inject all ACK losses");
        NS_TEST_EXPECT_MSG_GT(m_retransmissions, 0, "Stress test did not exercise retransmission");
        NS_TEST_EXPECT_MSG_EQ(m_completedMessages.size(),
                              MESSAGE_COUNT,
                              "Not every stress message completed");
        NS_TEST_EXPECT_MSG_EQ(m_sender->GetOutstandingPacketCount(senderPdc->GetPdcId()),
                              0,
                              "Stress run retained acknowledged Request state");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(senderPdc->GetState()),
                              static_cast<uint32_t>(UetPdcState::ACTIVE),
                              "Stress run moved the sender PDC out of ACTIVE");

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    std::unordered_map<uint32_t, uint32_t> m_dataAttempts;
    std::unordered_map<uint32_t, uint32_t> m_ackAttempts;
    std::set<uint64_t> m_completedMessages;
    uint32_t m_droppedData{0};
    uint32_t m_droppedAcks{0};
    uint32_t m_retransmissions{0};
};

class UetRodOrderingTestCase : public TestCase
{
  public:
    UetRodOrderingTestCase()
        : TestCase("ROD drops out-of-order packets and recovers in order with Go-Back-N")
    {
    }

  private:
    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        UetPdsHeader header;
        auto copy = packet->Copy();
        copy->RemoveHeader(header);
        if (header.GetType() != UetPdsType::ROD_REQUEST)
        {
            return;
        }

        const bool retransmission = (header.GetFlags() & 0x10) != 0;
        if (!retransmission && header.GetPsn() == 1 && !m_droppedFirstPacket)
        {
            m_droppedFirstPacket = true;
            return;
        }

        Simulator::Schedule(MicroSeconds(1),
                            &UetRodOrderingTestCase::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
        if (retransmission && header.GetPsn() == 2 && !m_duplicateScheduled)
        {
            m_duplicateScheduled = true;
            Simulator::Schedule(NanoSeconds(1500),
                                &UetRodOrderingTestCase::Deliver,
                                this,
                                m_receiver,
                                packet->Copy());
        }
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        UetPdsHeader header;
        auto copy = packet->Copy();
        copy->RemoveHeader(header);
        if (header.GetType() == UetPdsType::NACK)
        {
            ++m_nackCount;
        }
        Simulator::Schedule(MicroSeconds(1),
                            &UetRodOrderingTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    void MessageComplete(uint32_t, uint64_t messageId, uint32_t, Time)
    {
        m_completionOrder.push_back(messageId);
    }

    void Retransmission(uint32_t, uint32_t)
    {
        ++m_retransmissionCount;
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));
        auto senderPdc = m_sender->CreatePdc(UetDeliveryMode::ROD);
        auto receiverPdc = m_receiver->CreatePdc(UetDeliveryMode::ROD);
        m_sender->TransitionPdc(senderPdc->GetPdcId(), UetPdcState::OPENING);
        m_sender->TransitionPdc(senderPdc->GetPdcId(), UetPdcState::ACTIVE);
        m_receiver->TransitionPdc(receiverPdc->GetPdcId(), UetPdcState::OPENING);
        m_receiver->TransitionPdc(receiverPdc->GetPdcId(), UetPdcState::ACTIVE);
        m_sender->SetTransmitCallback(MakeCallback(&UetRodOrderingTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(
            MakeCallback(&UetRodOrderingTestCase::ReceiverTransmit, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetRodOrderingTestCase::MessageComplete, this));
        m_sender->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&UetRodOrderingTestCase::Retransmission, this));

        NS_TEST_EXPECT_MSG_EQ(
            m_sender->SendRodMessage(2, senderPdc->GetPdcId(), 1, Create<Packet>(100)),
            true,
            "First ROD message was rejected");
        NS_TEST_EXPECT_MSG_EQ(
            m_sender->SendRodMessage(2, senderPdc->GetPdcId(), 2, Create<Packet>(100)),
            true,
            "Second ROD message was rejected");
        NS_TEST_EXPECT_MSG_EQ(
            m_sender->SendRodMessage(2, senderPdc->GetPdcId(), 3, Create<Packet>(100)),
            true,
            "Third ROD message was rejected");
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(m_droppedFirstPacket, true, "First packet was not dropped");
        NS_TEST_EXPECT_MSG_EQ(m_duplicateScheduled, true, "Duplicate packet was not injected");
        NS_TEST_EXPECT_MSG_EQ(m_completionOrder.size(), 3, "ROD completion count is incorrect");
        NS_TEST_EXPECT_MSG_EQ(m_completionOrder.at(0), 1, "Second message bypassed first message");
        NS_TEST_EXPECT_MSG_EQ(m_completionOrder.at(1),
                              2,
                              "Second message was not delivered second");
        NS_TEST_EXPECT_MSG_EQ(m_completionOrder.at(2), 3, "Third message was not delivered third");
        NS_TEST_EXPECT_MSG_GT(m_nackCount, 0, "Sequence gap did not generate a NACK");
        NS_TEST_EXPECT_MSG_EQ(m_retransmissionCount,
                              3,
                              "Go-Back-N did not retransmit all outstanding packets");
        NS_TEST_EXPECT_MSG_EQ(m_sender->GetOutstandingPacketCount(senderPdc->GetPdcId()),
                              0,
                              "ROD ACKs left outstanding packets");

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    bool m_droppedFirstPacket{false};
    bool m_duplicateScheduled{false};
    uint32_t m_nackCount{0};
    uint32_t m_retransmissionCount{0};
    std::vector<uint64_t> m_completionOrder;
};

class UetMultiPdcIsolationTestCase : public TestCase
{
  public:
    UetMultiPdcIsolationTestCase()
        : TestCase("RUD loss recovery on one PDC does not block or corrupt another PDC")
    {
    }

  private:
    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader header;
        copy->RemoveHeader(header);
        if (header.GetType() != UetPdsType::RUD_REQUEST)
        {
            return;
        }

        if (header.GetSourcePdcId() == m_delayedPdcId && !m_droppedDelayedPdcPacket)
        {
            m_droppedDelayedPdcPacket = true;
            return;
        }
        Simulator::Schedule(NanoSeconds(100),
                            &UetMultiPdcIsolationTestCase::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        Simulator::Schedule(NanoSeconds(100),
                            &UetMultiPdcIsolationTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    void MessageComplete(uint32_t pdcId, uint64_t messageId, uint32_t bytes, Time)
    {
        NS_TEST_EXPECT_MSG_EQ(bytes, 64, "Multi-PDC payload length changed");
        const bool firstCompletion = m_completionTimes.emplace(messageId, Simulator::Now()).second;
        NS_TEST_EXPECT_MSG_EQ(firstCompletion,
                              true,
                              "A multi-PDC message completed more than once");
        m_completedPdcIds[messageId] = pdcId;
    }

    void Retransmission(uint32_t pdcId, uint32_t)
    {
        ++m_retransmissions[pdcId];
    }

    void Activate(Ptr<UetEndpoint> endpoint, uint32_t pdcId)
    {
        endpoint->TransitionPdc(pdcId, UetPdcState::OPENING);
        endpoint->TransitionPdc(pdcId, UetPdcState::ACTIVE);
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));

        auto delayedSenderPdc = m_sender->CreatePdc(UetDeliveryMode::RUD);
        auto fastSenderPdc = m_sender->CreatePdc(UetDeliveryMode::RUD);
        auto delayedReceiverPdc = m_receiver->CreatePdc(UetDeliveryMode::RUD);
        auto fastReceiverPdc = m_receiver->CreatePdc(UetDeliveryMode::RUD);
        m_delayedPdcId = delayedSenderPdc->GetPdcId();
        m_fastPdcId = fastSenderPdc->GetPdcId();
        NS_TEST_ASSERT_MSG_EQ(delayedReceiverPdc->GetPdcId(),
                              m_delayedPdcId,
                              "Delayed PDC IDs are not aligned for the direct-link test");
        NS_TEST_ASSERT_MSG_EQ(fastReceiverPdc->GetPdcId(),
                              m_fastPdcId,
                              "Fast PDC IDs are not aligned for the direct-link test");
        delayedSenderPdc->SetAttribute("RetransmissionTimeout", TimeValue(MicroSeconds(10)));
        fastSenderPdc->SetAttribute("RetransmissionTimeout", TimeValue(MicroSeconds(10)));
        Activate(m_sender, m_delayedPdcId);
        Activate(m_sender, m_fastPdcId);
        Activate(m_receiver, m_delayedPdcId);
        Activate(m_receiver, m_fastPdcId);

        m_sender->SetTransmitCallback(
            MakeCallback(&UetMultiPdcIsolationTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(
            MakeCallback(&UetMultiPdcIsolationTestCase::ReceiverTransmit, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetMultiPdcIsolationTestCase::MessageComplete, this));
        m_sender->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&UetMultiPdcIsolationTestCase::Retransmission, this));

        NS_TEST_ASSERT_MSG_EQ(m_sender->SendRudMessage(2, m_delayedPdcId, 101, Create<Packet>(64)),
                              true,
                              "Delayed-PDC message submission failed");
        NS_TEST_ASSERT_MSG_EQ(m_sender->SendRudMessage(2, m_fastPdcId, 202, Create<Packet>(64)),
                              true,
                              "Fast-PDC message submission failed");
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(m_completionTimes.size(), 2, "Both PDC messages did not complete");
        NS_TEST_EXPECT_MSG_EQ(m_completedPdcIds[101],
                              m_delayedPdcId,
                              "Delayed message completed on the wrong PDC");
        NS_TEST_EXPECT_MSG_EQ(m_completedPdcIds[202],
                              m_fastPdcId,
                              "Fast message completed on the wrong PDC");
        NS_TEST_EXPECT_MSG_LT(m_completionTimes[202],
                              m_completionTimes[101],
                              "Loss on one PDC blocked completion on the other PDC");
        NS_TEST_EXPECT_MSG_EQ(m_retransmissions[m_delayedPdcId],
                              1,
                              "Delayed PDC did not recover with one retry");
        NS_TEST_EXPECT_MSG_EQ(m_retransmissions[m_fastPdcId],
                              0,
                              "Loss recovery leaked into the unaffected PDC");
        NS_TEST_EXPECT_MSG_EQ(m_sender->GetOutstandingPacketCount(m_delayedPdcId),
                              0,
                              "Delayed PDC retained acknowledged state");
        NS_TEST_EXPECT_MSG_EQ(m_sender->GetOutstandingPacketCount(m_fastPdcId),
                              0,
                              "Fast PDC retained acknowledged state");

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    uint32_t m_delayedPdcId{0};
    uint32_t m_fastPdcId{0};
    bool m_droppedDelayedPdcPacket{false};
    std::unordered_map<uint64_t, Time> m_completionTimes;
    std::unordered_map<uint64_t, uint32_t> m_completedPdcIds;
    std::unordered_map<uint32_t, uint32_t> m_retransmissions;
};

class UetPsnWrapTestCase : public TestCase
{
  public:
    explicit UetPsnWrapTestCase(UetDeliveryMode deliveryMode)
        : TestCase(deliveryMode == UetDeliveryMode::RUD
                       ? "RUD delivery and cumulative ACK cross the 32-bit PSN wrap"
                       : "ROD Go-Back-N recovery crosses the 32-bit PSN wrap"),
          m_deliveryMode(deliveryMode)
    {
    }

  private:
    static constexpr uint32_t START_PSN = UINT32_MAX - 1;

    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader header;
        copy->RemoveHeader(header);
        const UetPdsType expectedType = m_deliveryMode == UetDeliveryMode::RUD
                                            ? UetPdsType::RUD_REQUEST
                                            : UetPdsType::ROD_REQUEST;
        if (header.GetType() != expectedType)
        {
            return;
        }

        const bool retransmission = (header.GetFlags() & 0x10) != 0;
        if (!retransmission)
        {
            m_initialPsns.push_back(header.GetPsn());
        }
        if (m_deliveryMode == UetDeliveryMode::ROD && !retransmission &&
            header.GetPsn() == UINT32_MAX && !m_droppedPsnMax)
        {
            m_droppedPsnMax = true;
            return;
        }
        Simulator::Schedule(NanoSeconds(100),
                            &UetPsnWrapTestCase::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        Simulator::Schedule(NanoSeconds(100),
                            &UetPsnWrapTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    void MessageComplete(uint32_t, uint64_t messageId, uint32_t, Time)
    {
        m_completedMessages.push_back(messageId);
    }

    void Retransmission(uint32_t, uint32_t psn)
    {
        m_retransmittedPsns.push_back(psn);
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));

        auto sourcePdc = m_sender->CreatePdc(m_deliveryMode);
        m_sourcePdcId = sourcePdc->GetPdcId();
        sourcePdc->ConfigureStartPsn(START_PSN);
        sourcePdc->SetAttribute("RetransmissionTimeout", TimeValue(MicroSeconds(20)));
        m_sender->TransitionPdc(m_sourcePdcId, UetPdcState::OPENING);

        m_sender->SetTransmitCallback(MakeCallback(&UetPsnWrapTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(MakeCallback(&UetPsnWrapTestCase::ReceiverTransmit, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetPsnWrapTestCase::MessageComplete, this));
        m_sender->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&UetPsnWrapTestCase::Retransmission, this));

        for (uint64_t messageId = 1; messageId <= 4; ++messageId)
        {
            const bool accepted =
                m_deliveryMode == UetDeliveryMode::RUD
                    ? m_sender->SendRudMessage(2, m_sourcePdcId, messageId, Create<Packet>(64))
                    : m_sender->SendRodMessage(2, m_sourcePdcId, messageId, Create<Packet>(64));
            NS_TEST_ASSERT_MSG_EQ(accepted, true, "Wrap-boundary message submission failed");
        }
        Simulator::Run();

        const std::vector<uint32_t> expectedInitialPsns{UINT32_MAX - 1, UINT32_MAX, 0, 1};
        NS_TEST_EXPECT_MSG_EQ(m_initialPsns == expectedInitialPsns,
                              true,
                              "Sender did not allocate monotonically wrapping PSNs");
        const std::vector<uint64_t> expectedMessages{1, 2, 3, 4};
        NS_TEST_EXPECT_MSG_EQ(m_completedMessages == expectedMessages,
                              true,
                              "Messages did not complete once in PSN order across wrap");
        NS_TEST_EXPECT_MSG_EQ(m_sender->GetOutstandingPacketCount(m_sourcePdcId),
                              0,
                              "Wrap-boundary ACK processing retained retransmit state");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(sourcePdc->GetState()),
                              static_cast<uint32_t>(UetPdcState::ACTIVE),
                              "Wrap-boundary transfer did not establish the PDC");

        if (m_deliveryMode == UetDeliveryMode::RUD)
        {
            NS_TEST_EXPECT_MSG_EQ(m_retransmittedPsns.size(),
                                  0,
                                  "In-order RUD wrap unexpectedly retransmitted a packet");
        }
        else
        {
            const std::vector<uint32_t> expectedRetransmissions{UINT32_MAX, 0, 1};
            NS_TEST_EXPECT_MSG_EQ(m_droppedPsnMax, true, "ROD wrap test did not inject its loss");
            NS_TEST_EXPECT_MSG_EQ(m_retransmittedPsns == expectedRetransmissions,
                                  true,
                                  "ROD Go-Back-N used numeric instead of serial PSN order");
        }

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    UetDeliveryMode m_deliveryMode;
    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    uint32_t m_sourcePdcId{0};
    bool m_droppedPsnMax{false};
    std::vector<uint32_t> m_initialPsns;
    std::vector<uint32_t> m_retransmittedPsns;
    std::vector<uint64_t> m_completedMessages;
};

class UetClearPsnTestCase : public TestCase
{
  public:
    UetClearPsnTestCase()
        : TestCase("Request CLEAR_PSN releases retained response state across PSN wrap")
    {
    }

  private:
    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader header;
        copy->RemoveHeader(header);
        if (header.GetType() != UetPdsType::RUD_REQUEST)
        {
            return;
        }
        m_requestPsns.push_back(header.GetPsn());
        m_clearPsns.push_back(header.GetPsn() + static_cast<int16_t>(header.GetClearPsnOffset()));
        Simulator::Schedule(NanoSeconds(100),
                            &UetClearPsnTestCase::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader header;
        copy->RemoveHeader(header);
        if (header.GetType() == UetPdsType::ACK_CC)
        {
            m_receiverPdcId = header.GetSourcePdcId();
        }
        Simulator::Schedule(NanoSeconds(100),
                            &UetClearPsnTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    void SendMessage(uint64_t messageId)
    {
        NS_TEST_ASSERT_MSG_EQ(
            m_sender->SendRudMessage(2, m_sourcePdcId, messageId, Create<Packet>(64)),
            true,
            "CLEAR_PSN test message submission failed");
    }

    void MessageComplete(uint32_t, uint64_t, uint32_t, Time)
    {
        ++m_completionCount;
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));

        auto sourcePdc = m_sender->CreatePdc(UetDeliveryMode::RUD);
        m_sourcePdcId = sourcePdc->GetPdcId();
        sourcePdc->ConfigureStartPsn(UINT32_MAX);
        m_sender->TransitionPdc(m_sourcePdcId, UetPdcState::OPENING);

        m_sender->SetTransmitCallback(MakeCallback(&UetClearPsnTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(MakeCallback(&UetClearPsnTestCase::ReceiverTransmit, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetClearPsnTestCase::MessageComplete, this));

        SendMessage(1);
        Simulator::Schedule(MicroSeconds(5), &UetClearPsnTestCase::SendMessage, this, 2);
        Simulator::Schedule(MicroSeconds(10), &UetClearPsnTestCase::SendMessage, this, 3);
        Simulator::Run();

        const std::vector<uint32_t> expectedRequestPsns{UINT32_MAX, 0, 1};
        const std::vector<uint32_t> expectedClearPsns{UINT32_MAX - 1, UINT32_MAX, 0};
        NS_TEST_EXPECT_MSG_EQ(m_requestPsns == expectedRequestPsns,
                              true,
                              "CLEAR_PSN test Requests did not cross the PSN wrap");
        NS_TEST_EXPECT_MSG_EQ(m_clearPsns == expectedClearPsns,
                              true,
                              "Requests did not carry the last contiguously received ACK PSN");
        NS_TEST_EXPECT_MSG_EQ(m_completionCount, 3, "CLEAR_PSN test did not complete all messages");
        NS_TEST_ASSERT_MSG_NE(m_receiverPdcId, 0, "CLEAR_PSN test did not learn the target PDCID");
        NS_TEST_EXPECT_MSG_EQ(m_receiver->GetPeerClearPsn(m_receiverPdcId),
                              0,
                              "Target did not advance peer CLEAR_PSN across wrap");
        NS_TEST_EXPECT_MSG_EQ(m_receiver->GetRetainedResponseCount(m_receiverPdcId),
                              1,
                              "CLEAR_PSN did not release older retained responses exactly");

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    uint32_t m_sourcePdcId{0};
    uint32_t m_receiverPdcId{0};
    uint32_t m_completionCount{0};
    std::vector<uint32_t> m_requestPsns;
    std::vector<uint32_t> m_clearPsns;
};

class UetUudDatagramTestCase : public TestCase
{
  public:
    UetUudDatagramTestCase()
        : TestCase("UUD sends one connectionless packet without reliability state")
    {
    }

  private:
    void SenderTransmit(Ptr<const Packet> packet)
    {
        ++m_wirePacketCount;
        UetPdsHeader header;
        auto copy = packet->Copy();
        copy->RemoveHeader(header);
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(header.GetType()),
                              static_cast<uint32_t>(UetPdsType::UUD_REQUEST),
                              "UUD packet used the wrong PDS type");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(header.GetNextHeader()),
                              static_cast<uint32_t>(UetNextHeader::REQUEST_MEDIUM),
                              "UUD packet used the wrong SES format");
        UetSesMediumHeader sesHeader;
        copy->RemoveHeader(sesHeader);
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(sesHeader.GetOpcode()),
                              static_cast<uint32_t>(UetSesOpcode::DATAGRAM_SEND),
                              "UUD packet used the wrong SES opcode");
        NS_TEST_EXPECT_MSG_EQ(packet->GetSize(),
                              UetPdsHeader::UUD_SIZE + UetSesMediumHeader::SERIALIZED_SIZE +
                                  sesHeader.GetRequestLength(),
                              "UUD packet has the wrong wire size");

        if (m_dropNext)
        {
            m_dropNext = false;
            return;
        }
        Simulator::Schedule(MicroSeconds(1),
                            &UetUudDatagramTestCase::Deliver,
                            this,
                            packet->Copy());
    }

    void Deliver(Ptr<const Packet> packet)
    {
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(m_receiver->ReceivePacket(packet)),
                              static_cast<uint32_t>(UetReceiveStatus::ACCEPTED),
                              "Valid UUD datagram was rejected");
    }

    void ReceiverTransmit(Ptr<const Packet>)
    {
        ++m_controlPacketCount;
    }

    void MessageComplete(uint32_t pdcId, uint64_t messageId, uint32_t bytes, Time latency)
    {
        ++m_completionCount;
        m_completedPdcId = pdcId;
        m_completedMessageId = messageId;
        m_completedBytes = bytes;
        m_completionLatency = latency;
    }

    void ReliabilityEvent(uint32_t, uint32_t)
    {
        ++m_reliabilityEventCount;
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));
        m_sender->SetTransmitCallback(MakeCallback(&UetUudDatagramTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(
            MakeCallback(&UetUudDatagramTestCase::ReceiverTransmit, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetUudDatagramTestCase::MessageComplete, this));
        m_sender->TraceConnectWithoutContext(
            "Ack",
            MakeCallback(&UetUudDatagramTestCase::ReliabilityEvent, this));
        m_sender->TraceConnectWithoutContext(
            "Nack",
            MakeCallback(&UetUudDatagramTestCase::ReliabilityEvent, this));
        m_sender->TraceConnectWithoutContext(
            "Timeout",
            MakeCallback(&UetUudDatagramTestCase::ReliabilityEvent, this));
        m_sender->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&UetUudDatagramTestCase::ReliabilityEvent, this));

        NS_TEST_EXPECT_MSG_EQ(m_sender->CreatePdc(UetDeliveryMode::UUD) == nullptr,
                              true,
                              "UUD incorrectly allocated a PDC");
        NS_TEST_EXPECT_MSG_EQ(m_sender->SendUudDatagram(2, Create<Packet>(4097)),
                              false,
                              "Oversized UUD datagram was accepted");
        NS_TEST_EXPECT_MSG_EQ(m_sender->SendUudDatagram(2, Create<Packet>(64)),
                              true,
                              "First UUD datagram was rejected");
        NS_TEST_EXPECT_MSG_EQ(m_sender->SendUudDatagram(2, Create<Packet>(128)),
                              true,
                              "Second UUD datagram was rejected");
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(m_wirePacketCount, 2, "UUD transmitted an unexpected packet count");
        NS_TEST_EXPECT_MSG_EQ(m_completionCount,
                              1,
                              "Lost UUD datagram was recovered or duplicated");
        NS_TEST_EXPECT_MSG_EQ(m_completedPdcId, 0, "UUD completion used a PDC");
        NS_TEST_EXPECT_MSG_EQ(m_completedMessageId, 0, "UUD completion used a message ID");
        NS_TEST_EXPECT_MSG_EQ(m_completedBytes, 128, "UUD completion byte count changed");
        NS_TEST_EXPECT_MSG_GT(m_completionLatency, NanoSeconds(0), "UUD latency was not measured");
        NS_TEST_EXPECT_MSG_EQ(m_controlPacketCount, 0, "UUD receiver generated ACK/NACK traffic");
        NS_TEST_EXPECT_MSG_EQ(m_reliabilityEventCount, 0, "UUD emitted a reliability event");
        NS_TEST_EXPECT_MSG_EQ(m_sender->GetOutstandingPacketCount(0),
                              0,
                              "UUD created retransmission state");

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    bool m_dropNext{true};
    uint32_t m_wirePacketCount{0};
    uint32_t m_controlPacketCount{0};
    uint32_t m_completionCount{0};
    uint32_t m_completedPdcId{0};
    uint64_t m_completedMessageId{0};
    uint32_t m_completedBytes{0};
    Time m_completionLatency{Seconds(0)};
    uint32_t m_reliabilityEventCount{0};
};

class UetSesEndToEndTestCase : public TestCase
{
  public:
    UetSesEndToEndTestCase()
        : TestCase("AI Base SES WRITE Immediate and atomic execute end-to-end over RUD")
    {
    }

  private:
    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        Simulator::Schedule(NanoSeconds(100),
                            &UetSesEndToEndTestCase::Deliver,
                            this,
                            m_receiver,
                            packet->Copy());
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        Simulator::Schedule(NanoSeconds(100),
                            &UetSesEndToEndTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    UetSesStandardHeader MakeRequest(UetSesOpcode opcode, uint16_t messageId) const
    {
        UetSesStandardHeader request;
        request.SetOpcode(opcode);
        request.SetMessageId(messageId);
        request.SetRelativeAddressing(true);
        request.SetRiGeneration(3);
        request.SetJobId(0x123456);
        request.SetPidOnFep(0x123);
        request.SetResourceIndex(0x234);
        request.SetInitiator(0xabcdef01);
        request.SetMemoryKey(m_memoryKey);
        return request;
    }

    void SendWrite()
    {
        const std::array<uint8_t, 8> value{0, 0, 0, 0, 0, 0, 0, 5};
        auto request = MakeRequest(UetSesOpcode::WRITE, 2);
        request.SetHeaderDataPresent(true);
        request.SetHeaderData(0xfeedface);
        request.SetDeliveryComplete(true);
        NS_TEST_ASSERT_MSG_EQ(m_sender->SendSesTransaction(2,
                                                          m_sourcePdcId,
                                                          UetDeliveryMode::RUD,
                                                          request,
                                                          Create<Packet>(value.data(), value.size())),
                              true,
                              "End-to-end WRITE submission failed");
    }

    void SendAtomic()
    {
        const std::array<uint8_t, 8> value{0, 0, 0, 0, 0, 0, 0, 5};
        auto request = MakeRequest(UetSesOpcode::ATOMIC, 3);
        UetAtomicExtensionHeader atomic;
        atomic.SetAtomicOpcode(UetAtomicOpcode::SUM);
        atomic.SetAtomicDatatype(UetAtomicDatatype::UINT64);
        NS_TEST_ASSERT_MSG_EQ(m_sender->SendSesTransaction(2,
                                                          m_sourcePdcId,
                                                          UetDeliveryMode::RUD,
                                                          request,
                                                          Create<Packet>(value.data(), value.size()),
                                                          &atomic),
                              true,
                              "End-to-end atomic submission failed");
    }

    void Close()
    {
        m_closeAccepted = m_sender->ClosePdc(2, m_sourcePdcId);
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));
        auto sourcePdc = m_sender->CreatePdc(UetDeliveryMode::RUD);
        m_sourcePdcId = sourcePdc->GetPdcId();
        m_sender->TransitionPdc(m_sourcePdcId, UetPdcState::OPENING);
        m_sender->SetTransmitCallback(MakeCallback(&UetSesEndToEndTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(
            MakeCallback(&UetSesEndToEndTestCase::ReceiverTransmit, this));
        m_bufferHandle = m_receiver->GetSesEngine()->RegisterBuffer(
            true,
            0x123456,
            0x123,
            0x234,
            3,
            m_memoryKey,
            0xabcdef01,
            UET_SES_ACCESS_WRITE | UET_SES_ACCESS_ATOMIC,
            64);
        NS_TEST_ASSERT_MSG_NE(m_bufferHandle, 0, "Target memory registration failed");

        NS_TEST_ASSERT_MSG_EQ(m_sender->SendRudMessage(2,
                                                      m_sourcePdcId,
                                                      1,
                                                      Create<Packet>(1)),
                              true,
                              "PDC establishment message failed");
        Simulator::Schedule(MicroSeconds(2), &UetSesEndToEndTestCase::SendWrite, this);
        Simulator::Schedule(MicroSeconds(4), &UetSesEndToEndTestCase::SendAtomic, this);
        Simulator::Schedule(MicroSeconds(8), &UetSesEndToEndTestCase::Close, this);
        Simulator::Run();

        std::array<uint8_t, 8> actual{};
        NS_TEST_ASSERT_MSG_EQ(m_receiver->GetSesEngine()->ReadBuffer(m_bufferHandle,
                                                                     0,
                                                                     actual.data(),
                                                                     actual.size()),
                              true,
                              "Target memory readback failed");
        const std::array<uint8_t, 8> expected{0, 0, 0, 0, 0, 0, 0, 10};
        NS_TEST_EXPECT_MSG_EQ(actual == expected, true, "End-to-end SES result is incorrect");
        UetSesResponseHeader writeResponse;
        NS_TEST_ASSERT_MSG_EQ(m_sender->GetSesResponse(2, writeResponse),
                              true,
                              "WRITE semantic response was not returned on the wire");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(writeResponse.GetReturnCode()),
                              static_cast<uint8_t>(UetSesReturnCode::OK),
                              "WRITE semantic response reported an error");
        NS_TEST_EXPECT_MSG_EQ(writeResponse.GetModifiedLength(),
                              8,
                              "WRITE response modified length is incorrect");
        UetSesResponseHeader atomicResponse;
        NS_TEST_ASSERT_MSG_EQ(m_sender->GetSesResponse(3, atomicResponse),
                              true,
                              "Atomic semantic response was not returned on the wire");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(atomicResponse.GetReturnCode()),
                              static_cast<uint8_t>(UetSesReturnCode::OK),
                              "Atomic semantic response reported an error");
        NS_TEST_EXPECT_MSG_EQ(m_closeAccepted, true, "PDC close command was rejected");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(sourcePdc->GetState()),
                              static_cast<uint8_t>(UetPdcState::CLOSED),
                              "PDC close handshake did not complete");

        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    uint32_t m_sourcePdcId{0};
    uint32_t m_bufferHandle{0};
    uint64_t m_memoryKey{0x0102030405060708ULL};
    bool m_closeAccepted{false};
};

class UetTrimRecoveryTestCase : public TestCase
{
  public:
    UetTrimRecoveryTestCase()
        : TestCase("AI Base endpoint converts a trimmed request into UET_TRIMMED recovery")
    {
    }

  private:
    void Deliver(Ptr<UetEndpoint> endpoint, Ptr<const Packet> packet)
    {
        endpoint->ReceivePacket(packet);
    }

    void SenderTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader header;
        copy->RemoveHeader(header);
        Ptr<Packet> delivered = packet->Copy();
        if (!m_injected && header.GetType() == UetPdsType::RUD_REQUEST)
        {
            m_injected = true;
            UetSimulationTag route;
            delivered->RemovePacketTag(route);
            route.SetTrimmed(true);
            route.SetOriginalPayloadLength(copy->GetSize());
            delivered = delivered->CreateFragment(0, UetPdsHeader::REQUEST_SIZE);
            delivered->AddPacketTag(route);
        }
        Simulator::Schedule(MicroSeconds(1),
                            &UetTrimRecoveryTestCase::Deliver,
                            this,
                            m_receiver,
                            delivered);
    }

    void ReceiverTransmit(Ptr<const Packet> packet)
    {
        auto copy = packet->Copy();
        UetPdsHeader header;
        copy->RemoveHeader(header);
        if (header.GetType() == UetPdsType::NACK)
        {
            m_nackCode = header.GetNackCode();
        }
        Simulator::Schedule(MicroSeconds(1),
                            &UetTrimRecoveryTestCase::Deliver,
                            this,
                            m_sender,
                            packet->Copy());
    }

    void Trimmed(Ptr<const Packet>, uint32_t, uint32_t)
    {
        ++m_trimTraceCount;
    }

    void Complete(uint32_t, uint64_t, uint32_t, Time)
    {
        ++m_completionCount;
    }

    void Retransmission(uint32_t, uint32_t)
    {
        ++m_retransmissionCount;
    }

    void DoRun() override
    {
        m_sender = CreateObject<UetEndpoint>();
        m_receiver = CreateObject<UetEndpoint>();
        m_sender->SetAttribute("EndpointId", UintegerValue(1));
        m_receiver->SetAttribute("EndpointId", UintegerValue(2));
        auto source = m_sender->CreatePdc(UetDeliveryMode::RUD);
        auto target = m_receiver->CreatePdc(UetDeliveryMode::RUD);
        m_sender->TransitionPdc(source->GetPdcId(), UetPdcState::OPENING);
        m_sender->TransitionPdc(source->GetPdcId(), UetPdcState::ACTIVE);
        m_receiver->TransitionPdc(target->GetPdcId(), UetPdcState::OPENING);
        m_receiver->TransitionPdc(target->GetPdcId(), UetPdcState::ACTIVE);
        source->SetAttribute("RetransmissionTimeout", TimeValue(MicroSeconds(50)));
        m_sender->SetTransmitCallback(MakeCallback(&UetTrimRecoveryTestCase::SenderTransmit, this));
        m_receiver->SetTransmitCallback(
            MakeCallback(&UetTrimRecoveryTestCase::ReceiverTransmit, this));
        m_receiver->TraceConnectWithoutContext(
            "PacketTrimmed",
            MakeCallback(&UetTrimRecoveryTestCase::Trimmed, this));
        m_receiver->TraceConnectWithoutContext(
            "MessageComplete",
            MakeCallback(&UetTrimRecoveryTestCase::Complete, this));
        m_sender->TraceConnectWithoutContext(
            "Retransmission",
            MakeCallback(&UetTrimRecoveryTestCase::Retransmission, this));

        NS_TEST_ASSERT_MSG_EQ(m_sender->SendRudMessage(2,
                                                      source->GetPdcId(),
                                                      91,
                                                      Create<Packet>(512)),
                              true,
                              "Trim recovery submission failed");
        Simulator::Run();
        NS_TEST_EXPECT_MSG_EQ(m_injected, true, "Test did not inject a trimmed request");
        NS_TEST_EXPECT_MSG_EQ(+m_nackCode, 0x01, "Receiver did not emit UET_TRIMMED");
        NS_TEST_EXPECT_MSG_EQ(m_trimTraceCount, 1, "Trim trace was not emitted exactly once");
        NS_TEST_EXPECT_MSG_EQ(m_retransmissionCount, 1, "Trimmed packet was not retransmitted once");
        NS_TEST_EXPECT_MSG_EQ(m_completionCount, 1, "Retransmitted message did not complete");
        NS_TEST_EXPECT_MSG_EQ(m_sender->GetOutstandingPacketCount(source->GetPdcId()),
                              0,
                              "Trim recovery left an outstanding packet");
        Simulator::Destroy();
        m_sender = nullptr;
        m_receiver = nullptr;
    }

    Ptr<UetEndpoint> m_sender;
    Ptr<UetEndpoint> m_receiver;
    bool m_injected{false};
    uint8_t m_nackCode{0};
    uint32_t m_trimTraceCount{0};
    uint32_t m_completionCount{0};
    uint32_t m_retransmissionCount{0};
};

class UetUdpTopologyTestCase : public TestCase
{
  public:
    explicit UetUdpTopologyTestCase(bool ipv6)
        : TestCase(ipv6 ? "UEC reliable delivery crosses a real IPv6-UDP point-to-point topology"
                        : "UEC reliable delivery crosses a real IPv4-UDP point-to-point topology"),
          m_ipv6(ipv6)
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        PointToPointHelper link;
        link.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
        link.SetDeviceAttribute("Mtu", UintegerValue(9000));
        link.SetChannelAttribute("Delay", StringValue("2us"));
        auto devices = link.Install(nodes);
        InternetStackHelper internet;
        internet.Install(nodes);
        auto sender = CreateObject<UetEndpoint>();
        auto receiver = CreateObject<UetEndpoint>();
        sender->SetAttribute("EndpointId", UintegerValue(1));
        receiver->SetAttribute("EndpointId", UintegerValue(2));
        auto senderTransport = CreateObject<UetUdpTransport>();
        auto receiverTransport = CreateObject<UetUdpTransport>();
        if (m_ipv6)
        {
            Ipv6AddressHelper addresses;
            addresses.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
            const auto interfaces = addresses.Assign(devices);
            NS_TEST_ASSERT_MSG_EQ(senderTransport->Bind6(nodes.Get(0), sender),
                                  true,
                                  "Sender IPv6/UDP binding failed");
            NS_TEST_ASSERT_MSG_EQ(receiverTransport->Bind6(nodes.Get(1), receiver),
                                  true,
                                  "Receiver IPv6/UDP binding failed");
            senderTransport->AddPeer(2, interfaces.GetAddress(1, 1));
            receiverTransport->AddPeer(1, interfaces.GetAddress(0, 1));
        }
        else
        {
            Ipv4AddressHelper addresses;
            addresses.SetBase("10.1.0.0", "255.255.255.0");
            const auto interfaces = addresses.Assign(devices);
            NS_TEST_ASSERT_MSG_EQ(senderTransport->Bind(nodes.Get(0), sender),
                                  true,
                                  "Sender IPv4/UDP binding failed");
            NS_TEST_ASSERT_MSG_EQ(receiverTransport->Bind(nodes.Get(1), receiver),
                                  true,
                                  "Receiver IPv4/UDP binding failed");
            senderTransport->AddPeer(2, interfaces.GetAddress(1));
            receiverTransport->AddPeer(1, interfaces.GetAddress(0));
        }

        auto pdc = sender->CreatePdc(UetDeliveryMode::RUD);
        sender->TransitionPdc(pdc->GetPdcId(), UetPdcState::OPENING);
        bool sendAccepted = false;
        Simulator::ScheduleNow([&sendAccepted, sender, pdc]() {
            sendAccepted = sender->SendRudMessage(2,
                                                  pdc->GetPdcId(),
                                                  0x55,
                                                  Create<Packet>(8192));
        });
        Simulator::Stop(MilliSeconds(5));
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(sendAccepted, true, "UDP-backed RUD send failed");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(pdc->GetState()),
                              static_cast<uint8_t>(UetPdcState::ACTIVE),
                              "PDC was not established across IPv4/UDP");
        NS_TEST_EXPECT_MSG_EQ(sender->GetOutstandingPacketCount(pdc->GetPdcId()),
                              0,
                              "UDP-backed RUD packets were not acknowledged");
        NS_TEST_EXPECT_MSG_GT(senderTransport->GetTransmittedDatagrams(),
                              0,
                              "Sender emitted no UDP datagram");
        NS_TEST_EXPECT_MSG_GT(receiverTransport->GetReceivedDatagrams(),
                              0,
                              "Receiver accepted no UDP datagram");
        NS_TEST_EXPECT_MSG_GT(receiverTransport->GetTransmittedDatagrams(),
                              0,
                              "Receiver emitted no acknowledgement datagram");
        NS_TEST_EXPECT_MSG_GT(senderTransport->GetReceivedDatagrams(),
                              0,
                              "Sender accepted no acknowledgement datagram");

        Simulator::Destroy();
    }

    bool m_ipv6;
};

class UetTrimmedPacketRulesTestCase : public TestCase
{
  public:
    UetTrimmedPacketRulesTestCase()
        : TestCase("Trimmed SYN and control traffic cannot create or mutate PDC state")
    {
    }

  private:
    Ptr<Packet> MakeTrimmed(UetPdsType type, uint8_t flags, uint16_t destinationPdc = 0)
    {
        UetPdsHeader header;
        header.SetType(type);
        header.SetNextHeader(UetNextHeader::NONE);
        header.SetFlags(flags);
        header.SetPsn(77);
        header.SetSourcePdcId(12);
        header.SetDestinationPdcId(destinationPdc);
        auto packet = Create<Packet>();
        packet->AddHeader(header);
        UetSimulationTag route;
        route.SetSourceEndpointId(1);
        route.SetDestinationEndpointId(2);
        route.SetTrimmed(true);
        packet->AddPacketTag(route);
        return packet;
    }

    void Transmit(Ptr<const Packet>)
    {
        ++m_transmissions;
    }

    void DoRun() override
    {
        auto endpoint = CreateObject<UetEndpoint>();
        endpoint->SetAttribute("EndpointId", UintegerValue(2));
        endpoint->SetTransmitCallback(MakeCallback(&UetTrimmedPacketRulesTestCase::Transmit, this));

        NS_TEST_EXPECT_MSG_EQ(
            static_cast<uint8_t>(endpoint->ReceivePacket(
                MakeTrimmed(UetPdsType::RUD_REQUEST, 0x04))),
            static_cast<uint8_t>(UetReceiveStatus::UNKNOWN_PDC),
            "A trimmed SYN was not rejected");
        NS_TEST_EXPECT_MSG_EQ(endpoint->GetPdcCount(), 0, "A trimmed SYN allocated a PDC");

        auto pdc = endpoint->CreatePdc(UetDeliveryMode::RUD);
        endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::OPENING);
        endpoint->TransitionPdc(pdc->GetPdcId(), UetPdcState::ACTIVE);
        NS_TEST_EXPECT_MSG_EQ(
            static_cast<uint8_t>(endpoint->ReceivePacket(
                MakeTrimmed(UetPdsType::CONTROL, 0, pdc->GetPdcId()))),
            static_cast<uint8_t>(UetReceiveStatus::ACCEPTED),
            "Trimmed control packet was not silently discarded");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint8_t>(pdc->GetState()),
                              static_cast<uint8_t>(UetPdcState::ACTIVE),
                              "Trimmed control packet mutated PDC state");
        NS_TEST_EXPECT_MSG_EQ(m_transmissions, 0, "Trimmed control packet elicited a response");
    }

    uint32_t m_transmissions{0};
};

class UetTestSuite : public TestSuite
{
  public:
    UetTestSuite()
        : TestSuite("uet", Type::UNIT)
    {
        AddTestCase(new UetWireHeaderLayoutTestCase, Duration::QUICK);
        AddTestCase(new UetSesEngineTestCase, Duration::QUICK);
        AddTestCase(new UetNsccAlgorithmTestCase, Duration::QUICK);
        AddTestCase(new UetEndpointDefaultsTestCase, Duration::QUICK);
        AddTestCase(new UetTraceContractTestCase, Duration::QUICK);
        AddTestCase(new UetPdcLifecycleTestCase, Duration::QUICK);
        AddTestCase(new UetPdcEstablishmentTestCase, Duration::QUICK);
        AddTestCase(new UetLostSynRetransmissionTestCase, Duration::QUICK);
        AddTestCase(new UetPacketDispatchTestCase, Duration::QUICK);
        AddTestCase(new UetRudRecoveryTestCase(UetRudRecoveryTestCase::RecoveryMode::NACK),
                    Duration::QUICK);
        AddTestCase(new UetRudRecoveryTestCase(UetRudRecoveryTestCase::RecoveryMode::TIMEOUT),
                    Duration::QUICK);
        AddTestCase(new UetRudRetryLimitTestCase, Duration::QUICK);
        AddTestCase(new UetAckCcSackProcessingTestCase, Duration::QUICK);
        AddTestCase(new UetRudMixedLossStressTestCase, Duration::QUICK);
        AddTestCase(new UetRodOrderingTestCase, Duration::QUICK);
        AddTestCase(new UetMultiPdcIsolationTestCase, Duration::QUICK);
        AddTestCase(new UetPsnWrapTestCase(UetDeliveryMode::RUD), Duration::QUICK);
        AddTestCase(new UetPsnWrapTestCase(UetDeliveryMode::ROD), Duration::QUICK);
        AddTestCase(new UetClearPsnTestCase, Duration::QUICK);
        AddTestCase(new UetUudDatagramTestCase, Duration::QUICK);
        AddTestCase(new UetSesEndToEndTestCase, Duration::QUICK);
        AddTestCase(new UetTrimRecoveryTestCase, Duration::QUICK);
        AddTestCase(new UetTrimmedPacketRulesTestCase, Duration::QUICK);
        AddTestCase(new UetUdpTopologyTestCase(false), Duration::QUICK);
        AddTestCase(new UetUdpTopologyTestCase(true), Duration::QUICK);
    }
};

static UetTestSuite g_uetTestSuite;
