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

#include "performance-logger.h"
#include "ns3/simulator.h"
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PerformanceLogger");


void PerformanceLogger::Initialize(const std::string& filename) {
    // Define new directory structure
    std::string baseDir = "performance-data";
    std::string dataDir = baseDir + "/data";

    // Create main directory
    if (access(baseDir.c_str(), F_OK) != 0) {
        if (mkdir(baseDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << baseDir);
        }
    }

    // Create data directory
    if (access(dataDir.c_str(), F_OK) != 0) {
        if (mkdir(dataDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << dataDir);
        }
    }

    const auto now = std::chrono::system_clock::now();
    const auto in_time_t = std::chrono::system_clock::to_time_t(now);
    const auto microsSinceEpoch =
        std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    const auto microsWithinSecond = static_cast<uint64_t>(microsSinceEpoch % 1000000);
    std::tm localTimeSnapshot;
    localtime_r(&in_time_t, &localTimeSnapshot);
    std::stringstream timestamp;
    timestamp << std::put_time(&localTimeSnapshot, "%Y%m%d_%H%M%S")
              << "_" << std::setw(6) << std::setfill('0') << microsWithinSecond
              << "_pid" << getpid();
    m_logTimestamp = timestamp.str ();

    // Throughput data file - separate directory
    std::string throughputLogDir = dataDir + "/throughput_logs";
    if (access(throughputLogDir.c_str(), F_OK) != 0) {
        if (mkdir(throughputLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << throughputLogDir);
        }
    }
    m_filename = throughputLogDir + "/throughput_" + timestamp.str() + ".csv";

    m_file.open(m_filename, std::ios::out | std::ios::trunc);
    if (!m_file.is_open()) {
        NS_FATAL_ERROR("Could not open throughput log file: " << m_filename);
    }
    // Write CSV header
    m_file << "Time,NodeId,DeviceId,VCId,Direction,DataSize\n";

    // Packing delay log file - separate directory
    std::string waitTimeLogDir = dataDir + "/pack_wait_time_logs";
    if (access(waitTimeLogDir.c_str(), F_OK) != 0) {
        if (mkdir(waitTimeLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << waitTimeLogDir);
        }
    }
    std::ostringstream packDelayFilename;
    packDelayFilename << waitTimeLogDir << "/pack_wait_time_" << timestamp.str() << ".csv";
    m_packDelayLog.open(packDelayFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_packDelayLog.is_open()) {
        NS_FATAL_ERROR("Could not open pack delay log file: " << packDelayFilename.str());
    }
    m_packDelayLog << "XpuId,SueId,DestXpuId,VcId,WaitTime(ns)" << std::endl; // CSV header

    // Packing quantity log file - separate directory
    std::string packNumLogDir = dataDir + "/pack_num_logs";
    if (access(packNumLogDir.c_str(), F_OK) != 0) {
        if (mkdir(packNumLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << packNumLogDir);
        }
    }
    std::ostringstream packNumFilename;
    packNumFilename << packNumLogDir << "/pack_num_" << timestamp.str() << ".csv";
    m_packNumLog.open(packNumFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_packNumLog.is_open()) {
        NS_FATAL_ERROR("Could not open pack num log file: " << packNumFilename.str());
    }
    m_packNumLog << "XpuId,SueId,DestXpuId,VcId,PackNums" << std::endl; // CSV header

    // LoadBalancer log file - separate directory
    std::string loadBalanceLogDir = dataDir + "/load_balance_logs";
    if (access(loadBalanceLogDir.c_str(), F_OK) != 0) {
        if (mkdir(loadBalanceLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << loadBalanceLogDir);
        }
    }
    std::ostringstream loadBalanceFilename;
    loadBalanceFilename << loadBalanceLogDir << "/load_balance_" << timestamp.str() << ".csv";
    m_loadBalanceLog.open(loadBalanceFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_loadBalanceLog.is_open()) {
        NS_FATAL_ERROR("Could not open load balance log file: " << loadBalanceFilename.str());
    }
    m_loadBalanceLog << "LocalXpuId,DestXpuId,VcId,SueId" << std::endl; // CSV header

    // Destination queue utilization log file - separate directory
    std::string destQueueLogDir = dataDir + "/destination_queue_logs";
    if (access(destQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(destQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << destQueueLogDir);
        }
    }
    std::ostringstream destQueueFilename;
    destQueueFilename << destQueueLogDir << "/destination_queue_" << timestamp.str() << ".csv";
    m_destinationQueueLog.open(destQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_destinationQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open destination queue log file: " << destQueueFilename.str());
    }
    m_destinationQueueLog << "TimeNs,XpuId,SueId,DestXpuId,VcId,CurrentSize,MaxSize,Utilization(%)" << std::endl;

    
    // Main queue utilization log file - separate directory
    std::string mainQueueLogDir = dataDir + "/main_queue_logs";
    if (access(mainQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(mainQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << mainQueueLogDir);
        }
    }
    std::ostringstream mainQueueFilename;
    mainQueueFilename << mainQueueLogDir << "/main_queue_" << timestamp.str() << ".csv";
    m_mainQueueLog.open(mainQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_mainQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open main queue log file: " << mainQueueFilename.str());
    }
    m_mainQueueLog << "TimeNs,NodeId,DeviceId,CurrentSize,MaxSize,Utilization(%)" << std::endl;

    // VC queue utilization log file - separate directory
    std::string vcQueueLogDir = dataDir + "/vc_queue_logs";
    if (access(vcQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(vcQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << vcQueueLogDir);
        }
    }
    std::ostringstream vcQueueFilename;
    vcQueueFilename << vcQueueLogDir << "/vc_queue_" << timestamp.str() << ".csv";
    m_vcQueueLog.open(vcQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_vcQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open VC queue log file: " << vcQueueFilename.str());
    }
    m_vcQueueLog << "TimeNs,NodeId,DeviceId,VCId,CurrentSize,MaxSize,Utilization(%)" << std::endl;

    // Link layer processing queue utilization log file - separate directory
    std::string processingQueueLogDir = dataDir + "/processing_queue_logs";
    if (access(processingQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(processingQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << processingQueueLogDir);
        }
    }
    std::ostringstream processingQueueFilename;
    processingQueueFilename << processingQueueLogDir << "/processing_queue_" << timestamp.str() << ".csv";
    m_processingQueueLog.open(processingQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_processingQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open processing queue log file: " << processingQueueFilename.str());
    }
    m_processingQueueLog << "TimeNs,NodeId,DeviceId,QueueLength,MaxSize,Utilization(%)" << std::endl;

    // XPU delay monitoring log file - separate directory
    std::string xpuDelayLogDir = dataDir + "/xpu_delay_logs";
    if (access(xpuDelayLogDir.c_str(), F_OK) != 0) {
        if (mkdir(xpuDelayLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << xpuDelayLogDir);
        }
    }
    std::ostringstream xpuDelayFilename;
    xpuDelayFilename << xpuDelayLogDir << "/xpu_delay_" << timestamp.str() << ".csv";
    m_xpuDelayLog.open(xpuDelayFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_xpuDelayLog.is_open()) {
        NS_FATAL_ERROR("Could not open XPU delay log file: " << xpuDelayFilename.str());
    }
    m_xpuDelayLog << "TimeNs,NodeId,PortId,Delay(ns),Location" << std::endl;

    // SUE buffer queue monitoring log file - separate directory
    std::string sueBufferQueueLogDir = dataDir + "/sue_buffer_queue_logs";
    if (access(sueBufferQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(sueBufferQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << sueBufferQueueLogDir);
        }
    }
    std::ostringstream sueBufferQueueFilename;
    sueBufferQueueFilename << sueBufferQueueLogDir << "/sue_buffer_queue_" << timestamp.str() << ".csv";
    m_sueBufferQueueLog.open(sueBufferQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_sueBufferQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open SUE buffer queue log file: " << sueBufferQueueFilename.str());
    }
    m_sueBufferQueueLog << "TimeNs,XpuId,BufferSize" << std::endl;

    // Link layer credit monitoring log file - separate directory
    std::string linkCreditLogDir = dataDir + "/link_credit_logs";
    if (access(linkCreditLogDir.c_str(), F_OK) != 0) {
        if (mkdir(linkCreditLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << linkCreditLogDir);
        }
    }
    std::ostringstream linkCreditFilename;
    linkCreditFilename << linkCreditLogDir << "/link_credit_" << timestamp.str() << ".csv";
    m_linkCreditLog.open(linkCreditFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_linkCreditLog.is_open()) {
        NS_FATAL_ERROR("Could not open link credit log file: " << linkCreditFilename.str());
    }
    m_linkCreditLog << "TimeNs,NodeId,DeviceId,VCId,Direction,Credits,MacAddress" << std::endl;

    // Initialize event-driven packet drop log file
    std::string dropLogDir = "performance-data/data/drop_logs";
    if (access(dropLogDir.c_str(), F_OK) != 0) {
        if (mkdir(dropLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << dropLogDir);
        }
    }
    std::ostringstream dropFilename;
    dropFilename << dropLogDir << "/packet_drop_" << timestamp.str() << ".csv";
    m_dropLog.open(dropFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_dropLog.is_open()) {
        NS_FATAL_ERROR("Could not open packet drop log file: " << dropFilename.str());
    }
    m_dropLog << "TimeNs,NodeId,DeviceId,VCId,DropReason,PacketSize,QueueSize" << std::endl;

    // Create application layer transmission log directory
    std::string appLayerTxLogDir = dataDir + "/app_layer_tx";
    if (access(appLayerTxLogDir.c_str(), F_OK) != 0) {
        if (mkdir(appLayerTxLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << appLayerTxLogDir);
        }
    }
    std::stringstream appLayerTxFilename;
    appLayerTxFilename << appLayerTxLogDir << "/app_layer_tx_" << timestamp.str() << ".csv";
    m_appLayerTxLog.open(appLayerTxFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_appLayerTxLog.is_open()) {
        NS_FATAL_ERROR("Could not open application layer transmission log file: " << appLayerTxFilename.str());
    }
    m_appLayerTxLog << "TimeNs,NodeId,VcId,PacketSize" << std::endl;

    // Optional: Output debug information to standard output
    // std::cout << "PerformanceLogger initialized with directories:" << std::endl;
    // std::cout << "  Performance logs: " << performanceLogDir << std::endl;
    // std::cout << "  Packing logs: " << packingLogDir << std::endl;
    // std::cout << "  Main data file: " << m_filename << std::endl;
}

