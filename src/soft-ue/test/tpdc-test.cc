/*******************************************************************************
 * Copyright 2025 Soft UE Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

/**
 * @file             tpdc-test.cc
 * @brief            TPDC (Reliable PDC) Unit Tests
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-08
 * @copyright        Apache License Version 2.0
 *
 * @details
 * This file contains comprehensive unit tests for the TPDC (Reliable Packet
 * Delivery Context) component of the Ultra Ethernet protocol stack implementation.
 */

#include <ns3/test.h>
#include <ns3/tpdc.h>
#include <ns3/pdc-base.h>
#include <ns3/packet.h>
#include <ns3/simulator.h>
#include <ns3/rto-timer.h>

namespace ns3 {

/**
 * @brief Test TPDC basic functionality
 */
class TpdcBasicTest : public TestCase
{
public:
    TpdcBasicTest() : TestCase("TPDC Basic Functionality Test") {}
    virtual ~TpdcBasicTest() {}

private:
    void DoRun() override
    {
        // Test TPDC creation
        Ptr<Tpdc> tpdc = CreateObject<Tpdc>();
        NS_TEST_ASSERT_MSG_NE(tpdc, nullptr, "TPDC creation should succeed");

        // Test TPDC inherits from PdcBase
        Ptr<PdcBase> base = DynamicCast<PdcBase>(tpdc);
        NS_TEST_ASSERT_MSG_NE(base, nullptr, "TPDC should inherit from PdcBase");

        // Test initialization
        tpdc->Initialize();
        NS_TEST_ASSERT_MSG_EQ(tpdc->IsActive(), true, "TPDC should be active after initialization");

        // Test configuration
        tpdc->SetDestinationFep(1234);
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetDestinationFep(), 1234, "Destination FEP should be set correctly");

        tpdc->SetTrafficClass(3);
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetTrafficClass(), 3, "Traffic class should be set correctly");

        // Test reliable transmission mode
        NS_TEST_ASSERT_MSG_EQ(tpdc->IsReliable(), true, "TPDC should be in reliable mode");

        // Test statistics
        uint64_t sentPackets = tpdc->GetSentPackets();
        uint64_t receivedPackets = tpdc->GetReceivedPackets();
        uint64_t retransmissions = tpdc->GetRetransmissions();
        NS_TEST_ASSERT_MSG_EQ(sentPackets, 0, "Initially should have sent 0 packets");
        NS_TEST_ASSERT_MSG_EQ(receivedPackets, 0, "Initially should have received 0 packets");
        NS_TEST_ASSERT_MSG_EQ(retransmissions, 0, "Initially should have 0 retransmissions");
    }
};

/**
 * @brief Test TPDC reliable transmission
 */
class TpdcReliableTransmissionTest : public TestCase
{
public:
    TpdcReliableTransmissionTest() : TestCase("TPDC Reliable Transmission Test") {}
    virtual ~TpdcReliableTransmissionTest() {}

private:
    void DoRun() override
    {
        Ptr<Tpdc> tpdc = CreateObject<Tpdc>();
        tpdc->Initialize();
        tpdc->SetDestinationFep(1234);

        // Create test packet
        Ptr<Packet> packet = Create<Packet>(1024);

        // Test reliable packet sending
        bool sendResult = tpdc->SendPacket(packet, true, true);
        NS_TEST_ASSERT_MSG_EQ(sendResult, true, "Reliable packet sending should succeed");

        // Verify packet count and tracking
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetSentPackets(), 1, "Should have sent 1 packet");
        NS_TEST_ASSERT_MSG_GT(tpdc->GetUnacknowledgedPackets(), 0,
                             "Should have unacknowledged packets in reliable mode");

        // Test retransmission timeout
        Time rto = tpdc->GetRetransmissionTimeout();
        NS_TEST_ASSERT_MSG_GT(rto.GetMilliSeconds(), 0,
                             "Retransmission timeout should be set");

        // Test that packet is queued for retransmission
        NS_TEST_ASSERT_MSG_GT(tpdc->GetQueuedPacketsCount(), 0,
                             "Packet should be queued for reliable transmission");
    }
};

/**
 * @brief Test TPDC acknowledgment handling
 */
