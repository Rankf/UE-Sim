/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2025 SUE-Sim Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PARAMETER_CONFIG_H
#define PARAMETER_CONFIG_H

#include "ns3/core-module.h"
#include <string>

namespace ns3 {

/**
 * \brief Timing configuration parameters
 */
struct TimingConfig
{
    double simulationTime;        //!< Total simulation time (seconds)
    double serverStart;           //!< Server start time (seconds)
    double clientStart;           //!< Client start time (seconds)
    double clientStopOffset;      //!< Client stop time offset
    double serverStopOffset;      //!< Server stop time offset
    double threadStartInterval;   //!< Thread start interval
};

/**
 * \brief Network topology configuration parameters
 */
struct NetworkConfig
{
    uint32_t nXpus;           //!< Number of XPU nodes
    uint32_t portsPerXpu;     //!< Number of ports per XPU
    uint32_t portsPerSue;     //!< Number of ports managed by each SUE (1/2/4)
    uint32_t suesPerXpu;      //!< Number of SUEs per XPU (calculated)
};

/**
 * \brief Fine-grained traffic flow configuration entry
 */
struct FineGrainedTrafficFlow
{
    uint32_t sourceXpuId;         //!< Source XPU ID
    uint32_t destXpuId;           //!< Destination XPU ID
    uint32_t sueId;               //!< SUE ID to use for sending
    uint32_t suePort;             //!< SUE port to use for sending
    uint32_t destPort;            //!< Optional destination port in soft_ue_truth mode
    double dataRate;              //!< Data rate for this flow (Mbps)
    uint32_t totalBytes;          //!< Total bytes to send for this flow
    uint8_t vcId;                 //!< Virtual channel ID (0-3, optional)
    double startTime;             //!< Start time relative to clientStart (seconds, parsed from ns in CSV)
    bool hasExplicitDestPort;     //!< Whether destPort was explicitly provided by config

    /**
     * \brief Constructor
     */
    FineGrainedTrafficFlow () : sourceXpuId(0), destXpuId(0), sueId(0), suePort(0),
                               destPort(0), dataRate(0.0), totalBytes(0), vcId(0), startTime(0.0),
                               hasExplicitDestPort(false) {}

    /**
     * \brief Constructor with parameters
     */
    FineGrainedTrafficFlow (uint32_t src, uint32_t dst, uint32_t sue, uint32_t port,
                           double rate, uint32_t bytes, uint8_t vc = 0, double start = 0.0)
        : sourceXpuId(src), destXpuId(dst), sueId(sue), suePort(port), destPort(0),
          dataRate(rate), totalBytes(bytes), vcId(vc), startTime(start), hasExplicitDestPort(false) {}
};

/**
 * \brief Traffic generation configuration parameters
 */
struct TrafficConfig
{
    uint32_t transactionSize;     //!< Transaction size (bytes)
    uint32_t maxBurstSize;        //!< Maximum burst size (bytes)
    uint32_t Mtu;                 //!< Device/link MTU including headers
    uint32_t PayloadMtu;          //!< UEC semantic payload MTU for soft_ue_truth
    uint8_t vcNum;                //!< Number of virtual channels
    double threadRate;            //!< Thread rate (Mbps)
    uint32_t totalBytesToSend;    //!< Total bytes to send (MB)
    bool enableTraceMode;         //!< Enable trace-based traffic generation
    std::string traceFilePath;    //!< Path to trace file for trace-based generation

    // New fine-grained traffic control parameters
    bool enableFineGrainedMode;   //!< Enable fine-grained traffic control mode
    std::string fineGrainedConfigFile; //!< Path to fine-grained traffic configuration file
    std::vector<FineGrainedTrafficFlow> fineGrainedFlows; //!< Parsed fine-grained traffic flows
};

/**
 * \brief Link layer configuration parameters
 */
struct LinkConfig
{
    double errorRate;              //!< Error rate
    std::string processingDelay;   //!< Processing delay per packet
    uint8_t numVcs;                //!< Number of link-layer VCs
    std::string LinkDataRate;      //!< Link data rate
    std::string ProcessingRate;    //!< Processing rate
    std::string LinkDelay;         //!< Link propagation delay
};

/**
 * \brief Queue buffer configuration parameters
 */
struct QueueConfig
{
    double vcQueueMaxKB;              //!< VC queue maximum size (KB)
    uint32_t vcQueueMaxBytes;         //!< VC queue max bytes (calculated)
    double processingQueueMaxKB;      //!< Processing queue maximum size (KB)
    uint32_t processingQueueMaxBytes; //!< Processing queue max bytes (calculated)
    double destQueueMaxKB;           //!< Destination queue maximum size (KB)
    uint32_t destQueueMaxBytes;      //!< Destination queue max bytes (calculated)
};

/**
 * \brief CBFC flow control configuration parameters
 */
struct CbfcConfig
{
    bool EnableLinkCBFC;      //!< Link CBFC enable
    uint32_t LinkCredits;     //!< Link layer initial CBFC credits
    uint32_t CreditBatchSize; //!< Credit accumulation threshold
    uint32_t SwitchCredits;   //!< Switch credits
    uint32_t HeaderSize;      //!< Header size (Ethernet + SUE headers)
    uint32_t BaseCredit;      //!< Base credit value for minimum packet

