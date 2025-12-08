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
 * @file             ipdc-test.cc
 * @brief            IPDC (Unreliable PDC) Unit Tests
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-08
 * @copyright        Apache License Version 2.0
 *
 * @details
 * This file contains comprehensive unit tests for the IPDC (Unreliable Packet
 * Delivery Context) component of the Ultra Ethernet protocol stack implementation.
 */

#include <ns3/test.h>
#include <ns3/ipdc.h>
#include <ns3/pdc-base.h>
#include <ns3/packet.h>
#include <ns3/simulator.h>

namespace ns3 {

/**
 * @brief Test IPDC basic functionality
 */
class IpdcBasicTest : public TestCase
{
public:
    IpdcBasicTest() : TestCase("IPDC Basic Functionality Test") {}
    virtual ~IpdcBasicTest() {}

private:
    void DoRun() override
    {
        // Test IPDC creation
        Ptr<Ipdc> ipdc = CreateObject<Ipdc>();
        NS_TEST_ASSERT_MSG_NE(ipdc, nullptr, "IPDC creation should succeed");

        // Test IPDC inherits from PdcBase
        Ptr<PdcBase> base = DynamicCast<PdcBase>(ipdc);
        NS_TEST_ASSERT_MSG_NE(base, nullptr, "IPDC should inherit from PdcBase");

        // Test initialization
        ipdc->Initialize();
        NS_TEST_ASSERT_MSG_EQ(ipdc->IsActive(), true, "IPDC should be active after initialization");

        // Test configuration
        ipdc->SetDestinationFep(1234);
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetDestinationFep(), 1234, "Destination FEP should be set correctly");

        ipdc->SetTrafficClass(2);
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetTrafficClass(), 2, "Traffic class should be set correctly");

        // Test statistics
        uint64_t sentPackets = ipdc->GetSentPackets();
        uint64_t receivedPackets = ipdc->GetReceivedPackets();
        NS_TEST_ASSERT_MSG_EQ(sentPackets, 0, "Initially should have sent 0 packets");
        NS_TEST_ASSERT_MSG_EQ(receivedPackets, 0, "Initially should have received 0 packets");
    }
};

/**
 * @brief Test IPDC packet sending
 */
class IpdcPacketSendingTest : public TestCase
{
public:
    IpdcPacketSendingTest() : TestCase("IPDC Packet Sending Test") {}
    virtual ~IpdcPacketSendingTest() {}

private:
    void DoRun() override
    {
        Ptr<Ipdc> ipdc = CreateObject<Ipdc>();
        ipdc->Initialize();
        ipdc->SetDestinationFep(1234);

        // Create test packet
        Ptr<Packet> packet = Create<Packet>(1024);

        // Test packet sending
        bool sendResult = ipdc->SendPacket(packet, true, true);
        NS_TEST_ASSERT_MSG_EQ(sendResult, true, "Packet sending should succeed");

        // Verify packet count
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetSentPackets(), 1, "Should have sent 1 packet");

        // Test sending multiple packets
        for (int i = 0; i < 5; i++)
        {
            Ptr<Packet> testPacket = Create<Packet>(512 + i * 100);
            bool result = ipdc->SendPacket(testPacket, false, false);
            NS_TEST_ASSERT_MSG_EQ(result, true, "Multiple packet sending should succeed");
        }

        NS_TEST_ASSERT_MSG_EQ(ipdc->GetSentPackets(), 6, "Should have sent 6 packets total");
    }
};

/**
 * @brief Test IPDC packet reception
 */
class IpdcPacketReceptionTest : public TestCase
{
public:
    IpdcPacketReceptionTest() : TestCase("IPDC Packet Reception Test") {}
    virtual ~IpdcPacketReceptionTest() {}

private:
    void DoRun() override
    {
        Ptr<Ipdc> ipdc = CreateObject<Ipdc>();
        ipdc->Initialize();

        // Create test packet
        Ptr<Packet> packet = Create<Packet>(1024);

        // Test packet reception
        bool receiveResult = ipdc->ReceivePacket(packet, 5678, 1234);
        NS_TEST_ASSERT_MSG_EQ(receiveResult, true, "Packet reception should succeed");

        // Verify packet count
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetReceivedPackets(), 1, "Should have received 1 packet");

        // Test receiving multiple packets
        for (int i = 0; i < 3; i++)
        {
            Ptr<Packet> testPacket = Create<Packet>(256 + i * 64);
            bool result = ipdc->ReceivePacket(testPacket, 5678 + i, 1234 + i);
            NS_TEST_ASSERT_MSG_EQ(result, true, "Multiple packet reception should succeed");
        }

