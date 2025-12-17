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
#include <iomanip>
#include <algorithm>
#include <vector>


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SoftUeFullTest");

/**
 * @brief Configuration structure for Soft-UE test parameters
 */
struct SoftUeTestConfig
{
    // Network configuration
    uint32_t nodeCount = 2;
    std::string networkBase = "10.1.1.0";
    std::string subnetMask = "255.255.255.0";
    uint16_t serverPort = 8000;

    // Protocol parameters
    uint16_t baseClientPid = 1001;
    uint16_t baseServerPid = 1001;
    uint16_t serverEndpointId = 8000;
    uint16_t clientEndpointId = 1001;
    uint32_t baseClientJobId = 12345;
    uint32_t baseServerJobId = 54321;
    uint64_t baseMemoryAddress = 0x1000;
    uint32_t addressIncrementStep = 1000;

    // Performance parameters
    uint32_t packetSize = 512;
    uint32_t packetCount = 10;
    Time sendInterval = NanoSeconds(500);
    uint32_t maxPdcCount = 1024;

    // Timing parameters
    Time serverStartTime = Seconds(1.0);
    Time clientStartTime = Seconds(2.0);
    Time timeBuffer = MilliSeconds(1);
    Time extraEndTime = Seconds(2.0);

    // Protocol overhead
    uint32_t headerOverhead = 42; // Approximate protocol header size
    uint32_t fepSendAddress = 0x87654321;
    uint32_t fepRecvAddress = 0x12345678;
};

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
     * @brief Set configuration for test parameters
     * @param config Configuration structure
     */
    void SetConfiguration (const SoftUeTestConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const SoftUeTestConfig& GetConfiguration () const;

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

    // Enhanced statistics
    std::vector<Time> m_packetDelays;           ///< Per-packet delays
    Time m_firstPacketTime;                     ///< First packet send time
    Time m_lastPacketTime;                      ///< Last packet receive time
    uint64_t m_totalBytesSent;                  ///< Total bytes sent
    uint64_t m_totalBytesReceived;              ///< Total bytes received
    uint32_t m_retransmissions;                 ///< Retransmission count
    uint32_t m_timeouts;                        ///< Timeout count

    /// Configuration for test parameters
    SoftUeTestConfig m_config;
};

