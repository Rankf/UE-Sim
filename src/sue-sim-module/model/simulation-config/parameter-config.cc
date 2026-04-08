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

#include "parameter-config.h"
#include "../sue-utils.h"
#include "ns3/core-module.h"
#include <iostream>
#include <iomanip>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ParameterConfig");

namespace {

constexpr uint32_t kSoftUeSemanticHeaderOverheadBytes = 128;
constexpr uint32_t kDefaultSoftUePayloadMtu = 2048;

bool
IsSupportedPayloadMtu (uint32_t payloadMtu)
{
    return payloadMtu == 2048u || payloadMtu == 4096u || payloadMtu == 8192u;
}

} // namespace

SueSimulationConfig::SueSimulationConfig ()
{
    // Initialize timing configuration
    timing.simulationTime = 3.00;
    timing.serverStart = 1.0;
    timing.clientStart = 2.0;
    timing.clientStopOffset = 0.1;
    timing.serverStopOffset = 0.01;
    timing.threadStartInterval = 0.1;

    // Initialize network configuration
    network.nXpus = 4;
    network.portsPerXpu = 8;
    network.portsPerSue = 2;
    network.suesPerXpu = 0; // Will be calculated

    // Initialize traffic configuration
    traffic.transactionSize = 256;
    traffic.maxBurstSize = 2048;
    traffic.Mtu = 2500;
    traffic.PayloadMtu = kDefaultSoftUePayloadMtu;
    traffic.vcNum = 4;
    traffic.threadRate = 3500000;
    traffic.totalBytesToSend = 50;
    traffic.enableTraceMode = false;  // Default to traditional traffic generation
    traffic.traceFilePath = "";       // Empty trace file path by default

    // Initialize fine-grained traffic configuration
    traffic.enableFineGrainedMode = false;  // Default to traditional traffic generation
    traffic.fineGrainedConfigFile = "";     // Empty fine-grained config file path by default

    // Initialize link configuration
    link.errorRate = 0.00;
    link.processingDelay = "10ns";
    link.numVcs = 4;
    link.LinkDataRate = "200Gbps";
    link.ProcessingRate = "200Gbps";
    link.LinkDelay = "10ns";

    // Initialize queue configuration
    queue.vcQueueMaxKB = 30.0;
    queue.vcQueueMaxBytes = 0; // Will be calculated
    queue.processingQueueMaxKB = 30.0;
    queue.processingQueueMaxBytes = 0; // Will be calculated
    queue.destQueueMaxKB = 30.0;
    queue.destQueueMaxBytes = 0; // Will be calculated

    // Initialize CBFC configuration
    cbfc.EnableLinkCBFC = true;
    cbfc.LinkCredits = 85;
    cbfc.CreditBatchSize = 1;
    cbfc.SwitchCredits = 85;
    cbfc.HeaderSize = 52;
    cbfc.BaseCredit = 1;

    // Initialize credit-to-byte mapping parameters
    cbfc.BytesPerCredit = 32;           // Bytes per credit

    // Initialize load balance configuration
    loadBalance.loadBalanceAlgorithm = 3;
    loadBalance.hashSeed = 12345;
    loadBalance.prime1 = 7919;
    loadBalance.prime2 = 9973;
    loadBalance.useVcInHash = true;
    loadBalance.enableBitOperations = true;
    loadBalance.enableAlternativePath = false;

    // Initialize trace configuration
    trace.statLoggingEnabled = true;
    trace.ClientStatInterval = "10us";
    
    // Initialize delay configuration
    delay.SchedulingInterval = "100ns";
    delay.PackingDelayPerPacket = "3ns";
    delay.destQueueSchedulingDelay = "5ns";
    delay.transactionClassificationDelay = "0ns";
    delay.packetCombinationDelay = "12ns";
    delay.ackProcessingDelay = "15ns";
    delay.vcSchedulingDelay = "8ns";
    delay.DataAddHeadDelay = "5ns";
    delay.additionalHeaderSize = 44;
    delay.creditGenerateDelay = "10ns";
    delay.CreUpdateAddHeadDelay = "3ns";
    delay.creditReturnProcessingDelay = "8ns";
    delay.batchCreditAggregationDelay = "5ns";
    delay.switchForwardDelay = "50ns";
    delay.processingQueueScheduleDelay = "5ns";

    //Initialize LLR configuration
    llr.m_llrEnabled = false;
    llr.LlrTimeout = "10000ns";
    llr.LlrWindowSize = 10;
    llr.AckAddHeaderDelay = "10ns";
    llr.AckProcessDelay = "10ns";

    // Initialize logging configuration
    logging.logLevel = "LOG_LEVEL_INFO";
    logging.enableAllComponents = true;
    systemScenarioMode.clear ();
    truthExperimentClass = "semantic_demo";
    truthPlannerMode.clear ();
    enableSoftUeObserver = true;
    enableSoftUeProtocolLogging = true;
    truthFlowCount = 0;
    truthOpsPerFlow = 0;
    truthPressureProfile = "baseline";
    truthSendPercent = 40;
    truthWritePercent = 30;
    truthReadPercent = 30;
    truthPayloadBytes = 0;
    truthUnexpectedMessages = 0;
    truthUnexpectedBytes = 0;
    truthArrivalBlocks = 0;
    truthReadTracks = 0;
    truthDropRate = 0.0;
    truthReorderWindowNs = 0;
    truthExtraDelayNs = 0;
    truthLinkDataRate = "1Gbps";
    truthOpSpacingNs = 0;
    truthInitialCredits = 0;
    truthCreditRefreshIntervalNs = 0;
    truthSendAdmissionMessages = 0;
    truthSendAdmissionBytes = 0;
    truthWriteBudgetMessages = 0;
    truthWriteBudgetBytes = 0;
    truthReadResponderMessages = 0;
    truthReadResponseBytes = 0;
    truthRetryTimeoutNs = 0;
    truthRequireFailureEvidence = false;
    truthProtocolCsvRequired = true;
    fabricTopologyMode = "explicit_multipath";
    fabricEndpointMode = "six_endpoint";
    fabricPathCount = 4;
    fabricPathDataRate = "100Gbps";
    fabricLinkDelay = "10ns";
    fabricUseEcmpHash = true;
    fabricDynamicPathSelection = false;
    fabricEnableEcnObservation = true;
    fabricTrafficPattern = "all_to_all";

}

