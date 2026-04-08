/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2025 SUE-Sim Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "application-deployer.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ApplicationDeployer");

namespace {

uint64_t
MakeTruthDeviceKey (uint32_t xpuId, uint32_t portId)
{
    return (static_cast<uint64_t> (xpuId) << 32) | static_cast<uint64_t> (portId);
}

class SoftUeTruthTrafficApp : public Application
{
public:
    static TypeId GetTypeId ();

    void Configure (const std::vector<std::vector<Ptr<SoftUeNetDevice>>>& deviceMatrix,
                    const std::vector<ApplicationDeployer::TruthPlannedOperation>& operations,
                    const std::string& truthExperimentClass,
                    const std::string& truthPressureProfile)
    {
        m_deviceMatrix = deviceMatrix;
        m_operations = operations;
        m_truthExperimentClass = truthExperimentClass;
        m_truthPressureProfile = truthPressureProfile;
    }

private:
    struct RemoteRegion
    {
        uint64_t writeRkey{0};
        uint64_t readRkey{0};
        std::vector<uint8_t> writeBuffer;
        std::vector<uint8_t> readBuffer;
    };

    void StartApplication () override
    {
        NS_ABORT_MSG_IF (m_deviceMatrix.size () < 2,
                         "soft_ue_truth scenario requires at least two XPU rows");
        for (uint32_t xpuIdx = 0; xpuIdx < m_deviceMatrix.size (); ++xpuIdx)
        {
            NS_ABORT_MSG_IF (m_deviceMatrix[xpuIdx].empty (),
                             "soft_ue_truth scenario requires at least one device per XPU");
            for (uint32_t portIdx = 0; portIdx < m_deviceMatrix[xpuIdx].size (); ++portIdx)
            {
                Ptr<SoftUeNetDevice> device = m_deviceMatrix[xpuIdx][portIdx];
                NS_ABORT_MSG_UNLESS (device != nullptr,
                                     "soft_ue_truth scenario requires concrete devices");
                Ptr<SesManager> ses = device->GetSesManager ();
                NS_ABORT_MSG_UNLESS (ses != nullptr,
                                     "soft_ue_truth scenario requires SES managers");
                m_sessions[MakeTruthDeviceKey (xpuIdx, portIdx)] = ses;
            }
        }

        uint32_t maxPayload = 32;
        for (const auto& op : m_operations)
        {
            maxPayload = std::max (maxPayload, op.payloadBytes);
        }

        for (uint32_t xpuIdx = 0; xpuIdx < m_deviceMatrix.size (); ++xpuIdx)
        {
            for (uint32_t portIdx = 0; portIdx < m_deviceMatrix[xpuIdx].size (); ++portIdx)
            {
                RemoteRegion region;
                region.writeRkey = 0x100000ULL + static_cast<uint64_t> (xpuIdx) * 0x1000ULL +
                                   static_cast<uint64_t> (portIdx) * 0x10ULL + 1ULL;
                region.readRkey = region.writeRkey + 1ULL;
                region.writeBuffer.assign (maxPayload, 0);
                region.readBuffer = MakePayload (maxPayload,
                                                static_cast<uint8_t> (17 + xpuIdx * 7 + portIdx * 5));

                Ptr<SesManager> ses = GetSes (xpuIdx, portIdx);
                ses->RegisterMemoryRegion (region.writeRkey,
                                           reinterpret_cast<uint64_t> (region.writeBuffer.data ()),
                                           region.writeBuffer.size (),
                                           true,
                                           true);
                ses->RegisterMemoryRegion (region.readRkey,
                                           reinterpret_cast<uint64_t> (region.readBuffer.data ()),
                                           region.readBuffer.size (),
                                           true,
                                           true);
                m_remoteRegions[MakeTruthDeviceKey (xpuIdx, portIdx)] = std::move (region);
            }
        }

        if (!m_operations.empty ())
        {
            const auto& firstOp = m_operations.front ();
            Simulator::Schedule (MicroSeconds (100),
                                 &SoftUeTruthTrafficApp::EmitDiagnosticHook,
                                 this,
                                 firstOp.srcXpuId,
                                 firstOp.srcPort,
                                 true);
            double stopOffset = m_operations.back ().startOffsetSeconds + 0.010;
            Simulator::Schedule (Seconds (stopOffset),
                                 &SoftUeTruthTrafficApp::EmitDiagnosticHook,
                                 this,
                                 firstOp.srcXpuId,
                                 firstOp.srcPort,
                                 false);
        }

        for (const auto& op : m_operations)
        {
            Simulator::Schedule (Seconds (op.startOffsetSeconds),
                                 &SoftUeTruthTrafficApp::ExecuteOperation,
                                 this,
                                 op);
        }
    }

    void StopApplication () override
    {
    }

    std::vector<uint8_t> MakePayload (uint32_t length, uint8_t seed) const
    {
        std::vector<uint8_t> payload (length, 0);
        for (uint32_t i = 0; i < length; ++i)
        {
            payload[i] = static_cast<uint8_t> ('a' + ((seed + i * 5 + i / 3) % 26));
        }
        return payload;
    }

    Ptr<Packet> MakePacket (const std::vector<uint8_t>& payload) const
    {
        return payload.empty () ? Create<Packet> (0) : Create<Packet> (payload.data (), payload.size ());
    }

    Ptr<SesManager> GetSes (uint32_t xpuId, uint32_t portId) const
    {
        auto it = m_sessions.find (MakeTruthDeviceKey (xpuId, portId));
        return it == m_sessions.end () ? nullptr : it->second;
    }

    RemoteRegion& GetRemoteRegion (uint32_t xpuId, uint32_t portId)
    {
        return m_remoteRegions[MakeTruthDeviceKey (xpuId, portId)];
    }

    std::vector<uint8_t>& GetReceiveBuffer (uint64_t jobId, uint32_t length)
    {
        auto& buffer = m_receiveBuffers[jobId];
        buffer.assign (length, 0);
        return buffer;
    }

    std::vector<uint8_t>& GetPayloadBuffer (uint64_t jobId, uint32_t length, uint8_t seed)
    {
        auto& payload = m_payloadBuffers[jobId];
        payload = MakePayload (length, seed);
        return payload;
    }

    Ptr<ExtendedOperationMetadata> MakeBaseMetadata (const ApplicationDeployer::TruthPlannedOperation& op) const
    {
        Ptr<ExtendedOperationMetadata> metadata = Create<ExtendedOperationMetadata> ();
        metadata->delivery_mode = static_cast<uint8_t> (DeliveryMode::RUD);
        metadata->job_id = static_cast<uint32_t> (op.jobId);
        metadata->messages_id = op.msgId;
        metadata->payload.length = op.payloadBytes;
        metadata->som = true;
        metadata->eom = true;
        metadata->expect_response = true;
        metadata->SetSourceEndpoint (m_deviceMatrix[op.srcXpuId][op.srcPort]->GetLocalFep (), 1);
        metadata->SetDestinationEndpoint (m_deviceMatrix[op.dstXpuId][op.dstPort]->GetLocalFep (), 1);
        return metadata;
    }

    void IssuePostReceive (ApplicationDeployer::TruthPlannedOperation op)
    {
        Ptr<SesManager> receiverSes = GetSes (op.dstXpuId, op.dstPort);
        Ptr<SoftUeNetDevice> sender = m_deviceMatrix[op.srcXpuId][op.srcPort];
        std::vector<uint8_t>& receiveBuffer = GetReceiveBuffer (op.jobId, op.payloadBytes);
        receiverSes->PostReceive (op.jobId,
                                  op.msgId,
                                  sender->GetLocalFep (),
                                  reinterpret_cast<uint64_t> (receiveBuffer.data ()),
                                  receiveBuffer.size ());
    }