void
PerformanceLogger::EnsureSoftUeLogsOpen ()
{
  if (m_softUeProtocolLog.is_open () && m_softUeCompletionLog.is_open () &&
      m_softUeFailureLog.is_open () && m_softUeDiagnosticLog.is_open () &&
      m_softUeTpdcSessionProgressLog.is_open ())
    {
      return;
    }
  if (m_logTimestamp.empty ())
    {
      return;
    }

  const std::string dataDir = "performance-data/data";

  std::string softUeProtocolDir = dataDir + "/soft_ue_protocol_logs";
  if (access (softUeProtocolDir.c_str (), F_OK) != 0)
    {
      if (mkdir (softUeProtocolDir.c_str (), 0777) != 0)
        {
          NS_FATAL_ERROR ("Failed to create directory: " << softUeProtocolDir);
        }
    }
  m_softUeProtocolLogPath = softUeProtocolDir + "/soft_ue_protocol_" + m_logTimestamp + ".csv";
  m_softUeProtocolLog.open (m_softUeProtocolLogPath, std::ios::out | std::ios::trunc);
  if (!m_softUeProtocolLog.is_open ())
    {
      NS_FATAL_ERROR ("Could not open soft-ue protocol log file: " << m_softUeProtocolLogPath);
    }
  m_softUeProtocolLog
      << "TimeNs,NodeId,IfIndex,LocalFep,TxBytes,RxBytes,TxPackets,RxPackets,DroppedPackets,"
      << "ActivePdcCount,AverageLatencyMs,ThroughputMbps,OpsStartedTotal,OpsTerminalTotal,"
      << "OpsSuccessTotal,OpsFailedTotal,OpsInFlight,SendStartedTotal,WriteStartedTotal,"
      << "ReadStartedTotal,SendSuccessTotal,WriteSuccessTotal,ReadSuccessTotal,"
      << "SendSuccessBytesTotal,WriteSuccessBytesTotal,ReadSuccessBytesTotal,"
      << "SuccessLatencySamples,SuccessLatencyMeanNs,SuccessLatencyMaxNs,ArrivalBlocksInUse,"
      << "UnexpectedMsgsInUse,UnexpectedBytesInUse,ReadTrackInUse,UnexpectedAllocFailures,"
      << "ArrivalAllocFailures,ReadTrackAllocFailures,StaleCleanupCount,ActiveRetryStates,"
      << "ActiveReadResponseStates,UnexpectedBufferedInUse,UnexpectedSemanticAcceptedInUse,"
      << "UnexpectedPartialInUse,TpdcInflightPackets,TpdcOutOfOrderPackets,TpdcPendingSacks,"
      << "TpdcPendingGapNacks,ActiveTpdcSessions,CreditRefreshSent,AckCtrlExtSent,"
      << "CreditGateBlocked,LegacyCreditGateBlocked,SendAdmissionBlockedTotal,SendAdmissionMessageLimitBlockedTotal,"
      << "SendAdmissionByteLimitBlockedTotal,SendAdmissionBothLimitBlockedTotal,WriteBudgetBlockedTotal,"
      << "ReadResponderBlockedTotal,TransportWindowBlockedTotal,SendAdmissionMessagesInUsePeak,"
      << "SendAdmissionBytesInUsePeak,WriteBudgetMessagesInUsePeak,WriteBudgetBytesInUsePeak,"
      << "ReadResponderMessagesInUsePeak,ReadResponseBudgetBytesInUsePeak,"
      << "BlockedQueuePushTotal,BlockedQueueWakeupTotal,"
      << "BlockedQueueDispatchTotal,BlockedQueueDepthMax,PeerQueueBlockedTotal,PendingResponseEnqueueTotal,"
      << "BlockedQueueWaitRecordedTotal,BlockedQueueWaitTotalNs,BlockedQueueWaitPeakNs,"
      << "BlockedQueueWakeupDelayRecordedTotal,BlockedQueueWakeupDelayTotalNs,BlockedQueueWakeupDelayPeakNs,"
      << "SendAdmissionReleaseCount,SendAdmissionReleaseBytesTotal,"
      << "BlockedQueueRedispatchFailAfterWakeupTotal,BlockedQueueRedispatchSuccessAfterWakeupTotal,"
      << "PendingResponseRetryTotal,PendingResponseDispatchFailuresTotal,"
      << "PendingResponseSuccessAfterRetryTotal,StaleTimeoutSkippedTotal,"
      << "StaleRetrySkippedTotal,LateResponseObservedTotal,SendDuplicateOkAfterTerminalTotal,SendDispatchStartedTotal,"
      << "SendResponseOkLiveTotal,SendResponseNonOkLiveTotal,"
      << "SendResponseMissingLiveRequestTotal,SendResponseAfterTerminalTotal,"
      << "SendTimeoutWithoutResponseTotal,SendTimeoutRetryWithoutResponseProgressTotal,"
      << "SackSent,GapNackSent,FabricPathCount,FabricTotalTxBytes,FabricTotalTxPackets,"
      << "FabricEcnMarkTotal,FabricTopHotspotEndpoint,FabricDynamicRoutingEnabled,"
      << "FabricDynamicAssignmentTotal,FabricDynamicPathReuseTotal,FabricActiveFlowAssignmentsPeak,"
      << "FabricPathScoreMinNs,FabricPathScoreMaxNs,FabricPathScoreMeanNs,"
      << "FabricPath0TxBytes,FabricPath1TxBytes,FabricPath2TxBytes,FabricPath3TxBytes,"
      << "FabricPath4TxBytes,FabricPath5TxBytes,FabricPath6TxBytes,FabricPath7TxBytes,"
      << "FabricPath0QueueDepthMax,FabricPath1QueueDepthMax,FabricPath2QueueDepthMax,FabricPath3QueueDepthMax,"
      << "FabricPath4QueueDepthMax,FabricPath5QueueDepthMax,FabricPath6QueueDepthMax,FabricPath7QueueDepthMax,"
      << "FabricPath0QueueDepthMeanMilli,FabricPath1QueueDepthMeanMilli,FabricPath2QueueDepthMeanMilli,FabricPath3QueueDepthMeanMilli,"
      << "FabricPath4QueueDepthMeanMilli,FabricPath5QueueDepthMeanMilli,FabricPath6QueueDepthMeanMilli,FabricPath7QueueDepthMeanMilli,"
      << "FabricPath0FlowCount,FabricPath1FlowCount,FabricPath2FlowCount,FabricPath3FlowCount,"
      << "FabricPath4FlowCount,FabricPath5FlowCount,FabricPath6FlowCount,FabricPath7FlowCount" << std::endl;

  std::string softUeCompletionDir = dataDir + "/soft_ue_completion_logs";
  if (access (softUeCompletionDir.c_str (), F_OK) != 0)
    {
      if (mkdir (softUeCompletionDir.c_str (), 0777) != 0)
        {
          NS_FATAL_ERROR ("Failed to create directory: " << softUeCompletionDir);
        }
    }
  m_softUeCompletionLogPath = softUeCompletionDir + "/soft_ue_completion_" + m_logTimestamp + ".csv";
  m_softUeCompletionLog.open (m_softUeCompletionLogPath, std::ios::out | std::ios::trunc);
  if (!m_softUeCompletionLog.is_open ())
    {
      NS_FATAL_ERROR ("Could not open soft-ue completion log file: " << m_softUeCompletionLogPath);
    }
  m_softUeCompletionLog
      << "TimeNs,NodeId,IfIndex,LocalFep,JobId,MsgId,PeerFep,Opcode,Success,ReturnCode,"
      << "FailureDomain,TerminalReason,PayloadBytes,LatencyNs,RetryCount,"
      << "SendStageValid,SendStageDispatchNs,SendStageDispatchWaitForAdmissionNs,"
      << "SendStageDispatchAfterAdmissionToFirstSendNs,SendStageInflightNs,"
      << "SendStageReceiveConsumeNs,SendStageCloseoutNs,SendStageEndToEndNs,"
      << "SendStageDispatchAttemptCount,SendStageDispatchBudgetBlockCount,"
      << "SendStageBlockedQueueEnqueueCount,SendStageBlockedQueueRedispatchCount,"
      << "SendStageBlockedQueueWaitTotalNs,SendStageBlockedQueueWaitPeakNs,"
      << "SendStageAdmissionReleaseSeenCount,SendStageBlockedQueueWakeupCount,"
      << "SendStageRedispatchFailAfterWakeupCount,SendStageRedispatchSuccessAfterWakeupCount,"
      << "SendStageAdmissionReleaseToWakeupTotalNs,SendStageAdmissionReleaseToWakeupPeakNs,"
      << "SendStageWakeupToRedispatchTotalNs,SendStageWakeupToRedispatchPeakNs,"
      << "ReadStageValid,ReadStageResponderBudgetGenerateNs,ReadStagePendingResponseQueueDispatchNs,"
      << "ReadStageResponseFirstPacketTxNs,ReadStageNetworkReturnVisibilityNs,"
      << "ReadStageFirstResponseVisibleNs,ReadStageReassemblyCompleteNs,ReadStageTerminalNs,"
      << "ReadStageEndToEndNs,"
      << "ReadPreAdmissionTracked,ReadPreContextAllocatedRetry,ReadPreTerminalResourceExhaust,"
      << "ReadPreFirstPacketNoContextCount,ReadPreArrivalReserveFailCount,"
      << "ReadPreTargetRegisteredAtNs,ReadPreFirstPacketNoContextAtNs,"
      << "ReadPreArrivalBlockReservedAtNs,ReadPreContextAllocatedRetryAtNs,"
      << "ReadPreTargetReleasedAtNs,ReadPreArrivalContextReleasedAtNs,"
      << "ReadPreTargetToFirstNoContextNs,ReadPreTargetToArrivalReservedNs,"
      << "ReadPreFirstNoContextToArrivalReservedNs,"
      << "ReadPreArrivalReservedToFirstResponseVisibleNs,"
      << "ReadPreTargetHoldToReleaseNs,ReadPreArrivalHoldToReleaseNs,"
      << "ReadPreTargetToTerminalResourceExhaustNs,"
      << "ReadPreFirstNoContextToTerminalResourceExhaustNs,"
      << "ReadRecoveryValid,ReadRecoveryTracked,ReadRecoveryMissingChunkIndex,"
      << "ReadRecoveryMissingTransportSeq,ReadRecoveryGapDetectedAtNs,"
      << "ReadRecoveryGapNackSentAtNs,ReadRecoveryGapNackObservedAtSenderNs,"
      << "ReadRecoveryMissingFragmentRetransmitTxAtNs,"
      << "ReadRecoveryMissingFragmentFirstVisibleAtNs,ReadRecoveryReassemblyUnblockedAtNs,"
      << "ReadRecoveryGapNackSentCount,ReadRecoveryGapNackObservedAtSenderCount,"
      << "ReadRecoveryMissingFragmentRetransmitCount,"
      << "ReadRecoveryDetectToNackNs,ReadRecoveryNackToObservedAtSenderNs,"
      << "ReadRecoveryObservedAtSenderToRetransmitNs,ReadRecoveryNackToRetransmitNs,"
      << "ReadRecoveryRetransmitToFirstVisibleNs,ReadRecoveryFirstVisibleToUnblockedNs,"
      << "ReadRecoveryDetectToUnblockedNs,ReadRecoveryRetransmitToTerminalNs,"
      << "ReadRecoveryDetectToTerminalNs" << std::endl;

  std::string softUeFailureDir = dataDir + "/soft_ue_failure_logs";
  if (access (softUeFailureDir.c_str (), F_OK) != 0)
    {
      if (mkdir (softUeFailureDir.c_str (), 0777) != 0)
        {
          NS_FATAL_ERROR ("Failed to create directory: " << softUeFailureDir);
        }
    }
  m_softUeFailureLogPath = softUeFailureDir + "/soft_ue_failure_" + m_logTimestamp + ".csv";
  m_softUeFailureLog.open (m_softUeFailureLogPath, std::ios::out | std::ios::trunc);
  if (!m_softUeFailureLog.is_open ())
    {
      NS_FATAL_ERROR ("Could not open soft-ue failure log file: " << m_softUeFailureLogPath);
    }
  m_softUeFailureLog
      << "TimeNs,NodeId,IfIndex,LocalFep,JobId,MsgId,PeerFep,Opcode,Stage,FailureDomain,"
      << "ReturnCode,Terminalized,TerminalReason,RetryCount,PendingPsns,UnexpectedPresent,"
      << "UnexpectedAccepted,UnexpectedMatched,UnexpectedChunksDone,UnexpectedExpectedChunks,"
      << "DiagnosticText" << std::endl;

  std::string softUeDiagnosticDir = dataDir + "/soft_ue_diagnostic_logs";
  if (access (softUeDiagnosticDir.c_str (), F_OK) != 0)
    {
      if (mkdir (softUeDiagnosticDir.c_str (), 0777) != 0)
        {
          NS_FATAL_ERROR ("Failed to create directory: " << softUeDiagnosticDir);
        }
    }
  m_softUeDiagnosticLogPath = softUeDiagnosticDir + "/soft_ue_diagnostic_" + m_logTimestamp + ".csv";
  m_softUeDiagnosticLog.open (m_softUeDiagnosticLogPath, std::ios::out | std::ios::trunc);
  if (!m_softUeDiagnosticLog.is_open ())
    {
      NS_FATAL_ERROR ("Could not open soft-ue diagnostic log file: " << m_softUeDiagnosticLogPath);
    }
  m_softUeDiagnosticLog << "TimeNs,NodeId,IfIndex,LocalFep,Name,Detail" << std::endl;

  std::string softUeTpdcSessionProgressDir = dataDir + "/soft_ue_tpdc_session_progress_logs";
  if (access (softUeTpdcSessionProgressDir.c_str (), F_OK) != 0)
    {
      if (mkdir (softUeTpdcSessionProgressDir.c_str (), 0777) != 0)
        {
          NS_FATAL_ERROR ("Failed to create directory: " << softUeTpdcSessionProgressDir);
        }
    }
  m_softUeTpdcSessionProgressLogPath =
      softUeTpdcSessionProgressDir + "/soft_ue_tpdc_session_progress_" + m_logTimestamp + ".csv";
  m_softUeTpdcSessionProgressLog.open (m_softUeTpdcSessionProgressLogPath,
                                       std::ios::out | std::ios::trunc);
  if (!m_softUeTpdcSessionProgressLog.is_open ())
    {
      NS_FATAL_ERROR ("Could not open soft-ue TPDC session progress log file: "
                      << m_softUeTpdcSessionProgressLogPath);
    }
  m_softUeTpdcSessionProgressLog
      << "TimeNs,NodeId,IfIndex,LocalFep,RemoteFep,PdcId,TxDataPacketsTotal,"
      << "TxControlPacketsTotal,RxDataPacketsTotal,RxControlPacketsTotal,AckSentTotal,"
      << "AckReceivedTotal,GapNackReceivedTotal,LastAckSequence,SendWindowBase,"
      << "NextSendSequence,SendBufferSizeCurrent,SendBufferSizeMax,RetransmissionsTotal,"
      << "RtoTimeoutsTotal,AckAdvanceEventsTotal" << std::endl;
}

