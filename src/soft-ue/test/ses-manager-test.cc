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
 * @file             ses-manager-test.cc
 * @brief            SES Manager Unit Tests
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-08
 * @copyright        Apache License Version 2.0
 *
 * @details
 * This file contains comprehensive unit tests for the SES Manager component
 * of the Ultra Ethernet protocol stack implementation.
 */

#include <ns3/test.h>
#include <ns3/ses-manager.h>
#include <ns3/pds-manager.h>
#include <ns3/soft-ue-net-device.h>
#include <ns3/operation-metadata.h>
#include <ns3/msn-entry.h>

namespace ns3 {

/**
 * @brief Test SES Manager basic functionality
 */
class SesManagerBasicTest : public TestCase
{
public:
    SesManagerBasicTest() : TestCase("SES Manager Basic Functionality Test") {}
    virtual ~SesManagerBasicTest() {}

private:
    void DoRun() override
    {
        // Test SES Manager creation and initialization
        Ptr<SesManager> sesManager = CreateObject<SesManager>();
        NS_TEST_ASSERT_MSG_NE(sesManager, nullptr, "SES Manager creation should succeed");

        // Test initialization
        sesManager->Initialize();
        NS_TEST_ASSERT_MSG_EQ(sesManager->HasPendingOperations(), false,
                             "Initially should have no pending operations");

        // Test request queue size
        NS_TEST_ASSERT_MSG_EQ(sesManager->GetRequestQueueSize(), 0,
                             "Initially request queue should be empty");

        // Test MSN table
        Ptr<MsnTable> msnTable = sesManager->GetMsnTable();
        NS_TEST_ASSERT_MSG_NE(msnTable, nullptr, "MSN table should be available");

        // Test statistics
        std::string stats = sesManager->GetStatistics();
        NS_TEST_ASSERT_MSG_NE(stats.length(), 0, "Statistics string should not be empty");

        // Test statistics reset
        sesManager->ResetStatistics();
        std::string resetStats = sesManager->GetStatistics();
        NS_TEST_ASSERT_MSG_NE(resetStats, stats, "Statistics should be reset");
    }
};

/**
 * @brief Test SES Manager send request processing
 */
class SesManagerSendRequestTest : public TestCase
{
public:
    SesManagerSendRequestTest() : TestCase("SES Manager Send Request Processing Test") {}
    virtual ~SesManagerSendRequestTest() {}

private:
    void DoRun() override
    {
        Ptr<SesManager> sesManager = CreateObject<SesManager>();
        sesManager->Initialize();

        // Create test metadata
        Ptr<ExtendedOperationMetadata> metadata = CreateObject<ExtendedOperationMetadata>();
        NS_TEST_ASSERT_MSG_NE(metadata, nullptr, "Metadata creation should succeed");

        // Test basic send request processing
        bool result = sesManager->ProcessSendRequest(metadata);
        NS_TEST_ASSERT_MSG_EQ(result, true, "Send request processing should succeed");

        // Test request queue size after sending
        size_t queueSize = sesManager->GetRequestQueueSize();
        NS_TEST_ASSERT_MSG_GT(queueSize, 0, "Request queue should not be empty after send request");

        // Test pending operations flag
        NS_TEST_ASSERT_MSG_EQ(sesManager->HasPendingOperations(), true,
                             "Should have pending operations after send request");
    }
};

/**
 * @brief Test SES Manager receive request processing
 */
class SesManagerReceiveRequestTest : public TestCase
{
public:
    SesManagerReceiveRequestTest() : TestCase("SES Manager Receive Request Processing Test") {}
    virtual ~SesManagerReceiveRequestTest() {}

private:
    void DoRun() override
    {
        Ptr<SesManager> sesManager = CreateObject<SesManager>();
        sesManager->Initialize();

        // Create test PDC to SES request
        PdcSesRequest request;
        request.version = 1;
        request.headerType = SES_HEADER_TYPE_STANDARD;
        request.pidOnFep = 1234;
        request.opcode = OP_TYPE_WRITE;
        request.rkey = 0x1000;
        request.startAddr = 0x2000;
        request.length = 1024;
        request.psn = 1;
        request.pdcId = 1;
        request.isFirstPacket = true;
        request.isLastPacket = true;
        request.destFep = 5678;

        // Test receive request processing
        bool result = sesManager->ProcessReceiveRequest(request);
        NS_TEST_ASSERT_MSG_EQ(result, true, "Receive request processing should succeed");
    }
};

/**
 * @brief Test SES Manager response processing
 */
class SesManagerResponseTest : public TestCase
{
public:
    SesManagerResponseTest() : TestCase("SES Manager Response Processing Test") {}
    virtual ~SesManagerResponseTest() {}

private:
    void DoRun() override
    {
        Ptr<SesManager> sesManager = CreateObject<SesManager>();
        sesManager->Initialize();

        // Create test PDC to SES response
        PdcSesResponse response;
        response.version = 1;
        response.headerType = SES_HEADER_TYPE_STANDARD;
        response.returnCode = RESPONSE_RETURN_CODE_SUCCESS;
        response.psn = 1;
        response.pdcId = 1;
        response.destFep = 5678;

        // Test receive response processing
        bool result = sesManager->ProcessReceiveResponse(response);
        NS_TEST_ASSERT_MSG_EQ(result, true, "Receive response processing should succeed");
    }
};

/**
 * @brief Test SES Manager validation methods
 */
class SesManagerValidationTest : public TestCase
{
public:
    SesManagerValidationTest() : TestCase("SES Manager Validation Methods Test") {}
    virtual ~SesManagerValidationTest() {}

private:
    void DoRun() override
    {
        Ptr<SesManager> sesManager = CreateObject<SesManager>();
        sesManager->Initialize();

        // Test with custom validation callbacks
        sesManager->SetJobIdValidator(MakeCallback(&SesManagerValidationTest::TestJobValidator, this));
        sesManager->SetPermissionChecker(MakeCallback(&SesManagerValidationTest::TestPermissionChecker, this));
        sesManager->SetMemoryRegionValidator(MakeCallback(&SesManagerValidationTest::TestMemoryValidator, this));

        // Test statistics after setting validators
        std::string stats = sesManager->GetStatistics();
        NS_TEST_ASSERT_MSG_NE(stats.length(), 0, "Statistics should be available with validators");

        // Test detailed logging
        sesManager->SetDetailedLogging(true);
        sesManager->SetDetailedLogging(false);
    }