void
SueSimulationConfig::ParseCommandLine (int argc, char* argv[])
{
    CommandLine cmd;

    // Timing parameters
    cmd.AddValue ("simulationTime", "Total simulation duration in seconds", timing.simulationTime);
    cmd.AddValue ("serverStart", "Server start time (seconds)", timing.serverStart);
    cmd.AddValue ("clientStart", "Client start time (seconds)", timing.clientStart);
    cmd.AddValue ("clientStopOffset", "Client stop time offset from simulation end (seconds)", timing.clientStopOffset);
    cmd.AddValue ("serverStopOffset", "Server stop time offset from simulation end (seconds)", timing.serverStopOffset);
    cmd.AddValue ("threadStartInterval", "Interval between thread start times (seconds)", timing.threadStartInterval);

    // Network topology parameters
    cmd.AddValue ("nXpus", "The number of XPU nodes", network.nXpus);
    cmd.AddValue ("portsPerXpu", "Number of ports per XPU", network.portsPerXpu);
    cmd.AddValue ("portsPerSue", "Number of ports per SUE (1/2/4)", network.portsPerSue);
    cmd.AddValue("threadRate", "Traffic generation rate per thread (Mbps)", traffic.threadRate);
    cmd.AddValue("totalBytesToSend", "Total bytes to send per XPU (MB)", traffic.totalBytesToSend);

    // Traffic generation parameters
    cmd.AddValue("transactionSize", "Size per transaction in bytes", traffic.transactionSize);
    cmd.AddValue("maxBurstSize", "Maximum burst size in bytes", traffic.maxBurstSize);
    cmd.AddValue("Mtu", "Device/link MTU in bytes (includes headers)", traffic.Mtu);
    cmd.AddValue("PayloadMtu", "UEC payload MTU for soft_ue_truth (2048/4096/8192)", traffic.PayloadMtu);
    cmd.AddValue("vcNum", "Number of virtual channels at application layer", traffic.vcNum);
    cmd.AddValue("enableTraceMode", "Enable trace-based traffic generation", traffic.enableTraceMode);
    cmd.AddValue("traceFilePath", "Path to trace file for trace-based generation", traffic.traceFilePath);
    cmd.AddValue("enableFineGrainedMode", "Enable fine-grained traffic control mode", traffic.enableFineGrainedMode);
    cmd.AddValue("fineGrainedConfigFile", "Path to fine-grained traffic configuration file", traffic.fineGrainedConfigFile);

    // Link layer parameters
    cmd.AddValue("errorRate", "The packet error rate for the links", link.errorRate);
    cmd.AddValue("processingDelay", "Processing delay per packet", link.processingDelay);
    cmd.AddValue("numVcs", "Number of virtual channels at link layer", link.numVcs);
    cmd.AddValue("LinkDataRate", "Link data rate", link.LinkDataRate);
    cmd.AddValue("ProcessingRate", "Link data rate", link.ProcessingRate);
    cmd.AddValue("LinkDelay", "Link propagation delay", link.LinkDelay);

    // Queue buffer size configuration
    cmd.AddValue("VcQueueMaxKB", "Maximum VC queue size in KB (default: 30KB)", queue.vcQueueMaxKB);
    cmd.AddValue("ProcessingQueueMaxKB", "Maximum processing queue size in KB (default: 30KB)", queue.processingQueueMaxKB);
    cmd.AddValue("DestQueueMaxKB", "Maximum destination queue size in KB (default: 30KB)", queue.destQueueMaxKB);

    // CBFC flow control parameters
    cmd.AddValue("EnableLinkCBFC", "Enable Credit-Based Flow Control", cbfc.EnableLinkCBFC);
    cmd.AddValue("LinkCredits", "Initial credits at link layer", cbfc.LinkCredits);
    cmd.AddValue("CreditBatchSize", "Credit accumulation threshold", cbfc.CreditBatchSize);
    cmd.AddValue("SwitchCredits", "Switch credits", cbfc.SwitchCredits);
    cmd.AddValue("HeaderSize", "Header size (Ethernet + SUE headers)", cbfc.HeaderSize);
    cmd.AddValue("BaseCredit", "Base credit value for minimum packet", cbfc.BaseCredit);

    // Credit calculation parameters
    cmd.AddValue("BytesPerCredit", "Bytes per credit (default: 256)", cbfc.BytesPerCredit);

    // Trace sampling parameters
    cmd.AddValue("StatLoggingEnabled", "Link Layer Stat Logging Enabled Switch", trace.statLoggingEnabled);
    cmd.AddValue("ClientStatInterval", "Client Statistic Interval", trace.ClientStatInterval);

    // Delay parameters - transmitter scheduling
    cmd.AddValue("SchedulingInterval", "Transmitter scheduler polling interval", delay.SchedulingInterval);
    cmd.AddValue("PackingDelayPerPacket", "Packet packing processing time", delay.PackingDelayPerPacket);
    cmd.AddValue("destQueueSchedulingDelay", "Destination queue scheduling delay", delay.destQueueSchedulingDelay);
    cmd.AddValue("transactionClassificationDelay", "Transaction classification delay", delay.transactionClassificationDelay);
    cmd.AddValue("packetCombinationDelay", "Packet combination delay", delay.packetCombinationDelay);
    cmd.AddValue("ackProcessingDelay", "ACK processing delay", delay.ackProcessingDelay);

    // Link layer delay parameters
    cmd.AddValue("vcSchedulingDelay", "VC queue scheduling delay", delay.vcSchedulingDelay);
    cmd.AddValue("DataAddHeadDelay", "Data packet header addition delay", delay.DataAddHeadDelay);

    // Credit-related delays
    cmd.AddValue("creditGenerateDelay", "Credit packet generation delay", delay.creditGenerateDelay);
    cmd.AddValue("CreUpdateAddHeadDelay", "Credit update packet header addition delay", delay.CreUpdateAddHeadDelay);
    cmd.AddValue("creditReturnProcessingDelay", "Credit return processing delay", delay.creditReturnProcessingDelay);
    cmd.AddValue("batchCreditAggregationDelay", "Batch credit aggregation delay", delay.batchCreditAggregationDelay);
    cmd.AddValue("switchForwardDelay", "Switch internal forwarding delay", delay.switchForwardDelay);

    // Processing queue delays
    cmd.AddValue("processingQueueScheduleDelay", "Processing queue scheduling delay", delay.processingQueueScheduleDelay);

    // Capacity reservation parameters
    cmd.AddValue("AdditionalHeaderSize", "Additional header size for capacity reservation (default: 46 bytes)", delay.additionalHeaderSize);

    // Load balancing parameters
    cmd.AddValue("loadBalanceAlgorithm", "Load balancing algorithm (0=SIMPLE_MOD, 1=MOD_WITH_SEED, 2=PRIME_HASH, 3=ENHANCED_HASH, 4=ROUND_ROBIN, 5=CONSISTENT_HASH)", loadBalance.loadBalanceAlgorithm);
    cmd.AddValue("hashSeed", "Hash seed for load balancing", loadBalance.hashSeed);
    cmd.AddValue("prime1", "First prime number for hash algorithms", loadBalance.prime1);
    cmd.AddValue("prime2", "Second prime number for enhanced hash", loadBalance.prime2);
    cmd.AddValue("useVcInHash", "Include VC ID in hash calculation", loadBalance.useVcInHash);
    cmd.AddValue("enableBitOperations", "Enable bit mixing operations in hash", loadBalance.enableBitOperations);
    cmd.AddValue("enableAlternativePath", "Enable alternative SUE path search when target is full", loadBalance.enableAlternativePath);

    //Llr related parameters
    cmd.AddValue("EnableLLR", "Enable Link Layer Reliability", llr.m_llrEnabled);
    cmd.AddValue("LlrTimeout", "LLR timeout value", llr.LlrTimeout);
    cmd.AddValue("LlrWindowSize", "LLR window size", llr.LlrWindowSize);
    cmd.AddValue("AckAddHeaderDelay", "ACK/NACK header adding delay", llr.AckAddHeaderDelay);
    cmd.AddValue("AckProcessDelay", "ACK/NACK processing delay", llr.AckProcessDelay);

    // Logging configuration parameters
    cmd.AddValue("logLevel", "Log level for all components (LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR, LOG_LEVEL_FUNCTION, LOG_LEVEL_LOGIC, LOG_LEVEL_ALL)", logging.logLevel);
    cmd.AddValue("enableAllComponents", "Enable logging for all SUE simulation components", logging.enableAllComponents);
    cmd.AddValue("systemScenarioMode", "Required system scenario mode: legacy_sue, soft_ue_truth, or soft_ue_fabric", systemScenarioMode);
    cmd.AddValue("truthExperimentClass", "Truth-backed experiment class: semantic_demo, system_smoke, or system_pressure", truthExperimentClass);
    cmd.AddValue("truthPlannerMode", "Optional truth planner override for soft_ue_truth: uniform or striped", truthPlannerMode);
    cmd.AddValue("enableSoftUeObserver", "Enable SoftUeObserverHelper for soft_ue_truth scenarios", enableSoftUeObserver);
    cmd.AddValue("enableSoftUeProtocolLogging", "Enable soft-ue protocol/failure/diagnostic CSV logging", enableSoftUeProtocolLogging);
    cmd.AddValue("truthFlowCount", "Maximum number of flows for uniform truth planner", truthFlowCount);
    cmd.AddValue("truthOpsPerFlow", "Operations per truth planner flow", truthOpsPerFlow);
    cmd.AddValue("truthPressureProfile", "Truth-backed pressure profile: baseline, credit_pressure, lossy, or mixed", truthPressureProfile);
    cmd.AddValue("truthSendPercent", "SEND percentage for truth-generated operations", truthSendPercent);
    cmd.AddValue("truthWritePercent", "WRITE percentage for truth-generated operations", truthWritePercent);
    cmd.AddValue("truthReadPercent", "READ percentage for truth-generated operations", truthReadPercent);
    cmd.AddValue("truthPayloadBytes", "Payload size for generated truth operations (0 uses transactionSize)", truthPayloadBytes);
    cmd.AddValue("truthUnexpectedMessages", "Truth-path unexpected message pool size", truthUnexpectedMessages);
    cmd.AddValue("truthUnexpectedBytes", "Truth-path unexpected byte pool size", truthUnexpectedBytes);
    cmd.AddValue("truthArrivalBlocks", "Truth-path arrival tracking capacity", truthArrivalBlocks);
    cmd.AddValue("truthReadTracks", "Truth-path read response track capacity", truthReadTracks);
    cmd.AddValue("truthDropRate", "Truth-path packet drop probability for pressure profiles", truthDropRate);
    cmd.AddValue("truthReorderWindowNs", "Truth-path reorder hold window in nanoseconds", truthReorderWindowNs);
    cmd.AddValue("truthExtraDelayNs", "Truth-path extra link delay in nanoseconds", truthExtraDelayNs);
    cmd.AddValue("truthLinkDataRate", "Truth-path shared fabric data rate", truthLinkDataRate);
    cmd.AddValue("truthOpSpacingNs", "Truth-path generated operation spacing in nanoseconds (0 uses profile default)", truthOpSpacingNs);
    cmd.AddValue("truthInitialCredits", "Truth-path initial credits per peer (0 uses soft-ue default)", truthInitialCredits);
    cmd.AddValue("truthCreditRefreshIntervalNs", "Truth-path credit refresh interval in nanoseconds (0 uses soft-ue default)", truthCreditRefreshIntervalNs);
    cmd.AddValue("truthSendAdmissionMessages", "Truth-path SEND admission message budget", truthSendAdmissionMessages);
    cmd.AddValue("truthSendAdmissionBytes", "Truth-path SEND admission byte budget", truthSendAdmissionBytes);
    cmd.AddValue("truthWriteBudgetMessages", "Truth-path WRITE target budget message limit", truthWriteBudgetMessages);
    cmd.AddValue("truthWriteBudgetBytes", "Truth-path WRITE target budget byte limit", truthWriteBudgetBytes);
    cmd.AddValue("truthReadResponderMessages", "Truth-path READ responder message budget", truthReadResponderMessages);
    cmd.AddValue("truthReadResponseBytes", "Truth-path READ response byte budget", truthReadResponseBytes);
    cmd.AddValue("truthRetryTimeoutNs", "Truth-path semantic retry timeout in nanoseconds (0 uses soft-ue default)", truthRetryTimeoutNs);
    cmd.AddValue("truthRequireFailureEvidence", "Require truth system_pressure profiles to surface pressure evidence", truthRequireFailureEvidence);
    cmd.AddValue("truthProtocolCsvRequired", "Require truth path to emit protocol CSV rows", truthProtocolCsvRequired);
    cmd.AddValue("fabricTopologyMode", "Fabric topology mode for soft_ue_fabric: shared_truth or explicit_multipath", fabricTopologyMode);
    cmd.AddValue("fabricEndpointMode", "Fabric endpoint layout for soft_ue_fabric: six_endpoint or six_by_six", fabricEndpointMode);
    cmd.AddValue("fabricPathCount", "Explicit multipath fabric path count", fabricPathCount);
    cmd.AddValue("fabricPathDataRate", "Explicit multipath fabric per-path data rate", fabricPathDataRate);
    cmd.AddValue("fabricLinkDelay", "Explicit multipath fabric per-path link delay", fabricLinkDelay);
    cmd.AddValue("fabricUseEcmpHash", "Enable stable ECMP hashing for explicit multipath fabric", fabricUseEcmpHash);
    cmd.AddValue("fabricDynamicPathSelection", "Enable adaptive sticky per-flow path selection for explicit multipath fabric", fabricDynamicPathSelection);
    cmd.AddValue("fabricEnableEcnObservation", "Enable ECN-style observation counters for explicit multipath fabric", fabricEnableEcnObservation);
    cmd.AddValue("fabricTrafficPattern", "Fabric traffic pattern for soft_ue_fabric: all_to_all, hotspot, or incast", fabricTrafficPattern);

    cmd.Parse(argc, argv);
}

