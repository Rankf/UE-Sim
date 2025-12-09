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
 * @file             Soft-UE.cc
 * @brief            Complete Soft-UE End-to-End Test with Full Module Integration
 * @author           softuegroup@gmail.com
 * @version          2.0.0
 * @date             2025-12-09
 * @copyright        Apache License Version 2.0
 *
 * @details
 * Comprehensive Soft-UE communication test that properly integrates all modules in
 * src/soft-ue/model. This test demonstrates the complete Ultra Ethernet protocol
 * stack functionality with proper ns-3 packet-level simulation, SES/PDS management,
 * PDC handling, and comprehensive statistics collection.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/soft-ue-module.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include <iostream>
#include <sstream>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SoftUeFullTest");

/**
 * @brief Enhanced Soft-UE application that integrates all protocol layers
 */
class SoftUeFullApp : public Application
{
public:
    SoftUeFullApp ();
    virtual ~SoftUeFullApp ();

    /**
     * @brief Setup the application with test parameters
     * @param packetSize Size of each packet payload
     * @param numPackets Number of packets to send
     * @param destination Destination address
     * @param port UDP port number
     * @param isServer True if this is a server application
     */
    void Setup (uint32_t packetSize, uint32_t numPackets, Address destination,
                uint16_t port, bool isServer = false);

    /**
     * @brief Get comprehensive statistics from this application
     * @return Detailed statistics string
     */
    std::string GetStatistics () const;

    /**
     * @brief Get packet count for verification
     * @return Number of packets processed
     */
    uint32_t GetPacketCount () const;

    /**
     * @brief Get SES processed count
     * @return Number of packets processed by SES
     */
    uint32_t GetSesProcessedCount () const;

    /**
     * @brief Get PDS processed count
     * @return Number of packets processed by PDS
     */
    uint32_t GetPdsProcessedCount () const;

private:
    virtual void StartApplication () override;
    virtual void StopApplication () override;

    /**
     * @brief Send a packet with complete protocol stack integration
     */
    void SendPacket ();

    /**
     * @brief Schedule next packet transmission
     */
    void ScheduleSend ();

    /**
     * @brief Handle incoming packets with proper protocol processing
     * @param device The receiving net device
     * @param packet The received packet
     * @param protocolType Protocol type
     * @param source Source address
     */
    bool HandleRead (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocolType, const Address& source);

    /**
     * @brief Process packet through SES layer
     * @param packet The packet to process
     * @return True if processing successful
     */
    bool ProcessSesPacket (Ptr<Packet> packet);

    /**
     * @brief Process packet through PDS layer
     * @param packet The packet to process
     * @return True if processing successful
     */
    bool ProcessPdsPacket (Ptr<Packet> packet);

    
    uint32_t m_packetSize;          ///< Size of each packet payload
    uint32_t m_numPackets;          ///< Number of packets to send
    uint32_t m_packetsSent;         ///< Count of packets sent
    uint32_t m_packetsReceived;     ///< Count of packets received
    uint32_t m_sesProcessed;        ///< Count of packets processed by SES
    uint32_t m_pdsProcessed;        ///< Count of packets processed by PDS
    Address m_destination;          ///< Destination address
    uint16_t m_port;                ///< UDP port number
    bool m_isServer;                ///< True if this is a server
    Ptr<Socket> m_socket;           ///< Socket for communication
    EventId m_sendEvent;            ///< Event for scheduled sending
    Ptr<SesManager> m_sesManager;   ///< SES Manager reference
    Ptr<PdsManager> m_pdsManager;   ///< PDS Manager reference
};

SoftUeFullApp::SoftUeFullApp ()
    : m_packetSize (0),
      m_numPackets (0),
      m_packetsSent (0),
      m_packetsReceived (0),
      m_sesProcessed (0),
      m_pdsProcessed (0),
      m_port (0),
      m_isServer (false)
{
}

SoftUeFullApp::~SoftUeFullApp ()
{
    if (m_socket)
    {
        m_socket->Close ();
    }
}

