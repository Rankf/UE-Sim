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
 * @file             pds-manager-test.cc
 * @brief            PDS Manager Unit Tests
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-08
 * @copyright        Apache License Version 2.0
 *
 * @details
 * This file contains comprehensive unit tests for the PDS Manager component
 * of the Ultra Ethernet protocol stack implementation.
 */

#include <ns3/test.h>
#include <ns3/pds-manager.h>
#include <ns3/ses-manager.h>
#include <ns3/soft-ue-net-device.h>
#include <ns3/packet.h>
#include <ns3/pds-common.h>

namespace ns3 {

/**
 * @brief Test PDS Manager basic functionality
 */
class PdsManagerBasicTest : public TestCase
{
public:
    PdsManagerBasicTest() : TestCase("PDS Manager Basic Functionality Test") {}
    virtual ~PdsManagerBasicTest() {}

private:
    void DoRun() override
    {
        // Test PDS Manager creation and initialization
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();
        NS_TEST_ASSERT_MSG_NE(pdsManager, nullptr, "PDS Manager creation should succeed");

        // Test initialization
        pdsManager->Initialize();
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetActivePdcs(), 0,
                             "Initially should have no active PDCs");

        // Test total active PDC count
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetTotalActivePdcCount(), 0,
                             "Total active PDC count should initially be zero");

        // Test statistics
        Ptr<PdsStatistics> stats = pdsManager->GetStatistics();
        NS_TEST_ASSERT_MSG_NE(stats, nullptr, "Statistics object should be available");

        // Test statistics string
        std::string statsString = pdsManager->GetStatisticsString();
        NS_TEST_ASSERT_MSG_NE(statsString.length(), 0, "Statistics string should not be empty");

        // Test statistics reset
        pdsManager->ResetStatistics();
        std::string resetStatsString = pdsManager->GetStatisticsString();
        NS_TEST_ASSERT_MSG_NE(resetStatsString, statsString, "Statistics should be reset");

        // Test statistics enable/disable
        pdsManager->SetStatisticsEnabled(false);
        NS_TEST_ASSERT_MSG_EQ(pdsManager->IsStatisticsEnabled(), false,
                             "Statistics should be disabled");

        pdsManager->SetStatisticsEnabled(true);
        NS_TEST_ASSERT_MSG_EQ(pdsManager->IsStatisticsEnabled(), true,
                             "Statistics should be enabled");
    }
};

/**
 * @brief Test PDS Manager SES request processing
 */
class PdsManagerSesRequestTest : public TestCase
{
public:
    PdsManagerSesRequestTest() : TestCase("PDS Manager SES Request Processing Test") {}
    virtual ~PdsManagerSesRequestTest() {}

private:
    void DoRun() override
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();
        pdsManager->Initialize();

        // Create test SES to PDS request
        SesPdsRequest request;
        request.version = 1;
        request.headerType = SES_HEADER_TYPE_STANDARD;
        request.pidOnFep = 1234;
        request.opcode = OP_TYPE_WRITE;
        request.destFep = 5678;
        request.tc = 0;  // Traffic class
        request.dm = 0;  // Delivery mode
        request.nextHdr = PDS_NEXT_HEADER_ROCE;
        request.jobLandingJob = 1;
        request.nextJob = 2;

        // Test SES request processing
        bool result = pdsManager->ProcessSesRequest(request);
        NS_TEST_ASSERT_MSG_EQ(result, true, "SES request processing should succeed");
    }
};

/**
 * @brief Test PDS Manager PDC allocation
 */
class PdsManagerPdcAllocationTest : public TestCase
{
public:
    PdsManagerPdcAllocationTest() : TestCase("PDS Manager PDC Allocation Test") {}
    virtual ~PdsManagerPdcAllocationTest() {}

private:
    void DoRun() override
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();
        pdsManager->Initialize();

        // Test PDC allocation
        uint16_t pdcId1 = pdsManager->AllocatePdc(1234, 0, 0, PDS_NEXT_HEADER_ROCE, 1, 2);
        NS_TEST_ASSERT_MSG_NE(pdcId1, 0, "First PDC allocation should succeed");
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetActivePdcs(), 1,
                             "Should have 1 active PDC after first allocation");

        uint16_t pdcId2 = pdsManager->AllocatePdc(5678, 1, 1, PDS_NEXT_HEADER_ROCE, 3, 4);
        NS_TEST_ASSERT_MSG_NE(pdcId2, 0, "Second PDC allocation should succeed");
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetActivePdcs(), 2,
                             "Should have 2 active PDCs after second allocation");

        NS_TEST_ASSERT_MSG_NE(pdcId1, pdcId2, "PDC IDs should be unique");

        // Test PDC release
        bool releaseResult = pdsManager->ReleasePdc(pdcId1);
        NS_TEST_ASSERT_MSG_EQ(releaseResult, true, "PDC release should succeed");
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetActivePdcs(), 1,
                             "Should have 1 active PDC after release");

        // Test releasing non-existent PDC
        bool releaseNonExistent = pdsManager->ReleasePdc(9999);
        NS_TEST_ASSERT_MSG_EQ(releaseNonExistent, false,
                             "Releasing non-existent PDC should fail");

        // Clean up
        pdsManager->ReleasePdc(pdcId2);
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetActivePdcs(), 0,
                             "Should have 0 active PDCs after cleanup");
    }
};