    // Credit-to-byte mapping parameters
    uint32_t BytesPerCredit;  //!< Bytes per credit (default: 256 bytes/credit)
};

/**
 * \brief Load balancing configuration parameters
 */
struct LoadBalanceConfig
{
    uint32_t loadBalanceAlgorithm;  //!< Load balancing algorithm
    uint32_t hashSeed;              //!< Hash seed
    uint32_t prime1;                //!< First prime number for hash algorithms
    uint32_t prime2;                //!< Second prime number for enhanced hash
    bool useVcInHash;               //!< Include VC ID in hash calculation
    bool enableBitOperations;       //!< Enable bit mixing operations
    bool enableAlternativePath;     //!< Enable alternative SUE path search when target is full
};

/**
 * \brief Trace sampling configuration parameters
 */
struct TraceConfig
{
    bool statLoggingEnabled;         //!< Link layer statistics collection enable
    std::string ClientStatInterval; //!< Client statistics interval
};

/**
 * \brief Delay configuration parameters
 */
struct DelayConfig
{
    // Transmitter delays
    std::string SchedulingInterval;              //!< Transmitter scheduler polling interval
    std::string PackingDelayPerPacket;           //!< Packet packing processing time
    std::string destQueueSchedulingDelay;        //!< Destination queue scheduling delay
    std::string transactionClassificationDelay;  //!< Transaction classification delay
    std::string packetCombinationDelay;         //!< Packet combination delay
    std::string ackProcessingDelay;             //!< ACK processing delay

    // Link layer delays
    std::string vcSchedulingDelay;               //!< VC queue scheduling delay
    std::string DataAddHeadDelay;                //!< Data packet header addition delay

    // Credit-related delays
    std::string creditGenerateDelay;            //!< Credit packet generation delay
    std::string CreUpdateAddHeadDelay;           //!< Credit packet header addition delay
    std::string creditReturnProcessingDelay;     //!< Credit return processing delay
    std::string batchCreditAggregationDelay;     //!< Batch credit aggregation delay
    std::string switchForwardDelay;            //!< Switch internal forwarding delay

    // Processing queue delays
    std::string processingQueueScheduleDelay;   //!< Processing queue scheduling delay

    // Capacity reservation parameters
    uint32_t additionalHeaderSize;              //!< Additional header size for capacity reservation
};

/**
 * \brief LLR configuration parameters
 */
struct LlrConfig
{
    bool m_llrEnabled;                    //!< Enable Link Layer Reliability
    std::string LlrTimeout;               //!< LLR timeout value
    uint32_t LlrWindowSize;               //!< LLR window size
    std::string AckAddHeaderDelay;        //!< ACK/NACK header adding delay
    std::string AckProcessDelay;          //!< ACK/NACK processing delay
};

/**
 * \brief Logging configuration parameters
 */
struct LoggingConfig
{
    std::string logLevel;                 //!< Log level for all components (LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, etc.)
    bool enableAllComponents;             //!< Enable logging for all SUE simulation components