    void ExecuteOperation (ApplicationDeployer::TruthPlannedOperation op)
    {
        Ptr<SesManager> senderSes = GetSes (op.srcXpuId, op.srcPort);
        Ptr<ExtendedOperationMetadata> metadata = MakeBaseMetadata (op);
        bool dispatched = false;

        switch (op.opType)
        {
        case OpType::SEND:
        {
            metadata->op_type = OpType::SEND;
            if (!op.delayedReceive)
            {
                IssuePostReceive (op);
            }
            else
            {
                Simulator::Schedule (MilliSeconds (std::max<uint32_t> (1u, op.receiveDelayMs)),
                                     &SoftUeTruthTrafficApp::IssuePostReceive,
                                     this,
                                     op);
            }

            std::vector<uint8_t>& payload = GetPayloadBuffer (op.jobId,
                                                              op.payloadBytes,
                                                              static_cast<uint8_t> (op.msgId & 0xff));
            dispatched = senderSes->ProcessSendRequest (metadata, MakePacket (payload));
            break;
        }
        case OpType::WRITE:
        {
            metadata->op_type = OpType::WRITE;
            RemoteRegion& remote = GetRemoteRegion (op.dstXpuId, op.dstPort);
            metadata->payload.start_addr = reinterpret_cast<uint64_t> (remote.writeBuffer.data ());
            metadata->memory.rkey = op.expectValidationFailure ? 0xdeadbeefULL : remote.writeRkey;

            std::vector<uint8_t>& payload = GetPayloadBuffer (op.jobId,
                                                              op.payloadBytes,
                                                              static_cast<uint8_t> (0x20 + (op.msgId & 0x3f)));
            dispatched = senderSes->ProcessSendRequest (metadata, MakePacket (payload));
            break;
        }
        case OpType::READ:
        {
            metadata->op_type = OpType::READ;
            RemoteRegion& remote = GetRemoteRegion (op.dstXpuId, op.dstPort);
            std::vector<uint8_t>& localBuffer = GetReceiveBuffer (op.jobId, op.payloadBytes);
            metadata->payload.start_addr = reinterpret_cast<uint64_t> (localBuffer.data ());
            metadata->payload.imm_data = reinterpret_cast<uint64_t> (remote.readBuffer.data ());
            metadata->memory.rkey = remote.readRkey;
            dispatched = senderSes->ProcessSendRequest (metadata, nullptr);
            break;
        }
        default:
            senderSes->NotifyPdsErrorEvent (0, -1, "unsupported truth op");
            return;
        }

        if (!dispatched)
        {
            senderSes->NotifyPdsErrorEvent (0,
                                            -1,
                                            "truth operation dispatch failed for job=" +
                                                std::to_string (op.jobId));
        }
    }

    void EmitDiagnosticHook (uint32_t xpuId, uint32_t portId, bool paused)
    {
        Ptr<SesManager> ses = GetSes (xpuId, portId);
        if (ses)
        {
            ses->NotifyPause (paused);
        }
    }

    std::vector<std::vector<Ptr<SoftUeNetDevice>>> m_deviceMatrix;
    std::vector<ApplicationDeployer::TruthPlannedOperation> m_operations;
    std::string m_truthExperimentClass;
    std::string m_truthPressureProfile;
    std::unordered_map<uint64_t, Ptr<SesManager>> m_sessions;
    std::unordered_map<uint64_t, RemoteRegion> m_remoteRegions;
    std::unordered_map<uint64_t, std::vector<uint8_t>> m_receiveBuffers;
    std::unordered_map<uint64_t, std::vector<uint8_t>> m_payloadBuffers;
};

NS_OBJECT_ENSURE_REGISTERED (SoftUeTruthTrafficApp);

TypeId
SoftUeTruthTrafficApp::GetTypeId ()
{
    static TypeId tid = TypeId ("ns3::SoftUeTruthTrafficApp")
        .SetParent<Application> ()
        .SetGroupName ("SueSimModule")
        .AddConstructor<SoftUeTruthTrafficApp> ();
    return tid;
}

} // namespace

ApplicationDeployer::ApplicationDeployer ()
  : m_truthPlannerMode ("legacy")
{
}

ApplicationDeployer::~ApplicationDeployer ()
{
}

void
ApplicationDeployer::DeployApplications (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder)
{
    NS_LOG_INFO ("Deploying applications on topology");

    m_softUeObserver.reset ();
    m_truthPlannedOperations.clear ();
    m_truthPlannerMode = "legacy";
    if (topologyBuilder.HasSoftUeTruthPath ())
    {
        InstallSoftUeTruthScenario (config, topologyBuilder);
    }
    else
    {
        InstallServers (config, topologyBuilder);
        InstallClientsAndTrafficGenerators (config, topologyBuilder);
        NS_LOG_INFO ("Running legacy_sue application path; this system run is not soft-ue truth-backed");
    }

    NS_LOG_INFO ("Application deployment completed");
}

bool
ApplicationDeployer::HasSoftUeObserver () const
{
    return m_softUeObserver != nullptr;
}

SoftUeObserverHelper*
ApplicationDeployer::GetSoftUeObserver ()
{
    return m_softUeObserver.get ();
}

const SoftUeObserverHelper*
ApplicationDeployer::GetSoftUeObserver () const
{
    return m_softUeObserver.get ();
}

const std::vector<ApplicationDeployer::TruthPlannedOperation>&
ApplicationDeployer::GetTruthPlannedOperations () const
{
    return m_truthPlannedOperations;
}

std::string
ApplicationDeployer::GetTruthPlannerMode () const
{
    return m_truthPlannerMode;
}

void
ApplicationDeployer::InstallServers (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t portsPerXpu = config.network.portsPerXpu;
    uint32_t transactionSize = config.traffic.transactionSize;
    double serverStart = config.timing.serverStart;
    double serverStop = config.GetServerStop ();

    NodeContainer* xpuNodes = topologyBuilder.GetXpuNodes ();

    // Install server applications (Each port of each XPU)
    for (uint32_t xpuIdx = 0; xpuIdx < nXpus; ++xpuIdx)
    {
        for (uint32_t portIdx = 0; portIdx < portsPerXpu; ++portIdx)
        {
            Ptr<SueServer> serverApp = CreateObject<SueServer>();
            serverApp->SetAttribute("Port", UintegerValue(8080 + portIdx));
            serverApp->SetAttribute("TransactionSize", UintegerValue(transactionSize));
            serverApp->SetPortInfo(xpuIdx, portIdx);

            xpuNodes->Get(xpuIdx)->AddApplication(serverApp);
            serverApp->SetStartTime(Seconds(serverStart));
            serverApp->SetStopTime(Seconds(serverStop));
        }
    }
}