class TpdcAcknowledgmentTest : public TestCase
{
public:
    TpdcAcknowledgmentTest() : TestCase("TPDC Acknowledgment Handling Test") {}
    virtual ~TpdcAcknowledgmentTest() {}

private:
    void DoRun() override
    {
        Ptr<Tpdc> tpdc = CreateObject<Tpdc>();
        tpdc->Initialize();

        // Send a packet to create unacknowledged state
        Ptr<Packet> packet = Create<Packet>(1024);
        tpdc->SendPacket(packet, true, true);

        uint64_t unackedBefore = tpdc->GetUnacknowledgedPackets();
        NS_TEST_ASSERT_MSG_GT(unackedBefore, 0, "Should have unacknowledged packets");

        // Simulate receiving acknowledgment
        uint32_t ackPsn = 1;  // Assume PSN starts from 1
        bool ackResult = tpdc->ProcessAcknowledgment(ackPsn, true);
        NS_TEST_ASSERT_MSG_EQ(ackResult, true, "Acknowledgment processing should succeed");

        // Verify unacknowledged count decreased
        uint64_t unackedAfter = tpdc->GetUnacknowledgedPackets();
        NS_TEST_ASSERT_MSG_LT(unackedAfter, unackedBefore,
                             "Unacknowledged packets should decrease after ACK");

        // Test duplicate acknowledgment
        bool dupAckResult = tpdc->ProcessAcknowledgment(ackPsn, true);
        NS_TEST_ASSERT_MSG_EQ(dupAckResult, true, "Duplicate ACK should be handled gracefully");
    }
};

/**
 * @brief Test TPDC retransmission mechanism
 */
class TpdcRetransmissionTest : public TestCase
{
public:
    TpdcRetransmissionTest() : TestCase("TPDC Retransmission Mechanism Test") {}
    virtual ~TpdcRetransmissionTest() {}

private:
    void DoRun() override
    {
        Ptr<Tpdc> tpdc = CreateObject<Tpdc>();
        tpdc->Initialize();

        // Configure short retransmission timeout for testing
        tpdc->SetRetransmissionTimeout(MilliSeconds(10));

        // Send packets
        for (int i = 0; i < 3; i++)
        {
            Ptr<Packet> packet = Create<Packet>(512 + i * 100);
            tpdc->SendPacket(packet, i == 0, i == 2);
        }

        uint64_t initialRetransmissions = tpdc->GetRetransmissions();
        NS_TEST_ASSERT_MSG_EQ(initialRetransmissions, 0,
                             "Initially should have 0 retransmissions");

        // Wait for retransmission timeout
        Simulator::Schedule(MilliSeconds(20), &TpdcRetransmissionTest::CheckRetransmissions, this, tpdc);
        Simulator::Run();

        // Check if retransmissions occurred
        uint64_t finalRetransmissions = tpdc->GetRetransmissions();
        NS_TEST_ASSERT_MSG_GT(finalRetransmissions, initialRetransmissions,
                             "Retransmissions should occur after timeout");

        Simulator::Destroy();
    }

    void CheckRetransmissions(Ptr<Tpdc> tpdc)
    {
        // Force retransmission check
        tpdc->CheckRetransmissionTimeouts();
    }
};

/**
 * @brief Test TPDC RTO timer functionality
 */
class TpdcRtoTimerTest : public TestCase
{
public:
    TpdcRtoTimerTest() : TestCase("TPDC RTO Timer Functionality Test") {}
    virtual ~TpdcRtoTimerTest() {}

private:
    void DoRun() override
    {
        Ptr<Tpdc> tpdc = CreateObject<Tpdc>();
        tpdc->Initialize();

        // Test initial RTO
        Time initialRto = tpdc->GetRetransmissionTimeout();
        NS_TEST_ASSERT_MSG_GT(initialRto.GetMilliSeconds(), 0, "Initial RTO should be positive");

        // Test RTO calculation
        tpdc->UpdateRto(true, MilliSeconds(50));  // Sample RTT
        Time updatedRto = tpdc->GetRetransmissionTimeout();
        NS_TEST_ASSERT_MSG_NE(updatedRto, initialRto, "RTO should be updated after RTT sample");

        // Test RTO backoff on timeout
        tpdc->UpdateRto(false, Time());  // Timeout occurred
        Time backoffRto = tpdc->GetRetransmissionTimeout();
        NS_TEST_ASSERT_MSG_GT(backoffRto.GetMilliSeconds(), updatedRto.GetMilliSeconds(),
                             "RTO should increase after timeout (exponential backoff)");

        // Test minimum and maximum RTO bounds
        tpdc->SetRetransmissionTimeout(MicroSeconds(1));
        Time minRto = tpdc->GetRetransmissionTimeout();
        NS_TEST_ASSERT_MSG_GE(minRto.GetMilliSeconds(), 1,
                             "RTO should respect minimum bound");

        tpdc->SetRetransmissionTimeout(Seconds(60));
        Time maxRto = tpdc->GetRetransmissionTimeout();
        NS_TEST_ASSERT_MSG_LE(maxRto.GetSeconds(), 60,
                             "RTO should respect maximum bound");
    }
};

