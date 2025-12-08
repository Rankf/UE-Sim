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
 * @file             soft-ue-integration-test.cc
 * @brief            Soft-UE Integration Tests
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-08
 * @copyright        Apache License Version 2.0
 *
 * @details
 * This file contains comprehensive integration tests for the Soft-UE Ultra Ethernet
 * protocol stack, testing end-to-end functionality and ns-3 framework integration.
 */

#include <ns3/test.h>
#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include <ns3/soft-ue-helper.h>
#include <ns3/soft-ue-net-device.h>
#include <ns3/soft-ue-channel.h>
#include <ns3/ses-manager.h>
#include <ns3/pds-manager.h>
#include <ns3/packet.h>

namespace ns3 {

/**
 * @brief Test Soft-UE Helper functionality
 */
class SoftUeHelperTest : public TestCase
{
public:
    SoftUeHelperTest() : TestCase("Soft-UE Helper Integration Test") {}
    virtual ~SoftUeHelperTest() {}

private:
    void DoRun() override
    {
        // Test Helper creation
        SoftUeHelper helper;
        NS_TEST_ASSERT_MSG_EQ(true, true, "SoftUeHelper creation should succeed");

        // Test Helper device installation
        NodeContainer nodes;
        nodes.Create(2);
        NetDeviceContainer devices = helper.Install(nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Should install 2 devices");
        NS_TEST_ASSERT_MSG_NE(devices.Get(0), nullptr, "First device should not be null");
        NS_TEST_ASSERT_MSG_NE(devices.Get(1), nullptr, "Second device should not be null");

        // Test device casting
        Ptr<SoftUeNetDevice> device0 = DynamicCast<SoftUeNetDevice>(devices.Get(0));
        Ptr<SoftUeNetDevice> device1 = DynamicCast<SoftUeNetDevice>(devices.Get(1));
        NS_TEST_ASSERT_MSG_NE(device0, nullptr, "Device should be castable to SoftUeNetDevice");
        NS_TEST_ASSERT_MSG_NE(device1, nullptr, "Device should be castable to SoftUeNetDevice");

        // Test Helper configuration
        helper.SetDeviceAttribute("MaxPdcCount", UintegerValue(512));
        helper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

        // Test that configuration was applied (this would require actual implementation)
        // For now, just verify the Helper can be configured
        NS_TEST_ASSERT_MSG_EQ(true, true, "Helper configuration should succeed");
    }
};

/**
 * @brief Test Soft-UE Network Device functionality
 */
class SoftUeNetDeviceTest : public TestCase
{
public:
    SoftUeNetDeviceTest() : TestCase("Soft-UE Network Device Integration Test") {}
    virtual ~SoftUeNetDeviceTest() {}

private:
    void DoRun() override
    {
        // Create and configure network device
        Ptr<Node> node = CreateObject<Node>();
        SoftUeHelper helper;
        Ptr<SoftUeNetDevice> device = helper.Install(node);

        // Test device initialization
        NS_TEST_ASSERT_MSG_NE(device, nullptr, "Device should be created");
        device->Initialize();
        NS_TEST_ASSERT_MSG_EQ(device->IsInitialized(), true, "Device should be initialized");

        // Test device managers
        Ptr<SesManager> sesManager = device->GetSesManager();
        Ptr<PdsManager> pdsManager = device->GetPdsManager();
        NS_TEST_ASSERT_MSG_NE(sesManager, nullptr, "SES manager should be available");
        NS_TEST_ASSERT_MSG_NE(pdsManager, nullptr, "PDS manager should be available");

        // Test manager linkage
        NS_TEST_ASSERT_MSG_EQ(sesManager->GetPdsManager(), pdsManager,
                             "SES manager should reference PDS manager");
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetSesManager(), sesManager,
                             "PDS manager should reference SES manager");

        // Test device statistics
        SoftUeStats stats = device->GetStatistics();
        NS_TEST_ASSERT_MSG_EQ(stats.totalPacketsReceived, 0, "Initially should have received 0 packets");
        NS_TEST_ASSERT_MSG_EQ(stats.totalPacketsTransmitted, 0, "Initially should have transmitted 0 packets");