void
ApplicationDeployer::InstallClientsAndTrafficGenerators (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t suesPerXpu = config.network.suesPerXpu;
    double clientStart = config.timing.clientStart;
    double clientStop = config.GetClientStop ();

    NodeContainer* xpuNodes = topologyBuilder.GetXpuNodes ();

    // Install client applications and traffic generators (SUE-based creation method)
    for (uint32_t xpuIdx = 0; xpuIdx < nXpus; ++xpuIdx)
    {
        // Create SueClient list for this XPU - Now based on SUE count
        std::vector<Ptr<SueClient>> sueClientsForXpu(suesPerXpu);

        // Create all SUE clients for this XPU
        for (uint32_t sueIdx = 0; sueIdx < suesPerXpu; ++sueIdx)
        {
            Ptr<SueClient> sueClient = CreateSueClient (xpuIdx, sueIdx, config, topologyBuilder);
            sueClientsForXpu[sueIdx] = sueClient;
        }

        // Create load balancer
        Ptr<LoadBalancer> loadBalancer = CreateLoadBalancer (xpuIdx, sueClientsForXpu, config);

        // Create traffic generator for this XPU
        if (config.traffic.enableFineGrainedMode)
        {
            /***************************************/
            // Fine-grained traffic control mode
            /**************************************/
            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Creating ConfigurableTrafficGenerator");
            Ptr<ConfigurableTrafficGenerator> configGen = CreateConfigurableTrafficGenerator (xpuIdx, loadBalancer, config);

            // Install configurable traffic generator to XPU node
            xpuNodes->Get(xpuIdx)->AddApplication(configGen);
            configGen->SetStartTime(Seconds(clientStart));
            configGen->SetStopTime(Seconds(clientStop));

            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Configurable traffic generator installed from "
                    << clientStart << "s to " << clientStop << "s");
        }
        else if (config.traffic.enableTraceMode) {
            /***************************************/
            // Trace mode
            /**************************************/
            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Creating TraceTrafficGenerator");
            Ptr<TraceTrafficGenerator> traceGen = CreateTraceTrafficGenerator (xpuIdx, loadBalancer, config);

            // Install trace traffic generator to XPU node
            xpuNodes->Get(xpuIdx)->AddApplication(traceGen);
            traceGen->SetStartTime(Seconds(clientStart));
            traceGen->SetStopTime(Seconds(clientStop));

            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Trace traffic generator installed from "
                    << clientStart << "s to " << clientStop << "s");
        } else {
            /***************************************/
            // Uniform traffic generation mode
            /**************************************/
            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Creating traditional TrafficGenerator");
            Ptr<TrafficGenerator> trafficGen = CreateTrafficGenerator (xpuIdx, loadBalancer, config);

            // Install traditional traffic generator to XPU node
            xpuNodes->Get(xpuIdx)->AddApplication(trafficGen);
            trafficGen->SetStartTime(Seconds(clientStart));
            trafficGen->SetStopTime(Seconds(clientStop));

            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": "
                    << std::to_string(config.traffic.threadRate)
                    << "Mbps traffic generator from "
                    << clientStart << "s to " << clientStop << "s");
        }

        // Set destination queue space available callback for each SUE
        for (uint32_t sueIdx = 0; sueIdx < suesPerXpu; ++sueIdx) {
            sueClientsForXpu[sueIdx]->SetDestQueueSpaceCallback([loadBalancer](uint32_t sueId, uint32_t destXpuId, uint8_t vcId) {
                loadBalancer->NotifyDestQueueSpaceAvailable(sueId, destXpuId, vcId);
            });
        }
        NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Destination queue space callbacks set for all SUEs");

        // Connect trace sources to PerformanceLogger
        PerformanceLogger& logger = PerformanceLogger::GetInstance();

        // Connect buffer queue change trace
        loadBalancer->TraceConnectWithoutContext("BufferQueueChange",
                                                MakeCallback(&PerformanceLogger::BufferQueueChangeTraceCallback, &logger));

        NS_LOG_INFO("XPU" << xpuIdx + 1 << ": LoadBalancer trace callbacks connected to PerformanceLogger");
    }
}

void
ApplicationDeployer::InstallSoftUeTruthScenario (const SueSimulationConfig& config,
                                                 TopologyBuilder& topologyBuilder)
{
    const auto& deviceMatrix = topologyBuilder.GetSoftUeDeviceMatrix ();
    NS_ABORT_MSG_IF (deviceMatrix.size () < 2,
                     "soft_ue_truth scenario requires at least two XPU soft-ue devices");
    for (uint32_t xpuIdx = 0; xpuIdx < deviceMatrix.size (); ++xpuIdx)
    {
        NS_ABORT_MSG_IF (deviceMatrix[xpuIdx].empty (),
                         "soft_ue_truth scenario requires per-port soft-ue devices");
    }

    if (config.enableSoftUeObserver)
    {
        m_softUeObserver = std::make_unique<SoftUeObserverHelper> ();
        m_softUeObserver->SetPerformanceLoggingEnabled (config.enableSoftUeProtocolLogging);
        m_softUeObserver->Install (topologyBuilder.GetSoftUeDevices ());
        NS_LOG_INFO ("SoftUeObserverHelper installed on soft_ue_truth topology");
    }
    else
    {
        NS_LOG_INFO ("soft_ue_truth selected with observer disabled; protocol truth remains active but"
                     " observer/logging export is off");
    }

    m_truthPlannedOperations = BuildTruthPlan (config, topologyBuilder);
    NS_ABORT_MSG_IF (m_truthPlannedOperations.empty (),
                     "soft_ue_truth scenario requires a non-empty truth operation plan");

    Ptr<SoftUeTruthTrafficApp> app = CreateObject<SoftUeTruthTrafficApp> ();
    app->Configure (deviceMatrix,
                    m_truthPlannedOperations,
                    config.truthExperimentClass,
                    config.truthPressureProfile);
    topologyBuilder.GetXpuNodes ()->Get (0)->AddApplication (app);
    app->SetStartTime (Seconds (config.timing.clientStart));
    app->SetStopTime (Seconds (config.GetClientStop ()));

    NS_LOG_INFO ("Installed soft_ue_truth traffic driver with " << m_truthPlannedOperations.size ()
                 << " planned operations via planner mode " << m_truthPlannerMode
                 << " and experiment class " << config.truthExperimentClass);
}

Ptr<SueClient>
ApplicationDeployer::CreateSueClient (uint32_t xpuIdx, uint32_t sueIdx,
                                      const SueSimulationConfig& config, TopologyBuilder& topologyBuilder)
{
    uint32_t transactionSize = config.traffic.transactionSize;
    uint32_t maxBurstSize = config.traffic.maxBurstSize;
    uint32_t destQueueMaxBytes = config.queue.destQueueMaxBytes;
    uint8_t vcNum = config.traffic.vcNum;
    std::string SchedulingInterval = config.delay.SchedulingInterval;
    std::string PackingDelayPerPacket = config.delay.PackingDelayPerPacket;
    std::string ClientStatInterval = config.trace.ClientStatInterval;
    uint32_t portsPerSue = config.network.portsPerSue;
    double clientStart = config.timing.clientStart;
    double serverStop = config.GetServerStop ();

    Ptr<SueClient> sueClient = CreateObject<SueClient>();
    sueClient->SetAttribute("TransactionSize", UintegerValue(transactionSize));
    sueClient->SetAttribute("MaxBurstSize", UintegerValue(maxBurstSize));
    sueClient->SetAttribute("DestQueueMaxBytes", UintegerValue(destQueueMaxBytes));
    sueClient->SetAttribute("vcNum", UintegerValue(vcNum));
    sueClient->SetAttribute("AdditionalHeaderSize", UintegerValue(config.delay.additionalHeaderSize));
    sueClient->SetAttribute("SchedulingInterval", StringValue(SchedulingInterval));
    sueClient->SetAttribute("PackingDelayPerPacket", StringValue(PackingDelayPerPacket));
    sueClient->SetAttribute("ClientStatInterval", StringValue(ClientStatInterval));

    // Set SUE information (no longer single port, but SUE identifier)
    sueClient->SetXpuInfo(xpuIdx, sueIdx);
    sueClient->SetSueId(sueIdx);

    // Prepare device list managed by SUE
    std::vector<std::vector<Ptr<NetDevice>>>& xpuDevices = topologyBuilder.GetXpuDevices();
    std::vector<Ptr<PointToPointSueNetDevice>> managedDevices;
    for (uint32_t portInSue = 0; portInSue < portsPerSue; ++portInSue) {
        uint32_t globalPortIdx = sueIdx * portsPerSue + portInSue;
        Ptr<NetDevice> netDev = xpuDevices[xpuIdx][globalPortIdx];
        Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(netDev);
        if (p2pDev) {
            managedDevices.push_back(p2pDev);
        }
    }

    // Set SUE managed devices
    sueClient->SetManagedDevices(managedDevices);

    NodeContainer* xpuNodes = topologyBuilder.GetXpuNodes();
    xpuNodes->Get(xpuIdx)->AddApplication(sueClient);
    sueClient->SetStartTime(Seconds(clientStart));
    sueClient->SetStopTime(Seconds(serverStop));

    NS_LOG_INFO("Created SUE" << (sueIdx + 1) << " for XPU" << (xpuIdx + 1)
               << " managing " << managedDevices.size() << " ports");

    return sueClient;
}