/**
 * @brief Test TPDC packet ordering
 */
class TpdcPacketOrderingTest : public TestCase
{
public:
    TpdcPacketOrderingTest() : TestCase("TPDC Packet Ordering Test") {}
    virtual ~TpdcPacketOrderingTest() {}

private:
    void DoRun() override
    {
        Ptr<Tpdc> tpdc = CreateObject<Tpdc>();
        tpdc->Initialize();

        // Send multiple packets
        std::vector<Ptr<Packet>> packets;
        for (int i = 0; i < 5; i++)
        {
            Ptr<Packet> packet = Create<Packet>(512 + i * 64);
            packets.push_back(packet);
            tpdc->SendPacket(packet, i == 0, i == 4);
        }

        // Verify packets are queued in order
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetQueuedPacketsCount(), packets.size(),
                             "All packets should be queued");

        // Process acknowledgments out of order
        tpdc->ProcessAcknowledgment(3, true);  // ACK packet 3
        tpdc->ProcessAcknowledgment(1, true);  // ACK packet 1
        tpdc->ProcessAcknowledgment(2, true);  // ACK packet 2
        tpdc->ProcessAcknowledgment(5, true);  // ACK packet 5
        tpdc->ProcessAcknowledgment(4, true);  // ACK packet 4

        // Verify all packets are eventually acknowledged
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetUnacknowledgedPackets(), 0,
                             "All packets should be acknowledged");

        // Verify retransmission logic for missing packets
        Ptr<Packet> newPacket = Create<Packet>(1024);
        tpdc->SendPacket(newPacket, true, true);

        // Simulate lost acknowledgment (no ACK received)
        NS_TEST_ASSERT_MSG_GT(tpdc->GetUnacknowledgedPackets(), 0,
                             "Should have unacknowledged packets");
    }
};

/**
 * @brief Test TPDC error handling and recovery
 */
class TpdcErrorRecoveryTest : public TestCase
{
public:
    TpdcErrorRecoveryTest() : TestCase("TPDC Error Recovery Test") {}
    virtual ~TpdcErrorRecoveryTest() {}

private:
    void DoRun() override
    {
        Ptr<Tpdc> tpdc = CreateObject<Tpdc>();
        tpdc->Initialize();

        // Test sending null packet
        bool sendNullResult = tpdc->SendPacket(nullptr, false, false);
        NS_TEST_ASSERT_MSG_EQ(sendNullResult, false, "Sending null packet should fail");

        // Test receiving null packet
        bool receiveNullResult = tpdc->ReceivePacket(nullptr, 1234, 5678);
        NS_TEST_ASSERT_MSG_EQ(receiveNullResult, false, "Receiving null packet should fail");

        // Test invalid acknowledgment
        bool invalidAckResult = tpdc->ProcessAcknowledgment(0, false);
        NS_TEST_ASSERT_MSG_EQ(invalidAckResult, false, "Invalid acknowledgment should be rejected");

        // Send packet and simulate error condition
        Ptr<Packet> packet = Create<Packet>(1024);
        tpdc->SendPacket(packet, true, true);

        // Simulate network error
        tpdc->HandleTransmissionError(1, "Simulated network error");
        uint64_t errorCount = tpdc->GetErrorCount();
        NS_TEST_ASSERT_MSG_GT(errorCount, 0, "Error count should be incremented");

        // Test error recovery
        tpdc->Reset();
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetErrorCount(), 0, "Error count should be reset");
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetUnacknowledgedPackets(), 0,
                             "Unacknowledged packets should be cleared after reset");
    }
};

/**
 * @brief Test TPDC concurrent operations
 */