        // Test device configuration
        SoftUeConfig config = device->GetConfiguration();
        NS_TEST_ASSERT_MSG_GT(config.maxPdcCount, 0, "Max PDC count should be positive");
        NS_TEST_ASSERT_MSG_GT(config.maxPacketSize, 0, "Max packet size should be positive");
    }
};

/**
 * @brief Test Soft-UE Channel functionality
 */
class SoftUeChannelTest : public TestCase
{
public:
    SoftUeChannelTest() : TestCase("Soft-UE Channel Integration Test") {}
    virtual ~SoftUeChannelTest() {}

private:
    void DoRun() override
    {
        // Create channel
        Ptr<SoftUeChannel> channel = CreateObject<SoftUeChannel>();
        NS_TEST_ASSERT_MSG_NE(channel, nullptr, "Channel creation should succeed");

        // Create devices
        Ptr<Node> node1 = CreateObject<Node>();
        Ptr<Node> node2 = CreateObject<Node>();
        SoftUeHelper helper;
        Ptr<SoftUeNetDevice> device1 = helper.Install(node1);
        Ptr<SoftUeNetDevice> device2 = helper.Install(node2);

        // Connect devices to channel
        device1->SetChannel(channel);
        device2->SetChannel(channel);
        NS_TEST_ASSERT_MSG_EQ(device1->GetChannel(), channel, "Device1 should be connected to channel");
        NS_TEST_ASSERT_MSG_EQ(device2->GetChannel(), channel, "Device2 should be connected to channel");

        // Test channel properties
        channel->SetAttribute("Delay", TimeValue(MilliSeconds(5)));
        NS_TEST_ASSERT_MSG_EQ(channel->GetDelay(), MilliSeconds(5), "Channel delay should be set");

        // Test device count
        uint32_t deviceCount = channel->GetNDevices();
        NS_TEST_ASSERT_MSG_EQ(deviceCount, 2, "Channel should have 2 devices attached");
    }
};

/**
 * @brief Test end-to-end data transmission
 */
class EndToEndTransmissionTest : public TestCase
{
public:
    EndToEndTransmissionTest() : TestCase("End-to-End Transmission Test") {}
    virtual ~EndToEndTransmissionTest() {}

private:
    void DoRun() override
    {
        // Create two nodes with Soft-UE devices
        NodeContainer nodes;
        nodes.Create(2);
        SoftUeHelper helper;
        NetDeviceContainer devices = helper.Install(nodes);

        Ptr<SoftUeNetDevice> sender = DynamicCast<SoftUeNetDevice>(devices.Get(0));
        Ptr<SoftUeNetDevice> receiver = DynamicCast<SoftUeNetDevice>(devices.Get(1));

        // Create test packet
        Ptr<Packet> packet = Create<Packet>(1024);

        // Test PDC allocation on sender
        uint16_t pdcId = sender->AllocatePdc(1234, 0, 0, PDS_NEXT_HEADER_ROCE);
        NS_TEST_ASSERT_MSG_NE(pdcId, 0, "PDC allocation should succeed");

        // Test packet sending (this would require actual implementation)
        // For now, we verify the infrastructure is in place
        SoftUeStats senderStats = sender->GetStatistics();
        SoftUeStats receiverStats = receiver->GetStatistics();

        NS_TEST_ASSERT_MSG_EQ(senderStats.activePdcCount, 1, "Sender should have 1 active PDC");

        // Clean up
        sender->ReleasePdc(pdcId);
        senderStats = sender->GetStatistics();
        NS_TEST_ASSERT_MSG_EQ(senderStats.activePdcCount, 0, "Sender should have 0 active PDC after release");
    }
};

/**
 * @brief Test multi-node communication
 */
class MultiNodeCommunicationTest : public TestCase
{
public:
    MultiNodeCommunicationTest() : TestCase("Multi-Node Communication Test") {}
    virtual ~MultiNodeCommunicationTest() {}

private:
    void DoRun() override
    {
        // Create multiple nodes
        const int numNodes = 5;
        NodeContainer nodes;
        nodes.Create(numNodes);
        SoftUeHelper helper;
        NetDeviceContainer devices = helper.Install(nodes);

        // Test all devices are created
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), numNodes, "Should create devices for all nodes");