void PerformanceLogger::LogDropStat(int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                   const std::string& direction, uint32_t count) {
    if (m_file.is_open()) {
        m_file << nanoTime << "," << XpuId << "," << devId << "," << static_cast<int>(vcId)
               << "," << direction << "," << count << "\n";
        m_file.flush(); // Ensure data is written to disk
    }
}


// === EVENT-DRIVEN STATISTICS FUNCTIONS ===

void PerformanceLogger::LogPacketTx(int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                   const std::string& direction, uint32_t packetSizeBits) {
    if (m_file.is_open()) {
        // Log individual packet transmission (event-driven)
        m_file << nanoTime << "," << XpuId << "," << devId << "," << static_cast<int>(vcId)
               << "," << direction << "," << packetSizeBits << "\n";  // Use packet size as DataSize
        m_file.flush(); // Ensure data is written to disk immediately
    }
}

void PerformanceLogger::LogPacketRx(int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                   const std::string& direction, uint32_t packetSizeBits) {
    if (m_file.is_open()) {
        // Log individual packet reception (event-driven)
        m_file << nanoTime << "," << XpuId << "," << devId << "," << static_cast<int>(vcId)
               << "," << direction << "," << packetSizeBits << "\n";  // Use packet size as DataSize
        m_file.flush(); // Ensure data is written to disk immediately
    }
}