        NS_TEST_ASSERT_MSG_EQ(ipdc->GetReceivedPackets(), 4, "Should have received 4 packets total");
    }
};

/**
 * @brief Test IPDC unreliable transmission characteristics
 */
class IpdcUnreliableTransmissionTest : public TestCase
{
public:
    IpdcUnreliableTransmissionTest() : TestCase("IPDC Unreliable Transmission Test") {}
    virtual ~IpdcUnreliableTransmissionTest() {}

private:
    void DoRun() override
    {
        Ptr<Ipdc> ipdc = CreateObject<Ipdc>();
        ipdc->Initialize();

        // Test that IPDC does not implement retransmission
        Ptr<Packet> packet = Create<Packet>(1024);

        // Send packet
        bool sendResult = ipdc->SendPacket(packet, true, true);
        NS_TEST_ASSERT_MSG_EQ(sendResult, true, "Packet sending should succeed");

        // IPDC should not track retransmissions or acknowledgments
        uint64_t retransmissions = ipdc->GetRetransmissions();
        NS_TEST_ASSERT_MSG_EQ(retransmissions, 0, "IPDC should not track retransmissions");

        // Test that timeouts are not used in unreliable mode
        Time timeout = ipdc->GetRetransmissionTimeout();
        NS_TEST_ASSERT_MSG_EQ(timeout, Time(0), "IPDC should not use retransmission timeout");
    }
};

/**
 * @brief Test IPDC concurrent operations
 */
class IpdcConcurrentTest : public TestCase
{
public:
    IpdcConcurrentTest() : TestCase("IPDC Concurrent Operations Test") {}
    virtual ~IpdcConcurrentTest() {}

private:
    void DoRun() override
    {
        Ptr<Ipdc> ipdc = CreateObject<Ipdc>();
        ipdc->Initialize();

        // Test concurrent sending and receiving
        std::vector<Ptr<Packet>> sendPackets;
        std::vector<Ptr<Packet>> receivePackets;

        // Create packets for sending
        for (int i = 0; i < 10; i++)
        {
            sendPackets.push_back(Create<Packet>(512 + i * 100));
            receivePackets.push_back(Create<Packet>(256 + i * 50));
        }

        // Send packets
        for (size_t i = 0; i < sendPackets.size(); i++)
        {
            bool sendResult = ipdc->SendPacket(sendPackets[i], i == 0, i == sendPackets.size() - 1);
            NS_TEST_ASSERT_MSG_EQ(sendResult, true, "Concurrent packet sending should succeed");
        }

        // Receive packets
        for (size_t i = 0; i < receivePackets.size(); i++)
        {
            bool receiveResult = ipdc->ReceivePacket(receivePackets[i], 1000 + i, 2000 + i);
            NS_TEST_ASSERT_MSG_EQ(receiveResult, true, "Concurrent packet reception should succeed");
        }

        // Verify counts
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetSentPackets(), sendPackets.size(),
                             "All sent packets should be counted");
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetReceivedPackets(), receivePackets.size(),
                             "All received packets should be counted");
    }
};

/**
 * @brief Test IPDC error handling
 */
class IpdcErrorHandlingTest : public TestCase
{
public:
    IpdcErrorHandlingTest() : TestCase("IPDC Error Handling Test") {}
    virtual ~IpdcErrorHandlingTest() {}

private:
    void DoRun() override
    {
        Ptr<Ipdc> ipdc = CreateObject<Ipdc>();
        ipdc->Initialize();

        // Test sending null packet
        bool sendNullResult = ipdc->SendPacket(nullptr, false, false);
        NS_TEST_ASSERT_MSG_EQ(sendNullResult, false, "Sending null packet should fail");

        // Test receiving null packet
        bool receiveNullResult = ipdc->ReceivePacket(nullptr, 1234, 5678);
        NS_TEST_ASSERT_MSG_EQ(receiveNullResult, false, "Receiving null packet should fail");

        // Test with empty packet (should still work for IPDC)
        Ptr<Packet> emptyPacket = Create<Packet>(0);
        bool sendEmptyResult = ipdc->SendPacket(emptyPacket, false, false);
        NS_TEST_ASSERT_MSG_EQ(sendEmptyResult, true, "Sending empty packet should succeed for IPDC");

        bool receiveEmptyResult = ipdc->ReceivePacket(emptyPacket, 1234, 5678);
        NS_TEST_ASSERT_MSG_EQ(receiveEmptyResult, true, "Receiving empty packet should succeed for IPDC");

        // Test error statistics
        uint64_t errorCount = ipdc->GetErrorCount();
        NS_TEST_ASSERT_MSG_GT(errorCount, 0, "Error count should be incremented for failed operations");
    }
};