Ptr<LoadBalancer>
ApplicationDeployer::CreateLoadBalancer (uint32_t xpuIdx,
                                        const std::vector<Ptr<SueClient>>& sueClientsForXpu,
                                        const SueSimulationConfig& config)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t hashSeed = config.loadBalance.hashSeed;
    uint32_t loadBalanceAlgorithm = config.loadBalance.loadBalanceAlgorithm;
    uint32_t prime1 = config.loadBalance.prime1;
    uint32_t prime2 = config.loadBalance.prime2;
    bool useVcInHash = config.loadBalance.useVcInHash;
    bool enableBitOperations = config.loadBalance.enableBitOperations;
    uint32_t suesPerXpu = config.network.suesPerXpu;

    // Create load balancer
    Ptr<LoadBalancer> loadBalancer = CreateObject<LoadBalancer>();
    loadBalancer->SetAttribute("LocalXpuId", UintegerValue(xpuIdx));
    loadBalancer->SetAttribute("MaxXpuId", UintegerValue(nXpus - 1));
    loadBalancer->SetAttribute("HashSeed", UintegerValue(hashSeed + xpuIdx * 31)); // Use command line parameter seed
    loadBalancer->SetAttribute("LoadBalanceAlgorithm", UintegerValue(loadBalanceAlgorithm));
    loadBalancer->SetAttribute("Prime1", UintegerValue(prime1));
    loadBalancer->SetAttribute("Prime2", UintegerValue(prime2));
    loadBalancer->SetAttribute("UseVcInHash", BooleanValue(useVcInHash));
    loadBalancer->SetAttribute("EnableBitOperations", BooleanValue(enableBitOperations));
    loadBalancer->SetAttribute("EnableAlternativePath", BooleanValue(config.loadBalance.enableAlternativePath));

    // Register SueClient to LoadBalancer
    for (uint32_t sueIdx = 0; sueIdx < suesPerXpu; ++sueIdx) {
        loadBalancer->AddSueClient(sueClientsForXpu[sueIdx], sueIdx);
    }

    return loadBalancer;
}

Ptr<TrafficGenerator>
ApplicationDeployer::CreateTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                           const SueSimulationConfig& config)
{
    // Create traditional traffic generator
    NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Creating traditional TrafficGenerator");
    uint32_t transactionSize = config.traffic.transactionSize;
    double threadRate = config.traffic.threadRate;
    uint32_t nXpus = config.network.nXpus;
    uint8_t vcNum = config.traffic.vcNum;
    uint32_t totalBytesToSend = config.traffic.totalBytesToSend;
    uint32_t maxBurstSize = config.traffic.maxBurstSize;

    // Create traffic generator for this XPU
    Ptr<TrafficGenerator> trafficGen = CreateObject<TrafficGenerator>();
    trafficGen->SetAttribute("TransactionSize", UintegerValue(transactionSize));
    trafficGen->SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(threadRate) + "Mbps")));
    trafficGen->SetAttribute("MinXpuId", UintegerValue(0));
    trafficGen->SetAttribute("MaxXpuId", UintegerValue(nXpus - 1));
    trafficGen->SetAttribute("MinVcId", UintegerValue(0));
    trafficGen->SetAttribute("MaxVcId", UintegerValue(vcNum - 1));
    trafficGen->SetAttribute("TotalBytesToSend", UintegerValue(totalBytesToSend));
    trafficGen->SetAttribute("MaxBurstSize", UintegerValue(maxBurstSize));

    // Configure traffic generator: Set load balancer
    trafficGen->SetLoadBalancer(loadBalancer);
    trafficGen->SetLocalXpuId(xpuIdx);  // 0-based

    // Set TrafficGenerator to LoadBalancer (for traffic control)
    loadBalancer->SetTrafficGenerator(trafficGen);

    return trafficGen;
}

Ptr<TraceTrafficGenerator>
ApplicationDeployer::CreateTraceTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                                 const SueSimulationConfig& config)
{
    uint32_t transactionSize = config.traffic.transactionSize;
    uint32_t nXpus = config.network.nXpus;
    uint8_t vcNum = config.traffic.vcNum;
    uint32_t maxBurstSize = config.traffic.maxBurstSize;
    std::string traceFilePath = config.traffic.traceFilePath;

    // Create trace traffic generator for this XPU
    Ptr<TraceTrafficGenerator> traceTrafficGen = CreateObject<TraceTrafficGenerator>();
    traceTrafficGen->SetAttribute("TransactionSize", UintegerValue(transactionSize));
    traceTrafficGen->SetAttribute("MinXpuId", UintegerValue(0));
    traceTrafficGen->SetAttribute("MaxXpuId", UintegerValue(nXpus - 1));
    traceTrafficGen->SetAttribute("MinVcId", UintegerValue(0));
    traceTrafficGen->SetAttribute("MaxVcId", UintegerValue(vcNum - 1));
    traceTrafficGen->SetAttribute("MaxBurstSize", UintegerValue(maxBurstSize));
    traceTrafficGen->SetAttribute("TraceFile", StringValue(traceFilePath));

    // Configure trace traffic generator
    traceTrafficGen->SetLoadBalancer(loadBalancer);
    traceTrafficGen->SetLocalXpuId(xpuIdx);  // 0-based

    // Note: LoadBalancer doesn't yet support SetTraceTrafficGenerator
    // This would need to be added to LoadBalancer class for full functionality
    
    // Set TrafficGenerator to LoadBalancer (for traffic control)
    loadBalancer->SetTrafficGenerator(traceTrafficGen);

    return traceTrafficGen;
}

Ptr<ConfigurableTrafficGenerator>
ApplicationDeployer::CreateConfigurableTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                                const SueSimulationConfig& config)
{
    NS_LOG_FUNCTION (this << xpuIdx << loadBalancer << &config);

    uint32_t transactionSize = config.traffic.transactionSize;
    uint32_t maxBurstSize = config.traffic.maxBurstSize;

    // Parse fine-grained traffic configuration
    std::vector<FineGrainedTrafficFlow> fineGrainedFlows = ParseFineGrainedTrafficConfig (config);

    // Create configurable traffic generator
    Ptr<ConfigurableTrafficGenerator> configTrafficGen = CreateObject<ConfigurableTrafficGenerator>();

    // Set attributes
    configTrafficGen->SetAttribute("TransactionSize", UintegerValue(transactionSize));
    configTrafficGen->SetAttribute("MaxBurstSize", UintegerValue(maxBurstSize));

    // Configure configurable traffic generator
    configTrafficGen->SetLoadBalancer(loadBalancer);
    configTrafficGen->SetLocalXpuId(xpuIdx);  // 0-based
    configTrafficGen->SetClientStartTime(config.timing.clientStart);  // Set client start time for accurate flow timing
    configTrafficGen->SetFineGrainedFlows(fineGrainedFlows);

    // Set TrafficGenerator to LoadBalancer (for traffic control)
    loadBalancer->SetTrafficGenerator(configTrafficGen);

    return configTrafficGen;
}

std::vector<ApplicationDeployer::TruthPlannedOperation>
ApplicationDeployer::BuildTruthPlan (const SueSimulationConfig& config,
                                     TopologyBuilder& topologyBuilder)
{
    if (config.systemScenarioMode == "soft_ue_fabric")
    {
        m_truthPlannerMode = "fabric_" + config.fabricTrafficPattern;
        return BuildFabricTruthPlan (config, topologyBuilder);
    }
    if (config.truthExperimentClass == "semantic_demo")
    {
        m_truthPlannerMode = "semantic_demo";
        return BuildSemanticDemoTruthPlan (config, topologyBuilder);
    }
    if (config.traffic.enableFineGrainedMode)
    {
        m_truthPlannerMode = "fine_grained";
        return BuildFineGrainedTruthPlan (config, topologyBuilder);
    }
    if (config.traffic.enableTraceMode)
    {
        m_truthPlannerMode = "trace";
        return BuildTraceTruthPlan (config, topologyBuilder);
    }

    if (config.truthPlannerMode == "striped")
    {
        m_truthPlannerMode = "striped";
        return BuildStripedTruthPlan (config, topologyBuilder);
    }

    m_truthPlannerMode = "uniform";
    return BuildUniformTruthPlan (config, topologyBuilder);
}