    /**
     * \brief Constructor with default values
     */
    LoggingConfig () : logLevel ("LOG_LEVEL_INFO"), enableAllComponents (true) {}
};

/**
 * \brief Main configuration structure containing all sub-configurations
 */
struct SueSimulationConfig
{
    TimingConfig timing;        //!< Timing-related parameters
    NetworkConfig network;      //!< Network topology parameters
    TrafficConfig traffic;      //!< Traffic generation parameters
    LinkConfig link;            //!< Link layer parameters
    QueueConfig queue;          //!< Queue buffer parameters
    CbfcConfig cbfc;            //!< CBFC flow control parameters
    LoadBalanceConfig loadBalance; //!< Load balancing parameters
    TraceConfig trace;          //!< Trace sampling parameters
    DelayConfig delay;          //!< Delay-related parameters
    LlrConfig llr;              //!< Llr related parameters
    LoggingConfig logging;      //!< Logging configuration parameters
    std::string systemScenarioMode; //!< System scenario mode: legacy_sue | soft_ue_truth | soft_ue_fabric
    std::string truthExperimentClass; //!< Truth-backed experiment class: semantic_demo|system_smoke|system_pressure
    std::string truthPlannerMode; //!< Optional truth planner override: auto|uniform|striped
    bool enableSoftUeObserver; //!< Enable SoftUeObserverHelper on truth-backed scenarios
    bool enableSoftUeProtocolLogging; //!< Enable protocol CSV logging via PerformanceLogger
    uint32_t truthFlowCount; //!< Maximum number of truth-backed flows for uniform planner
    uint32_t truthOpsPerFlow; //!< Number of operations per truth flow
    std::string truthPressureProfile; //!< Truth-backed pressure profile: baseline|credit_pressure|lossy|mixed
    uint32_t truthSendPercent; //!< SEND mix percentage for generated truth operations
    uint32_t truthWritePercent; //!< WRITE mix percentage for generated truth operations
    uint32_t truthReadPercent; //!< READ mix percentage for generated truth operations
    uint32_t truthPayloadBytes; //!< Payload size for generated truth operations
    uint32_t truthUnexpectedMessages; //!< Truth-path unexpected message limit
    uint32_t truthUnexpectedBytes; //!< Truth-path unexpected bytes limit
    uint32_t truthArrivalBlocks; //!< Truth-path arrival tracking limit
    uint32_t truthReadTracks; //!< Truth-path read tracking limit
    double truthDropRate; //!< Truth-path packet drop rate for lossy pressure profiles
    uint64_t truthReorderWindowNs; //!< Truth-path reorder hold window in nanoseconds
    uint64_t truthExtraDelayNs; //!< Truth-path extra link delay in nanoseconds
    std::string truthLinkDataRate; //!< Truth-path shared fabric data rate
    uint64_t truthOpSpacingNs; //!< Truth-path generated operation spacing in nanoseconds (0 uses profile default)
    uint32_t truthInitialCredits; //!< Truth-path initial send credits per peer (0 uses soft-ue default)
    uint64_t truthCreditRefreshIntervalNs; //!< Truth-path credit refresh interval in nanoseconds (0 uses default)
    uint32_t truthSendAdmissionMessages; //!< Truth-path SEND admission message budget
    uint64_t truthSendAdmissionBytes; //!< Truth-path SEND admission byte budget
    uint32_t truthWriteBudgetMessages; //!< Truth-path WRITE target-budget message limit
    uint64_t truthWriteBudgetBytes; //!< Truth-path WRITE target-budget byte limit
    uint32_t truthReadResponderMessages; //!< Truth-path READ responder message budget
    uint64_t truthReadResponseBytes; //!< Truth-path READ response byte budget
    uint64_t truthRetryTimeoutNs; //!< Truth-path semantic retry timeout in nanoseconds (0 uses default)
    bool truthRequireFailureEvidence; //!< Require pressure profiles to surface control/resource/packet evidence
    bool truthProtocolCsvRequired; //!< Whether truth-backed runs require protocol CSV data
    std::string fabricTopologyMode; //!< Fabric topology mode: shared_truth | explicit_multipath
    std::string fabricEndpointMode; //!< Fabric endpoint layout: six_endpoint | six_by_six
    uint32_t fabricPathCount; //!< Explicit fabric path count
    std::string fabricPathDataRate; //!< Explicit fabric per-path data rate
    std::string fabricLinkDelay; //!< Explicit fabric path propagation delay
    bool fabricUseEcmpHash; //!< Whether explicit fabric uses stable ECMP hashing
    bool fabricDynamicPathSelection; //!< Whether explicit fabric uses adaptive sticky per-flow path selection
    bool fabricEnableEcnObservation; //!< Whether explicit fabric tracks ECN-style queue threshold crossings
    std::string fabricTrafficPattern; //!< Explicit fabric traffic pattern: all_to_all | hotspot | incast

    /**
     * \brief Constructor with default values
     */
    SueSimulationConfig ();

    /**
     * \brief Parse command line arguments
     * \param argc Argument count
     * \param argv Argument vector
     */
    void ParseCommandLine (int argc, char* argv[]);

    /**
     * \brief Validate and calculate derived parameters
     */
    void ValidateAndCalculate ();

    /**
     * \brief Print configuration information
     */
    void PrintConfiguration () const;

    /**
     * \brief Calculate precise time points
     * \return Client stop time
     */
    double GetClientStop () const;

    /**
     * \brief Calculate precise time points
     * \return Server stop time
     */
    double GetServerStop () const;
};

} // namespace ns3

#endif /* PARAMETER_CONFIG_H */