/**
 * @brief Test IPDC resource management
 */
class IpdcResourceManagementTest : public TestCase
{
public:
    IpdcResourceManagementTest() : TestCase("IPDC Resource Management Test") {}
    virtual ~IpdcResourceManagementTest() {}

private:
    void DoRun() override
    {
        Ptr<Ipdc> ipdc = CreateObject<Ipdc>();
        ipdc->Initialize();

        // Test memory usage tracking
        uint64_t initialMemory = ipdc->GetMemoryUsage();
        NS_TEST_ASSERT_MSG_GT(initialMemory, 0, "Initial memory usage should be tracked");

        // Send multiple packets to test memory growth
        for (int i = 0; i < 100; i++)
        {
            Ptr<Packet> packet = Create<Packet>(1024);
            ipdc->SendPacket(packet, false, false);
        }

        uint64_t afterSendingMemory = ipdc->GetMemoryUsage();
        NS_TEST_ASSERT_MSG_GT(afterSendingMemory, initialMemory,
                             "Memory usage should increase after sending packets");

        // Test cleanup
        ipdc->Cleanup();
        uint64_t afterCleanupMemory = ipdc->GetMemoryUsage();
        NS_TEST_ASSERT_MSG_LT(afterCleanupMemory, afterSendingMemory,
                             "Memory usage should decrease after cleanup");

        // Test reset
        ipdc->Reset();
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetSentPackets(), 0, "Sent packet count should be reset");
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetReceivedPackets(), 0, "Received packet count should be reset");
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetErrorCount(), 0, "Error count should be reset");
    }
};

/**
 * @brief IPDC Performance Test
 */
class IpdcPerformanceTest : public TestCase
{
public:
    IpdcPerformanceTest() : TestCase("IPDC Performance Test") {}
    virtual ~IpdcPerformanceTest() {}

private:
    void DoRun() override
    {
        Ptr<Ipdc> ipdc = CreateObject<Ipdc>();
        ipdc->Initialize();

        // Test sending performance
        Time startTime = Simulator::Now();
        const int numPackets = 1000;

        for (int i = 0; i < numPackets; i++)
        {
            Ptr<Packet> packet = Create<Packet>(1024);
            ipdc->SendPacket(packet, i == 0, i == numPackets - 1);
        }

        Time sendTime = Simulator::Now();
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetSentPackets(), numPackets,
                             "All packets should be sent");

        // Test receiving performance
        for (int i = 0; i < numPackets; i++)
        {
            Ptr<Packet> packet = Create<Packet>(512);
            ipdc->ReceivePacket(packet, 1000 + i, 2000 + i);
        }

        Time receiveTime = Simulator::Now();
        NS_TEST_ASSERT_MSG_EQ(ipdc->GetReceivedPackets(), numPackets,
                             "All packets should be received");

        // Performance assertions (rough estimates)
        Time sendDuration = sendTime - startTime;
        Time receiveDuration = receiveTime - sendTime;

        NS_TEST_ASSERT_MSG_LT(sendDuration.GetMicroSeconds(), 10000,
                             "Sending 1000 packets should complete quickly");
        NS_TEST_ASSERT_MSG_LT(receiveDuration.GetMicroSeconds(), 10000,
                             "Receiving 1000 packets should complete quickly");

        // Test throughput calculation
        double sendThroughput = (double)numPackets / sendDuration.GetSeconds();
        double receiveThroughput = (double)numPackets / receiveDuration.GetSeconds();

        NS_TEST_ASSERT_MSG_GT(sendThroughput, 1000, "Send throughput should be reasonable");
        NS_TEST_ASSERT_MSG_GT(receiveThroughput, 1000, "Receive throughput should be reasonable");
    }
};

/**
 * @brief IPDC Test Suite
 */
class IpdcTestSuite : public TestSuite
{
public:
    IpdcTestSuite() : TestSuite("soft-ue-ipdc", Type::UNIT)
    {
        AddTestCase(new IpdcBasicTest(), Duration::QUICK);
        AddTestCase(new IpdcPacketSendingTest(), Duration::QUICK);
        AddTestCase(new IpdcPacketReceptionTest(), Duration::QUICK);
        AddTestCase(new IpdcUnreliableTransmissionTest(), Duration::QUICK);
        AddTestCase(new IpdcConcurrentTest(), Duration::QUICK);
        AddTestCase(new IpdcErrorHandlingTest(), Duration::QUICK);
        AddTestCase(new IpdcResourceManagementTest(), Duration::QUICK);
        AddTestCase(new IpdcPerformanceTest(), Duration::MEDIUM);
    }
};

static IpdcTestSuite g_ipdcTestSuite;

} // namespace ns3