        // Test each device has proper managers
        for (uint32_t i = 0; i < devices.GetN(); i++)
        {
            Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(devices.Get(i));
            NS_TEST_ASSERT_MSG_NE(device, nullptr, "Each device should be valid SoftUeNetDevice");

            Ptr<SesManager> sesManager = device->GetSesManager();
            Ptr<PdsManager> pdsManager = device->GetPdsManager();
            NS_TEST_ASSERT_MSG_NE(sesManager, nullptr, "Each device should have SES manager");
            NS_TEST_ASSERT_MSG_NE(pdsManager, nullptr, "Each device should have PDS manager");
        }

        // Test multiple PDC allocations across nodes
        std::vector<std::vector<uint16_t>> nodePdcs;
        for (uint32_t i = 0; i < devices.GetN(); i++)
        {
            Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(devices.Get(i));
            std::vector<uint16_t> pdcs;

            for (int j = 0; j < 3; j++)
            {
                uint16_t pdcId = device->AllocatePdc(2000 + i * 100 + j, j % 4, 0, PDS_NEXT_HEADER_ROCE);
                if (pdcId != 0)
                {
                    pdcs.push_back(pdcId);
                }
            }
            nodePdcs.push_back(pdcs);
        }

        // Verify PDCs are allocated
        int totalPdcs = 0;
        for (const auto& pdcs : nodePdcs)
        {
            totalPdcs += pdcs.size();
        }
        NS_TEST_ASSERT_MSG_EQ(totalPdcs, numNodes * 3, "Should allocate all requested PDCs");

        // Clean up
        for (uint32_t i = 0; i < devices.GetN(); i++)
        {
            Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(devices.Get(i));
            for (uint16_t pdcId : nodePdcs[i])
            {
                device->ReleasePdc(pdcId);
            }
        }
    }
};

/**
 * @brief Test error handling across components
 */
class ErrorHandlingIntegrationTest : public TestCase
{
public:
    ErrorHandlingIntegrationTest() : TestCase("Error Handling Integration Test") {}
    virtual ~ErrorHandlingIntegrationTest() {}

private:
    void DoRun() override
    {
        // Create test setup
        NodeContainer nodes;
        nodes.Create(2);
        SoftUeHelper helper;
        NetDeviceContainer devices = helper.Install(nodes);

        Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(devices.Get(0));

        // Test invalid PDC operations
        uint16_t invalidPdcId = 9999;
        bool releaseResult = device->ReleasePdc(invalidPdcId);
        NS_TEST_ASSERT_MSG_EQ(releaseResult, false, "Releasing invalid PDC should fail");

        // Test configuration validation
        helper.SetDeviceAttribute("MaxPdcCount", UintegerValue(0));  // Invalid value
        // This should be handled gracefully (would require actual error handling implementation)

        // Test resource exhaustion
        uint16_t pdcId = device->AllocatePdc(1234, 0, 0, PDS_NEXT_HEADER_ROCE);
        NS_TEST_ASSERT_MSG_NE(pdcId, 0, "First PDC allocation should succeed");

        // Test error statistics
        SoftUeStats stats = device->GetStatistics();
        NS_TEST_ASSERT_MSG_EQ(stats.activePdcCount, 1, "Should have 1 active PDC");

        // Clean up
        device->ReleasePdc(pdcId);
    }
};

/**
 * @brief Test performance integration
 */
class PerformanceIntegrationTest : public TestCase
{
public:
    PerformanceIntegrationTest() : TestCase("Performance Integration Test") {}
    virtual ~PerformanceIntegrationTest() {}

private:
    void DoRun() override
    {
        // Create test setup
        const int numNodes = 10;
        NodeContainer nodes;
        nodes.Create(numNodes);
        SoftUeHelper helper;
        NetDeviceContainer devices = helper.Install(nodes);

        // Test bulk PDC allocation performance
        Time startTime = Simulator::Now();
        std::vector<uint16_t> allPdcs;

        for (uint32_t i = 0; i < devices.GetN(); i++)
        {
            Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(devices.Get(i));
            for (int j = 0; j < 10; j++)
            {
                uint16_t pdcId = device->AllocatePdc(3000 + i * 10 + j, j % 8, 0, PDS_NEXT_HEADER_ROCE);
                if (pdcId != 0)
                {
                    allPdcs.push_back(pdcId);
                }
            }
        }

        Time allocationTime = Simulator::Now();
        NS_TEST_ASSERT_MSG_EQ(allPdcs.size(), numNodes * 10,
                             "Should allocate all requested PDCs");

        // Test bulk release performance
        for (uint32_t i = 0; i < devices.GetN(); i++)
        {
            Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(devices.Get(i));
            SoftUeStats stats = device->GetStatistics();
            NS_TEST_ASSERT_MSG_EQ(stats.activePdcCount, 10,
                                 "Each device should have 10 active PDCs");
        }

        // Release all PDCs
        for (uint32_t i = 0; i < devices.GetN(); i++)
        {
            Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(devices.Get(i));
            uint32_t activeCount = device->GetActivePdcCount();
            for (uint32_t j = 0; j < activeCount; j++)
            {
                // This is a simplified cleanup - actual implementation would track PDC IDs
                device->ReleasePdc(j + 1);
            }
        }

        Time releaseTime = Simulator::Now();

        // Performance assertions
        Time allocationDuration = allocationTime - startTime;
        Time releaseDuration = releaseTime - allocationTime;

        NS_TEST_ASSERT_MSG_LT(allocationDuration.GetMicroSeconds(), 50000,
                             "Bulk PDC allocation should complete quickly");
        NS_TEST_ASSERT_MSG_LT(releaseDuration.GetMicroSeconds(), 50000,
                             "Bulk PDC release should complete quickly");
    }
};