    // Test validation callback implementations
    bool TestJobValidator(uint64_t jobId) { return jobId > 0; }
    bool TestPermissionChecker(uint32_t endpoint, uint64_t jobId) { return true; }
    bool TestMemoryValidator(uint64_t rkey) { return rkey != 0; }
};

/**
 * @brief Test SES Manager PDS manager interaction
 */
class SesManagerPdsInteractionTest : public TestCase
{
public:
    SesManagerPdsInteractionTest() : TestCase("SES Manager PDS Manager Interaction Test") {}
    virtual ~SesManagerPdsInteractionTest() {}

private:
    void DoRun() override
    {
        Ptr<SesManager> sesManager = CreateObject<SesManager>();
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager>();

        // Test PDS manager assignment
        sesManager->SetPdsManager(pdsManager);
        Ptr<PdsManager> retrievedPds = sesManager->GetPdsManager();
        NS_TEST_ASSERT_MSG_EQ(retrievedPds, pdsManager, "Retrieved PDS manager should match assigned one");

        // Test PDS manager assignment in reverse
        pdsManager->SetSesManager(sesManager);
        Ptr<SesManager> retrievedSes = pdsManager->GetSesManager();
        NS_TEST_ASSERT_MSG_EQ(retrievedSes, sesManager, "Retrieved SES manager should match assigned one");
    }
};

/**
 * @brief Test SES Manager network device interaction
 */
class SesManagerNetDeviceTest : public TestCase
{
public:
    SesManagerNetDeviceTest() : TestCase("SES Manager Network Device Interaction Test") {}
    virtual ~SesManagerNetDeviceTest() {}

private:
    void DoRun() override
    {
        Ptr<SesManager> sesManager = CreateObject<SesManager>();
        Ptr<SoftUeNetDevice> netDevice = CreateObject<SoftUeNetDevice>();

        // Test network device assignment
        sesManager->SetNetDevice(netDevice);
        Ptr<SoftUeNetDevice> retrievedDevice = sesManager->GetNetDevice();
        NS_TEST_ASSERT_MSG_EQ(retrievedDevice, netDevice, "Retrieved network device should match assigned one");

        // Test PDS manager assignment in reverse
        netDevice->SetSesManager(sesManager);

        // Test statistics with network device
        sesManager->Initialize();
        std::string stats = sesManager->GetStatistics();
        NS_TEST_ASSERT_MSG_NE(stats.length(), 0, "Statistics should be available with network device");
    }
};

/**
 * @brief SES Manager Test Suite
 */
class SesManagerTestSuite : public TestSuite
{
public:
    SesManagerTestSuite() : TestSuite("soft-ue-ses", Type::UNIT)
    {
        AddTestCase(new SesManagerBasicTest(), Duration::QUICK);
        AddTestCase(new SesManagerSendRequestTest(), Duration::QUICK);
        AddTestCase(new SesManagerReceiveRequestTest(), Duration::QUICK);
        AddTestCase(new SesManagerResponseTest(), Duration::QUICK);
        AddTestCase(new SesManagerValidationTest(), Duration::QUICK);
        AddTestCase(new SesManagerPdsInteractionTest(), Duration::QUICK);
        AddTestCase(new SesManagerNetDeviceTest(), Duration::QUICK);
    }
};

static SesManagerTestSuite g_sesManagerTestSuite;

} // namespace ns3
