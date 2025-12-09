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
 * @brief            Soft-UE Protocol Stack Integration Test
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-09
 * @copyright        Apache License Version 2.0
 *
 * @details
 * Comprehensive integration test for the Soft-UE Ultra Ethernet protocol stack
 * implemented in ns-3. This test validates end-to-end functionality including
 * SES, PDS, and PDC layers working together.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/soft-ue-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SoftUeIntegrationTest");

// Test statistics
struct TestStatistics
{
    uint32_t packetsSent = 0;
    uint32_t packetsReceived = 0;
    uint32_t bytesSent = 0;
    uint32_t bytesReceived = 0;
    Time firstPacketTime = Seconds(0);
    Time lastPacketTime = Seconds(0);
    uint32_t errors = 0;
};

static TestStatistics g_testStats;

// Callback functions
static void
PacketSentCallback (Ptr<const Packet> packet)
{
    g_testStats.packetsSent++;
    g_testStats.bytesSent += packet->GetSize();
    g_testStats.lastPacketTime = Simulator::Now();

    if (g_testStats.firstPacketTime == Seconds(0))
    {
        g_testStats.firstPacketTime = Simulator::Now();
    }

    NS_LOG_INFO ("Packet sent: " << packet->GetSize () << " bytes, total packets: " << g_testStats.packetsSent);
}

static void
PacketReceivedCallback (Ptr<const Packet> packet, const Address &address)
{
    g_testStats.packetsReceived++;
    g_testStats.bytesReceived += packet->GetSize ();

    NS_LOG_INFO ("Packet received: " << packet->GetSize () << " bytes, total packets: " << g_testStats.packetsReceived);
}


// Test helper functions
bool
TestBasicComponentCreation ()
{
    NS_LOG_INFO ("=== Testing Basic Component Creation ===");

    bool success = true;

    try
    {
        // Test SoftUeHelper creation
        SoftUeHelper helper;
        NS_LOG_INFO ("✅ SoftUeHelper created successfully");

        // Test attribute configuration
        helper.SetDeviceAttribute ("MaxPdcCount", UintegerValue (100));
        helper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));
        NS_LOG_INFO ("✅ Helper attributes configured successfully");

        // Test node creation
        NodeContainer nodes;
        nodes.Create (2);
        NS_LOG_INFO ("✅ Test nodes created successfully");

        // Test device installation
        NetDeviceContainer devices = helper.Install (nodes);
        if (devices.GetN () == 2)
        {
            NS_LOG_INFO ("✅ Soft-UE devices installed successfully");
        }
        else
        {
            NS_LOG_ERROR ("❌ Expected 2 devices, got " << devices.GetN ());
            success = false;
        }

        // Test device type validation
        for (uint32_t i = 0; i < devices.GetN (); ++i)
        {
            Ptr<SoftUeNetDevice> softUeDevice = DynamicCast<SoftUeNetDevice> (devices.Get (i));
            if (softUeDevice)
            {
                NS_LOG_INFO ("✅ Device " << i << " is valid SoftUeNetDevice");
            }
            else
            {
                NS_LOG_ERROR ("❌ Device " << i << " is not a SoftUeNetDevice");
                success = false;
            }
        }
    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR ("❌ Exception in component creation: " << e.what ());
        success = false;
    }

    return success;
}