/**
 * @brief Test PDS Manager packet reception
 */
class PdsManagerPacketReceptionTest : public TestCase
{
public:
    PdsManagerPacketReceptionTest() : TestCase("PDS Manager Packet Reception Test") {}
    virtual ~PdsManagerPacketReceptionTest() {}

private:
    void DoRun() override
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();
        pdsManager->Initialize();

        // Create test packet
        Ptr<Packet> packet = Create<Packet>(1024);
        uint32_t sourceEndpoint = 1234;
        uint32_t destEndpoint = 5678;

        // Test packet reception
        bool result = pdsManager->ProcessReceivedPacket(packet, sourceEndpoint, destEndpoint);
        NS_TEST_ASSERT_MSG_EQ(result, true, "Packet reception should succeed");

        // Test with different endpoints
        bool result2 = pdsManager->ProcessReceivedPacket(Create<Packet>(512), 1111, 2222);
        NS_TEST_ASSERT_MSG_EQ(result2, true, "Packet reception with different endpoints should succeed");
    }
};

/**
 * @brief Test PDS Manager PDC packet sending
 */
class PdsManagerPacketSendingTest : public TestCase
{
public:
    PdsManagerPacketSendingTest() : TestCase("PDS Manager Packet Sending Test") {}
    virtual ~PdsManagerPacketSendingTest() {}

private:
    void DoRun() override
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();
        pdsManager->Initialize();

        // Allocate a PDC first
        uint16_t pdcId = pdsManager->AllocatePdc(1234, 0, 0, PDS_NEXT_HEADER_ROCE, 1, 2);
        NS_TEST_ASSERT_MSG_NE(pdcId, 0, "PDC allocation should succeed");

        // Create test packet
        Ptr<Packet> packet = Create<Packet>(1024);

        // Test packet sending through PDC
        bool sendResult = pdsManager->SendPacketThroughPdc(pdcId, packet, true, true);
        NS_TEST_ASSERT_MSG_EQ(sendResult, true, "Packet sending through PDC should succeed");

        // Test sending through non-existent PDC
        bool sendNonExistent = pdsManager->SendPacketThroughPdc(9999, packet, false, false);
        NS_TEST_ASSERT_MSG_EQ(sendNonExistent, false,
                             "Sending through non-existent PDC should fail");

        // Clean up
        pdsManager->ReleasePdc(pdcId);
    }
};

/**
 * @brief Test PDS Manager error handling
 */
class PdsManagerErrorHandlingTest : public TestCase
{
public:
    PdsManagerErrorHandlingTest() : TestCase("PDS Manager Error Handling Test") {}
    virtual ~PdsManagerErrorHandlingTest() {}

private:
    void DoRun() override
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();
        pdsManager->Initialize();

        // Test error handling with invalid PDC ID
        std::string errorDetails = "Test error details";
        pdsManager->HandlePdcError(9999, PDS_ERROR_PDC_NOT_FOUND, errorDetails);

        // Test error handling with valid PDC ID
        uint16_t pdcId = pdsManager->AllocatePdc(1234, 0, 0, PDS_NEXT_HEADER_ROCE, 1, 2);
        pdsManager->HandlePdcError(pdcId, PDS_ERROR_INVALID_PACKET, "Invalid packet format");

        // Test statistics after error handling
        Ptr<PdsStatistics> stats = pdsManager->GetStatistics();
        NS_TEST_ASSERT_MSG_NE(stats, nullptr, "Statistics should be available after error handling");

        // Clean up
        pdsManager->ReleasePdc(pdcId);
    }
};

/**
 * @brief Test PDS Manager SES manager interaction
 */
class PdsManagerSesInteractionTest : public TestCase
{
public:
    PdsManagerSesInteractionTest() : TestCase("PDS Manager SES Manager Interaction Test") {}
    virtual ~PdsManagerSesInteractionTest() {}

private:
    void DoRun() override
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();
        Ptr<SesManager> sesManager = CreateObject<SesManager>();

        // Test SES manager assignment
        pdsManager->SetSesManager(sesManager);
        Ptr<SesManager> retrievedSes = pdsManager->GetSesManager();
        NS_TEST_ASSERT_MSG_EQ(retrievedSes, sesManager, "Retrieved SES manager should match assigned one");

        // Test initialization with SES manager
        pdsManager->Initialize();
        sesManager->Initialize();

