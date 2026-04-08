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

/**
 * \file
 * \brief SUE-Sim main simulation program
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sue-sim-module-module.h"
#include "ns3/applications-module.h"
#include "ns3/sue-client.h"
#include "ns3/sue-server.h"
#include "ns3/traffic-generator.h"
#include "ns3/load-balancer.h"
#include "ns3/performance-logger.h"
#include "ns3/parameter-config.h"
#include "ns3/topology-builder.h"
#include "ns3/application-deployer.h"
#include "ns3/common-utils.h"
#include <iomanip>
#include <string>
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SueSimulation");

namespace {

uint32_t
CountCsvLines (const std::string& path)
{
    if (path.empty ())
    {
        return 0;
    }
    std::ifstream in (path);
    uint32_t lines = 0;
    std::string line;
    while (std::getline (in, line))
    {
        ++lines;
    }
    return lines;
}

bool
ProtocolCsvHasEvidence (const std::string& path, const std::string& columnName)
{
    if (path.empty ())
    {
        return false;
    }
    std::ifstream in (path);
    std::string line;
    if (!std::getline (in, line))
    {
        return false;
    }

    std::stringstream headerStream (line);
    std::string token;
    int32_t targetColumn = -1;
    uint32_t headerIndex = 0;
    while (std::getline (headerStream, token, ','))
    {
        if (token == columnName)
        {
            targetColumn = static_cast<int32_t> (headerIndex);
            break;
        }
        ++headerIndex;
    }
    if (targetColumn < 0)
    {
        return false;
    }

    while (std::getline (in, line))
    {
        if (line.empty ())
        {
            continue;
        }
        std::stringstream ss (line);
        uint32_t idx = 0;
        while (std::getline (ss, token, ','))
        {
            if (idx == static_cast<uint32_t> (targetColumn))
            {
                if (!token.empty () && token != "0")
                {
                    return true;
                }
                break;
            }
            ++idx;
        }
    }
    return false;
}

bool
FailureCsvHasDomain (const std::string& path, const std::string& domain)
{
    if (path.empty ())
    {
        return false;
    }
    std::ifstream in (path);
    std::string line;
    uint32_t lineNo = 0;
    while (std::getline (in, line))
    {
        ++lineNo;
        if (lineNo == 1 || line.empty ())
        {
            continue;
        }
        if (line.find ("," + domain + ",") != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

} // namespace

/**
 * \brief Main function for SUE simulation
 * \param argc Argument count
 * \param argv Argument vector
 * \return Exit status
 */
