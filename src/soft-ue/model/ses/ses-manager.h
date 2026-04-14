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
 * @file             ses-manager.h
 * @brief            Ultra Ethernet SES (Semantic Sub-layer) Manager
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-07
 * @copyright        Apache License Version 2.0
 *
 * @details
 * This file contains the SES Manager class which implements the Semantic
 * Sub-layer of the Ultra Ethernet protocol for ns-3 simulation.
 */

#ifndef SES_MANAGER_H
#define SES_MANAGER_H

#include <ns3/object.h>
#include <ns3/callback.h>
#include <ns3/event-id.h>
#include <ns3/traced-callback.h>
#include <deque>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>
#include "operation-metadata.h"
#include "msn-entry.h"
#include "../common/transport-layer.h"
#include "../common/soft-ue-packet-tag.h"

namespace ns3 {

// Forward declarations
class PdsManager;
class SoftUeNetDevice;

/**
 * @class SesManager
 * @brief Ultra Ethernet SES (Semantic Sub-layer) Manager
 *
 * The SES Manager implements the semantic sub-layer of Ultra Ethernet,
 * responsible for endpoint addressing, authorization, message types,
 * and semantic header formats.
 */
class SesManager : public Object
{
public:
    /**
     * @brief SES Manager state enumeration
     */
    enum SesState
    {
        SES_IDLE,        ///< SES manager is idle and ready to process requests
        SES_BUSY,        ///< SES manager is busy processing requests
        SES_ERROR        ///< SES manager is in error state
    };
    /**
     * @brief Get the type ID for this class
     * @return TypeId
     */
    static TypeId GetTypeId (void);

    /**
     * @brief Get the instance type ID
     * @return TypeId
     */
    virtual TypeId GetInstanceTypeId (void) const override;

    /**
     * @brief Default constructor
     */
    SesManager ();

    /**
     * @brief Destructor
     */
    virtual ~SesManager ();

    /**
     * @brief Set the associated PDS manager
     * @param pdsManager Pointer to PDS manager
     */
    void SetPdsManager (Ptr<PdsManager> pdsManager);

    /**
     * @brief Get the associated PDS manager
     * @return Pointer to PDS manager
     */
    Ptr<PdsManager> GetPdsManager (void) const;

    /**
     * @brief Set the associated network device
     * @param device Pointer to Soft-UE network device
     */
    void SetNetDevice (Ptr<SoftUeNetDevice> device);

    /**
     * @brief Get the associated network device
     * @return Pointer to Soft-UE network device
     */
    Ptr<SoftUeNetDevice> GetNetDevice (void) const;

    /**
     * @brief Initialize SES manager
     */
    void Initialize (void);

    /**
     * @brief Process send request from application layer
     * @param metadata Operation metadata
     * @return true if request was processed successfully
     */
    bool ProcessSendRequest (Ptr<ExtendedOperationMetadata> metadata);

    /**
     * @brief Process send request with packet (transaction-level; may fragment into multiple packets)
     * @param metadata Operation metadata (payload.length used for fragmentation when packet provided)
     * @param packet Packet to send (nullptr = validation only, no dispatch)
     * @return true if request was processed successfully
     */
    bool ProcessSendRequest (Ptr<ExtendedOperationMetadata> metadata, Ptr<Packet> packet);

    /**
     * @brief Process received request packet
     * @param request Received PDC to SES request
     * @return true if request was processed successfully
     */
    bool ProcessReceiveRequest (const PdcSesRequest& request);

    /**
     * @brief Process received response packet
     * @param response Received PDC to SES response
     * @return true if response was processed successfully
     */
    bool ProcessReceiveResponse (const PdcSesResponse& response);

    /**
     * @brief Send response to PDS layer
     * @param response SES to PDS response
     * @return true if response was sent successfully
     */
    bool SendResponseToPds (const SesPdsResponse& response);

    /**
     * @brief Get request queue size
     * @return Number of pending requests
     */
    size_t GetRequestQueueSize (void) const;

    /**
     * @brief Check if there are pending operations
     * @return true if there are pending operations
     */
    bool HasPendingOperations (void) const;

    /**
     * @brief Get MSN table reference
     * @return Reference to MSN table
     */
    Ptr<MsnTable> GetMsnTable (void) const;

    /**
     * @brief Set job ID validator callback
     * @param callback Callback function for job ID validation
     */
    void SetJobIdValidator (Callback<bool, uint64_t> callback);