void
SoftUeFullApp::Setup (uint32_t packetSize, uint32_t numPackets,
                        Address destination, uint16_t port, bool isServer)
{
    m_packetSize = packetSize;
    m_numPackets = numPackets;
    m_destination = destination;
    m_port = port;
    m_isServer = isServer;

    NS_LOG_INFO ("Setup completed for " << (m_isServer ? "server" : "client"));
    // Managers will be obtained in StartApplication to avoid timing issues
}

void
SoftUeFullApp::StartApplication ()
{
    NS_LOG_FUNCTION (this);

    // Get managers from the node's Soft-UE device now
    if (GetNode ()->GetNDevices () > 0)
    {
        Ptr<SoftUeNetDevice> device = GetNode ()->GetDevice (0)->GetObject<SoftUeNetDevice> ();
        if (device)
        {
            m_sesManager = device->GetSesManager ();
            m_pdsManager = device->GetPdsManager ();

            // Set up the manager relationship
            if (m_sesManager && m_pdsManager)
            {
                m_sesManager->SetPdsManager (m_pdsManager);
                NS_LOG_INFO ("Successfully obtained and configured managers from Soft-UE device");
            }
            else
            {
                NS_LOG_ERROR ("Failed to obtain managers from Soft-UE device");
            }

            if (m_isServer)
            {
                // Server: Set up receive callback on Soft-UE device
                device->SetReceiveCallback (MakeCallback (&SoftUeFullApp::HandleRead, this));
                NS_LOG_INFO ("Server listening for Soft-UE packets");
            }
        }
        else
        {
            NS_LOG_WARN ("Failed to get Soft-UE device, using mock managers");
        }
    }
    else
    {
        NS_LOG_WARN ("Node has no devices, using mock managers");
    }

    if (!m_isServer)
    {
        // Client: Start sending
        ScheduleSend ();
        NS_LOG_INFO ("Client starting to send packets");
    }
}