void
SueSimulationConfig::ValidateAndCalculate ()
{
    // Convert KB to bytes for queue configurations
    queue.vcQueueMaxBytes = static_cast<uint32_t>(queue.vcQueueMaxKB * 1024);
    queue.processingQueueMaxBytes = static_cast<uint32_t>(queue.processingQueueMaxKB * 1024);
    queue.destQueueMaxBytes = static_cast<uint32_t>(queue.destQueueMaxKB * 1024);

    // Parameter validation
    if (network.portsPerSue != 1 && network.portsPerSue != 2 && network.portsPerSue != 4) {
        NS_ABORT_MSG("portsPerSue must be 1, 2, or 4. Current value: " << network.portsPerSue);
    }
    if (network.portsPerXpu % network.portsPerSue != 0) {
        NS_ABORT_MSG("portsPerXpu (" << network.portsPerXpu << ") must be divisible by portsPerSue (" << network.portsPerSue << ")");
    }
    if (loadBalance.loadBalanceAlgorithm > 5) {
        NS_ABORT_MSG("loadBalanceAlgorithm must be 0-5 (0=SIMPLE_MOD, 1=MOD_WITH_SEED, 2=PRIME_HASH, 3=ENHANCED_HASH, 4=ROUND_ROBIN, 5=CONSISTENT_HASH). Current value: " << loadBalance.loadBalanceAlgorithm);
    }
    if (systemScenarioMode.empty ())
    {
        NS_ABORT_MSG ("systemScenarioMode is required. Pass --systemScenarioMode=soft_ue_truth or "
                      "--systemScenarioMode=legacy_sue.");
    }
    if (systemScenarioMode != "legacy_sue" &&
        systemScenarioMode != "soft_ue_truth" &&
        systemScenarioMode != "soft_ue_fabric")
    {
        NS_ABORT_MSG ("systemScenarioMode must be legacy_sue, soft_ue_truth, or soft_ue_fabric. Current value: "
                      << systemScenarioMode);
    }
    if (truthExperimentClass != "semantic_demo" &&
        truthExperimentClass != "system_smoke" &&
        truthExperimentClass != "system_pressure")
    {
        NS_ABORT_MSG ("truthExperimentClass must be semantic_demo, system_smoke, or system_pressure. Current value: "
                      << truthExperimentClass);
    }
    if (truthPressureProfile != "baseline" &&
        truthPressureProfile != "credit_pressure" &&
        truthPressureProfile != "lossy" &&
        truthPressureProfile != "mixed")
    {
        NS_ABORT_MSG ("truthPressureProfile must be baseline, credit_pressure, lossy, or mixed. Current value: "
                      << truthPressureProfile);
    }
    if (truthSendPercent + truthWritePercent + truthReadPercent != 100)
    {
        NS_ABORT_MSG ("truthSendPercent + truthWritePercent + truthReadPercent must equal 100. Current sum: "
                      << (truthSendPercent + truthWritePercent + truthReadPercent));
    }
    if (truthDropRate < 0.0 || truthDropRate > 1.0)
    {
        NS_ABORT_MSG ("truthDropRate must be in [0, 1]. Current value: " << truthDropRate);
    }
    if (!truthPlannerMode.empty () &&
        truthPlannerMode != "uniform" &&
        truthPlannerMode != "striped")
    {
        NS_ABORT_MSG ("truthPlannerMode must be empty, uniform, or striped. Current value: "
                      << truthPlannerMode);
    }
    if (systemScenarioMode == "soft_ue_truth")
    {
        if (!IsSupportedPayloadMtu (traffic.PayloadMtu))
        {
            NS_ABORT_MSG ("PayloadMtu must be one of 2048, 4096, or 8192 for soft_ue_truth. Current value: "
                          << traffic.PayloadMtu);
        }
        if (traffic.PayloadMtu + kSoftUeSemanticHeaderOverheadBytes > traffic.Mtu)
        {
            NS_ABORT_MSG ("PayloadMtu + semantic header overhead must fit within device MTU for soft_ue_truth. "
                          << "Current values: PayloadMtu=" << traffic.PayloadMtu
                          << " header_overhead=" << kSoftUeSemanticHeaderOverheadBytes
                          << " Mtu=" << traffic.Mtu);
        }
    }

    // Recalculate number of SUEs per XPU
    network.suesPerXpu = network.portsPerXpu / network.portsPerSue;

    if (truthFlowCount == 0)
    {
        truthFlowCount = network.nXpus * network.portsPerXpu;
    }
    if (truthOpsPerFlow == 0)
    {
        if (truthExperimentClass == "semantic_demo")
        {
            truthOpsPerFlow = 1;
        }
        else if (truthExperimentClass == "system_smoke")
        {
            truthOpsPerFlow = 3;
        }
        else
        {
            truthOpsPerFlow = 8;
        }
    }
    if (truthPayloadBytes == 0)
    {
        truthPayloadBytes = traffic.transactionSize;
    }
    const bool truthPressureClass = (truthExperimentClass == "system_pressure");
    const bool truthTightResourceProfile =
        truthPressureClass &&
        (truthPressureProfile == "credit_pressure" || truthPressureProfile == "mixed");
    if (truthUnexpectedMessages == 0)
    {
        truthUnexpectedMessages = truthTightResourceProfile ? 4u : 64u;
    }
    if (truthUnexpectedBytes == 0)
    {
        truthUnexpectedBytes = truthTightResourceProfile ? 8192u : (1u << 20);
    }
    if (truthArrivalBlocks == 0)
    {
        truthArrivalBlocks = truthTightResourceProfile ? 4u : 64u;
    }
    if (truthReadTracks == 0)
    {
        truthReadTracks = truthTightResourceProfile ? 4u : 64u;
    }
    if (truthCreditRefreshIntervalNs == 0)
    {
        truthCreditRefreshIntervalNs = 5'000'000ULL;
    }
    if (truthSendAdmissionMessages == 0)
    {
        truthSendAdmissionMessages = truthArrivalBlocks;
    }
    if (truthSendAdmissionBytes == 0)
    {
        truthSendAdmissionBytes = static_cast<uint64_t> (truthSendAdmissionMessages) *
                                  static_cast<uint64_t> (truthPayloadBytes);
    }
    if (truthWriteBudgetMessages == 0)
    {
        truthWriteBudgetMessages = truthArrivalBlocks;
    }
    if (truthWriteBudgetBytes == 0)
    {
        truthWriteBudgetBytes = static_cast<uint64_t> (truthArrivalBlocks) *
                                static_cast<uint64_t> (truthPayloadBytes);
    }
    if (truthReadResponderMessages == 0)
    {
        truthReadResponderMessages = truthReadTracks;
    }
    if (truthReadResponseBytes == 0)
    {
        truthReadResponseBytes = static_cast<uint64_t> (truthReadTracks) *
                                 static_cast<uint64_t> (truthPayloadBytes);
    }
    if (truthPressureClass &&
        (truthPressureProfile == "lossy" || truthPressureProfile == "mixed") &&
        truthDropRate == 0.0)
    {
        truthDropRate = 0.10;
    }
    if (truthPressureClass &&
        (truthPressureProfile == "lossy" || truthPressureProfile == "mixed") &&
        truthReorderWindowNs == 0)
    {
        truthReorderWindowNs = 200'000ULL;
    }
    if (truthPressureClass &&
        (truthPressureProfile == "lossy" || truthPressureProfile == "mixed") &&
        truthExtraDelayNs == 0)
    {
        truthExtraDelayNs = 50'000ULL;
    }
    if (truthPressureClass &&
        (truthPressureProfile == "credit_pressure" || truthPressureProfile == "mixed") &&
        truthInitialCredits == 0)
    {
        truthInitialCredits = 4;
    }
    if (truthPressureClass && truthPressureProfile != "baseline")
    {
        truthRequireFailureEvidence = true;
    }
    if (systemScenarioMode == "soft_ue_truth" || systemScenarioMode == "soft_ue_fabric")
    {
        const DataRate truthRate = SueStringUtils::ParseDataRateString (truthLinkDataRate);
        if (truthRate.GetBitRate () == 0)
        {
            NS_ABORT_MSG ("truthLinkDataRate must be a positive parsable data rate string for truth-backed modes. Current value: "
                          << truthLinkDataRate);
        }
    }
    if (systemScenarioMode == "soft_ue_fabric")
    {
        if (fabricTopologyMode != "shared_truth" && fabricTopologyMode != "explicit_multipath")
        {
            NS_ABORT_MSG ("fabricTopologyMode must be shared_truth or explicit_multipath. Current value: "
                          << fabricTopologyMode);
        }
        if (fabricEndpointMode != "six_endpoint" && fabricEndpointMode != "six_by_six")
        {
            NS_ABORT_MSG ("fabricEndpointMode must be six_endpoint or six_by_six. Current value: "
                          << fabricEndpointMode);
        }
        if (fabricTrafficPattern != "all_to_all" &&
            fabricTrafficPattern != "hotspot" &&
            fabricTrafficPattern != "incast")
        {
            NS_ABORT_MSG ("fabricTrafficPattern must be all_to_all, hotspot, or incast. Current value: "
                          << fabricTrafficPattern);
        }
        if (fabricPathCount == 0)
        {
            NS_ABORT_MSG ("fabricPathCount must be > 0 for soft_ue_fabric");
        }
        const DataRate fabricRate = SueStringUtils::ParseDataRateString (fabricPathDataRate);
        if (fabricRate.GetBitRate () == 0)
        {
            NS_ABORT_MSG ("fabricPathDataRate must be a positive parsable data rate string. Current value: "
                          << fabricPathDataRate);
        }
        const Time fabricDelay = SueStringUtils::ParseTimeIntervalString (fabricLinkDelay);
        if (fabricDelay.IsNegative ())
        {
            NS_ABORT_MSG ("fabricLinkDelay must be non-negative. Current value: " << fabricLinkDelay);
        }
    }
}