    /**
     * @brief Set permission checker callback
     * @param callback Callback function for permission checking
     */
    void SetPermissionChecker (Callback<bool, uint32_t, uint64_t> callback);

    /**
     * @brief Set memory region validator callback
     * @param callback Callback function for memory region validation
     */
    void SetMemoryRegionValidator (Callback<bool, uint64_t> callback);

    /**
     * @brief Set packet received callback
     * @param callback Callback function for packet reception events
     */
    void SetPacketReceivedCallback (Callback<void, Ptr<ExtendedOperationMetadata>> callback);

    /**
     * @brief Set packet sent callback
     * @param callback Callback function for packet transmission events
     */
    void SetPacketSentCallback (Callback<void, Ptr<ExtendedOperationMetadata>> callback);

    /**
     * @brief Enable/disable detailed logging
     * @param enable True to enable detailed logging
     */
    void SetDetailedLogging (bool enable);

    /**
     * @brief Get current SES state
     * @return Current SES state
     */
    SesState GetState (void) const;

    /**
     * @brief Check if SES manager is busy
     * @return true if SES manager is busy
     */
    bool IsBusy (void) const;

    /**
     * @brief Check if SES manager is in error state
     * @return true if SES manager is in error state
     */
    bool IsError (void) const;

    /**
     * @brief Reset SES manager to idle state
     */
    void Reset (void);

    /**
     * @brief Get statistics
     * @return Statistics string
     */
    std::string GetStatistics (void) const;

    /**
     * @brief Reset statistics
     */
    void ResetStatistics (void);