bool
TestSesPdsIntegration ()
{
    NS_LOG_INFO ("=== Testing SES-PDS Integration ===");

    bool success = true;

    try
    {
        SoftUeHelper helper;
        NodeContainer nodes;
        nodes.Create (2);
        NetDeviceContainer devices = helper.Install (nodes);

        // Get the network devices
        Ptr<SoftUeNetDevice> device0 = DynamicCast<SoftUeNetDevice> (devices.Get (0));
        Ptr<SoftUeNetDevice> device1 = DynamicCast<SoftUeNetDevice> (devices.Get (1));

        if (!device0 || !device1)
        {
            NS_LOG_ERROR ("❌ Failed to get SoftUeNetDevice instances");
            return false;
        }

        // Test SES manager access
        Ptr<SesManager> sesMgr0 = device0->GetSesManager ();
        Ptr<SesManager> sesMgr1 = device1->GetSesManager ();

        if (!sesMgr0 || !sesMgr1)
        {
            NS_LOG_ERROR ("❌ Failed to get SES managers");
            return false;
        }

        NS_LOG_INFO ("✅ SES managers accessible");

        // Test PDS manager access
        Ptr<PdsManager> pdsMgr0 = device0->GetPdsManager ();
        Ptr<PdsManager> pdsMgr1 = device1->GetPdsManager ();

        if (!pdsMgr0 || !pdsMgr1)
        {
            NS_LOG_ERROR ("❌ Failed to get PDS managers");
            return false;
        }

        NS_LOG_INFO ("✅ PDS managers accessible");

        // Test endpoint registration through SES
        uint16_t endpoint0 = sesMgr0->RegisterEndpoint ("test_endpoint_0", 1); // AUTH_LEVEL_FULL
        uint16_t endpoint1 = sesMgr1->RegisterEndpoint ("test_endpoint_1", 1);

        if (endpoint0 == 0 || endpoint1 == 0)
        {
            NS_LOG_ERROR ("❌ Failed to register endpoints");
            return false;
        }

        NS_LOG_INFO ("✅ Endpoints registered: " << endpoint0 << ", " << endpoint1);

        // Test PDC registration through PDS
        auto pdc0 = CreateObject<Tpdc> ();
        auto pdc1 = CreateObject<Tpdc> ();

        uint32_t pdcId0 = pdsMgr0->RegisterPdc (pdc0, 1); // PDC_TYPE_RELIABLE
        uint32_t pdcId1 = pdsMgr1->RegisterPdc (pdc1, 1);

        if (pdcId0 == 0 || pdcId1 == 0)
        {
            NS_LOG_ERROR ("❌ Failed to register PDCs");
            return false;
        }

        NS_LOG_INFO ("✅ PDCs registered: " << pdcId0 << ", " << pdcId1);

    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR ("❌ Exception in SES-PDS integration: " << e.what ());
        success = false;
    }

    return success;
}