void PerformanceLogger::LogAppStat(int64_t nanoTime, uint32_t xpuId, uint32_t devId, uint8_t vcId, double rate) {
    if (m_file.is_open()) {
        m_file << nanoTime << "," << xpuId << "," << devId  << "," << static_cast<int>(vcId)
               <<",APP," << rate << "\n";
        m_file.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogCreditStat(int64_t nanoTime, uint32_t NodeId, uint32_t devId, uint8_t vcId,
                                    const std::string& direction, uint32_t credits, const std::string& macAddress) {
    // Write independent link layer credit log file
    if (m_linkCreditLog.is_open()) {
        m_linkCreditLog << nanoTime << "," << NodeId << "," << devId << ","
                << static_cast<int>(vcId) << "," << direction << "," << credits << "," << macAddress << "\n";
        m_linkCreditLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogPackDelay(uint32_t xpuId, uint32_t sueId, uint32_t destXpuId,
                                     uint8_t vcId, int64_t waitTimeNs) {
    if (m_packDelayLog.is_open()) {
        m_packDelayLog << xpuId << "," << sueId << ","
                      << destXpuId << "," << static_cast<int>(vcId) << "," << waitTimeNs << std::endl;
        m_packDelayLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogPackNum(uint32_t xpuId, uint32_t sueId, uint32_t destXpuId,
                                  uint8_t vcId, uint32_t packNums) {
    if (m_packNumLog.is_open()) {
        m_packNumLog << xpuId << "," << sueId << ","
                    << destXpuId << "," << static_cast<int>(vcId) << "," << packNums << std::endl;
        m_packNumLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogLoadBalance(uint32_t localXpuId, uint32_t destXpuId, uint8_t vcId, uint32_t sueId) {
    if (m_loadBalanceLog.is_open()) {
        m_loadBalanceLog << localXpuId << "," << destXpuId << "," << static_cast<int>(vcId) << "," << sueId << std::endl;
        m_loadBalanceLog.flush(); // Ensure data is written to disk
    }
}

    // Queue utilization monitoring method implementation
void PerformanceLogger::LogDestinationQueueUsage(uint64_t timeNs, uint32_t xpuId, uint32_t sueId,
                                                   uint32_t destXpuId, uint8_t vcId, uint32_t currentBytes, uint32_t maxBytes) {
    if (m_destinationQueueLog.is_open()) {
        double utilization = (maxBytes > 0) ? (static_cast<double>(currentBytes) / maxBytes * 100.0) : 0.0;
        m_destinationQueueLog << timeNs << "," << xpuId << "," << sueId << ","
                             << destXpuId << "," << static_cast<int>(vcId) << "," << currentBytes << "," << maxBytes << ","
                             << std::fixed << std::setprecision(2) << utilization << std::endl;
        m_destinationQueueLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogMainQueueUsage(uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                         uint32_t currentSize, uint32_t maxSize) {
    if (m_mainQueueLog.is_open()) {
        double utilization = (maxSize > 0) ? (static_cast<double>(currentSize) / maxSize * 100.0) : 0.0;
        m_mainQueueLog << timeNs << "," << nodeId << "," << deviceId << ","
                      << currentSize << "," << maxSize << ","
                      << std::fixed << std::setprecision(2) << utilization << std::endl;
        m_mainQueueLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogVCQueueUsage(uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                       uint8_t vcId, uint32_t currentSize, uint32_t maxSize) {
    if (m_vcQueueLog.is_open()) {
        double utilization = (maxSize > 0) ? (static_cast<double>(currentSize) / maxSize * 100.0) : 0.0;
        m_vcQueueLog << timeNs << "," << nodeId << "," << deviceId << ","
                    << static_cast<int>(vcId) << "," << currentSize << "," << maxSize << ","
                    << std::fixed << std::setprecision(2) << utilization << std::endl;
        m_vcQueueLog.flush(); // Ensure data is written to disk
    }
}


    // Link layer processing queue monitoring method implementation
void PerformanceLogger::LogProcessingQueueUsage(uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                                 uint32_t currentSize, uint32_t maxSize) {
    if (m_processingQueueLog.is_open()) {
        double utilization = (maxSize > 0) ? (static_cast<double>(currentSize) / maxSize * 100.0) : 0.0;
        m_processingQueueLog << timeNs << "," << nodeId << "," << deviceId << ","
                            << currentSize << "," << maxSize << ","
                            << std::fixed << std::setprecision(2) << utilization << std::endl;
        m_processingQueueLog.flush(); // Ensure data is written to disk
    }
}

    // XPU delay statistics method implementation
void PerformanceLogger::LogXpuDelay(uint64_t timeNs, uint32_t xpuId, uint32_t portId, double delayNs) {
    if (m_xpuDelayLog.is_open()) {
        m_xpuDelayLog << timeNs << "," << xpuId << "," << portId << ","
                      << std::fixed << std::setprecision(3) << delayNs << "," << std::endl;
        m_xpuDelayLog.flush(); // Ensure data is written to disk
    }
}

// XPU delay statistics method with location implementation
void PerformanceLogger::LogXpuDelay(uint64_t timeNs, uint32_t xpuId, uint32_t portId, double delayNs, const std::string& location) {
    if (m_xpuDelayLog.is_open()) {
        m_xpuDelayLog << timeNs << "," << xpuId << "," << portId << ","
                      << std::fixed << std::setprecision(3) << delayNs << "," << location << std::endl;
        m_xpuDelayLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogAppLayerTx(uint64_t timeNs, uint32_t nodeId, uint8_t vcId, uint32_t packetSize) {
    if (m_appLayerTxLog.is_open()) {
        m_appLayerTxLog << timeNs << "," << nodeId << "," << static_cast<int>(vcId) << "," << packetSize << std::endl;
        m_appLayerTxLog.flush(); // Ensure data is written to disk
    }
}

PerformanceLogger::~PerformanceLogger() {
    if (m_file.is_open()) {
        m_file.close();
    }
    if (m_packDelayLog.is_open()) {
        m_packDelayLog.close();
    }
    if (m_packNumLog.is_open()) {
        m_packNumLog.close();
    }
    if (m_loadBalanceLog.is_open()) {
        m_loadBalanceLog.close();
    }
    if (m_destinationQueueLog.is_open()) {
        m_destinationQueueLog.close();
    }
        if (m_mainQueueLog.is_open()) {
        m_mainQueueLog.close();
    }
    if (m_vcQueueLog.is_open()) {
        m_vcQueueLog.close();
    }
    if (m_processingQueueLog.is_open()) {
        m_processingQueueLog.close();
    }
    if (m_xpuDelayLog.is_open()) {
        m_xpuDelayLog.close();
    }
    if (m_dropLog.is_open()) {
        m_dropLog.close();
    }
    if (m_sueBufferQueueLog.is_open()) {
        m_sueBufferQueueLog.close();
    }
    if (m_linkCreditLog.is_open()) {
        m_linkCreditLog.close();
    }
    if (m_appLayerTxLog.is_open()) {
        m_appLayerTxLog.close();
    }
    if (m_softUeProtocolLog.is_open()) {
        m_softUeProtocolLog.close();
    }
    if (m_softUeCompletionLog.is_open()) {
        m_softUeCompletionLog.close();
    }
    if (m_softUeFailureLog.is_open()) {
        m_softUeFailureLog.close();
    }
    if (m_softUeDiagnosticLog.is_open()) {
        m_softUeDiagnosticLog.close();
    }
}

void
PerformanceLogger::LogPacketDrop (int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                  const std::string& dropReason, uint32_t packetSize)
{
  NS_LOG_FUNCTION (nanoTime << XpuId << devId << (uint32_t)vcId << dropReason << packetSize);

  // Directly write packet drop data (event-driven)
  if (m_dropLog.is_open()) {
      m_dropLog << nanoTime << "," << XpuId << "," << devId << "," << (uint32_t)vcId << ","
                << dropReason << "," << packetSize << std::endl;
      m_dropLog.flush(); // Ensure data is written to disk
  }
}

void
PerformanceLogger::BufferQueueChangeTraceCallback (uint32_t bufferSize, uint32_t xpuId)
{
  NS_LOG_FUNCTION (this << bufferSize << xpuId);

  uint64_t timeNs = Simulator::Now ().GetNanoSeconds ();

  // Directly write buffer queue data
  if (m_sueBufferQueueLog.is_open()) {
      m_sueBufferQueueLog << timeNs << "," << xpuId << "," << bufferSize << std::endl;
      m_sueBufferQueueLog.flush(); // Ensure data is written to disk
  }
}

void
PerformanceLogger::LogSoftUeProtocolSnapshot (const SoftUeProtocolSnapshot& snapshot)
{
  EnsureSoftUeLogsOpen ();
  if (!m_softUeProtocolLog.is_open()) {
      return;
  }

  m_softUeProtocolLog
      << snapshot.timestamp_ns << ","
      << snapshot.node_id << ","
      << snapshot.if_index << ","
      << snapshot.local_fep << ","
      << snapshot.device_stats.totalBytesTransmitted << ","
      << snapshot.device_stats.totalBytesReceived << ","
      << snapshot.device_stats.totalPacketsTransmitted << ","
      << snapshot.device_stats.totalPacketsReceived << ","
      << snapshot.device_stats.droppedPackets << ","
      << snapshot.device_stats.activePdcCount << ","
      << snapshot.device_stats.averageLatency << ","
      << snapshot.device_stats.throughput << ","
      << snapshot.semantic_stats.ops_started_total << ","
      << snapshot.semantic_stats.ops_terminal_total << ","
      << snapshot.semantic_stats.ops_success_total << ","
      << snapshot.semantic_stats.ops_failed_total << ","
      << snapshot.semantic_stats.ops_in_flight << ","
      << snapshot.semantic_stats.send_started_total << ","
      << snapshot.semantic_stats.write_started_total << ","
      << snapshot.semantic_stats.read_started_total << ","
      << snapshot.semantic_stats.send_success_total << ","
      << snapshot.semantic_stats.write_success_total << ","
      << snapshot.semantic_stats.read_success_total << ","
      << snapshot.semantic_stats.send_success_bytes_total << ","
      << snapshot.semantic_stats.write_success_bytes_total << ","
      << snapshot.semantic_stats.read_success_bytes_total << ","
      << snapshot.semantic_stats.success_latency_samples << ","
      << snapshot.semantic_stats.success_latency_mean_ns << ","
      << snapshot.semantic_stats.success_latency_max_ns << ","
      << snapshot.resource_stats.arrival_blocks_in_use << ","
      << snapshot.resource_stats.unexpected_msgs_in_use << ","
      << snapshot.resource_stats.unexpected_bytes_in_use << ","
      << snapshot.resource_stats.read_track_in_use << ","
      << snapshot.resource_stats.unexpected_alloc_failures << ","
      << snapshot.resource_stats.arrival_alloc_failures << ","
      << snapshot.resource_stats.read_track_alloc_failures << ","
      << snapshot.resource_stats.stale_cleanup_count << ","
      << snapshot.runtime_stats.active_retry_states << ","
      << snapshot.runtime_stats.active_read_response_states << ","
      << snapshot.runtime_stats.unexpected_buffered_in_use << ","
      << snapshot.runtime_stats.unexpected_semantic_accepted_in_use << ","
      << snapshot.runtime_stats.unexpected_partial_in_use << ","
      << snapshot.runtime_stats.tpdc_inflight_packets << ","
      << snapshot.runtime_stats.tpdc_out_of_order_packets << ","
      << snapshot.runtime_stats.tpdc_pending_sacks << ","
      << snapshot.runtime_stats.tpdc_pending_gap_nacks << ","
      << snapshot.runtime_stats.active_tpdc_sessions << ","
      << snapshot.runtime_stats.credit_refresh_sent << ","
      << snapshot.runtime_stats.ack_ctrl_ext_sent << ","
      << snapshot.runtime_stats.credit_gate_blocked << ","
      << snapshot.runtime_stats.legacy_credit_gate_blocked << ","
      << snapshot.runtime_stats.send_admission_blocked_total << ","
      << snapshot.runtime_stats.send_admission_message_limit_blocked_total << ","
      << snapshot.runtime_stats.send_admission_byte_limit_blocked_total << ","
      << snapshot.runtime_stats.send_admission_both_limit_blocked_total << ","
      << snapshot.runtime_stats.write_budget_blocked_total << ","
      << snapshot.runtime_stats.read_responder_blocked_total << ","
      << snapshot.runtime_stats.transport_window_blocked_total << ","
      << snapshot.runtime_stats.send_admission_messages_in_use_peak << ","
      << snapshot.runtime_stats.send_admission_bytes_in_use_peak << ","
      << snapshot.runtime_stats.write_budget_messages_in_use_peak << ","
      << snapshot.runtime_stats.write_budget_bytes_in_use_peak << ","
      << snapshot.runtime_stats.read_responder_messages_in_use_peak << ","
      << snapshot.runtime_stats.read_response_budget_bytes_in_use_peak << ","
      << snapshot.runtime_stats.blocked_queue_push_total << ","
      << snapshot.runtime_stats.blocked_queue_wakeup_total << ","
      << snapshot.runtime_stats.blocked_queue_dispatch_total << ","
      << snapshot.runtime_stats.blocked_queue_depth_max << ","
      << snapshot.runtime_stats.peer_queue_blocked_total << ","
      << snapshot.runtime_stats.pending_response_enqueue_total << ","
      << snapshot.runtime_stats.blocked_queue_wait_recorded_total << ","
      << snapshot.runtime_stats.blocked_queue_wait_total_ns << ","
      << snapshot.runtime_stats.blocked_queue_wait_peak_ns << ","
      << snapshot.runtime_stats.blocked_queue_wakeup_delay_recorded_total << ","
      << snapshot.runtime_stats.blocked_queue_wakeup_delay_total_ns << ","
      << snapshot.runtime_stats.blocked_queue_wakeup_delay_peak_ns << ","
      << snapshot.runtime_stats.send_admission_release_count << ","
      << snapshot.runtime_stats.send_admission_release_bytes_total << ","
      << snapshot.runtime_stats.blocked_queue_redispatch_fail_after_wakeup_total << ","
      << snapshot.runtime_stats.blocked_queue_redispatch_success_after_wakeup_total << ","
      << snapshot.runtime_stats.pending_response_retry_total << ","
      << snapshot.runtime_stats.pending_response_dispatch_failures_total << ","
      << snapshot.runtime_stats.pending_response_success_after_retry_total << ","
      << snapshot.runtime_stats.stale_timeout_skipped_total << ","
      << snapshot.runtime_stats.stale_retry_skipped_total << ","
      << snapshot.runtime_stats.late_response_observed_total << ","
      << snapshot.runtime_stats.send_duplicate_ok_after_terminal_total << ","
      << snapshot.runtime_stats.send_dispatch_started_total << ","
      << snapshot.runtime_stats.send_response_ok_live_total << ","
      << snapshot.runtime_stats.send_response_nonok_live_total << ","
      << snapshot.runtime_stats.send_response_missing_live_request_total << ","
      << snapshot.runtime_stats.send_response_after_terminal_total << ","
      << snapshot.runtime_stats.send_timeout_without_response_total << ","
      << snapshot.runtime_stats.send_timeout_retry_without_response_progress_total << ","
      << snapshot.runtime_stats.sack_sent << ","
      << snapshot.runtime_stats.gap_nack_sent << ","
      << snapshot.runtime_stats.fabric_path_count << ","
      << snapshot.runtime_stats.fabric_total_tx_bytes << ","
      << snapshot.runtime_stats.fabric_total_tx_packets << ","
      << snapshot.runtime_stats.fabric_ecn_mark_total << ","
      << snapshot.runtime_stats.fabric_top_hotspot_endpoint << ","
      << snapshot.runtime_stats.fabric_dynamic_routing_enabled << ","
      << snapshot.runtime_stats.fabric_dynamic_assignment_total << ","
      << snapshot.runtime_stats.fabric_dynamic_path_reuse_total << ","
      << snapshot.runtime_stats.fabric_active_flow_assignments_peak << ","
      << snapshot.runtime_stats.fabric_path_score_min_ns << ","
      << snapshot.runtime_stats.fabric_path_score_max_ns << ","
      << snapshot.runtime_stats.fabric_path_score_mean_ns << ","
      << snapshot.runtime_stats.fabric_path_tx_bytes[0] << ","
      << snapshot.runtime_stats.fabric_path_tx_bytes[1] << ","
      << snapshot.runtime_stats.fabric_path_tx_bytes[2] << ","
      << snapshot.runtime_stats.fabric_path_tx_bytes[3] << ","
      << snapshot.runtime_stats.fabric_path_tx_bytes[4] << ","
      << snapshot.runtime_stats.fabric_path_tx_bytes[5] << ","
      << snapshot.runtime_stats.fabric_path_tx_bytes[6] << ","
      << snapshot.runtime_stats.fabric_path_tx_bytes[7] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_max[0] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_max[1] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_max[2] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_max[3] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_max[4] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_max[5] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_max[6] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_max[7] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_mean_milli[0] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_mean_milli[1] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_mean_milli[2] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_mean_milli[3] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_mean_milli[4] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_mean_milli[5] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_mean_milli[6] << ","
      << snapshot.runtime_stats.fabric_path_queue_depth_mean_milli[7] << ","
      << snapshot.runtime_stats.fabric_path_flow_count[0] << ","
      << snapshot.runtime_stats.fabric_path_flow_count[1] << ","
      << snapshot.runtime_stats.fabric_path_flow_count[2] << ","
      << snapshot.runtime_stats.fabric_path_flow_count[3] << ","
      << snapshot.runtime_stats.fabric_path_flow_count[4] << ","
      << snapshot.runtime_stats.fabric_path_flow_count[5] << ","
      << snapshot.runtime_stats.fabric_path_flow_count[6] << ","
      << snapshot.runtime_stats.fabric_path_flow_count[7] << std::endl;
  m_softUeProtocolLog.flush();
}

void
PerformanceLogger::LogSoftUeCompletion (const SoftUeCompletionRecord& record)
{
  EnsureSoftUeLogsOpen ();
  if (!m_softUeCompletionLog.is_open()) {
      return;
  }

  m_softUeCompletionLog
      << record.timestamp_ns << ","
      << record.node_id << ","
      << record.if_index << ","
      << record.local_fep << ","
      << record.job_id << ","
      << record.msg_id << ","
      << record.peer_fep << ","
      << OperationTypeToString(record.opcode) << ","
      << (record.success ? 1 : 0) << ","
      << static_cast<uint32_t>(record.return_code) << ","
      << record.failure_domain << ","
      << RequestTerminalReasonToString(record.terminal_reason) << ","
      << record.payload_bytes << ","
      << record.latency_ns << ","
      << record.retry_count << ","
      << (record.send_stage_valid ? 1 : 0) << ","
      << record.send_stage_dispatch_ns << ","
      << record.send_stage_dispatch_wait_for_admission_ns << ","
      << record.send_stage_dispatch_after_admission_to_first_send_ns << ","
      << record.send_stage_inflight_ns << ","
      << record.send_stage_receive_consume_ns << ","
      << record.send_stage_closeout_ns << ","
      << record.send_stage_end_to_end_ns << ","
      << record.send_stage_dispatch_attempt_count << ","
      << record.send_stage_dispatch_budget_block_count << ","
      << record.send_stage_blocked_queue_enqueue_count << ","
      << record.send_stage_blocked_queue_redispatch_count << ","
      << record.send_stage_blocked_queue_wait_total_ns << ","
      << record.send_stage_blocked_queue_wait_peak_ns << ","
      << record.send_stage_admission_release_seen_count << ","
      << record.send_stage_blocked_queue_wakeup_count << ","
      << record.send_stage_redispatch_fail_after_wakeup_count << ","
      << record.send_stage_redispatch_success_after_wakeup_count << ","
      << record.send_stage_admission_release_to_wakeup_total_ns << ","
      << record.send_stage_admission_release_to_wakeup_peak_ns << ","
      << record.send_stage_wakeup_to_redispatch_total_ns << ","
      << record.send_stage_wakeup_to_redispatch_peak_ns << ","
      << (record.read_stage_valid ? 1 : 0) << ","
      << record.read_stage_responder_budget_generate_ns << ","
      << record.read_stage_pending_response_queue_dispatch_ns << ","
      << record.read_stage_response_first_packet_tx_ns << ","
      << record.read_stage_network_return_visibility_ns << ","
      << record.read_stage_first_response_visible_ns << ","
      << record.read_stage_reassembly_complete_ns << ","
      << record.read_stage_terminal_ns << ","
      << record.read_stage_end_to_end_ns << ","
      << (record.read_pre_admission_tracked ? 1 : 0) << ","
      << (record.read_pre_context_allocated_retry ? 1 : 0) << ","
      << (record.read_pre_terminal_resource_exhaust ? 1 : 0) << ","
      << record.read_pre_first_packet_no_context_count << ","
      << record.read_pre_arrival_reserve_fail_count << ","
      << record.read_pre_target_registered_at_ns << ","
      << record.read_pre_first_packet_no_context_at_ns << ","
      << record.read_pre_arrival_block_reserved_at_ns << ","
      << record.read_pre_context_allocated_retry_at_ns << ","
      << record.read_pre_target_released_at_ns << ","
      << record.read_pre_arrival_context_released_at_ns << ","
      << record.read_pre_target_to_first_no_context_ns << ","
      << record.read_pre_target_to_arrival_reserved_ns << ","
      << record.read_pre_first_no_context_to_arrival_reserved_ns << ","
      << record.read_pre_arrival_reserved_to_first_response_visible_ns << ","
      << record.read_pre_target_hold_to_release_ns << ","
      << record.read_pre_arrival_hold_to_release_ns << ","
      << record.read_pre_target_to_terminal_resource_exhaust_ns << ","
      << record.read_pre_first_no_context_to_terminal_resource_exhaust_ns << ","
      << (record.read_recovery_valid ? 1 : 0) << ","
      << (record.read_recovery_tracked ? 1 : 0) << ","
      << record.read_recovery_missing_chunk_index << ","
      << record.read_recovery_missing_transport_seq << ","
      << record.read_recovery_gap_detected_at_ns << ","
      << record.read_recovery_gap_nack_sent_at_ns << ","
      << record.read_recovery_gap_nack_observed_at_sender_ns << ","
      << record.read_recovery_missing_fragment_retransmit_tx_at_ns << ","
      << record.read_recovery_missing_fragment_first_visible_at_ns << ","
      << record.read_recovery_reassembly_unblocked_at_ns << ","
      << record.read_recovery_gap_nack_sent_count << ","
      << record.read_recovery_gap_nack_observed_at_sender_count << ","
      << record.read_recovery_missing_fragment_retransmit_count << ","
      << record.read_recovery_detect_to_nack_ns << ","
      << record.read_recovery_nack_to_observed_at_sender_ns << ","
      << record.read_recovery_observed_at_sender_to_retransmit_ns << ","
      << record.read_recovery_nack_to_retransmit_ns << ","
      << record.read_recovery_retransmit_to_first_visible_ns << ","
      << record.read_recovery_first_visible_to_unblocked_ns << ","
      << record.read_recovery_detect_to_unblocked_ns << ","
      << record.read_recovery_retransmit_to_terminal_ns << ","
      << record.read_recovery_detect_to_terminal_ns << std::endl;
  m_softUeCompletionLog.flush();
}

void
PerformanceLogger::LogSoftUeFailure (const SoftUeFailureSnapshot& snapshot)
{
  EnsureSoftUeLogsOpen ();
  if (!m_softUeFailureLog.is_open()) {
      return;
  }

  m_softUeFailureLog
      << snapshot.timestamp_ns << ","
      << snapshot.node_id << ","
      << snapshot.if_index << ","
      << snapshot.local_fep << ","
      << snapshot.job_id << ","
      << snapshot.msg_id << ","
      << snapshot.peer_fep << ","
      << OperationTypeToString(snapshot.opcode) << ","
      << snapshot.stage << ","
      << snapshot.failure_domain << ","
      << static_cast<uint32_t>(snapshot.return_code) << ","
      << (snapshot.terminal_probe.terminalized ? 1 : 0) << ","
      << RequestTerminalReasonToString(snapshot.terminal_probe.reason) << ","
      << snapshot.retry_probe.retry_count << ","
      << snapshot.tx_probe.pending_psn_count << ","
      << (snapshot.unexpected_probe.present ? 1 : 0) << ","
      << (snapshot.unexpected_probe.semantic_accepted ? 1 : 0) << ","
      << (snapshot.unexpected_probe.matched_to_recv ? 1 : 0) << ","
      << snapshot.unexpected_probe.chunks_done << ","
      << snapshot.unexpected_probe.expected_chunks << ","
      << "\"" << snapshot.diagnostic_text << "\"" << std::endl;
  m_softUeFailureLog.flush();
}

void
PerformanceLogger::LogSoftUeDiagnostic (const SoftUeDiagnosticRecord& record)
{
  EnsureSoftUeLogsOpen ();
  if (!m_softUeDiagnosticLog.is_open()) {
      return;
  }

  m_softUeDiagnosticLog
      << record.timestamp_ns << ","
      << record.node_id << ","
      << record.if_index << ","
      << record.local_fep << ","
      << record.name << ","
      << "\"" << record.detail << "\"" << std::endl;
  m_softUeDiagnosticLog.flush();
}

void
PerformanceLogger::LogSoftUeTpdcSessionProgress (const TpdcSessionProgressRecord& record)
{
  EnsureSoftUeLogsOpen ();
  if (!m_softUeTpdcSessionProgressLog.is_open ())
    {
      return;
    }

  m_softUeTpdcSessionProgressLog
      << record.timestamp_ns << ","
      << record.node_id << ","
      << record.if_index << ","
      << record.local_fep << ","
      << record.remote_fep << ","
      << record.pdc_id << ","
      << record.tx_data_packets_total << ","
      << record.tx_control_packets_total << ","
      << record.rx_data_packets_total << ","
      << record.rx_control_packets_total << ","
      << record.ack_sent_total << ","
      << record.ack_received_total << ","
      << record.gap_nack_received_total << ","
      << record.last_ack_sequence << ","
      << record.send_window_base << ","
      << record.next_send_sequence << ","
      << record.send_buffer_size_current << ","
      << record.send_buffer_size_max << ","
      << record.retransmissions_total << ","
      << record.rto_timeouts_total << ","
      << record.ack_advance_events_total << std::endl;
  m_softUeTpdcSessionProgressLog.flush ();
}

bool
PerformanceLogger::IsInitialized () const
{
  return !m_logTimestamp.empty ();
}

std::string
PerformanceLogger::GetSoftUeProtocolLogPath () const
{
  return m_softUeProtocolLogPath;
}

std::string
PerformanceLogger::GetSoftUeCompletionLogPath () const
{
  return m_softUeCompletionLogPath;
}

std::string
PerformanceLogger::GetSoftUeFailureLogPath () const
{
  return m_softUeFailureLogPath;
}

std::string
PerformanceLogger::GetSoftUeDiagnosticLogPath () const
{
  return m_softUeDiagnosticLogPath;
}

std::string
PerformanceLogger::GetSoftUeTpdcSessionProgressLogPath () const
{
  return m_softUeTpdcSessionProgressLogPath;
}

PerformanceLogger&
PerformanceLogger::GetInstance ()
{
  static PerformanceLogger instance;
  return instance;
}

} // namespace ns3