void
SueSimulationConfig::PrintConfiguration () const
{
    // Display link layer configuration information
    std::cout << "Link Layer Configuration:" << std::endl;
    std::cout << "  Number of VCs: " << static_cast<int>(link.numVcs) << std::endl;
    std::cout << "  VC Queue Max Size: " << queue.vcQueueMaxKB << " KB ("
              << queue.vcQueueMaxBytes << " bytes)" << std::endl;
    std::cout << "  Processing Queue Max Size: " << queue.processingQueueMaxKB << " KB ("
              << queue.processingQueueMaxBytes << " bytes)" << std::endl;
    std::cout << "  Destination Queue Max Size: " << queue.destQueueMaxKB << " KB ("
              << queue.destQueueMaxBytes << " bytes)" << std::endl;
    std::cout << "  Link Data Rate: " << link.LinkDataRate << std::endl;
    std::cout << "  Processing Rate: " << link.ProcessingRate << std::endl;
    std::cout << "  Link Delay: " << link.LinkDelay << std::endl;
    std::cout << "  Enable Link CBFC: " << (cbfc.EnableLinkCBFC ? "true" : "false") << std::endl;
    std::cout << "  Link Credits: " << cbfc.LinkCredits << std::endl;
    std::cout << "  Credit Batch Size: " << cbfc.CreditBatchSize << std::endl;
    std::cout << "  Switch Credits: " << cbfc.SwitchCredits << std::endl;
    std::cout << "  Header Size: " << cbfc.HeaderSize << " bytes" << std::endl;
    std::cout << "  Base Credit: " << cbfc.BaseCredit << std::endl;
    std::cout << "  Bytes Per Credit: " << cbfc.BytesPerCredit << " bytes" << std::endl;
    std::cout << std::endl;

    // Display Traffic Generation configuration information
    std::cout << "Traffic Generation Configuration:" << std::endl;
    std::cout << "  Transaction Size: " << traffic.transactionSize << " bytes" << std::endl;
    std::cout << "  Max Burst Size: " << traffic.maxBurstSize << " bytes" << std::endl;
    std::cout << "  Device MTU: " << traffic.Mtu << " bytes" << std::endl;
    std::cout << "  Payload MTU: " << traffic.PayloadMtu << " bytes" << std::endl;
    std::cout << "  Semantic Header Overhead: " << kSoftUeSemanticHeaderOverheadBytes
              << " bytes" << std::endl;
    std::cout << "  Number of VCs: " << static_cast<int>(traffic.vcNum) << std::endl;
    std::cout << "  Thread Rate: " << traffic.threadRate << " Mbps" << std::endl;
    std::cout << "  Total Bytes to Send: " << traffic.totalBytesToSend << " MB" << std::endl;
    std::cout << "  Trace Mode: " << (traffic.enableTraceMode ? "ENABLED" : "DISABLED") << std::endl;
    if (traffic.enableTraceMode && !traffic.traceFilePath.empty()) {
        std::cout << "  Trace File Path: " << traffic.traceFilePath << std::endl;
    }
    std::cout << std::endl;

    // Display LoadBalancer configuration information
    std::cout << "LoadBalancer Configuration:" << std::endl;
    std::cout << "  Algorithm: " << loadBalance.loadBalanceAlgorithm;
    switch (loadBalance.loadBalanceAlgorithm) {
        case 0: std::cout << " (SIMPLE_MOD)"; break;
        case 1: std::cout << " (MOD_WITH_SEED)"; break;
        case 2: std::cout << " (PRIME_HASH)"; break;
        case 3: std::cout << " (ENHANCED_HASH)"; break;
        case 4: std::cout << " (ROUND_ROBIN)"; break;
        case 5: std::cout << " (CONSISTENT_HASH)"; break;
    }
    std::cout << std::endl;
    std::cout << "  Hash Seed: " << loadBalance.hashSeed << std::endl;
    std::cout << "  Prime1: " << loadBalance.prime1 << ", Prime2: " << loadBalance.prime2 << std::endl;
    std::cout << "  Use VC in Hash: " << (loadBalance.useVcInHash ? "true" : "false") << std::endl;
    std::cout << "  Enable Bit Operations: " << (loadBalance.enableBitOperations ? "true" : "false") << std::endl;
    std::cout << "  Enable Alternative Path: " << (loadBalance.enableAlternativePath ? "true" : "false") << std::endl;
    std::cout << std::endl;

    // Display Logging configuration information
    std::cout << "Logging Configuration:" << std::endl;
    std::cout << "  Log Level: " << logging.logLevel << std::endl;
    std::cout << "  Enable All Components: " << (logging.enableAllComponents ? "true" : "false") << std::endl;
    std::cout << std::endl;

    const bool truthBacked = (systemScenarioMode == "soft_ue_truth" || systemScenarioMode == "soft_ue_fabric");
    std::cout << "System Scenario Configuration:" << std::endl;
    std::cout << "  Scenario Mode: " << systemScenarioMode << std::endl;
    std::cout << "  SoftUe Observer Enabled: " << (enableSoftUeObserver ? "true" : "false") << std::endl;
    std::cout << "  SoftUe Protocol Logging Enabled: "
              << (enableSoftUeProtocolLogging ? "true" : "false") << std::endl;
    std::cout << "  Truth Experiment Class: " << truthExperimentClass << std::endl;
    std::cout << "  Truth Planner Mode: " << (truthPlannerMode.empty () ? "auto" : truthPlannerMode) << std::endl;
    std::cout << "  Truth Pressure Profile: " << truthPressureProfile << std::endl;
    std::cout << "  Truth Flow Count: " << truthFlowCount << std::endl;
    std::cout << "  Truth Ops Per Flow: " << truthOpsPerFlow << std::endl;
    std::cout << "  Truth Operation Mix: send=" << truthSendPercent
              << " write=" << truthWritePercent
              << " read=" << truthReadPercent << std::endl;
    std::cout << "  Truth Payload Bytes: " << truthPayloadBytes << std::endl;
    std::cout << "  Truth Resource Limits: unexpected_msgs=" << truthUnexpectedMessages
              << " unexpected_bytes=" << truthUnexpectedBytes
              << " arrival_blocks=" << truthArrivalBlocks
              << " read_tracks=" << truthReadTracks << std::endl;
    std::cout << "  Truth Pressure Knobs: drop_rate=" << truthDropRate
              << " reorder_window_ns=" << truthReorderWindowNs
              << " extra_delay_ns=" << truthExtraDelayNs
              << " link_data_rate=" << truthLinkDataRate
              << " op_spacing_ns=" << truthOpSpacingNs
              << " initial_credits=" << truthInitialCredits
              << " credit_refresh_interval_ns=" << truthCreditRefreshIntervalNs
              << " send_admission_messages=" << truthSendAdmissionMessages
              << " send_admission_bytes=" << truthSendAdmissionBytes
              << " write_budget_messages=" << truthWriteBudgetMessages
              << " write_budget_bytes=" << truthWriteBudgetBytes
              << " read_responder_messages=" << truthReadResponderMessages
              << " read_response_bytes=" << truthReadResponseBytes
              << " retry_timeout_ns=" << truthRetryTimeoutNs
              << " require_failure_evidence=" << (truthRequireFailureEvidence ? "true" : "false")
              << std::endl;
    std::cout << "  Truth Protocol CSV Required: "
              << (truthProtocolCsvRequired ? "true" : "false") << std::endl;
    if (systemScenarioMode == "soft_ue_fabric")
    {
        std::cout << "  Fabric Topology Mode: " << fabricTopologyMode << std::endl;
        std::cout << "  Fabric Endpoint Mode: " << fabricEndpointMode << std::endl;
        std::cout << "  Fabric Path Count: " << fabricPathCount << std::endl;
        std::cout << "  Fabric Path Data Rate: " << fabricPathDataRate << std::endl;
        std::cout << "  Fabric Link Delay: " << fabricLinkDelay << std::endl;
        std::cout << "  Fabric Use ECMP Hash: " << (fabricUseEcmpHash ? "true" : "false") << std::endl;
        std::cout << "  Fabric Dynamic Path Selection: " << (fabricDynamicPathSelection ? "true" : "false") << std::endl;
        std::cout << "  Fabric ECN Observation: " << (fabricEnableEcnObservation ? "true" : "false") << std::endl;
        std::cout << "  Fabric Traffic Pattern: " << fabricTrafficPattern << std::endl;
    }
    std::cout << "  soft_ue_truth_backed: " << (truthBacked ? "true" : "false") << std::endl;
    std::cout << std::endl;

    NS_LOG_INFO("Creating " << (truthBacked ? "soft-ue truth" : "legacy XPU-Switch")
               << " topology with " << network.nXpus << " XPUs ("
               << network.portsPerXpu << " ports/XPU, " << network.portsPerSue << " ports/SUE, "
               << network.suesPerXpu << " SUEs/XPU)");
    NS_LOG_INFO("Total simulation time: " << timing.simulationTime << " seconds");
    NS_LOG_INFO("Servers active: " << timing.serverStart << "s to " << GetServerStop () << "s");
    NS_LOG_INFO("Clients active: " << timing.clientStart << "s to " << GetClientStop () << "s");
    NS_LOG_INFO("Thread start interval: " << timing.threadStartInterval << "s");
    if (systemScenarioMode == "soft_ue_truth")
    {
        NS_LOG_INFO ("System scenario mode soft_ue_truth selected; system experiment is soft-ue truth-backed");
    }
    else if (systemScenarioMode == "soft_ue_fabric")
    {
        NS_LOG_INFO ("System scenario mode soft_ue_fabric selected; system experiment is explicit fabric truth-backed");
    }
    else
    {
        NS_LOG_INFO ("System scenario mode legacy_sue selected; this run is not soft-ue truth-backed");
    }
}

double
SueSimulationConfig::GetClientStop () const
{
    return timing.simulationTime - timing.clientStopOffset;
}

double
SueSimulationConfig::GetServerStop () const
{
    return timing.simulationTime - timing.serverStopOffset;
}

} // namespace ns3