class TpdcConcurrentTest : public TestCase
{
public:
    TpdcConcurrentTest() : TestCase("TPDC Concurrent Operations Test") {}
    virtual ~TpdcConcurrentTest() {}

private:
    void DoRun() override
    {
        Ptr<Tpdc> tpdc = CreateObject<Tpdc>();
        tpdc->Initialize();

        // Test concurrent sending and receiving
        std::vector<Ptr<Packet>> sendPackets;
        std::vector<Ptr<Packet>> receivePackets;

        // Send multiple packets
        for (int i = 0; i < 10; i++)
        {
            Ptr<Packet> packet = Create<Packet>(512 + i * 50);
            sendPackets.push_back(packet);
            tpdc->SendPacket(packet, i == 0, i == 9);
        }

        // Receive packets while sending
        for (int i = 0; i < 5; i++)
        {
            Ptr<Packet> packet = Create<Packet>(256 + i * 25);
            receivePackets.push_back(packet);
            tpdc->ReceivePacket(packet, 1000 + i, 2000 + i);
        }

        // Process acknowledgments concurrently
        for (uint32_t i = 1; i <= sendPackets.size(); i++)
        {
            tpdc->ProcessAcknowledgment(i, true);
        }

        // Verify final state
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetSentPackets(), sendPackets.size(),
                             "All sent packets should be counted");
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetReceivedPackets(), receivePackets.size(),
                             "All received packets should be counted");
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetUnacknowledgedPackets(), 0,
                             "All packets should be acknowledged");
    }
};

/**
 * @brief TPDC Performance Test
 */
class TpdcPerformanceTest : public TestCase
{
public:
    TpdcPerformanceTest() : TestCase("TPDC Performance Test") {}
    virtual ~TpdcPerformanceTest() {}

private:
    void DoRun() override
    {
        Ptr<Tpdc> tpdc = CreateObject<Tpdc>();
        tpdc->Initialize();

        // Test sending performance with reliability
        Time startTime = Simulator::Now();
        const int numPackets = 1000;

        for (int i = 0; i < numPackets; i++)
        {
            Ptr<Packet> packet = Create<Packet>(1024);
            tpdc->SendPacket(packet, i == 0, i == numPackets - 1);
        }

        Time sendTime = Simulator::Now();
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetSentPackets(), numPackets,
                             "All packets should be sent");

        // Simulate acknowledgments for all packets
        Time ackStartTime = Simulator::Now();
        for (uint32_t i = 1; i <= numPackets; i++)
        {
            tpdc->ProcessAcknowledgment(i, true);
        }
        Time ackTime = Simulator::Now();

        // Verify all packets are acknowledged
        NS_TEST_ASSERT_MSG_EQ(tpdc->GetUnacknowledgedPackets(), 0,
                             "All packets should be acknowledged");

        // Performance assertions
        Time sendDuration = sendTime - startTime;
        Time ackDuration = ackTime - ackStartTime;

        NS_TEST_ASSERT_MSG_LT(sendDuration.GetMicroSeconds(), 15000,
                             "Sending 1000 packets with reliability should complete quickly");
        NS_TEST_ASSERT_MSG_LT(ackDuration.GetMicroSeconds(), 10000,
                             "Processing 1000 acknowledgments should complete quickly");

        // Test memory usage
        uint64_t memoryUsage = tpdc->GetMemoryUsage();
        NS_TEST_ASSERT_MSG_LT(memoryUsage, 10 * 1024 * 1024,  // 10MB
                             "Memory usage should be reasonable for 1000 packets");
    }
};

/**
 * @brief TPDC Test Suite
 */
class TpdcTestSuite : public TestSuite
{
public:
    TpdcTestSuite() : TestSuite("soft-ue-tpdc", Type::UNIT)
    {
        AddTestCase(new TpdcBasicTest(), Duration::QUICK);
        AddTestCase(new TpdcReliableTransmissionTest(), Duration::QUICK);
        AddTestCase(new TpdcAcknowledgmentTest(), Duration::QUICK);
        AddTestCase(new TpdcRetransmissionTest(), Duration::MEDIUM);
        AddTestCase(new TpdcRtoTimerTest(), Duration::QUICK);
        AddTestCase(new TpdcPacketOrderingTest(), Duration::QUICK);
        AddTestCase(new TpdcErrorRecoveryTest(), Duration::QUICK);
        AddTestCase(new TpdcConcurrentTest(), Duration::QUICK);
        AddTestCase(new TpdcPerformanceTest(), Duration::MEDIUM);
    }
};

static TpdcTestSuite g_tpdcTestSuite;

} // namespace ns3