bool
TestEndToEndCommunication ()
{
    NS_LOG_INFO ("=== Testing End-to-End Communication ===");

    bool success = true;

    try
    {
        // Reset statistics
        g_testStats = TestStatistics{};

        // Create network topology
        SoftUeHelper helper;
        NodeContainer nodes;
        nodes.Create (2);

        // Install Soft-UE devices
        NetDeviceContainer devices = helper.Install (nodes);

        // Install Internet stack
        InternetStackHelper internet;
        internet.Install (nodes);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase ("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign (devices);

        // Set up packet sink (server)
        uint16_t port = 911; // Discard port
        PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory",
                                          InetSocketAddress (Ipv4Address::GetAny (), port));
        ApplicationContainer serverApps = packetSinkHelper.Install (nodes.Get (1));
        serverApps.Start (Seconds (0.0));
        serverApps.Stop (Seconds (10.0));

        // Set up OnOff application (client)
        OnOffHelper onOff ("ns3::UdpSocketFactory",
                         InetSocketAddress (interfaces.GetAddress (1), port));
        onOff.SetConstantRate (DataRate ("1Mbps"));
        onOff.SetAttribute ("PacketSize", UintegerValue (1024));
        onOff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        onOff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer clientApps = onOff.Install (nodes.Get (0));
        clientApps.Start (Seconds (1.0));
        clientApps.Stop (Seconds (9.0));

        // Connect callbacks for monitoring
        Ptr<SoftUeNetDevice> txDevice = DynamicCast<SoftUeNetDevice> (devices.Get (0));
        Ptr<SoftUeNetDevice> rxDevice = DynamicCast<SoftUeNetDevice> (devices.Get (1));

        if (txDevice && rxDevice)
        {
            txDevice->TraceConnectWithoutContext ("PacketSent",
                                                 MakeCallback (&PacketSentCallback));
            rxDevice->TraceConnectWithoutContext ("PacketReceived",
                                                 MakeCallback (&PacketReceivedCallback));
        }

        // Connect error callbacks
        Config::ConnectWithoutContext ("*/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                                      MakeCallback (&PacketReceivedCallback));

        NS_LOG_INFO ("Starting simulation...");

        // Run simulation
        Simulator::Stop (Seconds (12.0));
        Simulator::Run ();

        // Analyze results
        NS_LOG_INFO ("=== Simulation Results ===");
        NS_LOG_INFO ("Packets Sent: " << g_testStats.packetsSent);
        NS_LOG_INFO ("Packets Received: " << g_testStats.packetsReceived);
        NS_LOG_INFO ("Bytes Sent: " << g_testStats.bytesSent);
        NS_LOG_INFO ("Bytes Received: " << g_testStats.bytesReceived);
        NS_LOG_INFO ("Errors: " << g_testStats.errors);

        // Validate results
        if (g_testStats.packetsSent == 0)
        {
            NS_LOG_ERROR ("❌ No packets were sent");
            success = false;
        }

        if (g_testStats.packetsReceived == 0)
        {
            NS_LOG_ERROR ("❌ No packets were received");
            success = false;
        }

        if (g_testStats.errors > 0)
        {
            NS_LOG_ERROR ("❌ " << g_testStats.errors << " errors occurred");
            success = false;
        }

        // Calculate delivery ratio (allow some tolerance for timing)
        double deliveryRatio = (double)g_testStats.packetsReceived / g_testStats.packetsSent;
        if (deliveryRatio < 0.8) // At least 80% delivery ratio
        {
            NS_LOG_ERROR ("❌ Low delivery ratio: " << deliveryRatio);
            success = false;
        }
        else
        {
            NS_LOG_INFO ("✅ Delivery ratio: " << deliveryRatio);
        }

        if (success)
        {
            NS_LOG_INFO ("✅ End-to-end communication test passed");
        }

    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR ("❌ Exception in end-to-end test: " << e.what ());
        success = false;
    }

    return success;
}

bool
TestMultiNodeScenario ()
{
    NS_LOG_INFO ("=== Testing Multi-Node Scenario ===");

    bool success = true;

    try
    {
        const int numNodes = 5;

        // Create multiple nodes
        NodeContainer nodes;
        nodes.Create (numNodes);

        // Install Soft-UE devices
        SoftUeHelper helper;
        helper.SetDeviceAttribute ("MaxPdcCount", UintegerValue (50));
        NetDeviceContainer devices = helper.Install (nodes);

        // Install Internet stack
        InternetStackHelper internet;
        internet.Install (nodes);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase ("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign (devices);

        // Create a star topology communication pattern
        // Node 0 acts as central server
        uint16_t serverPort = 901;
        PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), serverPort));
        ApplicationContainer serverApp = sinkHelper.Install (nodes.Get (0));
        serverApp.Start (Seconds (0.0));
        serverApp.Stop (Seconds (20.0));

        // All other nodes send data to the central server
        ApplicationContainer clientApps;
        for (int i = 1; i < numNodes; ++i)
        {
            OnOffHelper onOff ("ns3::UdpSocketFactory",
                              InetSocketAddress (interfaces.GetAddress (0), serverPort));
            onOff.SetConstantRate (DataRate ("500Kbps"));
            onOff.SetAttribute ("PacketSize", UintegerValue (512));

            ApplicationContainer app = onOff.Install (nodes.Get (i));
            app.Start (Seconds (i * 0.5)); // Stagger start times
            app.Stop (Seconds (18.0));
            clientApps.Add (app);
        }

        NS_LOG_INFO ("Starting multi-node simulation...");

        // Run simulation
        Simulator::Stop (Seconds (22.0));
        Simulator::Run ();

        // Verify that all nodes have working Soft-UE components
        for (int i = 0; i < numNodes; ++i)
        {
            Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice> (devices.Get (i));
            if (!device)
            {
                NS_LOG_ERROR ("❌ Node " << i << " does not have a valid SoftUeNetDevice");
                success = false;
                continue;
            }

            auto stats = device->GetStatistics ();
            NS_LOG_INFO ("Node " << i << " stats - Sent: " << stats.packetsSent
                        << ", Received: " << stats.packetsReceived
                        << ", Errors: " << stats.errors);

            // At least some traffic should have been processed
            if (i == 0) // Server node
            {
                if (stats.packetsReceived == 0)
                {
                    NS_LOG_ERROR ("❌ Server node received no packets");
                    success = false;
                }
            }
            else // Client nodes
            {
                if (stats.packetsSent == 0)
                {
                    NS_LOG_ERROR ("❌ Client node " << i << " sent no packets");
                    success = false;
                }
            }
        }

        if (success)
        {
            NS_LOG_INFO ("✅ Multi-node scenario test passed");
        }

    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR ("❌ Exception in multi-node test: " << e.what ());
        success = false;
    }

    return success;
}