    void SetRudSemanticMode (bool enabled);
    bool IsRudSemanticModeEnabled (void) const;
    void ConfigureUnexpectedResources (uint32_t maxMessages, uint32_t maxBytes, uint32_t arrivalCapacity);
    void ConfigureArrivalTracking (uint32_t maxArrivalBlocks, uint32_t maxReadTracks);
    void ConfigureRetry (bool enabled, Time retryTimeout);
    void SetCreditGateEnabled (bool enabled);
    void SetAckControlEnabled (bool enabled);
    void SetCreditRefreshInterval (Time interval);
    void SetInitialCredits (uint32_t initialCredits);
    void SetPayloadMtu (uint32_t payloadMtu);
    void ConfigureSemanticBudgets (uint32_t sendAdmissionMessages,
                                   uint64_t sendAdmissionBytes,
                                   uint32_t writeBudgetMessages,
                                   uint64_t writeBudgetBytes,
                                   uint32_t readResponderMessages,
                                   uint64_t readResponseBytes);
    void SetValidationStrictness (ValidationStrictness strictness);
    void SetDebugSnapshotsEnabled (bool enabled);
    void SetAuthorizeAllJobs (bool allowAll);
    void AuthorizeJob (uint64_t jobId);
    void RevokeJobAuthorization (uint64_t jobId);
    void RegisterMemoryRegion (uint64_t rkey,
                               uint64_t startAddr,
                               uint32_t length,
                               bool readable = true,
                               bool writable = true);
    bool PostReceive (uint64_t jobId,
                      uint16_t msgId,
                      uint32_t srcFep,
                      uint64_t baseAddr,
                      uint32_t length);
    RequestTerminalProbe QueryRequestTerminalProbe (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const;
    SendRetryProbe QuerySendRetryProbe (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const;
    RequestTxProbe QueryRequestTxProbe (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const;
    UnexpectedSendProbe QueryUnexpectedSendProbe (uint64_t jobId, uint16_t msgId, uint32_t srcFep) const;
    RudResourceStats GetRudResourceStats (void) const;
    RudRuntimeStats GetRudRuntimeStats (void) const;
    SoftUeSemanticStats GetSoftUeSemanticStats (void) const;
    bool HasFailureSnapshot (void) const;
    SoftUeFailureSnapshot GetLastFailureSnapshot (void) const;
    std::vector<SoftUeDiagnosticRecord> GetRecentDiagnosticRecords (uint32_t limit) const;
    std::vector<SoftUeCompletionRecord> GetRecentCompletionRecords (uint32_t limit) const;
    const std::vector<ResponseEvent>& GetResponseHistory (void) const;
    const ResponseEvent* GetLatestResponseFor (uint64_t jobId, uint16_t msgId) const;
    bool SawReturnCodeFor (uint64_t jobId, uint16_t msgId, uint8_t returnCode) const;
    bool HasPendingReadResponses (void) const;

    /** Diagnostic hooks for observing internal PDS/PDC events. */
    void NotifyTxResponse (uint16_t pdcId);
    void NotifyPdsErrorEvent (uint16_t pdcId, int errorCode, const std::string& details);
    void NotifyEagerSize (uint32_t eagerSize);
    void NotifyPause (bool paused);
    void NotifyPeerCreditsAvailable (uint32_t peerFep);
    void NotifyExternalDiagnosticEvent (const std::string& name, const std::string& detail);
    void NotifyReadResponseGapDetected (uint64_t jobId,
                                        uint16_t msgId,
                                        uint32_t peerFep,
                                        uint32_t missingChunkIndex,
                                        uint64_t detectedAtNs);
    void NotifyReadResponseTargetRegistered (uint64_t jobId,
                                             uint16_t msgId,
                                             uint32_t peerFep,
                                             uint64_t registeredAtNs);
    void NotifyReadResponseFirstPacketNoContext (uint64_t jobId,
                                                 uint16_t msgId,
                                                 uint32_t peerFep,
                                                 uint64_t observedAtNs);
    void NotifyReadResponseArrivalBlockReserved (uint64_t jobId,
                                                 uint16_t msgId,
                                                 uint32_t peerFep,
                                                 uint64_t reservedAtNs);
    void NotifyReadResponseArrivalBlockReserveFailed (uint64_t jobId,
                                                      uint16_t msgId,
                                                      uint32_t peerFep,
                                                      uint64_t failedAtNs);
    void NotifyReadResponseArrivalContextReleased (uint64_t jobId,
                                                   uint16_t msgId,
                                                   uint32_t peerFep,
                                                   uint64_t releasedAtNs);
    void NotifyReadResponseTargetReleased (uint64_t jobId,
                                           uint16_t msgId,
                                           uint32_t peerFep,
                                           uint64_t releasedAtNs);
    void NotifyReadResponseGapNackSent (uint64_t jobId,
                                        uint16_t msgId,
                                        uint32_t peerFep,
                                        uint32_t missingTransportSeq,
                                        uint64_t sentAtNs);
    void NotifyReadResponseGapNackObservedAtSender (uint64_t jobId,
                                                    uint16_t msgId,
                                                    uint32_t peerFep,
                                                    uint32_t missingTransportSeq,
                                                    uint64_t observedAtNs);
    void NotifyReadResponseGapRetransmitTx (uint64_t jobId,
                                            uint16_t msgId,
                                            uint32_t peerFep,
                                            uint32_t missingTransportSeq,
                                            uint64_t retransmitTxAtNs);
    void NotifyReadResponseRecoveryVisible (uint64_t jobId,
                                            uint16_t msgId,
                                            uint32_t peerFep,
                                            uint32_t chunkIndex,
                                            uint64_t gapNackSentAtNs,
                                            uint64_t retransmitTxAtNs,
                                            uint64_t visibleAtNs);
    void NotifyReadResponseReassemblyUnblocked (uint64_t jobId,
                                                uint16_t msgId,
                                                uint32_t peerFep,
                                                uint32_t chunkIndex,
                                                uint64_t unblockedAtNs);

    // Traced callbacks for monitoring
    TracedCallback<Ptr<ExtendedOperationMetadata>> m_txTrace;        ///< Packet transmit trace
    TracedCallback<Ptr<ExtendedOperationMetadata>> m_rxTrace;        ///< Packet receive trace
    TracedCallback<std::string> m_errorTrace;                         ///< Error trace

protected:
    /**
     * @brief DoDispose method for cleanup
     */
    virtual void DoDispose (void) override;

private:
    enum class RetryTrigger : uint8_t
    {
        NONE = 0x00,
        RC_NO_MATCH = 0x01,
        RESPONSE_TIMEOUT = 0x02,
    };

    struct RequestKey
    {
        uint64_t jobId{0};
        uint16_t msgId{0};
        uint32_t peerFep{0};

        bool operator== (const RequestKey& other) const
        {
            return jobId == other.jobId &&
                   msgId == other.msgId &&
                   peerFep == other.peerFep;
        }
    };

    struct RequestKeyHash
    {
        std::size_t operator() (const RequestKey& key) const
        {
            std::size_t seed = std::hash<uint64_t>{}(key.jobId);
            seed ^= static_cast<std::size_t> (key.msgId) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<uint32_t>{}(key.peerFep) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    struct SimulatedMemoryRegion
    {
        uint64_t startAddr{0};
        uint32_t length{0};
        bool readable{true};
        bool writable{true};
    };

    struct ValidationResult
    {
        bool ok{true};
        uint8_t returnCode{static_cast<uint8_t> (ResponseReturnCode::RC_OK)};
        std::string reasonText;
    };

    struct RequestLifecycleState
    {
        Ptr<ExtendedOperationMetadata> metadata;
        Ptr<Packet> packet;
        bool waitingResponse{false};
        bool dispatchStarted{false};
        uint32_t attemptGeneration{0};
        uint32_t waitingResponseGeneration{0};
        uint32_t lastResponseGeneration{0};
        bool readResponseTargetRegistered{false};
        bool blockedOnCreditGate{false};
        bool blockedQueueLinked{false};
        uint64_t blockedQueueEnqueueNs{0};
        uint32_t dispatchAttemptCount{0};
        uint32_t dispatchBudgetBlockCount{0};
        uint32_t blockedQueueEnqueueCount{0};
        uint32_t blockedQueueRedispatchCount{0};
        uint64_t blockedQueueWaitAccumulatedNs{0};
        uint64_t blockedQueueWaitPeakNs{0};
        uint32_t sendAdmissionReleaseSeenCount{0};
        uint32_t blockedQueueWakeupCount{0};
        uint32_t redispatchFailAfterWakeupCount{0};
        uint32_t redispatchSuccessAfterWakeupCount{0};
        uint64_t lastAdmissionReleaseVisibleNs{0};
        uint64_t lastBlockedQueueWakeupNs{0};
        uint64_t admissionReleaseToWakeupAccumulatedNs{0};
        uint64_t admissionReleaseToWakeupPeakNs{0};
        uint64_t wakeupToRedispatchAccumulatedNs{0};
        uint64_t wakeupToRedispatchPeakNs{0};
        bool sendAdmissionReserved{false};
        uint32_t sendAdmissionReservedBytes{0};
        bool writeBudgetReserved{false};
        uint32_t writeBudgetReservedBytes{0};
        bool terminalized{false};
        RequestTerminalReason terminalReason{RequestTerminalReason::NONE};
        int64_t terminalizedAtMs{0};
        int64_t lastTxProgressMs{0};
        int64_t responseDeadlineMs{0};
        uint32_t pendingPsns{0};
        uint16_t lastRetryCount{0};
        uint16_t timeoutRetryCount{0};
        uint16_t noMatchRetryCount{0};
        RetryTrigger lastRetryTrigger{RetryTrigger::NONE};
        bool retryExhausted{false};
        uint32_t totalPacketCount{0};
        uint64_t createdAtNs{0};
        uint64_t terminalizedAtNs{0};
        uint64_t dispatchFirstAttemptNs{0};
        uint64_t dispatchAdmissionGrantedNs{0};
        uint64_t dispatchFirstFragmentSentNs{0};
        uint64_t receiveConsumeCompleteNs{0};
        uint64_t responseVisibleNs{0};
        uint64_t readResponseGeneratedNs{0};
        uint64_t readResponseFirstDispatchAttemptNs{0};
        uint64_t readResponseFirstFragmentSentNs{0};
        uint64_t readResponseFirstVisibleNs{0};
        uint64_t readResponseReassemblyCompleteNs{0};
        uint64_t readResponseTargetRegisteredNs{0};
        uint64_t readResponseFirstPacketNoContextNs{0};
        uint64_t readResponseArrivalBlockReservedNs{0};
        uint64_t readResponseContextAllocatedRetryNs{0};
        uint64_t readResponseArrivalContextReleasedNs{0};
        uint64_t readResponseTargetReleasedNs{0};
        uint32_t readResponseFirstPacketNoContextCount{0};
        uint32_t readResponseArrivalReserveFailCount{0};
        bool readRecoveryTracked{false};
        uint32_t readRecoveryMissingChunkIndex{0};
        uint32_t readRecoveryMissingTransportSeq{0};
        uint64_t readRecoveryGapDetectedAtNs{0};
        uint64_t readRecoveryGapNackSentAtNs{0};
        uint64_t readRecoveryGapNackObservedAtSenderNs{0};
        uint64_t readRecoveryMissingFragmentRetransmitTxAtNs{0};
        uint64_t readRecoveryMissingFragmentFirstVisibleAtNs{0};
        uint64_t readRecoveryReassemblyUnblockedAtNs{0};
        uint32_t readRecoveryGapNackSentCount{0};
        uint32_t readRecoveryGapNackObservedAtSenderCount{0};
        uint32_t readRecoveryMissingFragmentRetransmitCount{0};
        uint8_t finalReturnCode{static_cast<uint8_t> (ResponseReturnCode::RC_OK)};
        uint32_t completedPayloadBytes{0};
        bool countedInSemanticStats{false};
        EventId dispatchEvent;
    };

    struct BlockedPeerQueueState
    {
        std::deque<RequestKey> requests;
        EventId wakeupEvent;
        uint64_t lastWakeSignalNs{0};
    };

    struct RetryState
    {
        Ptr<ExtendedOperationMetadata> metadata;
        Ptr<Packet> packet;
        bool waitingResponse{false};
        uint16_t retryCount{0};
        int64_t nextRetryMs{0};
        uint32_t scheduledRetryGeneration{0};
        RetryTrigger trigger{RetryTrigger::NONE};
        EventId event;
    };

    struct PendingResponseState
    {
        Ptr<ExtendedOperationMetadata> metadata;
        Ptr<Packet> payload;
        uint8_t responseOpcode{0};
        uint8_t returnCode{0};
        uint32_t modifiedLength{0};
        bool responseWithData{false};
        bool readResponderBudgetReserved{false};
        uint32_t readResponderBudgetPeerFep{0};
        uint32_t readResponderBudgetBytes{0};
        uint64_t responderReceiveConsumeCompleteNs{0};
        uint64_t responseGeneratedNs{0};
        uint64_t responseFirstDispatchAttemptNs{0};
        uint64_t responseFirstFragmentSentNs{0};
        uint32_t packetCount{0};
        uint16_t retryCount{0};
        int64_t nextRetryMs{0};
        bool enqueued{false};
        bool terminalized{false};
    };

    struct PendingResponseQueueState
    {
        std::deque<RequestKey> responses;
        EventId drainEvent;
    };

    Ptr<PdsManager> m_pdsManager;               ///< Associated PDS manager
    Ptr<SoftUeNetDevice> m_netDevice;           ///< Associated network device
    Ptr<MsnTable> m_msnTable;                   ///< MSN table for message tracking

    // Request queues
    std::queue<Ptr<ExtendedOperationMetadata>> m_requestQueue;   ///< Pending send requests
    std::queue<PdcSesRequest> m_recvRequestQueue;              ///< Received requests
    std::queue<PdcSesResponse> m_recvResponseQueue;             ///< Received responses

    // Configuration
    uint16_t m_maxMessageId;                     ///< Maximum message ID
    uint16_t m_currentMessageId;                 ///< Current message ID counter
    uint32_t m_payloadMtu;                       ///< Explicit payload MTU (0 = derive from device MTU)
    bool m_detailedLogging;                      ///< Detailed logging flag

    // State management
    SesState m_state;                            ///< Current SES manager state

    // Callbacks
    Callback<bool, uint64_t> m_jobIdValidator;           ///< Job ID validator
    Callback<bool, uint32_t, uint64_t> m_permissionChecker; ///< Permission checker
    Callback<bool, uint64_t> m_memoryRegionValidator;     ///< Memory region validator
    Callback<void, Ptr<ExtendedOperationMetadata>> m_packetReceivedCallback; ///< Packet received callback
    Callback<void, Ptr<ExtendedOperationMetadata>> m_packetSentCallback;      ///< Packet sent callback

    // Timers and scheduling
    EventId m_processEventId;                   ///< Event ID for periodic processing
    Time m_processInterval;                     ///< Processing interval

    // Statistics
    mutable uint64_t m_totalSendRequests;       ///< Total send requests processed
    mutable uint64_t m_totalReceiveRequests;    ///< Total receive requests processed
    mutable uint64_t m_totalResponses;          ///< Total responses processed
    mutable uint64_t m_totalSuccessfulRequests; ///< Total successful requests processed
    mutable uint64_t m_totalErrors;             ///< Total errors encountered
    mutable uint64_t m_totalPacketsGenerated;   ///< Total packets generated
    mutable uint64_t m_totalPacketsConsumed;     ///< Total packets consumed
    bool m_rudSemanticModeEnabled;
    bool m_retryEnabled;
    bool m_debugSnapshotsEnabled;
    bool m_allowAllJobs;
    bool m_creditGateEnabled;
    bool m_ackControlEnabled;
    uint32_t m_maxUnexpectedMessages;
    uint32_t m_maxUnexpectedBytes;
    uint32_t m_arrivalTrackingCapacity;
    uint32_t m_maxReadTracks;
    uint32_t m_initialCredits;
    uint32_t m_sendAdmissionMessages;
    uint64_t m_sendAdmissionBytes;
    uint32_t m_writeBudgetMessages;
    uint64_t m_writeBudgetBytes;
    uint32_t m_readResponderMessages;
    uint64_t m_readResponseBytes;
    Time m_retryTimeout;
    Time m_creditRefreshInterval;
    ValidationStrictness m_validationStrictness;
    std::set<uint64_t> m_authorizedJobs;
    std::unordered_map<uint64_t, SimulatedMemoryRegion> m_memoryRegions;
    std::unordered_map<RequestKey, RequestLifecycleState, RequestKeyHash> m_requestStates;
    std::unordered_map<RequestKey, RetryState, RequestKeyHash> m_retryStates;
    std::unordered_map<uint32_t, BlockedPeerQueueState> m_blockedPeerQueues;
    std::unordered_map<RequestKey, PendingResponseState, RequestKeyHash> m_pendingResponses;
    std::unordered_map<uint32_t, PendingResponseQueueState> m_pendingResponseQueues;
    std::vector<ResponseEvent> m_responseHistory;
    std::vector<SoftUeDiagnosticRecord> m_diagnosticEvents;
    std::vector<SoftUeCompletionRecord> m_completionRecords;
    SoftUeSemanticStats m_semanticStats;
    uint64_t m_successLatencyTotalNs{0};
    uint64_t m_blockedQueuePushTotal{0};
    uint64_t m_blockedQueueWakeupTotal{0};
    uint64_t m_blockedQueueDispatchTotal{0};
    uint32_t m_blockedQueueDepthMax{0};
    uint64_t m_blockedQueueWaitRecordedTotal{0};
    uint64_t m_blockedQueueWaitTotalNs{0};
    uint64_t m_blockedQueueWaitPeakNs{0};
    uint64_t m_blockedQueueWakeupDelayRecordedTotal{0};
    uint64_t m_blockedQueueWakeupDelayTotalNs{0};
    uint64_t m_blockedQueueWakeupDelayPeakNs{0};
    uint64_t m_sendAdmissionReleaseCount{0};
    uint64_t m_sendAdmissionReleaseBytesTotal{0};
    uint64_t m_blockedQueueRedispatchFailAfterWakeupTotal{0};
    uint64_t m_blockedQueueRedispatchSuccessAfterWakeupTotal{0};
    uint64_t m_peerQueueBlockedTotal{0};
    uint64_t m_pendingResponseEnqueueTotal{0};
    uint64_t m_pendingResponseRetryTotal{0};
    uint64_t m_pendingResponseDispatchFailuresTotal{0};
    uint64_t m_pendingResponseSuccessAfterRetryTotal{0};
    uint64_t m_staleTimeoutSkippedTotal{0};
    uint64_t m_staleRetrySkippedTotal{0};
    uint64_t m_lateResponseObservedTotal{0};
    uint64_t m_sendDuplicateOkAfterTerminalTotal{0};
    uint64_t m_sendDispatchStartedTotal{0};
    uint64_t m_sendResponseOkLiveTotal{0};
    uint64_t m_sendResponseNonOkLiveTotal{0};
    uint64_t m_sendResponseMissingLiveRequestTotal{0};
    uint64_t m_sendResponseAfterTerminalTotal{0};
    uint64_t m_sendTimeoutWithoutResponseTotal{0};
    uint64_t m_sendTimeoutRetryWithoutResponseProgressTotal{0};
    uint32_t m_failNextResponseDispatchesForTesting{0};
    SoftUeFailureSnapshot m_lastFailureSnapshot;
    bool m_hasFailureSnapshot{false};
    static constexpr uint32_t kDiagnosticRecordLimit = 128;
    static constexpr uint32_t kCompletionRecordLimit = 1024;

    /**
     * @brief Initialize SES standard header
     * @param metadata Operation metadata
     * @return SES standard header
     */
    SesPdsRequest InitializeSesHeader (Ptr<ExtendedOperationMetadata> metadata);

    /**
     * @brief Generate next message ID
     * @param metadata Operation metadata
     * @return Next message ID
     */
    uint16_t GenerateMessageId (Ptr<ExtendedOperationMetadata> metadata);

    /**
     * @brief Calculate buffer offset
     * @param rkey Memory key
     * @param startAddr Start address
     * @return Buffer offset
     */
    uint64_t CalculateBufferOffset (uint64_t rkey, uint64_t startAddr);

    /**
     * @brief Parse received request into metadata
     * @param request Received request
     * @return Parsed operation metadata
     */
    Ptr<ExtendedOperationMetadata> ParseReceivedRequest (const PdcSesRequest& request);

    /**
     * @brief Decode RKEY to memory region
     * @param rkey Memory key
     * @return Memory region
     */
    MemoryRegion DecodeRkeyToMr (uint64_t rkey);

    /**
     * @brief Lookup memory region by key
     * @param key Memory key
     * @return Memory region
     */
    MemoryRegion LookupMrByKey (uint64_t key);

    /**
     * @brief Generate NACK response
     * @param errorCode Error code
     * @param originalRequest Original request
     * @return NACK response
     */
    SesPdsResponse GenerateNackResponse (ResponseReturnCode errorCode,
                                         const PdcSesRequest& originalRequest);

    // Validation methods
    bool ValidateVersion (uint8_t version);
    bool ValidateHeaderType (SESHeaderType type);
    bool ValidatePidOnFep (uint32_t pidOnFep, uint32_t jobId, bool relative);
    bool ValidateOperation (OpType opcode) const;
    bool ValidateDataLength (size_t expectedLength, size_t actualLength);
    bool ValidatePdcStatus (uint32_t pdcId, uint64_t psn);
    bool ValidateMemoryKey (uint64_t rkey, uint32_t messageId);
    bool ValidateMsn (uint64_t jobId, uint64_t psn, uint64_t expectedLength,
                     uint32_t pdcId, bool isFirstPacket, bool isLastPacket);
    bool ShouldSendAck (uint32_t messageId, bool deliveryComplete);

    /**
     * @brief Periodic processing
     */
    void DoPeriodicProcessing (void);

    /**
     * @brief Schedule periodic processing
     */
    void ScheduleProcessing (void);

    /**
     * @brief Log detailed information
     * @param function Function name
     * @param message Log message
     */
    void LogDetailed (const std::string& function, const std::string& message) const;
    void RecordDiagnosticEvent (const std::string& name, const std::string& detail);
    void RecordFailureSnapshot (const RequestKey& key,
                                OpType opcode,
                                uint8_t returnCode,
                                const std::string& stage,
                                const std::string& diagnosticText = "");
    std::string ClassifyFailureDomain (uint8_t returnCode,
                                       const RequestKey& key,
                                       const std::string& stage) const;
    void RecordSemanticStart (OpType opcode);
    void SetRequestCompletionOutcome (const RequestKey& key, uint8_t returnCode, uint32_t completedPayloadBytes);
    void RecordCompletion (const RequestKey& key, const RequestLifecycleState& state);
    uint64_t NowNs (void) const;

    /**
     * @brief Generate error trace
     * @param function Function name
     * @param error Error message
     */
    void LogError (const std::string& function, const std::string& error) const;

    /**
     * @brief Validate operation metadata
     * @param metadata Operation metadata to validate
     * @return true if metadata is valid
     */
    bool ValidateOperationMetadata (Ptr<ExtendedOperationMetadata> metadata) const;

    /**
     * @brief Authorization check against the current simulated job policy
     * @param metadata Operation metadata
     * @return true if authorized
     */
    bool ValidateAuthorization (Ptr<ExtendedOperationMetadata> metadata) const;

    /**
     * @brief Validate PDC SES request
     * @param request PDC SES request to validate
     * @return true if request is valid
     */
    bool ValidatePdcSesRequest (const PdcSesRequest& request) const;

    /**
     * @brief Validate PDC SES response
     * @param response PDC SES response to validate
     * @return true if response is valid
     */
    bool ValidatePdcSesResponse (const PdcSesResponse& response) const;

    /**
     * @brief Validate SES PDS response
     * @param response SES PDS response to validate
     * @return true if response is valid
     */
    bool ValidateSesPdsResponse (const SesPdsResponse& response) const;

    /**
     * @brief Process data operation response
     * @param response Data operation response
     * @return true if processing successful
     */
    bool ProcessDataOperationResponse (const PdcSesResponse& response);

    /**
     * @brief Process atomic operation response
     * @param response Atomic operation response
     * @return true if processing successful
     */
    bool ProcessAtomicOperationResponse (const PdcSesResponse& response);
    int64_t NowMs (void) const;
    uint32_t GetPayloadMtu (void) const;
    RequestKey BuildRequestKey (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const;
    RequestKey BuildRequestKey (const SoftUeHeaderTag& headerTag, const SoftUeMetadataTag& metadataTag) const;
    bool TryGetPacketTags (Ptr<Packet> packet, SoftUeHeaderTag* headerTag, SoftUeMetadataTag* metadataTag) const;
    bool DispatchSemanticPacket (Ptr<ExtendedOperationMetadata> metadata, Ptr<Packet> packet);
    bool TryReserveDispatchBudget (const RequestKey& key, RequestLifecycleState& state);
    void ReleaseDispatchBudget (const RequestKey& key, RequestLifecycleState& state);
    bool DispatchRequestPackets (const RequestKey& key);
    void ScheduleBlockedPeerWakeup (uint32_t peerFep);
    void DrainBlockedPeerQueue (uint32_t peerFep);
    void EnqueueBlockedRequest (const RequestKey& key);
    void RemoveBlockedRequest (const RequestKey& key);
    bool HasBlockedRequestsForPeer (uint32_t peerFep) const;
    bool IsBlockedQueueHead (const RequestKey& key) const;
    void HandleDispatchFailure (const RequestKey& key);
    void AttachSemanticTags (Ptr<Packet> packet,
                             Ptr<ExtendedOperationMetadata> metadata,
                             uint32_t fragmentOffset,
                             uint32_t requestLength,
                             bool isResponse,
                             uint8_t responseOpcode,
                             uint8_t returnCode,
                             uint32_t modifiedLength) const;
    bool HandleIncomingRequest (const PdcSesRequest& request,
                                const SoftUeHeaderTag& headerTag,
                                const SoftUeMetadataTag& metadataTag);
    bool HandleIncomingSendRequest (const RequestKey& key,
                                    const PdcSesRequest& request,
                                    const SoftUeMetadataTag& metadataTag);
    bool HandleIncomingWriteRequest (const RequestKey& key,
                                     const PdcSesRequest& request,
                                     const SoftUeMetadataTag& metadataTag);
    bool HandleIncomingReadRequest (const RequestKey& key,
                                    const SoftUeHeaderTag& headerTag,
                                    const SoftUeMetadataTag& metadataTag);
    bool TryDispatchPendingResponse (const RequestKey& key, PendingResponseState& state);
    void EnqueuePendingResponse (const RequestKey& key);
    void RemovePendingResponse (const RequestKey& key);
    void SchedulePendingResponseDrain (uint32_t peerFep, bool immediate);
    void DrainPendingResponseQueue (uint32_t peerFep);
    void ReleasePendingResponseBudget (PendingResponseState& state);
    ValidationResult ValidateInboundHeader (const SoftUeHeaderTag& headerTag,
                                            const SoftUeMetadataTag& metadataTag) const;
    ValidationResult ValidateRmaRequest (const SoftUeHeaderTag& headerTag,
                                         const SoftUeMetadataTag& metadataTag,
                                         bool writeRequest,
                                         uint32_t actualLength = 0) const;
    void SendSemanticResponseForRequest (const RequestKey& key,
                                         uint32_t localFep,
                                         uint32_t remoteFep,
                                         uint8_t responseOpcode,
                                         uint8_t returnCode,
                                         uint32_t modifiedLength,
                                         Ptr<Packet> payload = nullptr,
                                         bool readResponderBudgetReserved = false,
                                         uint32_t readResponderBudgetPeerFep = 0,
                                         uint32_t readResponderBudgetBytes = 0,
                                         uint64_t responderReceiveConsumeCompleteNs = 0,
                                         uint64_t responseGeneratedNs = 0);
    void RecordResponseEvent (uint64_t jobId,
                              uint16_t msgId,
                              uint8_t opcode,
                              uint8_t returnCode,
                              uint32_t modifiedLength);
    void TerminalizeRequest (const RequestKey& key, RequestTerminalReason reason);
    void ScheduleRetry (const RequestKey& key, RetryTrigger trigger);
    void ExecuteRetry (RequestKey key);
    bool WritePacketBytesToAddress (Ptr<Packet> packet, uint64_t baseAddr, uint32_t offset, uint32_t maxLength) const;
    std::vector<uint8_t> CopyPacketBytes (Ptr<Packet> packet) const;
    void ArmResponseTimeout (RequestLifecycleState& state);

  public:
    void ConfigureResponseDispatchFailuresForTesting (uint32_t count);
    void NotifyPeerSendAdmissionReleased (uint32_t peerFep, uint32_t payloadBytes);
};

} // namespace ns3

#endif /* SES_MANAGER_H */