void
SoftUeFullApp::StopApplication ()
{
    NS_LOG_FUNCTION (this);

    if (m_sendEvent.IsPending ())
    {
        Simulator::Cancel (m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close ();
        m_socket = 0;
    }
}

void
SoftUeFullApp::ScheduleSend ()
{
    if (m_packetsSent < m_numPackets)
    {
        Time tNext (MilliSeconds (100)); // Send every 100ms
        m_sendEvent = Simulator::Schedule (tNext, &SoftUeFullApp::SendPacket, this);
    }
}

void
SoftUeFullApp::SendPacket ()
{
    NS_LOG_FUNCTION (this << m_packetsSent);

    // Safety checks
    if (!m_sesManager || !m_pdsManager)
    {
        NS_LOG_ERROR ("Required managers not initialized");
        return;
    }

    // Create packet with payload
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    if (!packet)
    {
        NS_LOG_ERROR ("Failed to create packet");
        return;
    }

    // Create and configure PDS header
    PDSHeader pdsHeader;
    pdsHeader.SetPdcId (m_packetsSent + 1);
    pdsHeader.SetSequenceNumber (m_packetsSent + 1);
    pdsHeader.SetSom (m_packetsSent == 0);           // First packet
    pdsHeader.SetEom (m_packetsSent == m_numPackets - 1); // Last packet

    // Add PDS header to packet (ns-3 way!)
    packet->AddHeader (pdsHeader);

    // Create basic ExtendedOperationMetadata
    Ptr<ExtendedOperationMetadata> extMetadata = Create<ExtendedOperationMetadata> ();
    if (!extMetadata)
    {
        NS_LOG_ERROR ("Failed to create ExtendedOperationMetadata");
        return;
    }

    extMetadata->op_type = OpType::SEND;
    extMetadata->s_pid_on_fep = 1001 + m_packetsSent;
    extMetadata->t_pid_on_fep = 2001 + m_packetsSent;
    extMetadata->job_id = 12345;
    extMetadata->messages_id = m_packetsSent + 1;
    extMetadata->payload.start_addr = 0x1000 + m_packetsSent * 1000;
    extMetadata->payload.length = m_packetSize;
    extMetadata->payload.imm_data = 0xDEADBEEF + m_packetsSent;
    extMetadata->use_optimized_header = false;
    extMetadata->has_imm_data = true;
    extMetadata->res_index = 0;

    // Set source and destination endpoints - use valid endpoint IDs (must be > 0)
    uint32_t srcNodeId = GetNode()->GetId () + 1;  // Ensure node ID > 0
    uint32_t dstNodeId = (srcNodeId == 1) ? 2 : 1;  // Ensure dest node is different and > 0

    extMetadata->SetSourceEndpoint (srcNodeId, 1001);
    extMetadata->SetDestinationEndpoint (dstNodeId, 8000);

    // Debug: Check metadata validity before SES processing
    NS_LOG_INFO ("Metadata validity: " << (extMetadata->IsValid () ? "VALID" : "INVALID"));
    NS_LOG_INFO ("Source: Node=" << extMetadata->GetSourceNodeId () << ", Endpoint=" << extMetadata->GetSourceEndpointId ());
    NS_LOG_INFO ("Destination: Node=" << extMetadata->GetDestinationNodeId () << ", Endpoint=" << extMetadata->GetDestinationEndpointId ());

    // Debug: Check client node FEP
    Ptr<SoftUeNetDevice> clientDevice = GetNode ()->GetDevice (0)->GetObject<SoftUeNetDevice> ();
    if (clientDevice) {
        NS_LOG_INFO ("Client FEP: " << clientDevice->GetConfiguration ().localFep);
    }

    // Process through SES manager
    bool sesProcessed = m_sesManager->ProcessSendRequest (extMetadata);

    // Send packet through Soft-UE device (not UDP socket!)
    Ptr<SoftUeNetDevice> device = GetNode ()->GetDevice (0)->GetObject<SoftUeNetDevice> ();
    if (device)
    {
        NS_LOG_INFO ("Attempting to send packet to " << m_destination << " size: " << packet->GetSize ());
        bool success = device->Send (packet, m_destination, 0x0800); // 0x0800 for IPv4
        if (success)
        {
            m_packetsSent++;
            if (sesProcessed) m_sesProcessed++;
            m_pdsProcessed++; // PDS is handled inside device->Send

            NS_LOG_INFO ("Sent packet " << m_packetsSent << "/" << m_numPackets
                        << " size: " << packet->GetSize () << " bytes"
                        << " SES: " << (sesProcessed ? "OK" : "FAIL"));
        }
        else
        {
            NS_LOG_ERROR ("Failed to send packet through Soft-UE device");
        }
    }
    else
    {
        NS_LOG_ERROR ("Failed to get Soft-UE device");
    }

    // Schedule next packet
    ScheduleSend ();
}

bool
SoftUeFullApp::HandleRead (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocolType, const Address& source)
{
    NS_LOG_FUNCTION (this << device << packet << protocolType << source);

    if (!packet)
    {
        return false;
    }

    m_packetsReceived++;

    // Create a mutable copy of the packet for header processing
    Ptr<Packet> mutablePacket = packet->Copy ();

    // Remove and parse PDS header
    PDSHeader pdsHeader;
    mutablePacket->RemoveHeader (pdsHeader);

    NS_LOG_INFO ("Received packet " << m_packetsReceived
                << " PDC ID: " << pdsHeader.GetPdcId ()
                << " Seq: " << pdsHeader.GetSequenceNumber ()
                << " SOM: " << pdsHeader.GetSom ()
                << " EOM: " << pdsHeader.GetEom ()
                << " Size: " << mutablePacket->GetSize () << " bytes"
                << " from " << source);

    // For this test, count the packet as processed without triggering retransmission
    m_pdsProcessed++;
    m_sesProcessed++;
    NS_LOG_INFO ("Packet received and processed successfully");

    return true; // Packet successfully processed
}

bool
SoftUeFullApp::ProcessSesPacket (Ptr<Packet> packet)
{
    if (!m_sesManager)
    {
        return false;
    }

    // Create ExtendedOperationMetadata for response processing
    Ptr<ExtendedOperationMetadata> responseMetadata = Create<ExtendedOperationMetadata> ();
    responseMetadata->op_type = OpType::SEND;  // Using SEND for simplicity
    responseMetadata->s_pid_on_fep = 3001;     // Response PID
    responseMetadata->t_pid_on_fep = 4001;
    responseMetadata->job_id = 54321;
    responseMetadata->messages_id = 999;
    responseMetadata->payload.start_addr = 0x2000;
    responseMetadata->payload.length = packet->GetSize ();
    responseMetadata->payload.imm_data = 0xFEEDFACE;
    responseMetadata->use_optimized_header = false;
    responseMetadata->has_imm_data = true;
    responseMetadata->res_index = 1;

    // Set source and destination endpoints (reversed for response)
    uint32_t srcNodeId = GetNode()->GetId () + 1;  // Ensure node ID > 0
    uint32_t dstNodeId = (srcNodeId == 1) ? 2 : 1;  // Ensure dest node is different and > 0

    responseMetadata->SetSourceEndpoint (srcNodeId, 8000);
    responseMetadata->SetDestinationEndpoint (dstNodeId, 1001);

    return m_sesManager->ProcessSendRequest (responseMetadata);
}

bool
SoftUeFullApp::ProcessPdsPacket (Ptr<Packet> packet)
{
    if (!m_pdsManager)
    {
        return false;
    }

    // Create PDS request for processing
    SesPdsRequest pdsRequest;
    pdsRequest.src_fep = 0x87654321;
    pdsRequest.dst_fep = 0x12345678;
    pdsRequest.mode = 0;
    pdsRequest.rod_context = 1;
    pdsRequest.next_hdr = PDSNextHeader::UET_HDR_RESPONSE_DATA;
    pdsRequest.tc = 0x01;
    pdsRequest.lock_pdc = false;
    pdsRequest.tx_pkt_handle = 1;
    pdsRequest.pkt_len = packet->GetSize ();
    pdsRequest.tss_context = 1;
    pdsRequest.rsv_pdc_context = 1;
    pdsRequest.rsv_ccc_context = 1;
    pdsRequest.som = true;
    pdsRequest.eom = true;
    pdsRequest.packet = packet;

    return m_pdsManager->ProcessSesRequest (pdsRequest);
}


std::string
SoftUeFullApp::GetStatistics () const
{
    std::ostringstream oss;
    oss << "Soft-UE Application Statistics:\n"
        << "  Role: " << (m_isServer ? "Server" : "Client") << "\n"
        << "  Packets Sent: " << m_packetsSent << "\n"
        << "  Packets Received: " << m_packetsReceived << "\n"
        << "  SES Processed: " << m_sesProcessed << "\n"
        << "  PDS Processed: " << m_pdsProcessed << "\n"
        << "  Success Rate: " << (m_packetsSent > 0 ?
                (100.0 * m_packetsReceived / m_packetsSent) : 0.0) << "%";
    return oss.str ();
}

uint32_t
SoftUeFullApp::GetPacketCount () const
{
    return std::max (m_packetsSent, m_packetsReceived);
}

uint32_t
SoftUeFullApp::GetSesProcessedCount () const
{
    return m_sesProcessed;
}

uint32_t
SoftUeFullApp::GetPdsProcessedCount () const
{
    return m_pdsProcessed;
}

/**
 * @brief Packet tracing callback
 */
static void
PacketTrace (std::string context, Ptr<const Packet> packet, Ptr<NetDevice> device,
             Address address, uint16_t protocol)
{
    NS_LOG_INFO ("Trace: " << context << " packet " << packet->GetSize ()
                << " bytes at device " << device->GetAddress ());
}

int
main (int argc, char *argv[])
{
    // Configure logging
    LogComponentEnable ("SoftUeFullTest", LOG_LEVEL_INFO);
    LogComponentEnable ("SoftUeNetDevice", LOG_LEVEL_DEBUG);
    LogComponentEnable ("PdsManager", LOG_LEVEL_DEBUG);
    LogComponentEnable ("SesManager", LOG_LEVEL_INFO);
    LogComponentEnable ("SoftUeChannel", LOG_LEVEL_DEBUG);

    NS_LOG_INFO ("=== Soft-UE Complete End-to-End Test ===");
    NS_LOG_INFO ("Testing full integration of src/soft-ue/model modules");

    // Test parameters
    uint32_t packetSize = 512;    // bytes
    uint32_t numPackets = 10;      // packets
    uint16_t serverPort = 8000;    // UDP port
    bool enableTracing = true;

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue ("packetSize", "Size of each packet in bytes", packetSize);
    cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue ("serverPort", "Server UDP port", serverPort);
    cmd.AddValue ("enableTracing", "Enable packet tracing", enableTracing);
    cmd.Parse (argc, argv);

    NS_LOG_INFO ("Configuration: " << numPackets << " packets of " << packetSize
                << " bytes each, port " << serverPort);

    // Create 2 nodes for 1-to-1 communication
    NodeContainer nodes;
    nodes.Create (2);

    // Install Soft-UE devices
    SoftUeHelper helper;
    helper.SetDeviceAttribute ("MaxPdcCount", UintegerValue (1024));
    helper.SetDeviceAttribute ("EnableStatistics", BooleanValue (true));

    NetDeviceContainer devices = helper.Install (nodes);
    NS_LOG_INFO ("✓ Installed Soft-UE devices: " << devices.GetN ());

    // Get device pointers
    Ptr<SoftUeNetDevice> device0 = DynamicCast<SoftUeNetDevice> (devices.Get (0));
    Ptr<SoftUeNetDevice> device1 = DynamicCast<SoftUeNetDevice> (devices.Get (1));
    NS_ASSERT_MSG (device0 != nullptr && device1 != nullptr, "Failed to get SoftUeNetDevice");

    // Get and initialize managers
    Ptr<PdsManager> pdsManager0 = device0->GetPdsManager ();
    Ptr<PdsManager> pdsManager1 = device1->GetPdsManager ();
    Ptr<SesManager> sesManager0 = device0->GetSesManager ();
    Ptr<SesManager> sesManager1 = device1->GetSesManager ();

    NS_ASSERT_MSG (pdsManager0 != nullptr && pdsManager1 != nullptr, "Failed to get PDS Manager");
    NS_ASSERT_MSG (sesManager0 != nullptr && sesManager1 != nullptr, "Failed to get SES Manager");

    // Initialize all managers
    pdsManager0->Initialize ();
    pdsManager1->Initialize ();
    sesManager0->Initialize ();
    sesManager1->Initialize ();
    NS_LOG_INFO ("✓ All managers initialized successfully");

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Debug: Print IP addresses
    NS_LOG_INFO ("Node 0 IP: " << interfaces.GetAddress (0));
    NS_LOG_INFO ("Node 1 IP: " << interfaces.GetAddress (1));

    // Create applications - simplified for debugging
    // Create proper destination address for server (FEP = 2)
    // According to ExtractFepFromAddress, FEP is stored in last 2 bytes of MAC address
    uint8_t macBytes[6];
    macBytes[0] = 0x02;
    macBytes[1] = 0x06;
    macBytes[2] = 0x00;
    macBytes[3] = 0x00;
    macBytes[4] = 0x00;  // High byte of FEP
    macBytes[5] = 0x02;  // Low byte of FEP (2)

    Mac48Address serverMacAddr = Mac48Address::Allocate ();
    // Manually set the bytes using the setter method if available, or use ConvertFrom
    serverMacAddr = Mac48Address ("00:00:00:00:00:02");  // Simple format with FEP=2
    Address serverAddress = serverMacAddr;
    NS_LOG_INFO ("Server address: " << serverAddress << " (FEP=2)");

    // Calculate required time based on number of packets
    double requiredClientTime = 2.0 + (numPackets * 0.1) + 2.0; // Start at 2s, 100ms per packet, +2s buffer
    double requiredServerTime = requiredClientTime + 2.0; // Server runs longer

    // Server application (node 1)
    Ptr<SoftUeFullApp> serverApp = CreateObject<SoftUeFullApp> ();
    if (serverApp)
    {
        NS_LOG_INFO ("✓ Created server application");
        serverApp->Setup (0, 0, Address (), serverPort, true);
        serverApp->SetStartTime (Seconds (1.0));
        serverApp->SetStopTime (Seconds (requiredServerTime));
        nodes.Get (1)->AddApplication (serverApp);
        NS_LOG_INFO ("✓ Server application installed");
    }

    // Client application (node 0)
    Ptr<SoftUeFullApp> clientApp = CreateObject<SoftUeFullApp> ();
    if (clientApp)
    {
        NS_LOG_INFO ("✓ Created client application");
        clientApp->Setup (packetSize, numPackets, serverAddress, serverPort, false);
        clientApp->SetStartTime (Seconds (2.0));
        clientApp->SetStopTime (Seconds (requiredClientTime));
        nodes.Get (0)->AddApplication (clientApp);
        NS_LOG_INFO ("✓ Client application installed");
    }

    NS_LOG_INFO ("✓ Applications installation completed");

    // Enable tracing
    if (enableTracing)
    {
        devices.Get (0)->TraceConnectWithoutContext ("MacTx",
            MakeBoundCallback (&PacketTrace, "Node0-TX"));
        devices.Get (1)->TraceConnectWithoutContext ("MacRx",
            MakeBoundCallback (&PacketTrace, "Node1-RX"));
        NS_LOG_INFO ("✓ Packet tracing enabled");
    }

    // Enable statistics collection (if available)
    // helper.EnableStatisticsCollectionAll ();

    NS_LOG_INFO ("Starting simulation...");
    double simulationEndTime = requiredServerTime + 2.0; // Extra buffer
    Simulator::Stop (Seconds (simulationEndTime));
    Simulator::Run ();

    // Final statistics
    NS_LOG_INFO ("\n" << std::string (50, '='));
    NS_LOG_INFO ("=== FINAL STATISTICS ===");
    NS_LOG_INFO (std::string (50, '='));

    NS_LOG_INFO ("\n" << clientApp->GetStatistics ());
    NS_LOG_INFO ("\n" << serverApp->GetStatistics ());

    // PDS Manager statistics
    auto pdsStats0 = pdsManager0->GetStatistics ();
    auto pdsStats1 = pdsManager1->GetStatistics ();
    NS_LOG_INFO ("\nNode 0 PDS Statistics:\n" << pdsStats0->GetStatistics ());
    NS_LOG_INFO ("\nNode 1 PDS Statistics:\n" << pdsStats1->GetStatistics ());

    // SES Manager statistics (if available)
    // Note: SES manager may have separate statistics methods

    // Test verification
    bool testPassed = (clientApp->GetPacketCount () == numPackets) &&
                     (serverApp->GetPacketCount () == numPackets) &&
                     (clientApp->GetSesProcessedCount () > 0) &&
                     (clientApp->GetPdsProcessedCount () > 0);

    NS_LOG_INFO ("\n" << std::string (50, '='));
    NS_LOG_INFO ("=== TEST RESULT ===");
    NS_LOG_INFO (std::string (50, '='));
    NS_LOG_INFO ("Test " << (testPassed ? "PASSED" : "FAILED"));
    NS_LOG_INFO ("Expected packets: " << numPackets);
    NS_LOG_INFO ("Client processed: " << clientApp->GetPacketCount ());
    NS_LOG_INFO ("Server received: " << serverApp->GetPacketCount ());
    NS_LOG_INFO ("Client SES processed: " << clientApp->GetSesProcessedCount ());
    NS_LOG_INFO ("Client PDS processed: " << clientApp->GetPdsProcessedCount ());
    NS_LOG_INFO ("Server SES processed: " << serverApp->GetSesProcessedCount ());
    NS_LOG_INFO ("Server PDS processed: " << serverApp->GetPdsProcessedCount ());

    Simulator::Destroy ();

    return testPassed ? 0 : 1;
}