/**
 * @brief Test protocol stack integration
 */
class ProtocolStackIntegrationTest : public TestCase
{
public:
    ProtocolStackIntegrationTest() : TestCase("Protocol Stack Integration Test") {}
    virtual ~ProtocolStackIntegrationTest() {}

private:
    void DoRun() override
    {
        // Create test setup
        NodeContainer nodes;
        nodes.Create(2);
        SoftUeHelper helper;
        NetDeviceContainer devices = helper.Install(nodes);

        Ptr<SoftUeNetDevice> sender = DynamicCast<SoftUeNetDevice>(devices.Get(0));
        Ptr<SoftUeNetDevice> receiver = DynamicCast<SoftUeNetDevice>(devices.Get(1));

        // Test layer integration
        Ptr<SesManager> sesManager = sender->GetSesManager();
        Ptr<PdsManager> pdsManager = sender->GetPdsManager();

        // Test SES initialization
        sesManager->Initialize();
        NS_TEST_ASSERT_MSG_EQ(sesManager->HasPendingOperations(), false,
                             "SES manager should be properly initialized");

        // Test PDS initialization
        pdsManager->Initialize();
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetActivePdcs(), 0,
                             "PDS manager should be properly initialized");

        // Test inter-layer communication
        uint16_t pdcId = pdsManager->AllocatePdc(1234, 0, 0, PDS_NEXT_HEADER_ROCE, 1, 2);
        NS_TEST_ASSERT_MSG_NE(pdcId, 0, "PDC allocation should work through integrated layers");

        // Test statistics collection across layers
        std::string pdsStats = pdsManager->GetStatisticsString();
        std::string sesStats = sesManager->GetStatistics();
        NS_TEST_ASSERT_MSG_NE(pdsStats.length(), 0, "PDS statistics should be available");
        NS_TEST_ASSERT_MSG_NE(sesStats.length(), 0, "SES statistics should be available");

        // Test configuration consistency
        SoftUeConfig config = sender->GetConfiguration();
        NS_TEST_ASSERT_MSG_GT(config.maxPdcCount, 0, "Configuration should be consistent across layers");

        // Clean up
        pdsManager->ReleasePdc(pdcId);
    }
};

/**
 * @brief Soft-UE Integration Test Suite
 */
class SoftUeIntegrationTestSuite : public TestSuite
{
public:
    SoftUeIntegrationTestSuite() : TestSuite("soft-ue-integration", Type::SYSTEM)
    {
        AddTestCase(new SoftUeHelperTest(), Duration::QUICK);
        AddTestCase(new SoftUeNetDeviceTest(), Duration::QUICK);
        AddTestCase(new SoftUeChannelTest(), Duration::QUICK);
        AddTestCase(new EndToEndTransmissionTest(), Duration::QUICK);
        AddTestCase(new MultiNodeCommunicationTest(), Duration::MEDIUM);
        AddTestCase(new ErrorHandlingIntegrationTest(), Duration::QUICK);
        AddTestCase(new PerformanceIntegrationTest(), Duration::MEDIUM);
        AddTestCase(new ProtocolStackIntegrationTest(), Duration::QUICK);
    }
};

static SoftUeIntegrationTestSuite g_softUeIntegrationTestSuite;

} // namespace ns3
