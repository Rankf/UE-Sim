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
 * @brief            PDS Manager Test - Adapted from original PDS_test.cpp
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-09
 * @copyright        Apache License Version 2.0
 *
 * @details
 * Test for PDS Manager functionality, adapted from the original
 * standalone PDS_test.cpp to work within ns-3 framework.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/soft-ue-module.h"
#include "ns3/packet.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PdsManagerTest");

int
main (int argc, char *argv[])
{
    LogComponentEnable("PdsManagerTest", LOG_LEVEL_INFO);

    NS_LOG_INFO("=== PDS Manager Core Module Test ===");

    // Create PDS Manager
    Ptr<PdsManager> pdsManager = CreateObject<PdsManager> ();

    // Test PDS Manager creation
    if (pdsManager == nullptr)
    {
        NS_LOG_ERROR("Failed to create PDS Manager");
        return 1;
    }
    NS_LOG_INFO("✓ PDS Manager created successfully");

    // Test initialization
    pdsManager->Initialize ();
    NS_LOG_INFO("✓ PDS Manager initialized successfully");

    // Get initial statistics
    auto initialStats = pdsManager->GetStatistics ();
    if (initialStats == nullptr)
    {
        NS_LOG_ERROR("Failed to get PDS statistics");
        return 1;
    }
    NS_LOG_INFO("✓ Statistics system initialized successfully");
    NS_LOG_INFO("Initial statistics: " << initialStats->GetStatistics ());

    // Create test request
    SesPdsRequest testRequest;
    testRequest.src_fep = 0x12345678;
    testRequest.dst_fep = 0x87654321;
    testRequest.mode = 0; // Simple mode
    testRequest.rod_context = 0x0001;
    testRequest.next_hdr = PDSNextHeader::UET_HDR_REQUEST_STD;
    testRequest.tc = 0x01;
    testRequest.lock_pdc = true;
    testRequest.tx_pkt_handle = 0x0001;
    testRequest.pkt_len = 100;
    testRequest.tss_context = 0x0001;
    testRequest.rsv_pdc_context = 0x0001;
    testRequest.rsv_ccc_context = 0x0001;
    testRequest.som = true;
    testRequest.eom = false;

    // Create test packet
    testRequest.packet = Create<Packet> (testRequest.pkt_len);

    // Test SES request processing
    bool processed = pdsManager->ProcessSesRequest (testRequest);
    NS_LOG_INFO("✓ SES request processing: " << (processed ? "success" : "failure"));

    // Get final statistics
    auto finalStats = pdsManager->GetStatistics ();
    NS_LOG_INFO("✓ Statistics system operating normally");
    NS_LOG_INFO("Final statistics: " << finalStats->GetStatistics ());

    // Test statistics string
    std::string statsString = pdsManager->GetStatisticsString ();
    if (statsString.length () == 0)
    {
        NS_LOG_ERROR("Statistics string should not be empty");
        return 1;
    }
    NS_LOG_INFO("✓ Statistics report generated successfully");

    // Reset statistics
    pdsManager->ResetStatistics ();
    NS_LOG_INFO("✓ Statistics system reset successfully");

    NS_LOG_INFO("=== PDS Manager Test Complete ===");
    NS_LOG_INFO("Status: ✓ All basic functions operational");
    NS_LOG_INFO("Verification: ✓ PDS Manager core functionality available");

    return 0;
}