std::vector<ApplicationDeployer::TruthPlannedOperation>
ApplicationDeployer::BuildFabricTruthPlan (const SueSimulationConfig& config,
                                           TopologyBuilder& topologyBuilder)
{
    std::vector<TruthPlannedOperation> operations;
    const auto& matrix = topologyBuilder.GetSoftUeDeviceMatrix ();
    const uint32_t portsPerXpu = config.network.portsPerXpu;
    const uint32_t payloadBytes = GetTruthPayloadBytes (config);
    const double opSpacing = GetTruthOpSpacingSeconds (config);
    const uint32_t payloadPerPacket = std::max<uint32_t> (1u, config.traffic.PayloadMtu);
    const std::string pressureProfile =
        (config.truthExperimentClass == "system_pressure") ? config.truthPressureProfile : "baseline";
    const uint32_t senderRows = config.network.nXpus;
    const uint32_t totalRows = matrix.size ();
    const bool sixBySix = (config.fabricEndpointMode == "six_by_six");
    uint64_t nextJobId = 56000;
    uint16_t nextMsgId = 800;
    uint32_t ordinal = 0;

    struct Endpoint
    {
        uint32_t xpuId{0};
        uint32_t port{0};
    };

    std::vector<Endpoint> sources;
    std::vector<Endpoint> destinations;
    for (uint32_t xpuIdx = 0; xpuIdx < totalRows; ++xpuIdx)
    {
        const bool senderSide = !sixBySix || xpuIdx < senderRows;
        const bool receiverSide = !sixBySix || xpuIdx >= senderRows;
        for (uint32_t portIdx = 0; portIdx < portsPerXpu; ++portIdx)
        {
            if (senderSide)
            {
                sources.push_back ({xpuIdx, portIdx});
            }
            if (receiverSide)
            {
                destinations.push_back ({xpuIdx, portIdx});
            }
        }
    }

    std::vector<std::pair<Endpoint, Endpoint>> flows;
    if (config.fabricTrafficPattern == "hotspot" || config.fabricTrafficPattern == "incast")
    {
        NS_ABORT_MSG_UNLESS (!destinations.empty (), "fabric hotspot/incast requires destinations");
        const Endpoint hotspot = destinations.front ();
        for (const Endpoint& src : sources)
        {
            if (src.xpuId == hotspot.xpuId && src.port == hotspot.port)
            {
                continue;
            }
            if (sixBySix && src.xpuId >= senderRows)
            {
                continue;
            }
            flows.emplace_back (src, hotspot);
        }
    }
    else
    {
        for (const Endpoint& src : sources)
        {
            for (const Endpoint& dst : destinations)
            {
                if (src.xpuId == dst.xpuId && src.port == dst.port)
                {
                    continue;
                }
                if (!sixBySix && src.xpuId == dst.xpuId)
                {
                    continue;
                }
                if (sixBySix && !(src.xpuId < senderRows && dst.xpuId >= senderRows))
                {
                    continue;
                }
                flows.emplace_back (src, dst);
            }
        }
    }

    if (config.truthFlowCount > 0 && flows.size () > config.truthFlowCount)
    {
        flows.resize (config.truthFlowCount);
    }
    NS_ABORT_MSG_IF (flows.empty (), "soft_ue_fabric requires at least one planned flow");

    for (uint32_t opIdx = 0; opIdx < config.truthOpsPerFlow; ++opIdx)
    {
        for (const auto& flow : flows)
        {
            TruthPlannedOperation op;
            op.jobId = ++nextJobId;
            op.msgId = ++nextMsgId;
            op.srcXpuId = flow.first.xpuId;
            op.srcPort = flow.first.port;
            op.dstXpuId = flow.second.xpuId;
            op.dstPort = flow.second.port;
            op.opType = SelectTruthOpType (config, ordinal);
            op.payloadBytes = payloadBytes;
            op.startOffsetSeconds = 0.001 + static_cast<double> (ordinal) * opSpacing;
            op.delayedReceive = ShouldDelayTruthReceive (config, op.opType, ordinal);
            op.receiveDelayMs = op.delayedReceive ? GetTruthReceiveDelayMs (config, ordinal) : 0u;
            op.expectValidationFailure = false;
            op.forceMultiChunk = op.payloadBytes > payloadPerPacket;
            op.plannerMode = m_truthPlannerMode;
            op.pressureProfile = pressureProfile;
            operations.push_back (op);
            ++ordinal;
        }
    }

    return operations;
}

std::vector<ApplicationDeployer::TruthPlannedOperation>
ApplicationDeployer::BuildSemanticDemoTruthPlan (const SueSimulationConfig& config,
                                                 TopologyBuilder& topologyBuilder)
{
    std::vector<TruthPlannedOperation> operations;
    const auto& matrix = topologyBuilder.GetSoftUeDeviceMatrix ();
    const uint32_t portsPerXpu = config.network.portsPerXpu;
    const uint32_t payloadBytes = std::max<uint32_t> (config.truthPayloadBytes, 64u);
    const uint32_t secondaryPort = portsPerXpu > 1 ? 1u : 0u;

    auto pushOp = [&operations](uint64_t jobId,
                                uint16_t msgId,
                                uint32_t srcXpuId,
                                uint32_t srcPort,
                                uint32_t dstXpuId,
                                uint32_t dstPort,
                                OpType opType,
                                uint32_t bytes,
                                double startOffsetSeconds,
                                bool delayedReceive,
                                bool expectValidationFailure,
                                const std::string& plannerMode) {
        TruthPlannedOperation op;
        op.jobId = jobId;
        op.msgId = msgId;
        op.srcXpuId = srcXpuId;
        op.srcPort = srcPort;
        op.dstXpuId = dstXpuId;
        op.dstPort = dstPort;
        op.opType = opType;
        op.payloadBytes = bytes;
        op.startOffsetSeconds = startOffsetSeconds;
        op.delayedReceive = delayedReceive;
        op.receiveDelayMs = delayedReceive ? 15u : 0u;
        op.expectValidationFailure = expectValidationFailure;
        op.forceMultiChunk = false;
        op.plannerMode = plannerMode;
        op.pressureProfile = "baseline";
        operations.push_back (op);
    };

    pushOp (52001, 101, 0, 0, 1, 0, OpType::SEND, payloadBytes, 0.001, false, false, m_truthPlannerMode);
    pushOp (52002, 102, 0, secondaryPort, 1, secondaryPort, OpType::WRITE, payloadBytes, 0.005, false, false,
            m_truthPlannerMode);
    pushOp (52003, 103, 1, 0, 0, 0, OpType::READ, payloadBytes, 0.009, false, false, m_truthPlannerMode);
    pushOp (52004, 104, 0, 0, 1, 0, OpType::WRITE, payloadBytes, 0.013, false, true, m_truthPlannerMode);

    NS_ABORT_MSG_IF (matrix.size () < 2 || matrix[0].empty () || matrix[1].empty (),
                     "semantic_demo requires at least two XPU truth devices");
    return operations;
}