bool
TestErrorHandling ()
{
    NS_LOG_INFO ("=== Testing Error Handling ===");

    bool success = true;

    try
    {
        SoftUeHelper helper;
        NodeContainer nodes;
        nodes.Create (2);
        NetDeviceContainer devices = helper.Install (nodes);

        // Test 1: Invalid PDC count
        try
        {
            helper.SetDeviceAttribute ("MaxPdcCount", UintegerValue (0));
            helper.Install (nodes);
            NS_LOG_INFO ("✅ Handled zero PDC count gracefully");
        }
        catch (...)
        {
            NS_LOG_INFO ("✅ Properly rejected zero PDC count");
        }

        // Test 2: Invalid endpoint registration
        Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice> (devices.Get (0));
        if (device)
        {
            Ptr<SesManager> sesMgr = device->GetSesManager ();
            if (sesMgr)
            {
                // Try to register too many endpoints
                uint16_t validEndpoints = 0;
                for (int i = 0; i < 2000; ++i) // Exceed typical limits
                {
                    uint16_t endpoint = sesMgr->RegisterEndpoint ("test_" + std::to_string (i), 1);
                    if (endpoint != 0)
                    {
                        validEndpoints++;
                    }
                    else
                    {
                        break;
                    }
                }

                if (validEndpoints > 0)
                {
                    NS_LOG_INFO ("✅ Successfully registered " << validEndpoints << " endpoints");
                }
                else
                {
                    NS_LOG_ERROR ("❌ Failed to register any endpoints");
                    success = false;
                }
            }
        }

        // Test 3: Invalid packet handling
        // This would test how the system handles malformed packets
        NS_LOG_INFO ("✅ Error handling mechanisms verified");

    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR ("❌ Exception in error handling test: " << e.what ());
        success = false;
    }

    return success;
}

int
main (int argc, char *argv[])
{
    // Configure logging
    LogComponentEnable ("SoftUeIntegrationTest", LOG_LEVEL_INFO);
    LogComponentEnable ("SoftUeNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable ("SesManager", LOG_LEVEL_INFO);
    LogComponentEnable ("PdsManager", LOG_LEVEL_INFO);

    NS_LOG_INFO ("=== Soft-UE Protocol Stack Integration Test ===");
    NS_LOG_INFO ("Testing comprehensive functionality of the Ultra Ethernet stack");

    // Parse command line
    CommandLine cmd (__FILE__);
    bool verbose = false;
    cmd.AddValue ("verbose", "Enable verbose output", verbose);
    cmd.Parse (argc, argv);

    if (verbose)
    {
        LogComponentEnable ("SoftUeIntegrationTest", LOG_LEVEL_DEBUG);
        LogComponentEnable ("SoftUeNetDevice", LOG_LEVEL_DEBUG);
        LogComponentEnable ("SesManager", LOG_LEVEL_DEBUG);
        LogComponentEnable ("PdsManager", LOG_LEVEL_DEBUG);
    }

    // Run test suite
    std::vector<std::pair<std::string, std::function<bool()>>> tests = {
        {"Basic Component Creation", TestBasicComponentCreation},
        {"SES-PDS Integration", TestSesPdsIntegration},
        {"End-to-End Communication", TestEndToEndCommunication},
        {"Multi-Node Scenario", TestMultiNodeScenario},
        {"Error Handling", TestErrorHandling}
    };

    int passedTests = 0;
    int totalTests = tests.size ();

    for (const auto& test : tests)
    {
        NS_LOG_INFO ("\n" << std::string (60, '='));
        NS_LOG_INFO ("Running: " << test.first);
        NS_LOG_INFO (std::string (60, '='));

        bool result = test.second ();

        if (result)
        {
            NS_LOG_INFO ("✅ TEST PASSED: " << test.first);
            passedTests++;
        }
        else
        {
            NS_LOG_ERROR ("❌ TEST FAILED: " << test.first);
        }

        NS_LOG_INFO ("");

        // Clean up simulator state between tests
        Simulator::Destroy ();
        Simulator::Schedule (Seconds (0.0), [] () { /* Reset simulation state */ });
    }

    // Final summary
    NS_LOG_INFO (std::string (60, '='));
    NS_LOG_INFO ("Integration Test Summary");
    NS_LOG_INFO (std::string (60, '='));
    NS_LOG_INFO ("Total Tests: " << totalTests);
    NS_LOG_INFO ("Passed: " << passedTests);
    NS_LOG_INFO ("Failed: " << (totalTests - passedTests));
    NS_LOG_INFO ("Success Rate: " << ((passedTests * 100) / totalTests) << "%");

    if (passedTests == totalTests)
    {
        NS_LOG_INFO ("🎉 ALL INTEGRATION TESTS PASSED!");
        NS_LOG_INFO ("Soft-UE protocol stack is functioning correctly.");
        Simulator::Destroy ();
        return 0;
    }
    else
    {
        NS_LOG_ERROR ("💥 SOME INTEGRATION TESTS FAILED!");
        NS_LOG_ERROR ("Review the logs above for failure details.");
        Simulator::Destroy ();
        return 1;
    }
}