        // Test that both managers are properly linked
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetSesManager(), sesManager,
                             "PDS manager should maintain reference to SES manager");

        // Test statistics with linked managers
        std::string statsString = pdsManager->GetStatisticsString();
        NS_TEST_ASSERT_MSG_NE(statsString.length(), 0,
                             "Statistics should be available with linked managers");
    }
};

/**
 * @brief Test PDS Manager network device interaction
 */
class PdsManagerNetDeviceTest : public TestCase
{
public:
    PdsManagerNetDeviceTest() : TestCase("PDS Manager Network Device Interaction Test") {}
    virtual ~PdsManagerNetDeviceTest() {}

private:
    void DoRun() override
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();
        Ptr<SoftUeNetDevice> netDevice = CreateObject<SoftUeNetDevice>();

        // Test network device assignment
        pdsManager->SetNetDevice(netDevice);
        Ptr<SoftUeNetDevice> retrievedDevice = pdsManager->GetNetDevice();
        NS_TEST_ASSERT_MSG_EQ(retrievedDevice, netDevice,
                             "Retrieved network device should match assigned one");

        // Test initialization with network device
        pdsManager->Initialize();

        // Test PDC operations with network device
        uint16_t pdcId = pdsManager->AllocatePdc(1234, 0, 0, PDS_NEXT_HEADER_ROCE, 1, 2);
        NS_TEST_ASSERT_MSG_NE(pdcId, 0, "PDC allocation should succeed with network device");

        // Test packet operations with network device
        Ptr<Packet> packet = Create<Packet>(1024);
        bool sendResult = pdsManager->SendPacketThroughPdc(pdcId, packet, true, false);
        NS_TEST_ASSERT_MSG_EQ(sendResult, true, "Packet sending should succeed with network device");

        // Clean up
        pdsManager->ReleasePdc(pdcId);
    }
};

/**
 * @brief PDS Manager Performance Test
 */
class PdsManagerPerformanceTest : public TestCase
{
public:
    PdsManagerPerformanceTest() : TestCase("PDS Manager Performance Test") {}
    virtual ~PdsManagerPerformanceTest() {}

private:
    void DoRun() override
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();
        pdsManager->Initialize();

        // Test performance of multiple PDC allocations
        Time startTime = Simulator::Now();
        std::vector<uint16_t> allocatedPdcs;

        const int numPdcs = 100;
        for (int i = 0; i < numPdcs; i++)
        {
            uint16_t pdcId = pdsManager->AllocatePdc(2000 + i, i % 8, i % 2,
                                                   PDS_NEXT_HEADER_ROCE, i, i + 1);
            if (pdcId != 0)
            {
                allocatedPdcs.push_back(pdcId);
            }
        }

        Time allocationTime = Simulator::Now();
        NS_TEST_ASSERT_MSG_EQ(allocatedPdcs.size(), numPdcs,
                             "All PDCs should be allocated successfully");

        // Test performance of multiple PDC releases
        for (uint16_t pdcId : allocatedPdcs)
        {
            pdsManager->ReleasePdc(pdcId);
        }

        Time releaseTime = Simulator::Now();

        // Verify all PDCs are released
        NS_TEST_ASSERT_MSG_EQ(pdsManager->GetActivePdcs(), 0,
                             "All PDCs should be released");

        // Performance assertions (these are rough estimates and may need adjustment)
        NS_TEST_ASSERT_MSG_LT((allocationTime - startTime).GetMicroSeconds(), 1000,
                             "PDC allocation should complete quickly");
        NS_TEST_ASSERT_MSG_LT((releaseTime - allocationTime).GetMicroSeconds(), 1000,
                             "PDC release should complete quickly");
    }
};

/**
 * @brief PDS Manager Test Suite
 */
class PdsManagerTestSuite : public TestSuite
{
public:
    PdsManagerTestSuite() : TestSuite("soft-ue-pds", Type::UNIT)
    {
        AddTestCase(new PdsManagerBasicTest(), Duration::QUICK);
        AddTestCase(new PdsManagerSesRequestTest(), Duration::QUICK);
        AddTestCase(new PdsManagerPdcAllocationTest(), Duration::QUICK);
        AddTestCase(new PdsManagerPacketReceptionTest(), Duration::QUICK);
        AddTestCase(new PdsManagerPacketSendingTest(), Duration::QUICK);
        AddTestCase(new PdsManagerErrorHandlingTest(), Duration::QUICK);
        AddTestCase(new PdsManagerSesInteractionTest(), Duration::QUICK);
        AddTestCase(new PdsManagerNetDeviceTest(), Duration::QUICK);
        AddTestCase(new PdsManagerPerformanceTest(), Duration::MEDIUM);
    }
};

static PdsManagerTestSuite g_pdsManagerTestSuite;

} // namespace ns3