int
main (int argc, char* argv[])
{
    // Initialize timing and performance logging
    std::string sessionId = SueUtils::StartTiming ();
    SueUtils::InitializePerformanceLogger ("performance.csv");

    // === Simulation Parameters Configuration ===
    SueSimulationConfig config;
    config.ParseCommandLine (argc, argv);
    config.ValidateAndCalculate ();

    // Configure logging using parsed parameters
    SueUtils::ConfigureLogging (config);

    config.PrintConfiguration ();

    // Extract simulation time for convenience
    double simulationTime = config.timing.simulationTime;

    // ================= Topology Creation =================
    TopologyBuilder topologyBuilder;
    topologyBuilder.BuildTopology (config);

    // ================= Application Deployment =================
    ApplicationDeployer appDeployer;
    appDeployer.DeployApplications (config, topologyBuilder);

    // ================= Run Simulation =================
    NS_LOG_INFO("Starting SUE Simulation with XPU-Switch Topology");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation completed");

    const bool truthBacked = topologyBuilder.HasSoftUeTruthPath ();
    std::cout << "Scenario summary: mode=" << config.systemScenarioMode
              << " truth_experiment_class=" << config.truthExperimentClass
              << " truth_pressure_profile=" << config.truthPressureProfile
              << " soft_ue_truth_backed=" << (truthBacked ? "true" : "false")
              << std::endl;

    if (truthBacked)
    {
        PerformanceLogger& logger = PerformanceLogger::GetInstance ();
        std::cout << "soft-ue protocol log: "
                  << (logger.GetSoftUeProtocolLogPath ().empty ()
                          ? "(not generated)"
                          : logger.GetSoftUeProtocolLogPath ())
                  << std::endl;
        std::cout << "soft-ue completion log: "
                  << (logger.GetSoftUeCompletionLogPath ().empty ()
                          ? "(not generated)"
                          : logger.GetSoftUeCompletionLogPath ())
                  << std::endl;
        std::cout << "soft-ue failure log: "
                  << (logger.GetSoftUeFailureLogPath ().empty ()
                          ? "(not generated)"
                          : logger.GetSoftUeFailureLogPath ())
                  << std::endl;
        std::cout << "soft-ue diagnostic log: "
                  << (logger.GetSoftUeDiagnosticLogPath ().empty ()
                          ? "(not generated)"
                          : logger.GetSoftUeDiagnosticLogPath ())
                  << std::endl;
        std::cout << "soft-ue tpdc session progress log: "
                  << (logger.GetSoftUeTpdcSessionProgressLogPath ().empty ()
                          ? "(not generated)"
                          : logger.GetSoftUeTpdcSessionProgressLogPath ())
                  << std::endl;

        if (config.truthProtocolCsvRequired)
        {
            NS_ABORT_MSG_IF (CountCsvLines (logger.GetSoftUeProtocolLogPath ()) <= 1,
                             "soft_ue_truth run required protocol CSV output but no data rows were generated");
            NS_ABORT_MSG_IF (CountCsvLines (logger.GetSoftUeCompletionLogPath ()) <= 1,
                             "soft_ue_truth run required completion CSV output but no data rows were generated");
        }

        const auto& matrix = topologyBuilder.GetSoftUeDeviceMatrix ();
        uint64_t creditGateBlocked = 0;
        uint64_t creditRefreshSent = 0;
        uint64_t ackCtrlExtSent = 0;
        uint64_t unexpectedAllocFailures = 0;
        uint64_t arrivalAllocFailures = 0;
        uint64_t readTrackAllocFailures = 0;
        uint64_t tpdcInflightPackets = 0;
        uint64_t tpdcPendingSacks = 0;
        uint64_t tpdcPendingGapNacks = 0;
        uint64_t sackSent = 0;
        uint64_t gapNackSent = 0;
        for (const auto& row : matrix)
        {
            for (Ptr<SoftUeNetDevice> device : row)
            {
                const SoftUeProtocolSnapshot snapshot = device->GetProtocolSnapshot ();
                creditGateBlocked += snapshot.runtime_stats.credit_gate_blocked;
                creditRefreshSent += snapshot.runtime_stats.credit_refresh_sent;
                ackCtrlExtSent += snapshot.runtime_stats.ack_ctrl_ext_sent;
                unexpectedAllocFailures += snapshot.resource_stats.unexpected_alloc_failures;
                arrivalAllocFailures += snapshot.resource_stats.arrival_alloc_failures;
                readTrackAllocFailures += snapshot.resource_stats.read_track_alloc_failures;
                tpdcInflightPackets += snapshot.runtime_stats.tpdc_inflight_packets;
                tpdcPendingSacks += snapshot.runtime_stats.tpdc_pending_sacks;
                tpdcPendingGapNacks += snapshot.runtime_stats.tpdc_pending_gap_nacks;
                sackSent += snapshot.runtime_stats.sack_sent;
                gapNackSent += snapshot.runtime_stats.gap_nack_sent;
            }
        }
        if (!matrix.empty () && !matrix.front ().empty ())
        {
            Ptr<SoftUeNetDevice> sender = matrix.front ().front ();
            const SoftUeProtocolSnapshot snapshot = sender->GetProtocolSnapshot ();
            std::cout << "soft-ue protocol snapshot: "
                      << "node=" << snapshot.node_id
                      << " if=" << snapshot.if_index
                      << " fep=" << snapshot.local_fep
                      << " tx_bytes=" << snapshot.device_stats.totalBytesTransmitted
                      << " rx_bytes=" << snapshot.device_stats.totalBytesReceived
                      << " ops_started=" << snapshot.semantic_stats.ops_started_total
                      << " ops_success=" << snapshot.semantic_stats.ops_success_total
                      << " retries=" << snapshot.runtime_stats.active_retry_states
                      << std::endl;
        }
        if (config.truthExperimentClass == "system_pressure")
        {
            std::cout << "soft-ue pressure summary: "
                      << "truth_pressure_profile=" << config.truthPressureProfile
                      << " credit_gate_blocked=" << creditGateBlocked
                      << " credit_refresh_sent=" << creditRefreshSent
                      << " ack_ctrl_ext_sent=" << ackCtrlExtSent
                      << " unexpected_alloc_failures=" << unexpectedAllocFailures
                      << " tpdc_inflight=" << tpdcInflightPackets
                      << " tpdc_pending_sacks=" << tpdcPendingSacks
                      << " tpdc_pending_gap_nacks=" << tpdcPendingGapNacks
                      << " sack_sent=" << sackSent
                      << " gap_nack_sent=" << gapNackSent
                      << std::endl;

            if (config.truthRequireFailureEvidence)
            {
                const bool controlEvidence =
                    creditGateBlocked > 0 || creditRefreshSent > 0 || ackCtrlExtSent > 0 ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "CreditRefreshSent") ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "AckCtrlExtSent") ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "CreditGateBlocked");
                const bool resourceEvidence =
                    unexpectedAllocFailures > 0 || arrivalAllocFailures > 0 || readTrackAllocFailures > 0 ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "UnexpectedAllocFailures") ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "ArrivalAllocFailures") ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "ReadTrackAllocFailures") ||
                    FailureCsvHasDomain (logger.GetSoftUeFailureLogPath (), "resource");
                const bool packetEvidence =
                    tpdcPendingSacks > 0 || tpdcPendingGapNacks > 0 || sackSent > 0 || gapNackSent > 0 ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "TpdcPendingSacks") ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "TpdcPendingGapNacks") ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "SackSent") ||
                    ProtocolCsvHasEvidence (logger.GetSoftUeProtocolLogPath (), "GapNackSent") ||
                    FailureCsvHasDomain (logger.GetSoftUeFailureLogPath (), "packet_reliability");
                bool evidenceSatisfied = true;
                if (config.truthPressureProfile == "credit_pressure")
                {
                    evidenceSatisfied = controlEvidence;
                }
                else if (config.truthPressureProfile == "lossy")
                {
                    evidenceSatisfied = controlEvidence || packetEvidence;
                }
                else if (config.truthPressureProfile == "mixed")
                {
                    evidenceSatisfied = controlEvidence && (resourceEvidence || packetEvidence);
                }
                NS_ABORT_MSG_IF (!evidenceSatisfied,
                                 "truthRequireFailureEvidence enabled, but system_pressure run did not surface "
                                     "the required pressure evidence for profile "
                                     << config.truthPressureProfile);
            }
        }
        if (matrix.size () > 1 && !matrix[1].empty ())
        {
            Ptr<SoftUeNetDevice> receiver = matrix[1][0];
            if (receiver->HasFailureSnapshot ())
            {
                const SoftUeFailureSnapshot failure = receiver->GetLastFailureSnapshot ();
                std::cout << "soft-ue failure snapshot: "
                          << "job=" << failure.job_id
                          << " msg=" << failure.msg_id
                          << " stage=" << failure.stage
                          << " domain=" << failure.failure_domain
                          << " rc=" << static_cast<uint32_t> (failure.return_code)
                          << std::endl;
            }
        }
    }
    else
    {
        std::cout << "legacy system scenario (not soft-ue truth-backed)" << std::endl;
    }

    // ================= End Timing =================
    SueUtils::EndTiming (sessionId);

    return 0;
}