std::vector<ApplicationDeployer::TruthPlannedOperation>
ApplicationDeployer::BuildUniformTruthPlan (const SueSimulationConfig& config,
                                            TopologyBuilder& topologyBuilder)
{
    std::vector<TruthPlannedOperation> operations;
    const auto& matrix = topologyBuilder.GetSoftUeDeviceMatrix ();
    const uint32_t nXpus = matrix.size ();
    const uint32_t portsPerXpu = config.network.portsPerXpu;
    const uint32_t maxFlows = std::max<uint32_t> (1u, config.truthFlowCount);
    const uint32_t payloadBytes = GetTruthPayloadBytes (config);
    const double opSpacing = GetTruthOpSpacingSeconds (config);
    const std::string pressureProfile =
        (config.truthExperimentClass == "system_pressure") ? config.truthPressureProfile : "baseline";
    const uint32_t payloadPerPacket = std::max<uint32_t> (1u, config.traffic.PayloadMtu);
    uint64_t nextJobId = 53000;
    uint16_t nextMsgId = 200;
    uint32_t ordinal = 0;

    struct FlowPlan
    {
        uint32_t srcXpuId{0};
        uint32_t srcPort{0};
        uint32_t dstXpuId{0};
        uint32_t baseDstPort{0};
        uint32_t preferredDstPort{0};
        uint32_t flowIdx{0};
    };

    std::vector<FlowPlan> flows;
    flows.reserve (maxFlows);

    for (uint32_t flowIdx = 0; flowIdx < maxFlows; ++flowIdx)
    {
        const uint32_t srcGlobal = flowIdx % (nXpus * portsPerXpu);
        const uint32_t srcXpuId = srcGlobal / portsPerXpu;
        const uint32_t srcPort = srcGlobal % portsPerXpu;

        uint32_t dstXpuId = (srcXpuId + 1 + (flowIdx % std::max<uint32_t> (1u, nXpus - 1u))) % nXpus;
        if (dstXpuId == srcXpuId && nXpus > 1)
        {
            dstXpuId = (dstXpuId + 1) % nXpus;
        }

        const uint32_t baseDstPort =
            (srcPort + 1 + (flowIdx / std::max<uint32_t> (1u, nXpus))) % portsPerXpu;
        uint32_t dstPort = baseDstPort;
        if (config.truthExperimentClass == "system_pressure" &&
            pressureProfile == "credit_pressure")
        {
            dstPort = 0;
        }
        else if (config.truthExperimentClass == "system_pressure" && pressureProfile == "mixed" &&
                 (flowIdx % 2u) == 0u)
        {
            dstPort = 0;
        }
        flows.push_back ({srcXpuId, srcPort, dstXpuId, baseDstPort, dstPort, flowIdx});
    }

    for (uint32_t opIdx = 0; opIdx < config.truthOpsPerFlow; ++opIdx)
    {
        for (const FlowPlan& flow : flows)
        {
            TruthPlannedOperation op;
            op.jobId = ++nextJobId;
            op.msgId = ++nextMsgId;
            op.srcXpuId = flow.srcXpuId;
            op.srcPort = flow.srcPort;
            op.dstXpuId = flow.dstXpuId;
            if (config.truthExperimentClass == "system_pressure" && pressureProfile == "lossy")
            {
                op.dstPort = (flow.baseDstPort + ordinal + opIdx) % portsPerXpu;
            }
            else if (config.truthExperimentClass == "system_pressure" && pressureProfile == "mixed" &&
                     (flow.flowIdx % 2u) != 0u)
            {
                op.dstPort = (flow.baseDstPort + ordinal + opIdx) % portsPerXpu;
            }
            else
            {
                op.dstPort = flow.preferredDstPort;
            }
            op.opType = SelectTruthOpType (config, ordinal);
            op.payloadBytes = payloadBytes;
            op.startOffsetSeconds = 0.001 + static_cast<double> (ordinal) * opSpacing;
            op.delayedReceive = ShouldDelayTruthReceive (config, op.opType, ordinal);
            op.receiveDelayMs = op.delayedReceive ? GetTruthReceiveDelayMs (config, ordinal) : 0u;
            op.expectValidationFailure = false;
            op.forceMultiChunk = op.payloadBytes > payloadPerPacket;
            op.plannerMode = m_truthPlannerMode;
            op.pressureProfile = pressureProfile;
            operations.push_back (op);
            ++ordinal;
        }
    }

    return operations;
}

std::vector<ApplicationDeployer::TruthPlannedOperation>
ApplicationDeployer::BuildStripedTruthPlan (const SueSimulationConfig& config,
                                            TopologyBuilder& topologyBuilder)
{
    std::vector<TruthPlannedOperation> operations;
    const auto& matrix = topologyBuilder.GetSoftUeDeviceMatrix ();
    const uint32_t nXpus = matrix.size ();
    const uint32_t portsPerXpu = config.network.portsPerXpu;
    const uint32_t maxFlows = std::max<uint32_t> (1u, config.truthFlowCount);
    const uint32_t payloadBytes = GetTruthPayloadBytes (config);
    const double opSpacing = GetTruthOpSpacingSeconds (config);
    const std::string pressureProfile =
        (config.truthExperimentClass == "system_pressure") ? config.truthPressureProfile : "baseline";
    const uint32_t payloadPerPacket = std::max<uint32_t> (1u, config.traffic.PayloadMtu);
    uint64_t nextJobId = 53000;
    uint16_t nextMsgId = 200;
    uint32_t ordinal = 0;

    struct Endpoint
    {
        uint32_t xpuId{0};
        uint32_t port{0};
    };
    struct FlowPlan
    {
        uint32_t srcXpuId{0};
        uint32_t srcPort{0};
        uint32_t dstXpuId{0};
        uint32_t dstPort{0};
        uint32_t baseDstPort{0};
        uint32_t flowIdx{0};
    };

    std::vector<Endpoint> candidateEndpoints;
    candidateEndpoints.reserve (nXpus * portsPerXpu);
    for (uint32_t xpuIdx = 0; xpuIdx < nXpus; ++xpuIdx)
    {
        for (uint32_t portIdx = 0; portIdx < portsPerXpu; ++portIdx)
        {
            candidateEndpoints.push_back ({xpuIdx, portIdx});
        }
    }

    std::map<std::pair<uint32_t, uint32_t>, uint32_t> assignedCounts;
    std::vector<FlowPlan> flows;
    flows.reserve (maxFlows);
    for (uint32_t flowIdx = 0; flowIdx < maxFlows; ++flowIdx)
    {
        const uint32_t srcGlobal = flowIdx % (nXpus * portsPerXpu);
        const uint32_t srcXpuId = srcGlobal / portsPerXpu;
        const uint32_t srcPort = srcGlobal % portsPerXpu;

        Endpoint selected{0, 0};
        bool haveSelection = false;
        uint32_t selectedCount = 0;
        for (const Endpoint& endpoint : candidateEndpoints)
        {
            if (endpoint.xpuId == srcXpuId)
            {
                continue;
            }
            const auto key = std::make_pair (endpoint.xpuId, endpoint.port);
            const uint32_t count = assignedCounts[key];
            if (!haveSelection ||
                count < selectedCount ||
                (count == selectedCount &&
                 (key < std::make_pair (selected.xpuId, selected.port))))
            {
                selected = endpoint;
                selectedCount = count;
                haveSelection = true;
            }
        }
        NS_ABORT_MSG_UNLESS (haveSelection, "striped truth planner requires at least one remote endpoint");
        ++assignedCounts[std::make_pair (selected.xpuId, selected.port)];
        flows.push_back ({srcXpuId, srcPort, selected.xpuId, selected.port, selected.port, flowIdx});
    }

    for (uint32_t opIdx = 0; opIdx < config.truthOpsPerFlow; ++opIdx)
    {
        for (const FlowPlan& flow : flows)
        {
            TruthPlannedOperation op;
            op.jobId = ++nextJobId;
            op.msgId = ++nextMsgId;
            op.srcXpuId = flow.srcXpuId;
            op.srcPort = flow.srcPort;
            op.dstXpuId = flow.dstXpuId;
            if (config.truthExperimentClass == "system_pressure" && pressureProfile == "lossy")
            {
                op.dstPort = (flow.baseDstPort + ordinal + opIdx) % portsPerXpu;
            }
            else if (config.truthExperimentClass == "system_pressure" && pressureProfile == "mixed" &&
                     (flow.flowIdx % 2u) != 0u)
            {
                op.dstPort = (flow.baseDstPort + ordinal + opIdx) % portsPerXpu;
            }
            else
            {
                op.dstPort = flow.dstPort;
            }
            op.opType = SelectTruthOpType (config, ordinal);
            op.payloadBytes = payloadBytes;
            op.startOffsetSeconds = 0.001 + static_cast<double> (ordinal) * opSpacing;
            op.delayedReceive = ShouldDelayTruthReceive (config, op.opType, ordinal);
            op.receiveDelayMs = op.delayedReceive ? GetTruthReceiveDelayMs (config, ordinal) : 0u;
            op.expectValidationFailure = false;
            op.forceMultiChunk = op.payloadBytes > payloadPerPacket;
            op.plannerMode = m_truthPlannerMode;
            op.pressureProfile = pressureProfile;
            operations.push_back (op);
            ++ordinal;
        }
    }
    return operations;
}