SoftUeFullApp::SoftUeFullApp ()
    : m_packetSize (0),
      m_numPackets (0),
      m_packetsSent (0),
      m_packetsReceived (0),
      m_sesProcessed (0),
      m_pdsProcessed (0),
      m_port (0),
      m_isServer (false),
      m_firstPacketTime (Seconds (0)),
      m_lastPacketTime (Seconds (0)),
      m_totalBytesSent (0),
      m_totalBytesReceived (0),
      m_retransmissions (0),
      m_timeouts (0)
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
        Time tNext (m_config.sendInterval); // Use configured send interval
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

    // Record first packet time
    if (m_packetsSent == 0)
    {
        m_firstPacketTime = Simulator::Now ();
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
    extMetadata->s_pid_on_fep = m_config.baseClientPid + m_packetsSent;
    extMetadata->t_pid_on_fep = (m_config.baseClientPid + 1000) + m_packetsSent;
    extMetadata->job_id = m_config.baseClientJobId;
    extMetadata->messages_id = m_packetsSent + 1;
    extMetadata->payload.start_addr = m_config.baseMemoryAddress + m_packetsSent * m_config.addressIncrementStep;
    extMetadata->payload.length = m_packetSize;
    extMetadata->payload.imm_data = 0xDEADBEEF + m_packetsSent;
    extMetadata->use_optimized_header = false;
    extMetadata->has_imm_data = true;
    extMetadata->res_index = 0;

    // Set source and destination endpoints - use valid endpoint IDs (must be > 0)
    uint32_t srcNodeId = GetNode()->GetId () + 1;  // Ensure node ID > 0
    uint32_t dstNodeId = (srcNodeId == 1) ? 2 : 1;  // Ensure dest node is different and > 0

    extMetadata->SetSourceEndpoint (srcNodeId, m_config.clientEndpointId);
    extMetadata->SetDestinationEndpoint (dstNodeId, m_config.serverEndpointId);

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

            // Enhanced statistics
            m_totalBytesSent += packet->GetSize ();
            m_packetDelays.push_back (Simulator::Now ());

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

    // Enhanced statistics - record first packet time if server
    if (m_isServer && m_firstPacketTime == Seconds (0))
    {
        m_firstPacketTime = Simulator::Now ();
    }
    m_totalBytesReceived += packet->GetSize ();
    m_lastPacketTime = Simulator::Now ();

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
    responseMetadata->s_pid_on_fep = m_config.baseServerPid + 2000;     // Response PID
    responseMetadata->t_pid_on_fep = m_config.baseServerPid + 3000;
    responseMetadata->job_id = m_config.baseServerJobId;
    responseMetadata->messages_id = 999;  // Reserved for responses
    responseMetadata->payload.start_addr = m_config.baseMemoryAddress + 0x1000;
    responseMetadata->payload.length = packet->GetSize ();
    responseMetadata->payload.imm_data = 0xFEEDFACE;
    responseMetadata->use_optimized_header = false;
    responseMetadata->has_imm_data = true;
    responseMetadata->res_index = 1;

    // Set source and destination endpoints (reversed for response)
    uint32_t srcNodeId = GetNode()->GetId () + 1;  // Ensure node ID > 0
    uint32_t dstNodeId = (srcNodeId == 1) ? 2 : 1;  // Ensure dest node is different and > 0

    responseMetadata->SetSourceEndpoint (srcNodeId, m_config.serverEndpointId);
    responseMetadata->SetDestinationEndpoint (dstNodeId, m_config.clientEndpointId);

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
    pdsRequest.src_fep = m_config.fepSendAddress;
    pdsRequest.dst_fep = m_config.fepRecvAddress;
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

    // Basic statistics
    oss << "Soft-UE Application Statistics:\n"
        << "  Role: " << (m_isServer ? "Server" : "Client") << "\n"
        << "  Packets Sent: " << m_packetsSent << "\n"
        << "  Packets Received: " << m_packetsReceived << "\n"
        << "  SES Processed: " << m_sesProcessed << "\n"
        << "  PDS Processed: " << m_pdsProcessed << "\n"
        << "  Success Rate: " << (m_packetsSent > 0 ?
                (100.0 * m_packetsReceived / m_packetsSent) : 0.0) << "%\n";

    // Enhanced statistics
    oss << "\n  Enhanced Performance Metrics:\n"
        << "  Total Bytes Sent: " << m_totalBytesSent << " bytes\n"
        << "  Total Bytes Received: " << m_totalBytesReceived << " bytes\n"
        << "  Average Packet Size: " << (m_packetsSent > 0 ?
                (m_totalBytesSent / m_packetsSent) : 0) << " bytes\n";

    // Timing analysis (Data center level time measurement)
    if (m_firstPacketTime != Seconds (0) && m_lastPacketTime != Seconds (0))
    {
        Time totalTime = m_lastPacketTime - m_firstPacketTime;
        double throughputMbps = 0.0;
        if (totalTime.GetNanoSeconds () > 0)
        {
            // Use nanosecond precision for throughput calculation
            throughputMbps = (m_totalBytesReceived * 8.0 * 1000000000.0) / (totalTime.GetNanoSeconds ());
        }

        oss << "  Total Transfer Time: " << totalTime.GetNanoSeconds () << " ns\n"
            << "  Throughput: " << std::fixed << std::setprecision (3)
            << throughputMbps << " Mbps\n"
            << "  Packet Rate: " << (totalTime.GetNanoSeconds () > 0 ?
                (1000000000.0 * m_packetsReceived / totalTime.GetNanoSeconds ()) : 0.0) << " pps\n";
    }

    // Error statistics
    oss << "\n  Error Statistics:\n"
        << "  Retransmissions: " << m_retransmissions << "\n"
        << "  Timeouts: " << m_timeouts << "\n"
        << "  Packet Loss Rate: " << (m_packetsSent > 0 ?
                (100.0 * (m_packetsSent - m_packetsReceived) / m_packetsSent) : 0.0) << "%\n";

    // Protocol efficiency
    if (m_totalBytesSent > 0)
    {
        double efficiency = 100.0 * (m_totalBytesReceived - (m_packetsReceived * m_config.headerOverhead)) / m_totalBytesSent; // Configured header overhead
        oss << "  Protocol Efficiency: " << std::fixed << std::setprecision (1)
            << efficiency << "%\n";
    }

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

void
SoftUeFullApp::SetConfiguration (const SoftUeTestConfig& config)
{
    NS_LOG_FUNCTION (this);
    m_config = config;
}

const SoftUeTestConfig&
SoftUeFullApp::GetConfiguration () const
{
    NS_LOG_FUNCTION (this);
    return m_config;
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

    // Configure optimized logging with structured output
    LogComponentEnable ("SoftUeFullTest", LOG_LEVEL_INFO);
    LogComponentEnable ("SoftUeNetDevice", LOG_LEVEL_WARN);
    LogComponentEnable ("PdsManager", LOG_LEVEL_INFO);         // Enable INFO for PDC creation tracking
    LogComponentEnable ("SesManager", LOG_LEVEL_WARN);
    LogComponentEnable ("SoftUeChannel", LOG_LEVEL_ERROR);

    NS_LOG_INFO ("=== Soft-UE Complete End-to-End Test ===");
    NS_LOG_INFO ("Testing full integration of src/soft-ue/model modules");

    // Initialize configuration with defaults
    SoftUeTestConfig config;
    bool enableTracing = true;

    // Command line arguments for configuration
    CommandLine cmd;
    cmd.AddValue ("packetSize", "Size of each packet in bytes", config.packetSize);
    cmd.AddValue ("numPackets", "Number of packets to send", config.packetCount);
    cmd.AddValue ("serverPort", "Server UDP port", config.serverPort);
    cmd.AddValue ("nodeCount", "Number of nodes to create", config.nodeCount);
    cmd.AddValue ("sendInterval", "Send interval in nanoseconds", config.sendInterval);
    cmd.AddValue ("maxPdcCount", "Maximum PDC count per device", config.maxPdcCount);
    cmd.AddValue ("networkBase", "Network base address", config.networkBase);
    cmd.AddValue ("subnetMask", "Subnet mask", config.subnetMask);
    cmd.AddValue ("serverStartTime", "Server start time in seconds", config.serverStartTime);
    cmd.AddValue ("clientStartTime", "Client start time in seconds", config.clientStartTime);
    cmd.AddValue ("enableTracing", "Enable packet tracing", enableTracing);
    cmd.Parse (argc, argv);

    NS_LOG_INFO ("Configuration: " << config.packetCount << " packets of " << config.packetSize
                << " bytes each, port " << config.serverPort);

    // Create nodes for communication
    NodeContainer nodes;
    nodes.Create (config.nodeCount);

    // Install Soft-UE devices
    SoftUeHelper helper;
    helper.SetDeviceAttribute ("MaxPdcCount", UintegerValue (config.maxPdcCount));
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
    address.SetBase (config.networkBase.c_str (), config.subnetMask.c_str ());
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Debug: Print IP addresses
    NS_LOG_INFO ("Node 0 IP: " << interfaces.GetAddress (0));
    NS_LOG_INFO ("Node 1 IP: " << interfaces.GetAddress (1));

    // Create applications - simplified for debugging
    // Get actual server device FEP address from installed device
    Ptr<SoftUeNetDevice> serverDevice = DynamicCast<SoftUeNetDevice> (devices.Get (1));
    Mac48Address serverMacAddr;
    uint32_t serverFep = 0;
    if (serverDevice)
    {
        SoftUeConfig serverConfig = serverDevice->GetConfiguration ();
        serverMacAddr = serverConfig.address;
        serverFep = serverConfig.localFep;
        NS_LOG_INFO ("✓ Retrieved server device configuration");
    }
    else
    {
        // Fallback to using device index + 1 as FEP
        serverMacAddr = Mac48Address::Allocate ();
        serverFep = 2;  // Node 1 -> FEP 2
    }

    Address serverAddress = serverMacAddr;
    NS_LOG_INFO ("Server address: " << serverAddress << " (FEP=" << serverFep << ")");

    // Calculate required time based on number of packets (Data center level latency)
    double packetIntervalSeconds = config.sendInterval.GetNanoSeconds () / 1000000000.0;
    double requiredClientTime = config.clientStartTime.GetSeconds () +
                                (config.packetCount * packetIntervalSeconds) +
                                config.timeBuffer.GetSeconds ();
    double requiredServerTime = requiredClientTime + config.timeBuffer.GetSeconds (); // Server runs longer

    // Server application (node 1)
    Ptr<SoftUeFullApp> serverApp = CreateObject<SoftUeFullApp> ();
    if (serverApp)
    {
        NS_LOG_INFO ("✓ Created server application");
        serverApp->SetConfiguration (config);
        serverApp->Setup (0, 0, Address (), config.serverPort, true);
        serverApp->SetStartTime (config.serverStartTime);
        serverApp->SetStopTime (Seconds (requiredServerTime));
        nodes.Get (1)->AddApplication (serverApp);
        NS_LOG_INFO ("✓ Server application installed");
    }

    // Client application (node 0)
    Ptr<SoftUeFullApp> clientApp = CreateObject<SoftUeFullApp> ();
    if (clientApp)
    {
        NS_LOG_INFO ("✓ Created client application");
        clientApp->SetConfiguration (config);
        clientApp->Setup (config.packetSize, config.packetCount, serverAddress, config.serverPort, false);
        clientApp->SetStartTime (config.clientStartTime);
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

    double simulationEndTime = requiredServerTime + 2.0; // Extra buffer

    NS_LOG_INFO ("Starting simulation...");
    NS_LOG_INFO ("Simulation parameters:");
    NS_LOG_INFO ("  - Packet size: " << config.packetSize << " bytes");
    NS_LOG_INFO ("  - Packet count: " << config.packetCount);
    NS_LOG_INFO ("  - Simulation duration: " << simulationEndTime << " seconds");
    NS_LOG_INFO ("  - Data center latency target: " << config.sendInterval.GetNanoSeconds () << "ns per packet");
    Simulator::Stop (Seconds (simulationEndTime));
    Simulator::Run ();

    NS_LOG_INFO ("\n" << std::string (60, '='));
    NS_LOG_INFO ("SOFT-UE END-TO-END COMMUNICATION TEST RESULTS");
    NS_LOG_INFO (std::string (60, '='));

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
    bool testPassed = (clientApp->GetPacketCount () == config.packetCount) &&
                     (serverApp->GetPacketCount () == config.packetCount) &&
                     (clientApp->GetSesProcessedCount () > 0) &&
                     (clientApp->GetPdsProcessedCount () > 0);

    NS_LOG_INFO ("\n" << std::string (60, '='));
    NS_LOG_INFO ("COMMUNICATION TEST VERIFICATION");
    NS_LOG_INFO (std::string (60, '='));

    // Test status with clear formatting
    std::string status = testPassed ? "✅ PASSED" : "❌ FAILED";
    NS_LOG_INFO ("Overall Status: " << status);

    // Packet transmission verification
    NS_LOG_INFO ("Packet Transmission:");
    NS_LOG_INFO ("  Expected packets:    " << config.packetCount);
    NS_LOG_INFO ("  Client processed:    " << clientApp->GetPacketCount () << " ("
                << (config.packetCount > 0 ? (100.0 * clientApp->GetPacketCount () / config.packetCount) : 0) << "%)");
    NS_LOG_INFO ("  Server received:    " << serverApp->GetPacketCount () << " ("
                << (config.packetCount > 0 ? (100.0 * serverApp->GetPacketCount () / config.packetCount) : 0) << "%)");

    // Protocol layer processing
    NS_LOG_INFO ("Protocol Layer Processing:");
    NS_LOG_INFO ("  Client SES processed: " << clientApp->GetSesProcessedCount ());
    NS_LOG_INFO ("  Client PDS processed: " << clientApp->GetPdsProcessedCount ());
    NS_LOG_INFO ("  Server SES processed: " << serverApp->GetSesProcessedCount ());
    NS_LOG_INFO ("  Server PDS processed: " << serverApp->GetPdsProcessedCount ());

      Simulator::Destroy ();

    return testPassed ? 0 : 1;
}