std::vector<ApplicationDeployer::TruthPlannedOperation>
ApplicationDeployer::BuildFineGrainedTruthPlan (const SueSimulationConfig& config,
                                                TopologyBuilder& topologyBuilder)
{
    std::vector<TruthPlannedOperation> operations;
    const auto& matrix = topologyBuilder.GetSoftUeDeviceMatrix ();
    const uint32_t nXpus = matrix.size ();
    const uint32_t portsPerXpu = config.network.portsPerXpu;
    const uint32_t payloadBytes = GetTruthPayloadBytes (config);
    const double opSpacing = GetTruthOpSpacingSeconds (config);
    const std::string pressureProfile =
        (config.truthExperimentClass == "system_pressure") ? config.truthPressureProfile : "baseline";
    const uint32_t payloadPerPacket = std::max<uint32_t> (1u, config.traffic.PayloadMtu);
    auto flows = ParseFineGrainedTrafficConfig (config);
    uint64_t nextJobId = 54000;
    uint16_t nextMsgId = 400;
    uint32_t ordinal = 0;

    for (const auto& flow : flows)
    {
        const uint32_t srcXpuId = flow.sourceXpuId % nXpus;
        const uint32_t dstXpuId = flow.destXpuId % nXpus;
        const uint32_t rawSrcPort = flow.sueId * config.network.portsPerSue + flow.suePort;
        const uint32_t srcPort = rawSrcPort % portsPerXpu;
        const uint32_t destPort = (flow.hasExplicitDestPort ? flow.destPort
                                                            : (rawSrcPort % portsPerXpu)) %
                                  portsPerXpu;
        const uint32_t opCount = std::max<uint32_t> (1u,
                                                     static_cast<uint32_t> ((flow.totalBytes + payloadBytes - 1) /
                                                                            payloadBytes));

        for (uint32_t opIdx = 0; opIdx < opCount; ++opIdx, ++ordinal)
        {
            TruthPlannedOperation op;
            op.jobId = ++nextJobId;
            op.msgId = ++nextMsgId;
            op.srcXpuId = srcXpuId;
            op.srcPort = srcPort;
            op.dstXpuId = dstXpuId;
            op.dstPort = destPort;
            op.opType = SelectTruthOpType (config, ordinal);
            op.payloadBytes = payloadBytes;
            op.startOffsetSeconds = flow.startTime + static_cast<double> (opIdx) * opSpacing;
            op.delayedReceive = ShouldDelayTruthReceive (config, op.opType, ordinal);
            op.receiveDelayMs = op.delayedReceive ? GetTruthReceiveDelayMs (config, ordinal) : 0u;
            op.expectValidationFailure = false;
            op.forceMultiChunk = op.payloadBytes > payloadPerPacket;
            op.plannerMode = m_truthPlannerMode;
            op.pressureProfile = pressureProfile;
            operations.push_back (op);
        }
    }

    return operations;
}

std::vector<ApplicationDeployer::TruthPlannedOperation>
ApplicationDeployer::BuildTraceTruthPlan (const SueSimulationConfig& config,
                                          TopologyBuilder& topologyBuilder)
{
    std::vector<TruthPlannedOperation> operations;
    const auto& matrix = topologyBuilder.GetSoftUeDeviceMatrix ();
    const uint32_t nXpus = matrix.size ();
    const uint32_t portsPerXpu = config.network.portsPerXpu;
    const uint32_t payloadBytes = GetTruthPayloadBytes (config);
    const std::string pressureProfile =
        (config.truthExperimentClass == "system_pressure") ? config.truthPressureProfile : "baseline";
    const uint32_t payloadPerPacket = std::max<uint32_t> (1u, config.traffic.PayloadMtu);
    const uint32_t maxOps = std::max<uint32_t> (1u, config.truthFlowCount * config.truthOpsPerFlow);
    std::ifstream traceFile (config.traffic.traceFilePath);
    NS_ABORT_MSG_UNLESS (traceFile.is_open (),
                         "Cannot open truth trace file: " << config.traffic.traceFilePath);

    std::string line;
    uint64_t nextJobId = 55000;
    uint16_t nextMsgId = 600;
    uint64_t firstTimestampNs = std::numeric_limits<uint64_t>::max ();

    while (std::getline (traceFile, line) && operations.size () < maxOps)
    {
        if (line.empty () || line[0] == '#')
        {
            continue;
        }
        if (line.find ("Timestamp") != std::string::npos || line.find ("timestamp") != std::string::npos)
        {
            continue;
        }

        std::istringstream iss (line);
        std::vector<std::string> tokens;
        std::string token;
        while (std::getline (iss, token, ','))
        {
            token.erase (0, token.find_first_not_of (" \t"));
            const auto pos = token.find_last_not_of (" \t");
            if (pos != std::string::npos)
            {
                token.erase (pos + 1);
            }
            tokens.push_back (token);
        }
        if (tokens.size () < 6)
        {
            continue;
        }

        const uint64_t timestampNs = static_cast<uint64_t> (std::stoull (tokens[1]));
        const uint32_t gpuId = static_cast<uint32_t> (std::stoul (tokens[2]));
        const uint32_t dieId = static_cast<uint32_t> (std::stoul (tokens[3]));
        const std::string operation = tokens[4];

        if (firstTimestampNs == std::numeric_limits<uint64_t>::max ())
        {
            firstTimestampNs = timestampNs;
        }

        TruthPlannedOperation op;
        op.jobId = ++nextJobId;
        op.msgId = ++nextMsgId;
        op.srcXpuId = 0;
        op.srcPort = dieId % portsPerXpu;
        op.dstXpuId = gpuId % nXpus;
        if (op.dstXpuId == op.srcXpuId && nXpus > 1)
        {
            op.dstXpuId = (op.dstXpuId + 1) % nXpus;
        }
        op.dstPort = op.srcPort % portsPerXpu;
        if (operation == "LOAD")
        {
            op.opType = OpType::READ;
        }
        else if (operation == "STORE")
        {
            op.opType = OpType::WRITE;
        }
        else
        {
            op.opType = OpType::SEND;
        }
        op.payloadBytes = payloadBytes;
        op.startOffsetSeconds = static_cast<double> (timestampNs - firstTimestampNs) / 1e9;
        op.delayedReceive = ShouldDelayTruthReceive (config, op.opType, operations.size ());
        op.receiveDelayMs =
            op.delayedReceive ? GetTruthReceiveDelayMs (config, operations.size ()) : 0u;
        op.expectValidationFailure = false;
        op.forceMultiChunk = op.payloadBytes > payloadPerPacket;
        op.plannerMode = m_truthPlannerMode;
        op.pressureProfile = pressureProfile;
        operations.push_back (op);
    }

    return operations;
}

OpType
ApplicationDeployer::SelectTruthOpType (const SueSimulationConfig& config, uint32_t ordinal) const
{
    const uint32_t bucket = ordinal % 100u;
    if (bucket < config.truthSendPercent)
    {
        return OpType::SEND;
    }
    if (bucket < config.truthSendPercent + config.truthWritePercent)
    {
        return OpType::WRITE;
    }
    return OpType::READ;
}

uint32_t
ApplicationDeployer::GetTruthPayloadBytes (const SueSimulationConfig& config) const
{
    const uint32_t baseBytes = std::max<uint32_t> (config.truthPayloadBytes, 32u);
    if (config.truthExperimentClass != "system_pressure")
    {
        return baseBytes;
    }

    const uint32_t payloadPerPacket = std::max<uint32_t> (1u, config.traffic.PayloadMtu);
    if (config.truthPressureProfile == "credit_pressure" || config.truthPressureProfile == "lossy")
    {
        return std::max<uint32_t> (baseBytes, payloadPerPacket * 3u + 17u);
    }
    if (config.truthPressureProfile == "mixed")
    {
        return std::max<uint32_t> (baseBytes, payloadPerPacket * 4u + 17u);
    }
    return baseBytes;
}

bool
ApplicationDeployer::ShouldDelayTruthReceive (const SueSimulationConfig& config,
                                              OpType opType,
                                              uint32_t ordinal) const
{
    if (config.truthExperimentClass != "system_pressure" || opType != OpType::SEND)
    {
        return false;
    }
    if (config.truthPressureProfile == "mixed")
    {
        return (ordinal % 2u) == 0u;
    }
    if (config.truthPressureProfile == "lossy")
    {
        return (ordinal % 3u) == 0u;
    }
    return false;
}

uint32_t
ApplicationDeployer::GetTruthReceiveDelayMs (const SueSimulationConfig& config, uint32_t ordinal) const
{
    if (config.truthPressureProfile == "mixed")
    {
        return 18u + static_cast<uint32_t> (ordinal % 5u);
    }
    if (config.truthPressureProfile == "lossy")
    {
        return 6u + static_cast<uint32_t> (ordinal % 3u);
    }
    return 15u;
}

double
ApplicationDeployer::GetTruthOpSpacingSeconds (const SueSimulationConfig& config) const
{
    if (config.truthOpSpacingNs > 0)
    {
        return static_cast<double> (config.truthOpSpacingNs) / 1'000'000'000.0;
    }
    if (config.truthExperimentClass != "system_pressure")
    {
        return 0.001;
    }
    if (config.truthPressureProfile == "credit_pressure")
    {
        return 0.00020;
    }
    if (config.truthPressureProfile == "lossy")
    {
        return 0.00025;
    }
    if (config.truthPressureProfile == "mixed")
    {
        return 0.00015;
    }
    return 0.00050;
}

std::vector<FineGrainedTrafficFlow>
ApplicationDeployer::ParseFineGrainedTrafficConfig (const SueSimulationConfig& config)
{
    NS_LOG_FUNCTION (this << &config);

    std::vector<FineGrainedTrafficFlow> flows;

    if (!config.traffic.enableFineGrainedMode || config.traffic.fineGrainedConfigFile.empty())
    {
        NS_LOG_WARN("Fine-grained mode not enabled or config file not specified");
        return flows;
    }

    std::ifstream configFile(config.traffic.fineGrainedConfigFile);
    if (!configFile.is_open())
    {
        NS_LOG_ERROR("Cannot open fine-grained traffic configuration file: " << config.traffic.fineGrainedConfigFile);
        exit(0);
        return flows;
    }

    std::string line;
    uint32_t lineNumber = 0;

    while (std::getline(configFile, line))
    {
        lineNumber++;

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        // Skip CSV header line (support legacy, timestamped, and destPort-extended formats)
        if ((line.find("sourceXpuId") != std::string::npos &&
            line.find("destXpuId") != std::string::npos) ||
            (line.find("timestamp") != std::string::npos))
        {
            continue;
        }

        // Parse line:
        // Timestamped format:
        //   timestamp(ns),sourceXpuId,destXpuId,sueId,suePort,vcId,dataRate,totalBytes[,destPort]
        // Legacy format:
        //   sourceXpuId,destXpuId,sueId,suePort,vcId,dataRate,totalBytes[,destPort]
        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(iss, token, ','))
        {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);
            tokens.push_back(token);
        }

        if (tokens.size() < 7)
        {
            NS_LOG_WARN("Invalid line " << lineNumber << " in fine-grained config: " << line
                      << " (expected at least 7 fields, got " << tokens.size() << ")");
            continue;
        }

        try
        {
            FineGrainedTrafficFlow flow;

            // Check if this is the timestamped format
            if (tokens.size() >= 8)
            {
                // Format: timestamp(ns),sourceXpuId,destXpuId,sueId,suePort,vcId,dataRate,totalBytes[,destPort]
                // Parse timestamp as nanoseconds and convert to seconds for internal use
                double timestampNs = std::stod(tokens[0]);
                flow.startTime = timestampNs / 1e9;                           // Convert ns to seconds
                flow.sourceXpuId = static_cast<uint32_t>(std::stoul(tokens[1])); // 0-based
                flow.destXpuId = static_cast<uint32_t>(std::stoul(tokens[2]));   // 0-based
                flow.sueId = static_cast<uint32_t>(std::stoul(tokens[3]));      // 0-based
                flow.suePort = static_cast<uint32_t>(std::stoul(tokens[4]));         // 0-based port
                flow.vcId = static_cast<uint8_t>(std::stoul(tokens[5]));            // VC ID (0-3)
                flow.dataRate = std::stod(tokens[6]);                                // Mbps
                flow.totalBytes = static_cast<uint32_t>(std::stoul(tokens[7]));     // Bytes
                if (tokens.size() >= 9)
                {
                    flow.destPort = static_cast<uint32_t>(std::stoul(tokens[8]));
                    flow.hasExplicitDestPort = true;
                }
            }
            else
            {
                // Legacy format: sourceXpuId,destXpuId,sueId,suePort,vcId,dataRate,totalBytes[,destPort]
                flow.startTime = 0.0;  // Default start time for legacy format
                flow.sourceXpuId = static_cast<uint32_t>(std::stoul(tokens[0])); // 0-based
                flow.destXpuId = static_cast<uint32_t>(std::stoul(tokens[1]));   // 0-based
                flow.sueId = static_cast<uint32_t>(std::stoul(tokens[2]));      // 0-based
                flow.suePort = static_cast<uint32_t>(std::stoul(tokens[3]));         // 0-based port
                flow.vcId = static_cast<uint8_t>(std::stoul(tokens[4]));            // VC ID (0-3)
                flow.dataRate = std::stod(tokens[5]);                                // Mbps
                flow.totalBytes = static_cast<uint32_t>(std::stoul(tokens[6]));     // Bytes
                if (tokens.size() >= 8)
                {
                    flow.destPort = static_cast<uint32_t>(std::stoul(tokens[7]));
                    flow.hasExplicitDestPort = true;
                }
            }

            // Validate VC ID range
            if (flow.vcId > 3)
            {
                NS_LOG_WARN("VC ID " << (uint32_t)flow.vcId << " out of range (0-3) on line " << lineNumber
                          << ", using VC 0");
                flow.vcId = 0;
            }

            // Validate start time
            if (flow.startTime < 0.0)
            {
                NS_LOG_WARN("Start time " << flow.startTime << " is negative on line " << lineNumber
                          << ", using 0.0");
                flow.startTime = 0.0;
            }

            flows.push_back(flow);

            NS_LOG_INFO("Parsed flow: XPU" << flow.sourceXpuId + 1 << " -> XPU" << flow.destXpuId + 1
                      << " via SUE" << flow.sueId + 1 << ":Port" << flow.suePort << " at " << flow.dataRate << " Mbps"
                      << " on VC" << (uint32_t)flow.vcId << " for " << flow.totalBytes << " bytes"
                      << " destPort=" << (flow.hasExplicitDestPort ? std::to_string (flow.destPort) : "auto")
                      << " (start: " << flow.startTime << "s after client start)");
        }
        catch (const std::exception& e)
        {
            NS_LOG_ERROR("Error parsing line " << lineNumber << " in fine-grained config: " << e.what());
        }
    }

    configFile.close();

    NS_LOG_INFO("Parsed " << flows.size() << " fine-grained traffic flows from "
              << config.traffic.fineGrainedConfigFile);

    return flows;
}

} // namespace ns3
