#include "ses-manager.h"

#include "../network/soft-ue-net-device.h"
#include "../pds/pds-manager.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SesManager");
NS_OBJECT_ENSURE_REGISTERED (SesManager);

namespace {

constexpr uint32_t kDefaultDeviceMtu = 1500;
constexpr uint32_t kSemanticHeaderOverhead = 128;

uint32_t
PayloadCapacityFromDeviceMtu (uint32_t mtu)
{
  if (mtu <= kSemanticHeaderOverhead)
    {
      return 1;
    }
  return mtu - kSemanticHeaderOverhead;
}

} // namespace

TypeId
SesManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SesManager")
    .SetParent<Object> ()
    .SetGroupName ("SoftUe")
    .AddConstructor<SesManager> ();
  return tid;
}

TypeId
SesManager::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

SesManager::SesManager ()
  : m_maxMessageId (65535),
    m_currentMessageId (0),
    m_payloadMtu (0),
    m_detailedLogging (false),
    m_state (SES_IDLE),
    m_totalSendRequests (0),
    m_totalReceiveRequests (0),
    m_totalResponses (0),
    m_totalSuccessfulRequests (0),
    m_totalErrors (0),
    m_totalPacketsGenerated (0),
    m_totalPacketsConsumed (0),
    m_rudSemanticModeEnabled (true),
    m_retryEnabled (true),
    m_debugSnapshotsEnabled (true),
    m_allowAllJobs (true),
    m_creditGateEnabled (false),
    m_ackControlEnabled (false),
    m_maxUnexpectedMessages (64),
    m_maxUnexpectedBytes (1u << 20),
    m_arrivalTrackingCapacity (64),
    m_maxReadTracks (64),
    m_initialCredits (0),
    m_sendAdmissionMessages (64),
    m_sendAdmissionBytes (64ull * 2048ull),
    m_writeBudgetMessages (64),
    m_writeBudgetBytes (64ull * 2048ull),
    m_readResponderMessages (64),
    m_readResponseBytes (64ull * 2048ull),
    m_retryTimeout (MilliSeconds (80)),
    m_creditRefreshInterval (MilliSeconds (5)),
    m_validationStrictness (ValidationStrictness::STRICT)
{
  m_processInterval = MilliSeconds (10);
}

SesManager::~SesManager ()
{
}

void
SesManager::DoDispose (void)
{
  Reset ();
  m_msnTable = nullptr;
  m_pdsManager = nullptr;
  m_netDevice = nullptr;
  Object::DoDispose ();
}

void
SesManager::Initialize (void)
{
  if (!m_msnTable)
    {
      m_msnTable = Create<MsnTable> ();
    }
  ScheduleProcessing ();
}

void
SesManager::SetPdsManager (Ptr<PdsManager> pdsManager)
{
  m_pdsManager = pdsManager;
  if (m_pdsManager)
    {
      m_pdsManager->ConfigureReceiveSemantics (m_maxUnexpectedMessages,
                                              m_maxUnexpectedBytes,
                                              m_arrivalTrackingCapacity,
                                              m_retryTimeout);
      m_pdsManager->ConfigureControlPlane (m_creditGateEnabled,
                                          m_ackControlEnabled,
                                          m_creditRefreshInterval,
                                          m_maxReadTracks,
                                          m_initialCredits);
      m_pdsManager->ConfigureSemanticBudgets (m_sendAdmissionMessages,
                                             m_sendAdmissionBytes,
                                             m_writeBudgetMessages,
                                             m_writeBudgetBytes,
                                             m_readResponderMessages,
                                             m_readResponseBytes);
      m_pdsManager->SetPayloadMtu (m_payloadMtu);
    }
}

Ptr<PdsManager>
SesManager::GetPdsManager (void) const
{
  return m_pdsManager;
}

void
SesManager::SetNetDevice (Ptr<SoftUeNetDevice> device)
{
  m_netDevice = device;
}

Ptr<SoftUeNetDevice>
SesManager::GetNetDevice (void) const
{
  return m_netDevice;
}

bool
SesManager::ProcessSendRequest (Ptr<ExtendedOperationMetadata> metadata)
{
  return ProcessSendRequest (metadata, nullptr);
}

bool
SesManager::ProcessSendRequest (Ptr<ExtendedOperationMetadata> metadata, Ptr<Packet> packet)
{
  if (!metadata)
    {
      LogError ("ProcessSendRequest", "metadata is null");
      return false;
    }

  m_totalSendRequests++;
  m_state = SES_BUSY;

  if (metadata->job_id == 0)
    {
      metadata->job_id = static_cast<uint32_t> ((metadata->GetSourceNodeId () << 16) ^
                                                (GenerateMessageId (metadata) + 1));
    }
  if (metadata->messages_id == 0)
    {
      metadata->messages_id = GenerateMessageId (metadata);
    }
  if (metadata->delivery_mode > static_cast<uint8_t> (DeliveryMode::UOD))
    {
      metadata->delivery_mode = static_cast<uint8_t> (DeliveryMode::RUD);
    }

  Ptr<Packet> requestPacket = packet;
  if (!requestPacket)
    {
      if (metadata->op_type == OpType::READ)
        {
          requestPacket = Create<Packet> (1);
        }
      else
        {
          LogError ("ProcessSendRequest", "packet is required for non-READ operations");
          m_totalErrors++;
          m_state = SES_IDLE;
          return false;
        }
    }

  metadata->payload.length = std::max<uint64_t> (metadata->payload.length, requestPacket->GetSize ());
  if (!ValidateOperationMetadata (metadata))
    {
      const RequestKey failedKey =
          BuildRequestKey (metadata->job_id,
                           static_cast<uint16_t> (metadata->messages_id),
                           metadata->GetDestinationNodeId ());
      RecordFailureSnapshot (failedKey,
                             metadata->op_type,
                             static_cast<uint8_t> (ResponseReturnCode::RC_PROTOCOL_ERROR),
                             "local_metadata_validation_failed",
                             "validate_operation_metadata_failed");
      m_totalErrors++;
      m_state = SES_IDLE;
      return false;
    }
  if (!m_pdsManager)
    {
      LogError ("ProcessSendRequest", "PDS manager is not available");
      m_totalErrors++;
      m_state = SES_IDLE;
      return false;
    }

  AuthorizeJob (metadata->job_id);

  RequestKey key = BuildRequestKey (metadata->job_id,
                                    static_cast<uint16_t> (metadata->messages_id),
                                    metadata->GetDestinationNodeId ());
  auto pendingRetryIt = m_retryStates.find (key);
  if (metadata->is_retry &&
      pendingRetryIt != m_retryStates.end () &&
      pendingRetryIt->second.event.IsPending ())
    {
      Simulator::Cancel (pendingRetryIt->second.event);
      m_retryStates.erase (pendingRetryIt);
      ++m_staleRetrySkippedTotal;
    }
  RequestLifecycleState& requestState = m_requestStates[key];
  const uint16_t preservedRetryCount = requestState.lastRetryCount;
  const uint16_t preservedTimeoutRetryCount = requestState.timeoutRetryCount;
  const uint16_t preservedNoMatchRetryCount = requestState.noMatchRetryCount;
  const RetryTrigger preservedLastRetryTrigger = requestState.lastRetryTrigger;
  const bool preservedRetryExhausted = requestState.retryExhausted;
  const uint64_t preservedCreatedAtNs = requestState.createdAtNs;
  const bool preservedReadResponseTargetRegistered = requestState.readResponseTargetRegistered;
  const uint32_t preservedAttemptGeneration = requestState.attemptGeneration;
  const uint32_t preservedLastResponseGeneration = requestState.lastResponseGeneration;
  const uint64_t preservedDispatchFirstAttemptNs = requestState.dispatchFirstAttemptNs;
  const uint64_t preservedDispatchAdmissionGrantedNs = requestState.dispatchAdmissionGrantedNs;
  const uint64_t preservedDispatchFirstFragmentSentNs = requestState.dispatchFirstFragmentSentNs;
  const uint64_t preservedReceiveConsumeCompleteNs = requestState.receiveConsumeCompleteNs;
  const uint64_t preservedResponseVisibleNs = requestState.responseVisibleNs;
  const uint64_t preservedReadResponseGeneratedNs = requestState.readResponseGeneratedNs;
  const uint64_t preservedReadResponseFirstVisibleNs = requestState.readResponseFirstVisibleNs;
  const uint64_t preservedReadResponseReassemblyCompleteNs =
      requestState.readResponseReassemblyCompleteNs;
  const uint32_t preservedDispatchAttemptCount = requestState.dispatchAttemptCount;
  const uint32_t preservedDispatchBudgetBlockCount = requestState.dispatchBudgetBlockCount;
  const uint32_t preservedBlockedQueueEnqueueCount = requestState.blockedQueueEnqueueCount;
  const uint32_t preservedBlockedQueueRedispatchCount = requestState.blockedQueueRedispatchCount;
  const uint64_t preservedBlockedQueueWaitAccumulatedNs = requestState.blockedQueueWaitAccumulatedNs;
  const uint64_t preservedBlockedQueueWaitPeakNs = requestState.blockedQueueWaitPeakNs;
  const uint32_t preservedSendAdmissionReleaseSeenCount = requestState.sendAdmissionReleaseSeenCount;
  const uint32_t preservedBlockedQueueWakeupCount = requestState.blockedQueueWakeupCount;
  const uint32_t preservedRedispatchFailAfterWakeupCount =
      requestState.redispatchFailAfterWakeupCount;
  const uint32_t preservedRedispatchSuccessAfterWakeupCount =
      requestState.redispatchSuccessAfterWakeupCount;
  const uint64_t preservedAdmissionReleaseToWakeupAccumulatedNs =
      requestState.admissionReleaseToWakeupAccumulatedNs;
  const uint64_t preservedAdmissionReleaseToWakeupPeakNs =
      requestState.admissionReleaseToWakeupPeakNs;
  const uint64_t preservedWakeupToRedispatchAccumulatedNs =
      requestState.wakeupToRedispatchAccumulatedNs;
  const uint64_t preservedWakeupToRedispatchPeakNs =
      requestState.wakeupToRedispatchPeakNs;
  requestState = RequestLifecycleState ();
  requestState.lastRetryCount = preservedRetryCount;
  requestState.timeoutRetryCount = preservedTimeoutRetryCount;
  requestState.noMatchRetryCount = preservedNoMatchRetryCount;
  requestState.lastRetryTrigger = preservedLastRetryTrigger;
  requestState.retryExhausted = preservedRetryExhausted;
  requestState.attemptGeneration = preservedAttemptGeneration;
  requestState.lastResponseGeneration = preservedLastResponseGeneration;
  requestState.createdAtNs =
      (metadata->is_retry && preservedCreatedAtNs != 0) ? preservedCreatedAtNs : NowNs ();
  requestState.dispatchFirstAttemptNs =
      metadata->is_retry ? preservedDispatchFirstAttemptNs : 0;
  requestState.dispatchAdmissionGrantedNs =
      metadata->is_retry ? preservedDispatchAdmissionGrantedNs : 0;
  requestState.dispatchFirstFragmentSentNs =
      metadata->is_retry ? preservedDispatchFirstFragmentSentNs : 0;
  requestState.receiveConsumeCompleteNs =
      metadata->is_retry ? preservedReceiveConsumeCompleteNs : 0;
  requestState.responseVisibleNs =
      metadata->is_retry ? preservedResponseVisibleNs : 0;
  requestState.readResponseGeneratedNs =
      metadata->is_retry ? preservedReadResponseGeneratedNs : 0;
  requestState.readResponseFirstVisibleNs =
      metadata->is_retry ? preservedReadResponseFirstVisibleNs : 0;
  requestState.readResponseReassemblyCompleteNs =
      metadata->is_retry ? preservedReadResponseReassemblyCompleteNs : 0;
  requestState.dispatchAttemptCount =
      metadata->is_retry ? preservedDispatchAttemptCount : 0;
  requestState.dispatchBudgetBlockCount =
      metadata->is_retry ? preservedDispatchBudgetBlockCount : 0;
  requestState.blockedQueueEnqueueCount =
      metadata->is_retry ? preservedBlockedQueueEnqueueCount : 0;
  requestState.blockedQueueRedispatchCount =
      metadata->is_retry ? preservedBlockedQueueRedispatchCount : 0;
  requestState.blockedQueueWaitAccumulatedNs =
      metadata->is_retry ? preservedBlockedQueueWaitAccumulatedNs : 0;
  requestState.blockedQueueWaitPeakNs =
      metadata->is_retry ? preservedBlockedQueueWaitPeakNs : 0;
  requestState.sendAdmissionReleaseSeenCount =
      metadata->is_retry ? preservedSendAdmissionReleaseSeenCount : 0;
  requestState.blockedQueueWakeupCount =
      metadata->is_retry ? preservedBlockedQueueWakeupCount : 0;
  requestState.redispatchFailAfterWakeupCount =
      metadata->is_retry ? preservedRedispatchFailAfterWakeupCount : 0;
  requestState.redispatchSuccessAfterWakeupCount =
      metadata->is_retry ? preservedRedispatchSuccessAfterWakeupCount : 0;
  requestState.admissionReleaseToWakeupAccumulatedNs =
      metadata->is_retry ? preservedAdmissionReleaseToWakeupAccumulatedNs : 0;
  requestState.admissionReleaseToWakeupPeakNs =
      metadata->is_retry ? preservedAdmissionReleaseToWakeupPeakNs : 0;
  requestState.wakeupToRedispatchAccumulatedNs =
      metadata->is_retry ? preservedWakeupToRedispatchAccumulatedNs : 0;
  requestState.wakeupToRedispatchPeakNs =
      metadata->is_retry ? preservedWakeupToRedispatchPeakNs : 0;
  requestState.metadata = CopyObject<ExtendedOperationMetadata> (metadata);
  requestState.packet = requestPacket->Copy ();
  requestState.waitingResponse = false;
  requestState.lastTxProgressMs = NowMs ();
  requestState.pendingPsns = metadata->CalculatePacketCount (GetPayloadMtu ());
  requestState.totalPacketCount = requestState.pendingPsns;
  requestState.readResponseTargetRegistered =
      metadata->is_retry && preservedReadResponseTargetRegistered;
  if (!metadata->is_retry || preservedCreatedAtNs == 0)
    {
      RecordSemanticStart (metadata->op_type);
    }

  if (!DispatchRequestPackets (key))
    {
      HandleDispatchFailure (key);
      m_state = SES_IDLE;
      return false;
    }

  if (!metadata->expect_response &&
      requestState.dispatchStarted &&
      !requestState.blockedOnCreditGate &&
      !requestState.blockedQueueLinked)
    {
      SetRequestCompletionOutcome (key,
                                   static_cast<uint8_t> (ResponseReturnCode::RC_OK),
                                   static_cast<uint32_t> (metadata->payload.length));
      TerminalizeRequest (key, RequestTerminalReason::RC_OK_RESPONSE);
    }

  m_totalSuccessfulRequests++;
  m_state = SES_IDLE;
  return true;
}

bool
SesManager::ProcessReceiveRequest (const PdcSesRequest& request)
{
  m_totalReceiveRequests++;

  if (!ValidatePdcSesRequest (request))
    {
      m_totalErrors++;
      return false;
    }

  if (!m_rudSemanticModeEnabled)
    {
      if (m_netDevice && request.packet)
        {
          m_netDevice->DeliverReceivedPacket (request.packet);
        }
      return true;
    }

  SoftUeHeaderTag headerTag;
  SoftUeMetadataTag metadataTag;
  if (!TryGetPacketTags (request.packet, &headerTag, &metadataTag))
    {
      LogError ("ProcessReceiveRequest", "semantic tags missing on request packet");
      m_totalErrors++;
      return false;
    }

  return HandleIncomingRequest (request, headerTag, metadataTag);
}

bool
SesManager::ProcessReceiveResponse (const PdcSesResponse& response)
{
  m_totalResponses++;
  if (!ValidatePdcSesResponse (response))
    {
      m_totalErrors++;
      return false;
    }

  SoftUeHeaderTag headerTag;
  SoftUeMetadataTag metadataTag;
  if (!TryGetPacketTags (response.packet, &headerTag, &metadataTag) || !metadataTag.IsResponse ())
    {
      LogError ("ProcessReceiveResponse", "semantic response tags missing");
      m_totalErrors++;
      return false;
    }

  const RequestKey key = BuildRequestKey (headerTag, metadataTag);
  NS_LOG_INFO ("SES response job=" << key.jobId
               << " msg=" << key.msgId
               << " peer=" << key.peerFep
               << " opcode=" << static_cast<uint32_t> (metadataTag.GetResponseOpCode ())
               << " rc=" << static_cast<uint32_t> (metadataTag.GetReturnCode ()));

  auto requestIt = m_requestStates.find (key);
  if (requestIt == m_requestStates.end ())
    {
      if (metadataTag.GetOperationType () == OpType::SEND)
        {
          ++m_sendResponseMissingLiveRequestTotal;
          RecordDiagnosticEvent ("SendResponseMissingLiveRequest",
                                 "job=" + std::to_string (key.jobId) +
                                 " msg=" + std::to_string (key.msgId) +
                                 " peer=" + std::to_string (key.peerFep) +
                                 " rc=" + std::to_string (metadataTag.GetReturnCode ()) +
                                 " opcode=" + std::to_string (metadataTag.GetResponseOpCode ()));
        }
      return true;
    }

  RequestLifecycleState& state = requestIt->second;
  state.lastTxProgressMs = NowMs ();

  if (state.terminalized)
    {
      const bool duplicateOkAfterSuccessfulSend =
          metadataTag.GetOperationType () == OpType::SEND &&
          state.terminalReason == RequestTerminalReason::RC_OK_RESPONSE &&
          state.finalReturnCode == static_cast<uint8_t> (ResponseReturnCode::RC_OK) &&
          metadataTag.GetReturnCode () == static_cast<uint8_t> (ResponseReturnCode::RC_OK) &&
          metadataTag.GetResponseOpCode () ==
              static_cast<uint8_t> (ResponseOpCode::UET_DEFAULT_RESPONSE);
      if (duplicateOkAfterSuccessfulSend)
        {
          ++m_sendDuplicateOkAfterTerminalTotal;
          RecordDiagnosticEvent ("SendDuplicateOkAfterTerminal",
                                 "job=" + std::to_string (key.jobId) +
                                 " msg=" + std::to_string (key.msgId) +
                                 " peer=" + std::to_string (key.peerFep) +
                                 " gen=" + std::to_string (state.attemptGeneration));
          return true;
        }
      ++m_lateResponseObservedTotal;
      if (metadataTag.GetOperationType () == OpType::SEND)
        {
          ++m_sendResponseAfterTerminalTotal;
          RecordDiagnosticEvent ("SendResponseAfterTerminal",
                                 "job=" + std::to_string (key.jobId) +
                                 " msg=" + std::to_string (key.msgId) +
                                 " peer=" + std::to_string (key.peerFep) +
                                 " rc=" + std::to_string (metadataTag.GetReturnCode ()) +
                                 " opcode=" + std::to_string (metadataTag.GetResponseOpCode ()) +
                                 " gen=" + std::to_string (state.attemptGeneration));
        }
      return true;
    }

  if (metadataTag.GetOperationType () == OpType::SEND &&
      metadataTag.GetReturnCode () == static_cast<uint8_t> (ResponseReturnCode::RC_OK))
    {
      if (state.responseVisibleNs == 0)
        {
          state.responseVisibleNs = NowNs ();
        }
      SoftUeTimingTag timingTag;
      if (response.packet && response.packet->PeekPacketTag (timingTag))
        {
          const Time auxTimestamp = timingTag.GetAuxTimestamp ();
          if (auxTimestamp != Time::Max () && auxTimestamp.GetNanoSeconds () > 0)
            {
              const uint64_t auxNs = static_cast<uint64_t> (auxTimestamp.GetNanoSeconds ());
              if (state.receiveConsumeCompleteNs == 0 || auxNs < state.receiveConsumeCompleteNs)
                {
                  state.receiveConsumeCompleteNs = auxNs;
                }
            }
        }
      if (state.sendAdmissionReserved && state.receiveConsumeCompleteNs != 0)
        {
          // SEND semantic admission protects receiver consume capacity. Once the requester learns
          // that the responder has fully consumed the SEND payload, release that budget
          // immediately instead of waiting for requester-side terminal bookkeeping.
          ReleaseDispatchBudget (key, state);
        }
    }

  if (metadataTag.GetReturnCode () == static_cast<uint8_t> (ResponseReturnCode::RC_OK))
    {
      if (metadataTag.GetResponseOpCode () == static_cast<uint8_t> (ResponseOpCode::UET_RESPONSE_W_DATA))
        {
          if (state.readResponseFirstVisibleNs == 0)
            {
              state.readResponseFirstVisibleNs = NowNs ();
            }
          SoftUeTimingTag timingTag;
          if (response.packet && response.packet->PeekPacketTag (timingTag))
            {
              const Time auxTimestamp = timingTag.GetAuxTimestamp ();
              if (auxTimestamp != Time::Max () && auxTimestamp.GetNanoSeconds () > 0)
                {
                  const uint64_t auxNs = static_cast<uint64_t> (auxTimestamp.GetNanoSeconds ());
                  if (state.readResponseGeneratedNs == 0 || auxNs < state.readResponseGeneratedNs)
                    {
                      state.readResponseGeneratedNs = auxNs;
                    }
                }
            }
          if (!m_pdsManager)
            {
              SetRequestCompletionOutcome (key,
                                           static_cast<uint8_t> (ResponseReturnCode::RC_INTERNAL_ERROR),
                                           0);
              TerminalizeRequest (key, RequestTerminalReason::PROTOCOL_ERROR);
              return true;
            }

          PdcRxSemanticResult rxResult =
              m_pdsManager->HandleIncomingReadResponsePacket (response.pdc_id,
                                                             headerTag,
                                                             metadataTag,
                                                             response.packet);
          if (!rxResult.handled)
            {
              return true;
            }
          if (rxResult.delivery_complete)
            {
              if (state.readResponseReassemblyCompleteNs == 0)
                {
                  state.readResponseReassemblyCompleteNs = NowNs ();
                }
              state.readResponseTargetRegistered = false;
              state.lastResponseGeneration = state.attemptGeneration;
              RecordResponseEvent (key.jobId,
                                   key.msgId,
                                   metadataTag.GetResponseOpCode (),
                                   metadataTag.GetReturnCode (),
                                   metadataTag.GetModifiedLength ());
              SetRequestCompletionOutcome (
                  key,
                  metadataTag.GetReturnCode (),
                  metadataTag.GetModifiedLength () != 0
                      ? metadataTag.GetModifiedLength ()
                      : (state.metadata ? static_cast<uint32_t> (state.metadata->payload.length) : 0u));
              TerminalizeRequest (key, RequestTerminalReason::RC_OK_RESPONSE);
            }
          else
            {
              state.waitingResponse = true;
              state.waitingResponseGeneration = state.attemptGeneration;
              ArmResponseTimeout (state);
            }
          return true;
        }

      state.waitingResponse = false;
      state.pendingPsns = 0;
      state.responseDeadlineMs = 0;
      state.lastResponseGeneration = state.attemptGeneration;
      if (state.metadata && state.metadata->op_type == OpType::SEND)
        {
          ++m_sendResponseOkLiveTotal;
          RecordDiagnosticEvent ("SendResponseClosedLiveRequest",
                                 "job=" + std::to_string (key.jobId) +
                                 " msg=" + std::to_string (key.msgId) +
                                 " peer=" + std::to_string (key.peerFep) +
                                 " gen=" + std::to_string (state.attemptGeneration));
        }
      RecordResponseEvent (key.jobId,
                           key.msgId,
                           metadataTag.GetResponseOpCode (),
                           metadataTag.GetReturnCode (),
                           metadataTag.GetModifiedLength ());
      SetRequestCompletionOutcome (
          key,
          metadataTag.GetReturnCode (),
          state.metadata ? static_cast<uint32_t> (state.metadata->payload.length) : 0u);
      TerminalizeRequest (key, RequestTerminalReason::RC_OK_RESPONSE);
      return true;
    }

  if (metadataTag.GetReturnCode () == static_cast<uint8_t> (ResponseReturnCode::RC_NO_MATCH) &&
      metadataTag.GetOperationType () == OpType::SEND)
    {
      ++m_sendResponseNonOkLiveTotal;
      RecordDiagnosticEvent ("SendResponseNoMatchLiveRequest",
                             "job=" + std::to_string (key.jobId) +
                             " msg=" + std::to_string (key.msgId) +
                             " peer=" + std::to_string (key.peerFep) +
                             " gen=" + std::to_string (state.attemptGeneration));
      RecordResponseEvent (key.jobId,
                           key.msgId,
                           metadataTag.GetResponseOpCode (),
                           metadataTag.GetReturnCode (),
                           metadataTag.GetModifiedLength ());
      RecordFailureSnapshot (key,
                             metadataTag.GetOperationType (),
                             metadataTag.GetReturnCode (),
                             "send_rc_no_match",
                             "received_rc_no_match_response");
      if (!m_retryEnabled)
        {
          SetRequestCompletionOutcome (key, metadataTag.GetReturnCode (), 0);
          TerminalizeRequest (key, RequestTerminalReason::RC_NO_MATCH_TERMINAL);
          return true;
        }
      ScheduleRetry (key, RetryTrigger::RC_NO_MATCH);
      return true;
    }

  if (metadataTag.GetOperationType () == OpType::SEND)
    {
      ++m_sendResponseNonOkLiveTotal;
      RecordDiagnosticEvent ("SendResponseErrorLiveRequest",
                             "job=" + std::to_string (key.jobId) +
                             " msg=" + std::to_string (key.msgId) +
                             " peer=" + std::to_string (key.peerFep) +
                             " rc=" + std::to_string (metadataTag.GetReturnCode ()) +
                             " opcode=" + std::to_string (metadataTag.GetResponseOpCode ()) +
                             " gen=" + std::to_string (state.attemptGeneration));
    }
  RecordResponseEvent (key.jobId,
                       key.msgId,
                       metadataTag.GetResponseOpCode (),
                       metadataTag.GetReturnCode (),
                       metadataTag.GetModifiedLength ());
  RecordFailureSnapshot (key,
                         metadataTag.GetOperationType (),
                         metadataTag.GetReturnCode (),
                         "response_error",
                         "received_non_ok_response");
  SetRequestCompletionOutcome (key, metadataTag.GetReturnCode (), 0);
  TerminalizeRequest (key, RequestTerminalReason::PROTOCOL_ERROR);
  return true;
}

bool
SesManager::SendResponseToPds (const SesPdsResponse& response)
{
  if (!ValidateSesPdsResponse (response))
    {
      return false;
    }
  return true;
}

size_t
SesManager::GetRequestQueueSize (void) const
{
  return m_requestQueue.size ();
}

bool
SesManager::HasPendingOperations (void) const
{
  const bool hasLiveRequest =
      std::any_of (m_requestStates.begin (),
                   m_requestStates.end (),
                   [] (const auto& entry) {
                     return !entry.second.terminalized ||
                            entry.second.waitingResponse ||
                            entry.second.pendingPsns != 0;
                   });
  return hasLiveRequest ||
         !m_retryStates.empty () ||
         HasPendingReadResponses () ||
         GetRudRuntimeStats ().unexpected_buffered_in_use != 0 ||
         GetRudResourceStats ().arrival_blocks_in_use != 0;
}

Ptr<MsnTable>
SesManager::GetMsnTable (void) const
{
  return m_msnTable;
}

void
SesManager::SetJobIdValidator (Callback<bool, uint64_t> callback)
{
  m_jobIdValidator = callback;
}

void
SesManager::SetPermissionChecker (Callback<bool, uint32_t, uint64_t> callback)
{
  m_permissionChecker = callback;
}

void
SesManager::SetMemoryRegionValidator (Callback<bool, uint64_t> callback)
{
  m_memoryRegionValidator = callback;
}

void
SesManager::SetPacketReceivedCallback (Callback<void, Ptr<ExtendedOperationMetadata>> callback)
{
  m_packetReceivedCallback = callback;
}

void
SesManager::SetPacketSentCallback (Callback<void, Ptr<ExtendedOperationMetadata>> callback)
{
  m_packetSentCallback = callback;
}

void
SesManager::SetDetailedLogging (bool enable)
{
  m_detailedLogging = enable;
}

void
SesManager::SetRudSemanticMode (bool enabled)
{
  m_rudSemanticModeEnabled = enabled;
}

bool
SesManager::IsRudSemanticModeEnabled (void) const
{
  return m_rudSemanticModeEnabled;
}

void
SesManager::ConfigureUnexpectedResources (uint32_t maxMessages, uint32_t maxBytes, uint32_t arrivalCapacity)
{
  NS_LOG_INFO ("SES configure unexpected resources max_messages=" << maxMessages
               << " max_bytes=" << maxBytes
               << " arrival_capacity=" << arrivalCapacity);
  m_maxUnexpectedMessages = maxMessages;
  m_maxUnexpectedBytes = maxBytes;
  m_arrivalTrackingCapacity = arrivalCapacity;
  if (m_pdsManager)
    {
      m_pdsManager->ConfigureReceiveSemantics (m_maxUnexpectedMessages,
                                              m_maxUnexpectedBytes,
                                              m_arrivalTrackingCapacity,
                                              m_retryTimeout);
      m_pdsManager->ConfigureControlPlane (m_creditGateEnabled,
                                          m_ackControlEnabled,
                                          m_creditRefreshInterval,
                                          m_maxReadTracks,
                                          m_initialCredits);
    }
}

void
SesManager::ConfigureArrivalTracking (uint32_t maxArrivalBlocks, uint32_t maxReadTracks)
{
  m_arrivalTrackingCapacity = maxArrivalBlocks;
  m_maxReadTracks = maxReadTracks;
  if (m_pdsManager)
    {
      m_pdsManager->ConfigureReceiveSemantics (m_maxUnexpectedMessages,
                                              m_maxUnexpectedBytes,
                                              m_arrivalTrackingCapacity,
                                              m_retryTimeout);
      m_pdsManager->ConfigureControlPlane (m_creditGateEnabled,
                                          m_ackControlEnabled,
                                          m_creditRefreshInterval,
                                          m_maxReadTracks,
                                          m_initialCredits);
    }
}

void
SesManager::ConfigureRetry (bool enabled, Time retryTimeout)
{
  m_retryEnabled = enabled;
  m_retryTimeout = retryTimeout;
  if (m_pdsManager)
    {
      m_pdsManager->ConfigureReceiveSemantics (m_maxUnexpectedMessages,
                                              m_maxUnexpectedBytes,
                                              m_arrivalTrackingCapacity,
                                              m_retryTimeout);
    }
}

void
SesManager::SetCreditGateEnabled (bool enabled)
{
  m_creditGateEnabled = enabled;
  if (m_pdsManager)
    {
      m_pdsManager->ConfigureControlPlane (m_creditGateEnabled,
                                          m_ackControlEnabled,
                                          m_creditRefreshInterval,
                                          m_maxReadTracks,
                                          m_initialCredits);
    }
}

void
SesManager::SetAckControlEnabled (bool enabled)
{
  m_ackControlEnabled = enabled;
  if (m_pdsManager)
    {
      m_pdsManager->ConfigureControlPlane (m_creditGateEnabled,
                                          m_ackControlEnabled,
                                          m_creditRefreshInterval,
                                          m_maxReadTracks,
                                          m_initialCredits);
    }
}

void
SesManager::SetCreditRefreshInterval (Time interval)
{
  m_creditRefreshInterval = interval;
  if (m_pdsManager)
    {
      m_pdsManager->ConfigureControlPlane (m_creditGateEnabled,
                                          m_ackControlEnabled,
                                          m_creditRefreshInterval,
                                          m_maxReadTracks,
                                          m_initialCredits);
    }
}

void
SesManager::SetInitialCredits (uint32_t initialCredits)
{
  m_initialCredits = initialCredits;
  if (m_pdsManager)
    {
      m_pdsManager->ConfigureControlPlane (m_creditGateEnabled,
                                          m_ackControlEnabled,
                                          m_creditRefreshInterval,
                                          m_maxReadTracks,
                                          m_initialCredits);
    }
}

void
SesManager::SetPayloadMtu (uint32_t payloadMtu)
{
  m_payloadMtu = payloadMtu;
  if (m_pdsManager)
    {
      m_pdsManager->SetPayloadMtu (payloadMtu);
    }
}

void
SesManager::ConfigureSemanticBudgets (uint32_t sendAdmissionMessages,
                                      uint64_t sendAdmissionBytes,
                                      uint32_t writeBudgetMessages,
                                      uint64_t writeBudgetBytes,
                                      uint32_t readResponderMessages,
                                      uint64_t readResponseBytes)
{
  m_sendAdmissionMessages = sendAdmissionMessages;
  m_sendAdmissionBytes = sendAdmissionBytes;
  m_writeBudgetMessages = writeBudgetMessages;
  m_writeBudgetBytes = writeBudgetBytes;
  m_readResponderMessages = readResponderMessages;
  m_readResponseBytes = readResponseBytes;
  if (m_pdsManager)
    {
      m_pdsManager->ConfigureSemanticBudgets (m_sendAdmissionMessages,
                                             m_sendAdmissionBytes,
                                             m_writeBudgetMessages,
                                             m_writeBudgetBytes,
                                             m_readResponderMessages,
                                             m_readResponseBytes);
    }
}

void
SesManager::SetValidationStrictness (ValidationStrictness strictness)
{
  m_validationStrictness = strictness;
}

void
SesManager::SetDebugSnapshotsEnabled (bool enabled)
{
  m_debugSnapshotsEnabled = enabled;
}

void
SesManager::SetAuthorizeAllJobs (bool allowAll)
{
  m_allowAllJobs = allowAll;
}

void
SesManager::AuthorizeJob (uint64_t jobId)
{
  if (jobId != 0)
    {
      m_authorizedJobs.insert (jobId);
    }
}

void
SesManager::RevokeJobAuthorization (uint64_t jobId)
{
  m_authorizedJobs.erase (jobId);
}

void
SesManager::RegisterMemoryRegion (uint64_t rkey,
                                  uint64_t startAddr,
                                  uint32_t length,
                                  bool readable,
                                  bool writable)
{
  SimulatedMemoryRegion region;
  region.startAddr = startAddr;
  region.length = length;
  region.readable = readable;
  region.writable = writable;
  m_memoryRegions[rkey] = region;
}

bool
SesManager::PostReceive (uint64_t jobId,
                         uint16_t msgId,
                         uint32_t srcFep,
                         uint64_t baseAddr,
                         uint32_t length)
{
  return m_pdsManager &&
         m_pdsManager->RegisterPostedReceive (jobId, msgId, srcFep, baseAddr, length);
}

RequestTerminalProbe
SesManager::QueryRequestTerminalProbe (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const
{
  RequestTerminalProbe probe;
  const RequestKey key = BuildRequestKey (jobId, msgId, peerFep);
  auto it = m_requestStates.find (key);
  auto retryIt = m_retryStates.find (key);
  if (it == m_requestStates.end () && retryIt == m_retryStates.end ())
    {
      return probe;
    }
  probe.present = true;
  probe.retry_present = (retryIt != m_retryStates.end ());
  if (it != m_requestStates.end ())
    {
      probe.terminalized = it->second.terminalized;
      probe.reason = it->second.terminalReason;
      probe.terminalized_at_ms = it->second.terminalizedAtMs;
    }
  return probe;
}

SendRetryProbe
SesManager::QuerySendRetryProbe (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const
{
  SendRetryProbe probe;
  const RequestKey key = BuildRequestKey (jobId, msgId, peerFep);
  auto it = m_retryStates.find (key);
  auto requestIt = m_requestStates.find (key);
  if (requestIt != m_requestStates.end ())
    {
      probe.waiting_response = requestIt->second.waitingResponse;
      probe.retry_count = requestIt->second.lastRetryCount;
      probe.next_retry_ms = requestIt->second.responseDeadlineMs;
      probe.timeout_armed = requestIt->second.waitingResponse && requestIt->second.responseDeadlineMs != 0;
      probe.last_trigger_timeout = requestIt->second.lastRetryTrigger == RetryTrigger::RESPONSE_TIMEOUT;
      probe.last_trigger_no_match = requestIt->second.lastRetryTrigger == RetryTrigger::RC_NO_MATCH;
      probe.exhausted = requestIt->second.retryExhausted;
      probe.present = probe.timeout_armed || probe.exhausted;
    }
  if (it != m_retryStates.end ())
    {
      probe.present = true;
      probe.waiting_response = it->second.waitingResponse;
      probe.retry_count = std::max<uint16_t> (probe.retry_count, it->second.retryCount);
      probe.next_retry_ms = it->second.nextRetryMs;
      probe.timeout_armed = probe.timeout_armed || it->second.waitingResponse;
      probe.last_trigger_timeout = it->second.trigger == RetryTrigger::RESPONSE_TIMEOUT;
      probe.last_trigger_no_match = it->second.trigger == RetryTrigger::RC_NO_MATCH;
    }
  return probe;
}

RequestTxProbe
SesManager::QueryRequestTxProbe (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const
{
  RequestTxProbe probe;
  const RequestKey key = BuildRequestKey (jobId, msgId, peerFep);
  auto it = m_requestStates.find (key);
  if (it == m_requestStates.end ())
    {
      return probe;
    }
  probe.present = true;
  probe.has_tx_pkt_map_entries = it->second.waitingResponse || m_retryStates.count (key) != 0;
  probe.has_tx_pkt_buffer_entries = (it->second.packet != nullptr);
  probe.oldest_pending_psn = probe.has_tx_pkt_map_entries ? 1 : 0;
  probe.pending_psn_count = it->second.pendingPsns;
  probe.pending_control_only = (it->second.metadata &&
                                it->second.metadata->op_type == OpType::READ);
  probe.pending_data_only = (it->second.metadata &&
                             (it->second.metadata->op_type == OpType::SEND ||
                              it->second.metadata->op_type == OpType::WRITE));
  probe.last_tx_progress_ms = it->second.lastTxProgressMs;
  return probe;
}

UnexpectedSendProbe
SesManager::QueryUnexpectedSendProbe (uint64_t jobId, uint16_t msgId, uint32_t srcFep) const
{
  return m_pdsManager
             ? m_pdsManager->QueryUnexpectedSendProbe (jobId, msgId, srcFep)
             : UnexpectedSendProbe ();
}

RudResourceStats
SesManager::GetRudResourceStats (void) const
{
  return m_pdsManager ? m_pdsManager->GetRudResourceStats () : RudResourceStats ();
}

RudRuntimeStats
SesManager::GetRudRuntimeStats (void) const
{
  RudRuntimeStats stats;
  stats.active_retry_states = static_cast<uint32_t> (m_retryStates.size ());
  stats.blocked_queue_push_total = static_cast<uint32_t> (m_blockedQueuePushTotal);
  stats.blocked_queue_wakeup_total = static_cast<uint32_t> (m_blockedQueueWakeupTotal);
  stats.blocked_queue_dispatch_total = static_cast<uint32_t> (m_blockedQueueDispatchTotal);
  stats.blocked_queue_depth_max = m_blockedQueueDepthMax;
  stats.blocked_queue_wait_recorded_total = static_cast<uint32_t> (m_blockedQueueWaitRecordedTotal);
  stats.blocked_queue_wait_total_ns = m_blockedQueueWaitTotalNs;
  stats.blocked_queue_wait_peak_ns = m_blockedQueueWaitPeakNs;
  stats.blocked_queue_wakeup_delay_recorded_total =
      static_cast<uint32_t> (m_blockedQueueWakeupDelayRecordedTotal);
  stats.blocked_queue_wakeup_delay_total_ns = m_blockedQueueWakeupDelayTotalNs;
  stats.blocked_queue_wakeup_delay_peak_ns = m_blockedQueueWakeupDelayPeakNs;
  stats.send_admission_release_count = static_cast<uint32_t> (m_sendAdmissionReleaseCount);
  stats.send_admission_release_bytes_total = m_sendAdmissionReleaseBytesTotal;
  stats.blocked_queue_redispatch_fail_after_wakeup_total =
      static_cast<uint32_t> (m_blockedQueueRedispatchFailAfterWakeupTotal);
  stats.blocked_queue_redispatch_success_after_wakeup_total =
      static_cast<uint32_t> (m_blockedQueueRedispatchSuccessAfterWakeupTotal);
  stats.peer_queue_blocked_total = static_cast<uint32_t> (m_peerQueueBlockedTotal);
  stats.pending_response_enqueue_total = static_cast<uint32_t> (m_pendingResponseEnqueueTotal);
  stats.pending_response_retry_total = static_cast<uint32_t> (m_pendingResponseRetryTotal);
  stats.pending_response_dispatch_failures_total =
      static_cast<uint32_t> (m_pendingResponseDispatchFailuresTotal);
  stats.pending_response_success_after_retry_total =
      static_cast<uint32_t> (m_pendingResponseSuccessAfterRetryTotal);
  stats.stale_timeout_skipped_total = static_cast<uint32_t> (m_staleTimeoutSkippedTotal);
  stats.stale_retry_skipped_total = static_cast<uint32_t> (m_staleRetrySkippedTotal);
  stats.late_response_observed_total = static_cast<uint32_t> (m_lateResponseObservedTotal);
  stats.send_duplicate_ok_after_terminal_total =
      static_cast<uint32_t> (m_sendDuplicateOkAfterTerminalTotal);
  stats.send_dispatch_started_total = static_cast<uint32_t> (m_sendDispatchStartedTotal);
  stats.send_response_ok_live_total = static_cast<uint32_t> (m_sendResponseOkLiveTotal);
  stats.send_response_nonok_live_total = static_cast<uint32_t> (m_sendResponseNonOkLiveTotal);
  stats.send_response_missing_live_request_total =
      static_cast<uint32_t> (m_sendResponseMissingLiveRequestTotal);
  stats.send_response_after_terminal_total =
      static_cast<uint32_t> (m_sendResponseAfterTerminalTotal);
  stats.send_timeout_without_response_total =
      static_cast<uint32_t> (m_sendTimeoutWithoutResponseTotal);
  stats.send_timeout_retry_without_response_progress_total =
      static_cast<uint32_t> (m_sendTimeoutRetryWithoutResponseProgressTotal);
  if (m_pdsManager)
    {
      m_pdsManager->FillRudRuntimeStats (&stats);
      stats.tpdc_inflight_packets = m_pdsManager->GetTpdcInflightPackets ();
      stats.tpdc_out_of_order_packets = m_pdsManager->GetTpdcOutOfOrderPackets ();
      stats.tpdc_pending_sacks = m_pdsManager->GetTpdcPendingSacks ();
      stats.tpdc_pending_gap_nacks = m_pdsManager->GetTpdcPendingGapNacks ();
      stats.active_tpdc_sessions = m_pdsManager->GetActiveTpdcCount ();
    }
  return stats;
}

SoftUeSemanticStats
SesManager::GetSoftUeSemanticStats (void) const
{
  SoftUeSemanticStats stats = m_semanticStats;
  stats.ops_in_flight = 0;
  for (const auto& entry : m_requestStates)
    {
      if (entry.second.metadata != nullptr && !entry.second.terminalized)
        {
          ++stats.ops_in_flight;
        }
    }
  if (stats.success_latency_samples != 0)
    {
      stats.success_latency_mean_ns =
          m_successLatencyTotalNs / std::max<uint64_t> (1u, stats.success_latency_samples);
    }
  return stats;
}

bool
SesManager::HasFailureSnapshot (void) const
{
  return m_hasFailureSnapshot;
}

SoftUeFailureSnapshot
SesManager::GetLastFailureSnapshot (void) const
{
  return m_lastFailureSnapshot;
}

std::vector<SoftUeDiagnosticRecord>
SesManager::GetRecentDiagnosticRecords (uint32_t limit) const
{
  if (limit == 0 || limit >= m_diagnosticEvents.size ())
    {
      return m_diagnosticEvents;
    }
  return std::vector<SoftUeDiagnosticRecord> (m_diagnosticEvents.end () - limit,
                                              m_diagnosticEvents.end ());
}

std::vector<SoftUeCompletionRecord>
SesManager::GetRecentCompletionRecords (uint32_t limit) const
{
  if (limit == 0 || limit >= m_completionRecords.size ())
    {
      return m_completionRecords;
    }
  return std::vector<SoftUeCompletionRecord> (m_completionRecords.end () - limit,
                                              m_completionRecords.end ());
}

const std::vector<ResponseEvent>&
SesManager::GetResponseHistory (void) const
{
  return m_responseHistory;
}

const ResponseEvent*
SesManager::GetLatestResponseFor (uint64_t jobId, uint16_t msgId) const
{
  for (auto it = m_responseHistory.rbegin (); it != m_responseHistory.rend (); ++it)
    {
      if (it->job_id == jobId && it->msg_id == msgId)
        {
          return &(*it);
        }
    }
  return nullptr;
}

bool
SesManager::SawReturnCodeFor (uint64_t jobId, uint16_t msgId, uint8_t returnCode) const
{
  for (const auto& event : m_responseHistory)
    {
      if (event.job_id == jobId && event.msg_id == msgId && event.return_code == returnCode)
        {
          return true;
        }
    }
  return false;
}

bool
SesManager::HasPendingReadResponses (void) const
{
  return m_pdsManager && m_pdsManager->HasPendingReadResponseState ();
}

void
SesManager::NotifyTxResponse (uint16_t pdcId)
{
  RecordDiagnosticEvent ("NotifyTxResponse", "pdc_id=" + std::to_string (pdcId));
}

void
SesManager::NotifyPdsErrorEvent (uint16_t pdcId, int errorCode, const std::string& details)
{
  RecordDiagnosticEvent ("NotifyPdsErrorEvent",
                         "pdc_id=" + std::to_string (pdcId) +
                         " error=" + std::to_string (errorCode) + " " + details);
}

void
SesManager::NotifyEagerSize (uint32_t eagerSize)
{
  RecordDiagnosticEvent ("NotifyEagerSize", "size=" + std::to_string (eagerSize));
}

void
SesManager::NotifyPause (bool paused)
{
  RecordDiagnosticEvent ("NotifyPause", paused ? "paused" : "running");
}

void
SesManager::NotifyPeerCreditsAvailable (uint32_t peerFep)
{
  auto queueIt = m_blockedPeerQueues.find (peerFep);
  if (queueIt != m_blockedPeerQueues.end () && !queueIt->second.requests.empty ())
    {
      queueIt->second.lastWakeSignalNs = Simulator::Now ().GetNanoSeconds ();
    }
  ScheduleBlockedPeerWakeup (peerFep);
  SchedulePendingResponseDrain (peerFep, true);
}

void
SesManager::NotifyPeerSendAdmissionReleased (uint32_t peerFep, uint32_t payloadBytes)
{
  ++m_sendAdmissionReleaseCount;
  m_sendAdmissionReleaseBytesTotal += payloadBytes;

  auto queueIt = m_blockedPeerQueues.find (peerFep);
  if (queueIt != m_blockedPeerQueues.end () && !queueIt->second.requests.empty ())
    {
      const RequestKey headKey = queueIt->second.requests.front ();
      auto requestIt = m_requestStates.find (headKey);
      if (requestIt != m_requestStates.end () && !requestIt->second.terminalized)
        {
          ++requestIt->second.sendAdmissionReleaseSeenCount;
          requestIt->second.lastAdmissionReleaseVisibleNs = NowNs ();
        }
    }

  NotifyPeerCreditsAvailable (peerFep);
}

void
SesManager::ConfigureResponseDispatchFailuresForTesting (uint32_t count)
{
  m_failNextResponseDispatchesForTesting = count;
}

std::string
SesManager::GetStatistics (void) const
{
  std::ostringstream oss;
  oss << "SES Manager Statistics:"
      << "\n  Total Send Requests: " << m_totalSendRequests
      << "\n  Total Receive Requests: " << m_totalReceiveRequests
      << "\n  Total Responses: " << m_totalResponses
      << "\n  Total Successful Requests: " << m_totalSuccessfulRequests
      << "\n  Total Errors: " << m_totalErrors
      << "\n  Packets Generated: " << m_totalPacketsGenerated
      << "\n  Packets Consumed: " << m_totalPacketsConsumed
      << "\n  Active Retry States: " << m_retryStates.size ()
      << "\n  Unexpected Buffered: " << GetRudRuntimeStats ().unexpected_buffered_in_use;
  return oss.str ();
}

void
SesManager::ResetStatistics (void)
{
  m_totalSendRequests = 0;
  m_totalReceiveRequests = 0;
  m_totalResponses = 0;
  m_totalSuccessfulRequests = 0;
  m_totalErrors = 0;
  m_totalPacketsGenerated = 0;
  m_totalPacketsConsumed = 0;
  m_responseHistory.clear ();
  m_diagnosticEvents.clear ();
  m_completionRecords.clear ();
  m_semanticStats = SoftUeSemanticStats ();
  m_successLatencyTotalNs = 0;
  m_blockedQueuePushTotal = 0;
  m_blockedQueueWakeupTotal = 0;
  m_blockedQueueDispatchTotal = 0;
  m_blockedQueueDepthMax = 0;
  m_blockedQueueWaitRecordedTotal = 0;
  m_blockedQueueWaitTotalNs = 0;
  m_blockedQueueWaitPeakNs = 0;
  m_blockedQueueWakeupDelayRecordedTotal = 0;
  m_blockedQueueWakeupDelayTotalNs = 0;
  m_blockedQueueWakeupDelayPeakNs = 0;
  m_sendAdmissionReleaseCount = 0;
  m_sendAdmissionReleaseBytesTotal = 0;
  m_blockedQueueRedispatchFailAfterWakeupTotal = 0;
  m_blockedQueueRedispatchSuccessAfterWakeupTotal = 0;
  m_peerQueueBlockedTotal = 0;
  m_pendingResponseEnqueueTotal = 0;
  m_pendingResponseRetryTotal = 0;
  m_pendingResponseDispatchFailuresTotal = 0;
  m_pendingResponseSuccessAfterRetryTotal = 0;
  m_staleTimeoutSkippedTotal = 0;
  m_staleRetrySkippedTotal = 0;
  m_lateResponseObservedTotal = 0;
  m_sendDuplicateOkAfterTerminalTotal = 0;
  m_sendDispatchStartedTotal = 0;
  m_sendResponseOkLiveTotal = 0;
  m_sendResponseNonOkLiveTotal = 0;
  m_sendResponseMissingLiveRequestTotal = 0;
  m_sendResponseAfterTerminalTotal = 0;
  m_sendTimeoutWithoutResponseTotal = 0;
  m_sendTimeoutRetryWithoutResponseProgressTotal = 0;
  m_lastFailureSnapshot = SoftUeFailureSnapshot ();
  m_hasFailureSnapshot = false;
}

SesPdsRequest
SesManager::InitializeSesHeader (Ptr<ExtendedOperationMetadata> metadata)
{
  SesPdsRequest request;
  request.src_fep = metadata->GetSourceNodeId ();
  request.dst_fep = metadata->GetDestinationNodeId ();
  request.mode = metadata->delivery_mode;
  request.rod_context = static_cast<uint16_t> (metadata->messages_id);
  request.next_hdr = PDSNextHeader::UET_HDR_REQUEST_STD;
  request.tc = 0;
  request.lock_pdc = false;
  request.tx_pkt_handle = 0;
  request.packet = nullptr;
  request.pkt_len = 0;
  request.tss_context = 0;
  request.rsv_pdc_context = 0;
  request.rsv_ccc_context = 0;
  request.som = metadata->som;
  request.eom = metadata->eom;
  return request;
}

uint16_t
SesManager::GenerateMessageId (Ptr<ExtendedOperationMetadata> metadata)
{
  if (metadata && metadata->messages_id != 0)
    {
      return static_cast<uint16_t> (metadata->messages_id);
    }
  if (m_currentMessageId >= m_maxMessageId)
    {
      m_currentMessageId = 1;
    }
  else
    {
      ++m_currentMessageId;
    }
  return m_currentMessageId;
}

uint64_t
SesManager::CalculateBufferOffset (uint64_t rkey, uint64_t startAddr)
{
  auto it = m_memoryRegions.find (rkey);
  if (it == m_memoryRegions.end () || startAddr < it->second.startAddr)
    {
      return startAddr;
    }
  return startAddr - it->second.startAddr;
}

Ptr<ExtendedOperationMetadata>
SesManager::ParseReceivedRequest (const PdcSesRequest& request)
{
  SoftUeHeaderTag headerTag;
  SoftUeMetadataTag metadataTag;
  if (!TryGetPacketTags (request.packet, &headerTag, &metadataTag))
    {
      return nullptr;
    }

  Ptr<ExtendedOperationMetadata> metadata = Create<ExtendedOperationMetadata> ();
  metadata->op_type = metadataTag.GetOperationType ();
  metadata->job_id = static_cast<uint32_t> (headerTag.GetJobId ());
  metadata->messages_id = metadataTag.GetMessageId ();
  metadata->payload.length = metadataTag.GetRequestLength ();
  if (metadata->op_type == OpType::READ)
    {
      metadata->payload.start_addr = 0;
      metadata->payload.imm_data = metadataTag.GetRemoteAddress ();
    }
  else
    {
      metadata->payload.start_addr = metadataTag.GetRemoteAddress ();
    }
  metadata->memory.rkey = metadataTag.GetRemoteKey ();
  metadata->delivery_mode = static_cast<uint8_t> (DeliveryMode::RUD);
  metadata->is_retry = metadataTag.IsRetry ();
  metadata->SetSourceEndpoint (headerTag.GetSourceEndpoint (), 1);
  metadata->SetDestinationEndpoint (headerTag.GetDestinationEndpoint (), 1);
  return metadata;
}

MemoryRegion
SesManager::DecodeRkeyToMr (uint64_t rkey)
{
  MemoryRegion region{};
  auto it = m_memoryRegions.find (rkey);
  if (it != m_memoryRegions.end ())
    {
      region.start_addr = it->second.startAddr;
      region.length = it->second.length;
    }
  return region;
}

MemoryRegion
SesManager::LookupMrByKey (uint64_t key)
{
  return DecodeRkeyToMr (key);
}

SesPdsResponse
SesManager::GenerateNackResponse (ResponseReturnCode errorCode,
                                  const PdcSesRequest& originalRequest)
{
  SesPdsResponse response{};
  response.pdc_id = originalRequest.pdc_id;
  response.src_fep = 0;
  response.dst_fep = 0;
  response.rx_pkt_handle = originalRequest.rx_pkt_handle;
  response.gtd_del = false;
  response.ses_nack = true;
  response.nack_payload.nack_code = static_cast<uint8_t> (errorCode);
  response.packet = Create<Packet> (1);
  response.rsp_len = 1;
  return response;
}

bool
SesManager::ValidateVersion (uint8_t version)
{
  return version == 1;
}

bool
SesManager::ValidateHeaderType (SESHeaderType type)
{
  switch (type)
    {
    case SESHeaderType::STANDARD_HEADER:
    case SESHeaderType::OPTIMIZED_HEADER:
    case SESHeaderType::SMALL_MESSAGE_RMA_HEADER:
    case SESHeaderType::SEMANTIC_RESPONSE_HEADER:
    case SESHeaderType::SEMANTIC_RESPONSE_WITH_DATA_HEADER:
    case SESHeaderType::OPTIMIZED_RESPONSE_WITH_DATA_HEADER:
      return true;
    default:
      return false;
    }
}

bool
SesManager::ValidatePidOnFep (uint32_t pidOnFep, uint32_t jobId, bool relative)
{
  if (jobId == 0)
    {
      return false;
    }
  if (relative)
    {
      return pidOnFep < 65536;
    }
  return pidOnFep != 0;
}

bool
SesManager::ValidateOperation (OpType opcode) const
{
  return opcode == OpType::SEND ||
         opcode == OpType::READ ||
         opcode == OpType::WRITE ||
         opcode == OpType::DEFERRABLE;
}

bool
SesManager::ValidateDataLength (size_t expectedLength, size_t actualLength)
{
  return expectedLength == actualLength;
}

bool
SesManager::ValidatePdcStatus (uint32_t pdcId, uint64_t psn)
{
  return pdcId != UINT32_MAX && psn < (1u << 31);
}

bool
SesManager::ValidateMemoryKey (uint64_t rkey, uint32_t) // messageId unused in simulation
{
  if (!m_memoryRegionValidator.IsNull ())
    {
      return m_memoryRegionValidator (rkey);
    }
  return m_memoryRegions.find (rkey) != m_memoryRegions.end ();
}

bool
SesManager::ValidateMsn (uint64_t jobId,
                         uint64_t psn,
                         uint64_t expectedLength,
                         uint32_t pdcId,
                         bool isFirstPacket,
                         bool isLastPacket)
{
  if (!m_msnTable)
    {
      return true;
    }
  return m_msnTable->ValidatePsn (jobId, psn, expectedLength, pdcId, isFirstPacket, isLastPacket);
}

SesManager::ValidationResult
SesManager::ValidateInboundHeader (const SoftUeHeaderTag& headerTag,
                                   const SoftUeMetadataTag& metadataTag) const
{
  ValidationResult result;
  if (headerTag.GetPdsType () != PDSType::RUD_REQ)
    {
      result.ok = false;
      result.returnCode = static_cast<uint8_t> (ResponseReturnCode::RC_PROTOCOL_ERROR);
      result.reasonText = "invalid_pds_type";
      return result;
    }
  if (!ValidateOperation (metadataTag.GetOperationType ()))
    {
      result.ok = false;
      result.returnCode = static_cast<uint8_t> (ResponseReturnCode::RC_INVALID_OP);
      result.reasonText = "invalid_operation";
      return result;
    }
  if (headerTag.GetPdcId () == UINT16_MAX ||
      headerTag.GetPsn () >= (1u << 31))
    {
      result.ok = false;
      result.returnCode = static_cast<uint8_t> (ResponseReturnCode::RC_PROTOCOL_ERROR);
      result.reasonText = "invalid_pdc_or_psn";
      return result;
    }
  if (m_validationStrictness == ValidationStrictness::STRICT)
    {
      if (headerTag.GetJobId () == 0 ||
          headerTag.GetSourceEndpoint () == 0 ||
          headerTag.GetDestinationEndpoint () == 0)
        {
          result.ok = false;
          result.returnCode = static_cast<uint8_t> (ResponseReturnCode::RC_PROTOCOL_ERROR);
          result.reasonText = "strict_header_validation_failed";
        }
    }
  return result;
}

SesManager::ValidationResult
SesManager::ValidateRmaRequest (const SoftUeHeaderTag& headerTag,
                                const SoftUeMetadataTag& metadataTag,
                                bool writeRequest,
                                uint32_t actualLength) const
{
  ValidationResult result = ValidateInboundHeader (headerTag, metadataTag);
  if (!result.ok)
    {
      return result;
    }

  if (!m_allowAllJobs && m_authorizedJobs.count (headerTag.GetJobId ()) == 0)
    {
      result.ok = false;
      result.returnCode = static_cast<uint8_t> (ResponseReturnCode::RC_ACCESS_DENIED);
      result.reasonText = "job_not_authorized";
      return result;
    }

  auto regionIt = m_memoryRegions.find (metadataTag.GetRemoteKey ());
  if (regionIt == m_memoryRegions.end ())
    {
      result.ok = false;
      result.returnCode = static_cast<uint8_t> (ResponseReturnCode::RC_INVALID_KEY);
      result.reasonText = "rkey_not_found";
      return result;
    }

  if (writeRequest ? !regionIt->second.writable : !regionIt->second.readable)
    {
      result.ok = false;
      result.returnCode = static_cast<uint8_t> (ResponseReturnCode::RC_ACCESS_DENIED);
      result.reasonText = "permission_denied";
      return result;
    }

  const uint64_t remoteAddr =
      writeRequest ? (metadataTag.GetRemoteAddress () + metadataTag.GetFragmentOffset ())
                   : metadataTag.GetRemoteAddress ();
  const uint64_t requestedLength =
      writeRequest ? std::max<uint32_t> (1u, actualLength) : metadataTag.GetRequestLength ();
  const uint64_t regionEnd = regionIt->second.startAddr + regionIt->second.length;
  if (remoteAddr < regionIt->second.startAddr || (remoteAddr + requestedLength) > regionEnd)
    {
      result.ok = false;
      result.returnCode = static_cast<uint8_t> (ResponseReturnCode::RC_ADDR_UNREACHABLE);
      result.reasonText = "address_out_of_range";
      return result;
    }

  return result;
}

bool
SesManager::ShouldSendAck (uint32_t, bool deliveryComplete)
{
  return deliveryComplete;
}

void
SesManager::DoPeriodicProcessing (void)
{
  if (!m_rudSemanticModeEnabled)
    {
      return;
    }

  const int64_t now = NowMs ();

  std::vector<uint32_t> pendingPeers;
  pendingPeers.reserve (m_pendingResponseQueues.size ());
  for (const auto& entry : m_pendingResponseQueues)
    {
      pendingPeers.push_back (entry.first);
    }
  for (uint32_t peerFep : pendingPeers)
    {
      DrainPendingResponseQueue (peerFep);
    }

  for (auto& entry : m_requestStates)
    {
      RequestLifecycleState& state = entry.second;
      if (state.terminalized || !state.waitingResponse || state.responseDeadlineMs == 0)
        {
          continue;
        }
      auto retryIt = m_retryStates.find (entry.first);
      if (retryIt != m_retryStates.end () && retryIt->second.event.IsPending ())
        {
          continue;
        }
      if (state.waitingResponseGeneration != state.attemptGeneration)
        {
          ++m_staleTimeoutSkippedTotal;
          state.responseDeadlineMs = 0;
          continue;
        }
      if (now < state.responseDeadlineMs)
        {
          continue;
        }
      if (!m_retryEnabled)
        {
          state.retryExhausted = true;
          RecordFailureSnapshot (entry.first,
                                 state.metadata ? state.metadata->op_type : OpType::SEND,
                                 static_cast<uint8_t> (ResponseReturnCode::RC_INTERNAL_ERROR),
                                 "response_timeout_exhausted",
                                 "retry_disabled_response_timeout_exhausted");
          SetRequestCompletionOutcome (
              entry.first,
              static_cast<uint8_t> (ResponseReturnCode::RC_INTERNAL_ERROR),
              0);
          TerminalizeRequest (entry.first, RequestTerminalReason::RESPONSE_TIMEOUT_EXHAUSTED);
          continue;
        }
      ScheduleRetry (entry.first, RetryTrigger::RESPONSE_TIMEOUT);
    }
  if (m_pdsManager)
    {
      m_pdsManager->PruneReceiveState ();
    }
  ScheduleProcessing ();
}

void
SesManager::ScheduleProcessing (void)
{
  if (!m_processEventId.IsPending ())
    {
      m_processEventId = Simulator::Schedule (m_processInterval, &SesManager::DoPeriodicProcessing, this);
    }
}

void
SesManager::LogDetailed (const std::string& function, const std::string& message) const
{
  if (m_detailedLogging)
    {
      NS_LOG_DEBUG (function << ": " << message);
    }
}

void
SesManager::RecordDiagnosticEvent (const std::string& name, const std::string& detail)
{
  SoftUeDiagnosticRecord event;
  event.name = name;
  event.detail = detail;
  event.timestamp_ns = NowNs ();
  event.node_id = (m_netDevice && m_netDevice->GetNode ()) ? m_netDevice->GetNode ()->GetId () : 0;
  event.if_index = m_netDevice ? m_netDevice->GetIfIndex () : 0;
  event.local_fep = m_netDevice ? m_netDevice->GetLocalFep () : 0;
  m_diagnosticEvents.push_back (event);
  if (m_diagnosticEvents.size () > kDiagnosticRecordLimit)
    {
      m_diagnosticEvents.erase (m_diagnosticEvents.begin (),
                                m_diagnosticEvents.begin () +
                                    (m_diagnosticEvents.size () - kDiagnosticRecordLimit));
    }
  if (m_netDevice)
    {
      m_netDevice->NotifyProtocolDiagnostic (event);
    }
  LogDetailed (name, detail);
}

void
SesManager::RecordFailureSnapshot (const RequestKey& key,
                                   OpType opcode,
                                   uint8_t returnCode,
                                   const std::string& stage,
                                   const std::string& diagnosticText)
{
  if (!m_debugSnapshotsEnabled)
    {
      return;
    }

  SoftUeFailureSnapshot snapshot;
  snapshot.timestamp_ns = NowNs ();
  snapshot.node_id = (m_netDevice && m_netDevice->GetNode ()) ? m_netDevice->GetNode ()->GetId () : 0;
  snapshot.if_index = m_netDevice ? m_netDevice->GetIfIndex () : 0;
  snapshot.local_fep = m_netDevice ? m_netDevice->GetLocalFep () : 0;
  snapshot.job_id = key.jobId;
  snapshot.msg_id = key.msgId;
  snapshot.peer_fep = key.peerFep;
  snapshot.opcode = opcode;
  snapshot.stage = stage;
  snapshot.return_code = returnCode;
  snapshot.failure_domain = ClassifyFailureDomain (returnCode, key, stage);
  snapshot.terminal_probe = QueryRequestTerminalProbe (key.jobId, key.msgId, key.peerFep);
  snapshot.tx_probe = QueryRequestTxProbe (key.jobId, key.msgId, key.peerFep);
  snapshot.unexpected_probe = QueryUnexpectedSendProbe (key.jobId, key.msgId, key.peerFep);
  snapshot.retry_probe = QuerySendRetryProbe (key.jobId, key.msgId, key.peerFep);
  snapshot.resource_stats = GetRudResourceStats ();
  snapshot.runtime_stats = GetRudRuntimeStats ();
  snapshot.diagnostic_text = diagnosticText;
  m_lastFailureSnapshot = snapshot;
  m_hasFailureSnapshot = true;
  if (m_netDevice)
    {
      m_netDevice->NotifyProtocolFailure (snapshot);
    }
}

void
SesManager::RecordSemanticStart (OpType opcode)
{
  ++m_semanticStats.ops_started_total;
  switch (opcode)
    {
    case OpType::SEND:
      ++m_semanticStats.send_started_total;
      break;
    case OpType::WRITE:
      ++m_semanticStats.write_started_total;
      break;
    case OpType::READ:
      ++m_semanticStats.read_started_total;
      break;
    default:
      break;
    }
}

void
SesManager::SetRequestCompletionOutcome (const RequestKey& key,
                                         uint8_t returnCode,
                                         uint32_t completedPayloadBytes)
{
  auto it = m_requestStates.find (key);
  if (it == m_requestStates.end ())
    {
      return;
    }
  it->second.finalReturnCode = returnCode;
  it->second.completedPayloadBytes = completedPayloadBytes;
}

void
SesManager::RecordCompletion (const RequestKey& key, const RequestLifecycleState& state)
{
  if (state.countedInSemanticStats)
    {
      return;
    }

  const bool success =
      state.terminalReason == RequestTerminalReason::RC_OK_RESPONSE &&
      state.finalReturnCode == static_cast<uint8_t> (ResponseReturnCode::RC_OK);
  const uint64_t latencyNs =
      (state.terminalizedAtNs >= state.createdAtNs) ? (state.terminalizedAtNs - state.createdAtNs) : 0;

  ++m_semanticStats.ops_terminal_total;
  if (success)
    {
      ++m_semanticStats.ops_success_total;
      switch (state.metadata ? state.metadata->op_type : OpType::SEND)
        {
        case OpType::SEND:
          ++m_semanticStats.send_success_total;
          m_semanticStats.send_success_bytes_total += state.completedPayloadBytes;
          break;
        case OpType::WRITE:
          ++m_semanticStats.write_success_total;
          m_semanticStats.write_success_bytes_total += state.completedPayloadBytes;
          break;
        case OpType::READ:
          ++m_semanticStats.read_success_total;
          m_semanticStats.read_success_bytes_total += state.completedPayloadBytes;
          break;
        default:
          break;
        }
      ++m_semanticStats.success_latency_samples;
      m_successLatencyTotalNs += latencyNs;
      m_semanticStats.success_latency_mean_ns =
          m_successLatencyTotalNs /
          std::max<uint64_t> (1u, m_semanticStats.success_latency_samples);
      m_semanticStats.success_latency_max_ns =
          std::max<uint64_t> (m_semanticStats.success_latency_max_ns, latencyNs);
    }
  else
    {
      ++m_semanticStats.ops_failed_total;
    }

  SoftUeCompletionRecord record;
  record.timestamp_ns = state.terminalizedAtNs;
  record.node_id = (m_netDevice && m_netDevice->GetNode ()) ? m_netDevice->GetNode ()->GetId () : 0;
  record.if_index = m_netDevice ? m_netDevice->GetIfIndex () : 0;
  record.local_fep = m_netDevice ? m_netDevice->GetLocalFep () : 0;
  record.job_id = key.jobId;
  record.msg_id = key.msgId;
  record.peer_fep = key.peerFep;
  record.opcode = state.metadata ? state.metadata->op_type : OpType::SEND;
  record.success = success;
  record.return_code = state.finalReturnCode;
  record.failure_domain =
      success ? "" : ClassifyFailureDomain (state.finalReturnCode, key, "completion");
  record.terminal_reason = state.terminalReason;
  record.payload_bytes = state.completedPayloadBytes;
  record.latency_ns = latencyNs;
  record.retry_count = state.lastRetryCount;
  const bool validSendStage =
      success &&
      record.opcode == OpType::SEND &&
      state.createdAtNs > 0 &&
      state.dispatchFirstAttemptNs >= state.createdAtNs &&
      state.dispatchAdmissionGrantedNs >= state.dispatchFirstAttemptNs &&
      state.dispatchFirstFragmentSentNs >= state.dispatchAdmissionGrantedNs &&
      state.receiveConsumeCompleteNs >= state.dispatchFirstFragmentSentNs &&
      state.responseVisibleNs >= state.receiveConsumeCompleteNs &&
      state.terminalizedAtNs >= state.responseVisibleNs;
  if (validSendStage)
    {
      record.send_stage_valid = true;
      record.send_stage_dispatch_ns = state.dispatchFirstFragmentSentNs - state.createdAtNs;
      record.send_stage_dispatch_wait_for_admission_ns =
          state.dispatchAdmissionGrantedNs - state.dispatchFirstAttemptNs;
      record.send_stage_dispatch_after_admission_to_first_send_ns =
          state.dispatchFirstFragmentSentNs - state.dispatchAdmissionGrantedNs;
      record.send_stage_inflight_ns =
          state.receiveConsumeCompleteNs - state.dispatchFirstFragmentSentNs;
      record.send_stage_receive_consume_ns =
          state.responseVisibleNs - state.receiveConsumeCompleteNs;
      record.send_stage_closeout_ns = state.terminalizedAtNs - state.responseVisibleNs;
      record.send_stage_end_to_end_ns = state.terminalizedAtNs - state.createdAtNs;
    }
  if (record.opcode == OpType::SEND)
    {
      record.send_stage_dispatch_attempt_count = state.dispatchAttemptCount;
      record.send_stage_dispatch_budget_block_count = state.dispatchBudgetBlockCount;
      record.send_stage_blocked_queue_enqueue_count = state.blockedQueueEnqueueCount;
      record.send_stage_blocked_queue_redispatch_count = state.blockedQueueRedispatchCount;
      record.send_stage_blocked_queue_wait_total_ns = state.blockedQueueWaitAccumulatedNs;
      record.send_stage_blocked_queue_wait_peak_ns = state.blockedQueueWaitPeakNs;
      record.send_stage_admission_release_seen_count = state.sendAdmissionReleaseSeenCount;
      record.send_stage_blocked_queue_wakeup_count = state.blockedQueueWakeupCount;
      record.send_stage_redispatch_fail_after_wakeup_count =
          state.redispatchFailAfterWakeupCount;
      record.send_stage_redispatch_success_after_wakeup_count =
          state.redispatchSuccessAfterWakeupCount;
      record.send_stage_admission_release_to_wakeup_total_ns =
          state.admissionReleaseToWakeupAccumulatedNs;
      record.send_stage_admission_release_to_wakeup_peak_ns =
          state.admissionReleaseToWakeupPeakNs;
      record.send_stage_wakeup_to_redispatch_total_ns =
          state.wakeupToRedispatchAccumulatedNs;
      record.send_stage_wakeup_to_redispatch_peak_ns =
          state.wakeupToRedispatchPeakNs;
    }
  const bool validReadStage =
      success &&
      record.opcode == OpType::READ &&
      state.createdAtNs > 0 &&
      state.dispatchFirstFragmentSentNs >= state.createdAtNs &&
      state.readResponseGeneratedNs >= state.dispatchFirstFragmentSentNs &&
      state.readResponseFirstVisibleNs >= state.readResponseGeneratedNs &&
      state.readResponseReassemblyCompleteNs >= state.readResponseFirstVisibleNs &&
      state.terminalizedAtNs >= state.readResponseReassemblyCompleteNs;
  if (validReadStage)
    {
      record.read_stage_valid = true;
      record.read_stage_responder_budget_generate_ns =
          state.readResponseGeneratedNs - state.dispatchFirstFragmentSentNs;
      record.read_stage_first_response_visible_ns =
          state.readResponseFirstVisibleNs - state.readResponseGeneratedNs;
      record.read_stage_reassembly_complete_ns =
          state.readResponseReassemblyCompleteNs - state.readResponseFirstVisibleNs;
      record.read_stage_terminal_ns =
          state.terminalizedAtNs - state.readResponseReassemblyCompleteNs;
      record.read_stage_end_to_end_ns =
          state.terminalizedAtNs - state.createdAtNs;
    }
  m_completionRecords.push_back (record);
  if (m_completionRecords.size () > kCompletionRecordLimit)
    {
      m_completionRecords.erase (m_completionRecords.begin (),
                                 m_completionRecords.begin () +
                                     (m_completionRecords.size () - kCompletionRecordLimit));
    }
  if (m_netDevice)
    {
      m_netDevice->NotifyProtocolCompletion (record);
    }
}

std::string
SesManager::ClassifyFailureDomain (uint8_t returnCode,
                                   const RequestKey& key,
                                   const std::string& stage) const
{
  if (returnCode == static_cast<uint8_t> (ResponseReturnCode::RC_INVALID_KEY) ||
      returnCode == static_cast<uint8_t> (ResponseReturnCode::RC_ACCESS_DENIED) ||
      returnCode == static_cast<uint8_t> (ResponseReturnCode::RC_ADDR_UNREACHABLE) ||
      returnCode == static_cast<uint8_t> (ResponseReturnCode::RC_PROTOCOL_ERROR) ||
      returnCode == static_cast<uint8_t> (ResponseReturnCode::RC_INVALID_OP))
    {
      return "validation";
    }

  if (returnCode == static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST))
    {
      return "resource";
    }

  if (stage == "credit_gate_blocked" ||
      stage == "send_admission_blocked" ||
      stage == "write_budget_blocked" ||
      stage == "read_response_budget_blocked" ||
      stage == "transport_window_blocked")
    {
      return "control_plane";
    }

  auto requestIt = m_requestStates.find (key);
  if (requestIt != m_requestStates.end () &&
      (requestIt->second.terminalReason == RequestTerminalReason::RETRY_EXHAUSTED ||
       requestIt->second.terminalReason == RequestTerminalReason::RESPONSE_TIMEOUT_EXHAUSTED))
    {
      return "packet_reliability";
    }

  if (returnCode == static_cast<uint8_t> (ResponseReturnCode::RC_NO_MATCH) ||
      stage.find ("unexpected") != std::string::npos ||
      stage.find ("message") != std::string::npos)
    {
      return "message_lifecycle";
    }

  if (GetRudResourceStats ().unexpected_alloc_failures != 0 ||
      GetRudResourceStats ().arrival_alloc_failures != 0 ||
      GetRudResourceStats ().read_track_alloc_failures != 0)
    {
      return "resource";
    }

  return "packet_reliability";
}

void
SesManager::LogError (const std::string& function, const std::string& error) const
{
  NS_LOG_ERROR (function << ": " << error);
  m_errorTrace (function + ": " + error);
}

bool
SesManager::ValidateOperationMetadata (Ptr<ExtendedOperationMetadata> metadata) const
{
  if (!metadata || !metadata->IsValid ())
    {
      return false;
    }
  if (!ValidateOperation (metadata->op_type))
    {
      return false;
    }
  if (metadata->job_id == 0 || metadata->messages_id == 0)
    {
      return false;
    }
  if (metadata->GetSourceNodeId () == 0 || metadata->GetDestinationNodeId () == 0)
    {
      return false;
    }
  if (!ValidateAuthorization (metadata))
    {
      return false;
    }
  if ((metadata->op_type == OpType::READ || metadata->op_type == OpType::WRITE) &&
      metadata->memory.rkey == 0)
    {
      return false;
    }
  if (metadata->payload.length == 0 && metadata->op_type != OpType::READ)
    {
      return false;
    }
  return true;
}

bool
SesManager::ValidateAuthorization (Ptr<ExtendedOperationMetadata> metadata) const
{
  if (!metadata)
    {
      return false;
    }
  if (!m_permissionChecker.IsNull ())
    {
      return m_permissionChecker (metadata->GetSourceNodeId (), metadata->job_id);
    }
  return m_allowAllJobs || m_authorizedJobs.count (metadata->job_id) != 0;
}

SesManager::SesState
SesManager::GetState (void) const
{
  return m_state;
}

bool
SesManager::IsBusy (void) const
{
  return m_state == SES_BUSY;
}

bool
SesManager::IsError (void) const
{
  return m_state == SES_ERROR;
}

void
SesManager::Reset (void)
{
  while (!m_requestQueue.empty ())
    {
      m_requestQueue.pop ();
    }
  while (!m_recvRequestQueue.empty ())
    {
      m_recvRequestQueue.pop ();
    }
  while (!m_recvResponseQueue.empty ())
    {
      m_recvResponseQueue.pop ();
    }

  for (auto& retry : m_retryStates)
    {
      if (retry.second.event.IsPending ())
        {
          Simulator::Cancel (retry.second.event);
        }
    }
  for (auto& request : m_requestStates)
    {
      if (request.second.dispatchEvent.IsPending ())
        {
          Simulator::Cancel (request.second.dispatchEvent);
        }
    }
  for (auto& blockedPeer : m_blockedPeerQueues)
    {
      if (blockedPeer.second.wakeupEvent.IsPending ())
        {
          Simulator::Cancel (blockedPeer.second.wakeupEvent);
        }
    }
  for (auto& pendingPeer : m_pendingResponseQueues)
    {
      if (pendingPeer.second.drainEvent.IsPending ())
        {
          Simulator::Cancel (pendingPeer.second.drainEvent);
        }
    }
  m_requestStates.clear ();
  m_retryStates.clear ();
  m_blockedPeerQueues.clear ();
  m_pendingResponses.clear ();
  m_pendingResponseQueues.clear ();
  m_diagnosticEvents.clear ();
  m_completionRecords.clear ();
  m_semanticStats = SoftUeSemanticStats ();
  m_successLatencyTotalNs = 0;
  m_blockedQueuePushTotal = 0;
  m_blockedQueueWakeupTotal = 0;
  m_blockedQueueDispatchTotal = 0;
  m_blockedQueueDepthMax = 0;
  m_blockedQueueWaitRecordedTotal = 0;
  m_blockedQueueWaitTotalNs = 0;
  m_blockedQueueWaitPeakNs = 0;
  m_blockedQueueWakeupDelayRecordedTotal = 0;
  m_blockedQueueWakeupDelayTotalNs = 0;
  m_blockedQueueWakeupDelayPeakNs = 0;
  m_sendAdmissionReleaseCount = 0;
  m_sendAdmissionReleaseBytesTotal = 0;
  m_blockedQueueRedispatchFailAfterWakeupTotal = 0;
  m_blockedQueueRedispatchSuccessAfterWakeupTotal = 0;
  m_peerQueueBlockedTotal = 0;
  m_pendingResponseEnqueueTotal = 0;
  m_pendingResponseRetryTotal = 0;
  m_pendingResponseDispatchFailuresTotal = 0;
  m_pendingResponseSuccessAfterRetryTotal = 0;
  m_staleTimeoutSkippedTotal = 0;
  m_staleRetrySkippedTotal = 0;
  m_lateResponseObservedTotal = 0;
  m_sendDuplicateOkAfterTerminalTotal = 0;
  m_lastFailureSnapshot = SoftUeFailureSnapshot ();
  m_hasFailureSnapshot = false;
  if (m_pdsManager)
    {
      m_pdsManager->ResetReceiveState ();
    }

  if (m_processEventId.IsPending ())
    {
      Simulator::Cancel (m_processEventId);
    }

  m_state = SES_IDLE;
  m_currentMessageId = 0;
}

bool
SesManager::ValidatePdcSesRequest (const PdcSesRequest& request) const
{
  if (!request.packet)
    {
      return false;
    }
  if (request.pdc_id == UINT16_MAX)
    {
      return false;
    }
  return request.packet->GetSize () <= GetPayloadMtu ();
}

bool
SesManager::ValidatePdcSesResponse (const PdcSesResponse& response) const
{
  return response.packet != nullptr;
}

bool
SesManager::ValidateSesPdsResponse (const SesPdsResponse& response) const
{
  return response.packet != nullptr;
}

bool
SesManager::ProcessDataOperationResponse (const PdcSesResponse& response)
{
  return ProcessReceiveResponse (response);
}

bool
SesManager::ProcessAtomicOperationResponse (const PdcSesResponse& response)
{
  return ProcessReceiveResponse (response);
}

int64_t
SesManager::NowMs (void) const
{
  return Simulator::Now ().GetMilliSeconds ();
}

uint32_t
SesManager::GetPayloadMtu (void) const
{
  if (m_payloadMtu > 0)
    {
      return m_payloadMtu;
    }
  return PayloadCapacityFromDeviceMtu (m_netDevice ? m_netDevice->GetMtu () : kDefaultDeviceMtu);
}

uint64_t
SesManager::NowNs (void) const
{
  return Simulator::Now ().GetNanoSeconds ();
}

SesManager::RequestKey
SesManager::BuildRequestKey (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const
{
  RequestKey key;
  key.jobId = jobId;
  key.msgId = msgId;
  key.peerFep = peerFep;
  return key;
}

SesManager::RequestKey
SesManager::BuildRequestKey (const SoftUeHeaderTag& headerTag, const SoftUeMetadataTag& metadataTag) const
{
  return BuildRequestKey (headerTag.GetJobId (),
                          static_cast<uint16_t> (metadataTag.GetMessageId ()),
                          headerTag.GetSourceEndpoint ());
}

bool
SesManager::TryGetPacketTags (Ptr<Packet> packet,
                              SoftUeHeaderTag* headerTag,
                              SoftUeMetadataTag* metadataTag) const
{
  if (!packet || !headerTag || !metadataTag)
    {
      return false;
    }
  return packet->PeekPacketTag (*headerTag) &&
         packet->PeekPacketTag (*metadataTag);
}

bool
SesManager::DispatchSemanticPacket (Ptr<ExtendedOperationMetadata> metadata, Ptr<Packet> packet)
{
  SesPdsRequest request = InitializeSesHeader (metadata);
  request.packet = packet;
  request.pkt_len = static_cast<uint16_t> (packet->GetSize ());
  return m_pdsManager->DispatchPacket (request);
}

bool
SesManager::TryReserveDispatchBudget (const RequestKey& key, RequestLifecycleState& state)
{
  if (!m_pdsManager || !state.metadata)
    {
      return false;
    }

  const uint32_t payloadBytes = static_cast<uint32_t> (state.metadata->payload.length);
  switch (state.metadata->op_type)
    {
    case OpType::SEND:
      if (state.sendAdmissionReserved)
        {
          return true;
        }
      if (!m_pdsManager->TryReserveSendAdmission (key.peerFep, payloadBytes))
        {
          return false;
        }
      state.sendAdmissionReserved = true;
      state.sendAdmissionReservedBytes = payloadBytes;
      return true;
    case OpType::WRITE:
      if (state.writeBudgetReserved)
        {
          return true;
        }
      if (!m_pdsManager->TryReserveWriteBudget (key.peerFep, payloadBytes))
        {
          return false;
        }
      state.writeBudgetReserved = true;
      state.writeBudgetReservedBytes = payloadBytes;
      return true;
    case OpType::READ:
      return true;
    default:
      return true;
    }
}

void
SesManager::ReleaseDispatchBudget (const RequestKey& key, RequestLifecycleState& state)
{
  if (!m_pdsManager)
    {
      return;
    }
  if (state.sendAdmissionReserved)
    {
      m_pdsManager->ReleaseSendAdmission (key.peerFep, state.sendAdmissionReservedBytes);
      state.sendAdmissionReserved = false;
      state.sendAdmissionReservedBytes = 0;
    }
  if (state.writeBudgetReserved)
    {
      m_pdsManager->ReleaseWriteBudget (key.peerFep, state.writeBudgetReservedBytes);
      state.writeBudgetReserved = false;
      state.writeBudgetReservedBytes = 0;
    }
}

bool
SesManager::DispatchRequestPackets (const RequestKey& key)
{
  auto it = m_requestStates.find (key);
  if (it == m_requestStates.end () || !it->second.metadata || !it->second.packet || !m_pdsManager)
    {
      return false;
    }

  RequestLifecycleState& state = it->second;
  Ptr<ExtendedOperationMetadata> metadata = state.metadata;
  Ptr<Packet> requestPacket = state.packet;
  ++state.dispatchAttemptCount;
  if (state.dispatchFirstAttemptNs == 0)
    {
      state.dispatchFirstAttemptNs = NowNs ();
    }
  const uint32_t packetCount = std::max<uint32_t> (1u, state.totalPacketCount);
  if (state.terminalized)
    {
      RemoveBlockedRequest (key);
      return false;
    }
  if (state.blockedQueueLinked && !IsBlockedQueueHead (key))
    {
      state.blockedOnCreditGate = true;
      return true;
    }
  if (!TryReserveDispatchBudget (key, state))
    {
      ++state.dispatchBudgetBlockCount;
      const std::string stage =
          (metadata->op_type == OpType::WRITE) ? "write_budget_blocked" : "send_admission_blocked";
      const char* reason = state.blockedQueueLinked
                               ? "dispatch_deferred_waiting_for_peer_queue"
                               : "dispatch_deferred_waiting_for_semantic_budget";
      RecordFailureSnapshot (key,
                             metadata->op_type,
                             static_cast<uint8_t> (ResponseReturnCode::RC_OK),
                             stage,
                             reason);
      if (state.blockedQueueLinked)
        {
          ++m_peerQueueBlockedTotal;
        }
      EnqueueBlockedRequest (key);
      state.blockedOnCreditGate = true;
      return true;
    }

  if (state.blockedQueueLinked)
    {
      RemoveBlockedRequest (key);
    }
  state.blockedOnCreditGate = false;
  if (state.dispatchAdmissionGrantedNs == 0)
    {
      state.dispatchAdmissionGrantedNs = NowNs ();
    }

  if (metadata->op_type == OpType::READ &&
      !state.readResponseTargetRegistered)
    {
      if (!m_pdsManager->RegisterReadResponseTarget (metadata->job_id,
                                                     static_cast<uint16_t> (metadata->messages_id),
                                                     metadata->GetDestinationNodeId (),
                                                     metadata->payload.start_addr,
                                                     static_cast<uint32_t> (metadata->payload.length)))
        {
          RecordResponseEvent (key.jobId,
                               key.msgId,
                               static_cast<uint8_t> (ResponseOpCode::UET_NACK),
                               static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST),
                               0);
          RecordFailureSnapshot (key,
                                 metadata->op_type,
                                 static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST),
                                 "read_track_exhausted",
                                 "register_read_response_target_failed");
          SetRequestCompletionOutcome (key,
                                       static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST),
                                       0);
          TerminalizeRequest (key, RequestTerminalReason::PROTOCOL_ERROR);
          return false;
        }
      state.readResponseTargetRegistered = true;
    }

  const uint32_t payloadPerPacket = GetPayloadMtu ();
  const uint32_t totalLength = static_cast<uint32_t> (metadata->payload.length);
  for (uint32_t i = 0; i < packetCount; ++i)
    {
      const uint32_t offset = i * payloadPerPacket;
      const uint32_t fragLen =
          (metadata->op_type == OpType::READ)
              ? requestPacket->GetSize ()
              : std::min<uint32_t> (payloadPerPacket, totalLength - offset);
      Ptr<Packet> fragment = (packetCount == 1 || metadata->op_type == OpType::READ)
                                 ? requestPacket->Copy ()
                                 : requestPacket->CreateFragment (offset, fragLen);
      AttachSemanticTags (fragment,
                          metadata,
                          offset,
                          totalLength,
                          false,
                          0,
                          0,
                          totalLength);
      if (packetCount > 1)
        {
          fragment->AddPacketTag (SoftUeFragmentTag (i + 1, packetCount));
        }
      fragment->AddPacketTag (SoftUeTransactionTag (1, 1));
      fragment->AddPacketTag (SoftUeTimingTag (Simulator::Now ()));

      if (!DispatchSemanticPacket (metadata, fragment))
        {
          ReleaseDispatchBudget (key, state);
          return false;
        }
      if (state.dispatchFirstFragmentSentNs == 0)
        {
          state.dispatchFirstFragmentSentNs = NowNs ();
        }
      m_totalPacketsGenerated++;
    }
  ++state.attemptGeneration;
  state.dispatchStarted = true;
  state.waitingResponse = metadata->expect_response;
  state.waitingResponseGeneration = state.waitingResponse ? state.attemptGeneration : 0;
  ArmResponseTimeout (state);
  state.lastTxProgressMs = NowMs ();
  if (metadata->op_type == OpType::SEND && metadata->expect_response)
    {
      ++m_sendDispatchStartedTotal;
      RecordDiagnosticEvent ("SendDispatchStarted",
                             "job=" + std::to_string (key.jobId) +
                             " msg=" + std::to_string (key.msgId) +
                             " peer=" + std::to_string (key.peerFep) +
                             " gen=" + std::to_string (state.attemptGeneration) +
                             " packets=" + std::to_string (packetCount));
    }
  return true;
}

void
SesManager::ScheduleBlockedPeerWakeup (uint32_t peerFep)
{
  auto queueIt = m_blockedPeerQueues.find (peerFep);
  if (queueIt == m_blockedPeerQueues.end () || queueIt->second.requests.empty ())
    {
      return;
    }
  if (queueIt->second.wakeupEvent.IsPending ())
    {
      return;
    }
  queueIt->second.wakeupEvent =
      Simulator::ScheduleNow (&SesManager::DrainBlockedPeerQueue, this, peerFep);
}

void
SesManager::DrainBlockedPeerQueue (uint32_t peerFep)
{
  auto queueIt = m_blockedPeerQueues.find (peerFep);
  if (queueIt == m_blockedPeerQueues.end ())
    {
      return;
    }

  ++m_blockedQueueWakeupTotal;
  if (queueIt->second.lastWakeSignalNs != 0)
    {
      const uint64_t wakeDelayNs =
          Simulator::Now ().GetNanoSeconds () - queueIt->second.lastWakeSignalNs;
      ++m_blockedQueueWakeupDelayRecordedTotal;
      m_blockedQueueWakeupDelayTotalNs += wakeDelayNs;
      m_blockedQueueWakeupDelayPeakNs = std::max (m_blockedQueueWakeupDelayPeakNs, wakeDelayNs);
      queueIt->second.lastWakeSignalNs = 0;
    }
  while (!queueIt->second.requests.empty ())
    {
      const RequestKey key = queueIt->second.requests.front ();
      auto requestIt = m_requestStates.find (key);
      if (requestIt == m_requestStates.end () || requestIt->second.terminalized)
        {
          RemoveBlockedRequest (key);
          queueIt = m_blockedPeerQueues.find (peerFep);
          if (queueIt == m_blockedPeerQueues.end ())
            {
              return;
            }
          continue;
        }

      const uint64_t wakeNs = NowNs ();
      ++requestIt->second.blockedQueueWakeupCount;
      requestIt->second.lastBlockedQueueWakeupNs = wakeNs;
      if (requestIt->second.lastAdmissionReleaseVisibleNs != 0)
        {
          const uint64_t releaseToWakeupNs = wakeNs - requestIt->second.lastAdmissionReleaseVisibleNs;
          requestIt->second.admissionReleaseToWakeupAccumulatedNs += releaseToWakeupNs;
          requestIt->second.admissionReleaseToWakeupPeakNs =
              std::max (requestIt->second.admissionReleaseToWakeupPeakNs, releaseToWakeupNs);
        }

      if (requestIt->second.blockedQueueEnqueueNs != 0)
        {
          const uint64_t waitNs =
              Simulator::Now ().GetNanoSeconds () - requestIt->second.blockedQueueEnqueueNs;
          ++m_blockedQueueWaitRecordedTotal;
          m_blockedQueueWaitTotalNs += waitNs;
          m_blockedQueueWaitPeakNs = std::max (m_blockedQueueWaitPeakNs, waitNs);
          requestIt->second.blockedQueueWaitAccumulatedNs += waitNs;
          requestIt->second.blockedQueueWaitPeakNs =
              std::max (requestIt->second.blockedQueueWaitPeakNs, waitNs);
          requestIt->second.blockedQueueEnqueueNs = 0;
        }

      ++m_blockedQueueDispatchTotal;
      ++requestIt->second.blockedQueueRedispatchCount;
      const uint64_t redispatchNs = NowNs ();
      if (requestIt->second.lastBlockedQueueWakeupNs != 0)
        {
          const uint64_t wakeupToRedispatchNs = redispatchNs - requestIt->second.lastBlockedQueueWakeupNs;
          requestIt->second.wakeupToRedispatchAccumulatedNs += wakeupToRedispatchNs;
          requestIt->second.wakeupToRedispatchPeakNs =
              std::max (requestIt->second.wakeupToRedispatchPeakNs, wakeupToRedispatchNs);
        }
      const bool dispatched = DispatchRequestPackets (key);
      auto updatedIt = m_requestStates.find (key);
      if (!dispatched)
        {
          HandleDispatchFailure (key);
          queueIt = m_blockedPeerQueues.find (peerFep);
          if (queueIt == m_blockedPeerQueues.end ())
            {
              return;
            }
          continue;
        }

      if (updatedIt != m_requestStates.end () && !updatedIt->second.terminalized)
        {
          const bool stillBlocked =
              updatedIt->second.blockedQueueLinked || updatedIt->second.blockedOnCreditGate;
          if (stillBlocked)
            {
              ++updatedIt->second.redispatchFailAfterWakeupCount;
              ++m_blockedQueueRedispatchFailAfterWakeupTotal;
            }
          else
            {
              ++updatedIt->second.redispatchSuccessAfterWakeupCount;
              ++m_blockedQueueRedispatchSuccessAfterWakeupTotal;
            }
        }

      if (updatedIt == m_requestStates.end ())
        {
          queueIt = m_blockedPeerQueues.find (peerFep);
          if (queueIt == m_blockedPeerQueues.end ())
            {
              return;
            }
          continue;
        }

      if (updatedIt->second.blockedQueueLinked || updatedIt->second.blockedOnCreditGate)
        {
          break;
        }

      if (!updatedIt->second.metadata->expect_response && updatedIt->second.dispatchStarted)
        {
          SetRequestCompletionOutcome (key,
                                       static_cast<uint8_t> (ResponseReturnCode::RC_OK),
                                       static_cast<uint32_t> (updatedIt->second.metadata->payload.length));
          TerminalizeRequest (key, RequestTerminalReason::RC_OK_RESPONSE);
        }

      queueIt = m_blockedPeerQueues.find (peerFep);
      if (queueIt == m_blockedPeerQueues.end ())
        {
          return;
        }
    }
}

void
SesManager::EnqueueBlockedRequest (const RequestKey& key)
{
  auto requestIt = m_requestStates.find (key);
  if (requestIt == m_requestStates.end ())
    {
      return;
    }
  RequestLifecycleState& state = requestIt->second;
  if (state.blockedQueueLinked)
    {
      return;
    }

  BlockedPeerQueueState& queue = m_blockedPeerQueues[key.peerFep];
  queue.requests.push_back (key);
  state.blockedQueueLinked = true;
  state.blockedOnCreditGate = true;
  state.blockedQueueEnqueueNs = Simulator::Now ().GetNanoSeconds ();
  ++state.blockedQueueEnqueueCount;
  ++m_blockedQueuePushTotal;
  m_blockedQueueDepthMax = std::max<uint32_t> (m_blockedQueueDepthMax,
                                               static_cast<uint32_t> (queue.requests.size ()));
}

void
SesManager::RemoveBlockedRequest (const RequestKey& key)
{
  auto requestIt = m_requestStates.find (key);
  if (requestIt != m_requestStates.end ())
    {
      requestIt->second.blockedQueueLinked = false;
      requestIt->second.blockedOnCreditGate = false;
      requestIt->second.blockedQueueEnqueueNs = 0;
    }

  auto queueIt = m_blockedPeerQueues.find (key.peerFep);
  if (queueIt == m_blockedPeerQueues.end ())
    {
      return;
    }

  auto& requests = queueIt->second.requests;
  for (auto it = requests.begin (); it != requests.end (); ++it)
    {
      if (*it == key)
        {
          requests.erase (it);
          break;
        }
    }
  if (requests.empty ())
    {
      if (queueIt->second.wakeupEvent.IsPending ())
        {
          Simulator::Cancel (queueIt->second.wakeupEvent);
        }
      m_blockedPeerQueues.erase (queueIt);
    }
}

bool
SesManager::HasBlockedRequestsForPeer (uint32_t peerFep) const
{
  auto it = m_blockedPeerQueues.find (peerFep);
  return it != m_blockedPeerQueues.end () && !it->second.requests.empty ();
}

bool
SesManager::IsBlockedQueueHead (const RequestKey& key) const
{
  auto it = m_blockedPeerQueues.find (key.peerFep);
  return it != m_blockedPeerQueues.end () &&
         !it->second.requests.empty () &&
         it->second.requests.front () == key;
}

void
SesManager::HandleDispatchFailure (const RequestKey& key)
{
  auto it = m_requestStates.find (key);
  if (it == m_requestStates.end () || it->second.terminalized)
    {
      RemoveBlockedRequest (key);
      return;
    }
  RecordFailureSnapshot (key,
                         it->second.metadata ? it->second.metadata->op_type : OpType::SEND,
                         static_cast<uint8_t> (ResponseReturnCode::RC_INTERNAL_ERROR),
                         "dispatch_failed",
                         "dispatch_request_packets_failed");
  SetRequestCompletionOutcome (key,
                               static_cast<uint8_t> (ResponseReturnCode::RC_INTERNAL_ERROR),
                               0);
  TerminalizeRequest (key, RequestTerminalReason::PROTOCOL_ERROR);
  ++m_totalErrors;
}

void
SesManager::AttachSemanticTags (Ptr<Packet> packet,
                                Ptr<ExtendedOperationMetadata> metadata,
                                uint32_t fragmentOffset,
                                uint32_t requestLength,
                                bool isResponse,
                                uint8_t responseOpcode,
                                uint8_t returnCode,
                                uint32_t modifiedLength) const
{
  SoftUeHeaderTag headerTag;
  packet->RemovePacketTag (headerTag);
  headerTag.SetPdsType (isResponse ? PDSType::RUDI_RESP : PDSType::RUD_REQ);
  headerTag.SetPdcId (0);
  headerTag.SetPsn (0);
  headerTag.SetSourceEndpoint (metadata->GetSourceNodeId ());
  headerTag.SetDestinationEndpoint (metadata->GetDestinationNodeId ());
  headerTag.SetJobId (metadata->job_id);
  packet->AddPacketTag (headerTag);

  SoftUeMetadataTag metadataTag;
  packet->RemovePacketTag (metadataTag);
  metadataTag = SoftUeMetadataTag (*metadata);
  metadataTag.SetMessageId (metadata->messages_id);
  metadataTag.SetRequestLength (requestLength);
  metadataTag.SetFragmentOffset (fragmentOffset);
  metadataTag.SetRemoteAddress (metadata->op_type == OpType::READ ? metadata->payload.imm_data
                                                                  : metadata->payload.start_addr);
  metadataTag.SetRemoteKey (metadata->memory.rkey);
  metadataTag.SetIsRetry (metadata->is_retry);
  metadataTag.SetIsResponse (isResponse);
  metadataTag.SetResponseOpCode (responseOpcode);
  metadataTag.SetReturnCode (returnCode);
  metadataTag.SetModifiedLength (modifiedLength);
  packet->AddPacketTag (metadataTag);
}

bool
SesManager::HandleIncomingRequest (const PdcSesRequest& request,
                                   const SoftUeHeaderTag& headerTag,
                                   const SoftUeMetadataTag& metadataTag)
{
  const RequestKey key = BuildRequestKey (headerTag, metadataTag);
  const ValidationResult headerValidation = ValidateInboundHeader (headerTag, metadataTag);
  if (!headerValidation.ok)
    {
      RecordFailureSnapshot (key,
                             metadataTag.GetOperationType (),
                             headerValidation.returnCode,
                             "inbound_header_validation_failed",
                             headerValidation.reasonText);
      SendSemanticResponseForRequest (key,
                                      headerTag.GetDestinationEndpoint (),
                                      headerTag.GetSourceEndpoint (),
                                      static_cast<uint8_t> (ResponseOpCode::UET_NACK),
                                      headerValidation.returnCode,
                                      0);
      return false;
    }

  switch (metadataTag.GetOperationType ())
    {
    case OpType::SEND:
      return HandleIncomingSendRequest (key, request, metadataTag);
    case OpType::WRITE:
      return HandleIncomingWriteRequest (key, request, metadataTag);
    case OpType::READ:
      return HandleIncomingReadRequest (key, headerTag, metadataTag);
    default:
      RecordFailureSnapshot (key,
                             metadataTag.GetOperationType (),
                             static_cast<uint8_t> (ResponseReturnCode::RC_INVALID_OP),
                             "unsupported_inbound_operation",
                             "unsupported_operation_type");
      SendSemanticResponseForRequest (key,
                                      headerTag.GetDestinationEndpoint (),
                                      headerTag.GetSourceEndpoint (),
                                      static_cast<uint8_t> (ResponseOpCode::UET_NACK),
                                      static_cast<uint8_t> (ResponseReturnCode::RC_INVALID_OP),
                                      0);
      return false;
    }
}

bool
SesManager::HandleIncomingSendRequest (const RequestKey& key,
                                       const PdcSesRequest& request,
                                       const SoftUeMetadataTag& metadataTag)
{
  const uint32_t localEndpoint = m_netDevice ? m_netDevice->GetLocalFep () : 0;
  if (!m_pdsManager)
    {
      return false;
    }

  SoftUeHeaderTag headerTag;
  if (!request.packet || !request.packet->PeekPacketTag (headerTag))
    {
      return false;
    }

  const PdcRxSemanticResult rxResult =
      m_pdsManager->HandleIncomingSendPacket (request.pdc_id, headerTag, metadataTag, request.packet);
  if (!rxResult.handled || !rxResult.response_ready)
    {
      return rxResult.handled;
    }

  if (rxResult.return_code != static_cast<uint8_t> (ResponseReturnCode::RC_OK))
    {
      const std::string stage =
          rxResult.return_code == static_cast<uint8_t> (ResponseReturnCode::RC_NO_MATCH)
              ? "send_unexpected_no_match"
              : (rxResult.return_code == static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST)
                     ? "send_receive_resource_exhaust"
                     : "send_receive_error");
      RecordFailureSnapshot (key, OpType::SEND, rxResult.return_code, stage, "incoming_send_request_failed");
    }

  SendSemanticResponseForRequest (key,
                                  localEndpoint,
                                  key.peerFep,
                                  rxResult.response_opcode,
                                  rxResult.return_code,
                                  rxResult.modified_length,
                                  nullptr,
                                  false,
                                  0,
                                  0,
                                  rxResult.receive_consume_complete_ns);
  return true;
}

bool
SesManager::HandleIncomingWriteRequest (const RequestKey& key,
                                        const PdcSesRequest& request,
                                        const SoftUeMetadataTag& metadataTag)
{
  const uint32_t localEndpoint = m_netDevice ? m_netDevice->GetLocalFep () : 0;
  if (!m_pdsManager)
    {
      return false;
    }
  SoftUeHeaderTag headerTag;
  request.packet->PeekPacketTag (headerTag);
  const ValidationResult validation =
      ValidateRmaRequest (headerTag, metadataTag, true, request.packet ? request.packet->GetSize () : 0);
  if (!validation.ok)
    {
      RecordFailureSnapshot (key, OpType::WRITE, validation.returnCode, "write_validation_failed", validation.reasonText);
      SendSemanticResponseForRequest (key,
                                      localEndpoint,
                                      key.peerFep,
                                      static_cast<uint8_t> (ResponseOpCode::UET_NACK),
                                      validation.returnCode,
                                      0);
      return false;
    }

  if (!m_pdsManager->TryReserveWriteBudget (key.peerFep, request.packet->GetSize ()))
    {
      RecordFailureSnapshot (key,
                             OpType::WRITE,
                             static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST),
                             "write_budget_blocked",
                             "incoming_write_budget_exhausted");
      SendSemanticResponseForRequest (key,
                                      localEndpoint,
                                      key.peerFep,
                                      static_cast<uint8_t> (ResponseOpCode::UET_NACK),
                                      static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST),
                                      0);
      return false;
    }

  const uint64_t targetAddr = metadataTag.GetRemoteAddress () + metadataTag.GetFragmentOffset ();
  WritePacketBytesToAddress (request.packet, targetAddr, 0, request.packet->GetSize ());
  m_pdsManager->ReleaseWriteBudget (key.peerFep, request.packet->GetSize ());
  SendSemanticResponseForRequest (key,
                                  localEndpoint,
                                  key.peerFep,
                                  static_cast<uint8_t> (ResponseOpCode::UET_DEFAULT_RESPONSE),
                                  static_cast<uint8_t> (ResponseReturnCode::RC_OK),
                                  request.packet->GetSize ());
  return true;
}

bool
SesManager::HandleIncomingReadRequest (const RequestKey& key,
                                       const SoftUeHeaderTag& headerTag,
                                       const SoftUeMetadataTag& metadataTag)
{
  if (!m_pdsManager)
    {
      return false;
    }
  const ValidationResult validation = ValidateRmaRequest (headerTag, metadataTag, false, 0);
  if (!validation.ok)
    {
      RecordFailureSnapshot (key, OpType::READ, validation.returnCode, "read_validation_failed", validation.reasonText);
      SendSemanticResponseForRequest (key,
                                      headerTag.GetDestinationEndpoint (),
                                      key.peerFep,
                                      static_cast<uint8_t> (ResponseOpCode::UET_NACK),
                                      validation.returnCode,
                                      0);
      return false;
    }

  const uint64_t readAddr = metadataTag.GetRemoteAddress ();
  const uint32_t readLen = metadataTag.GetRequestLength ();
  Ptr<Packet> payload = Create<Packet> (readLen);
  if (readLen != 0)
    {
      std::vector<uint8_t> bytes (readLen);
      std::memcpy (bytes.data (), reinterpret_cast<const void*> (readAddr), readLen);
      payload = Create<Packet> (bytes.data (), bytes.size ());
    }

  if (!m_pdsManager->TryReserveReadResponderBudget (key.peerFep, readLen))
    {
      RecordFailureSnapshot (key,
                             OpType::READ,
                             static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST),
                             "read_response_budget_blocked",
                             "incoming_read_response_budget_exhausted");
      SendSemanticResponseForRequest (key,
                                      headerTag.GetDestinationEndpoint (),
                                      key.peerFep,
                                      static_cast<uint8_t> (ResponseOpCode::UET_NACK),
                                      static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST),
                                      0);
      return false;
    }

  const uint64_t responseGeneratedNs = NowNs ();
  SendSemanticResponseForRequest (key,
                                  headerTag.GetDestinationEndpoint (),
                                  key.peerFep,
                                  static_cast<uint8_t> (ResponseOpCode::UET_RESPONSE_W_DATA),
                                  static_cast<uint8_t> (ResponseReturnCode::RC_OK),
                                  readLen,
                                  payload,
                                  true,
                                  key.peerFep,
                                  readLen,
                                  0,
                                  responseGeneratedNs);
  return true;
}

bool
SesManager::TryDispatchPendingResponse (const RequestKey& key, PendingResponseState& state)
{
  if (!m_pdsManager)
    {
      return false;
    }

  if (!state.metadata)
    {
      return false;
    }

  if (m_failNextResponseDispatchesForTesting > 0)
    {
      --m_failNextResponseDispatchesForTesting;
      ++m_pendingResponseDispatchFailuresTotal;
      return false;
    }

  Ptr<Packet> responsePacket = state.payload ? state.payload->Copy () : Create<Packet> (1);
  const uint32_t payloadPerPacket = GetPayloadMtu ();
  const uint32_t totalLength = state.responseWithData ? state.modifiedLength : responsePacket->GetSize ();
  const uint32_t packetCount = state.packetCount == 0 ? 1u : state.packetCount;

  for (uint32_t i = 0; i < packetCount; ++i)
    {
      const uint32_t offset = i * payloadPerPacket;
      const uint32_t fragLen = (!state.responseWithData || packetCount == 1)
                                   ? responsePacket->GetSize ()
                                   : std::min<uint32_t> (payloadPerPacket, totalLength - offset);
      Ptr<Packet> fragment =
          (!state.responseWithData || packetCount == 1)
              ? responsePacket->Copy ()
              : responsePacket->CreateFragment (offset, fragLen);
      AttachSemanticTags (fragment,
                          state.metadata,
                          offset,
                          state.modifiedLength,
                          true,
                          state.responseOpcode,
                          state.returnCode,
                          state.modifiedLength);
      if (packetCount > 1)
        {
          fragment->AddPacketTag (SoftUeFragmentTag (i + 1, packetCount));
        }
      fragment->AddPacketTag (SoftUeTransactionTag (1, 1));
      SoftUeTimingTag timingTag (Simulator::Now ());
      if (state.responseWithData && state.responseGeneratedNs > 0)
        {
          timingTag.SetAuxTimestamp (NanoSeconds (state.responseGeneratedNs));
        }
      else if (state.responderReceiveConsumeCompleteNs > 0)
        {
          timingTag.SetAuxTimestamp (NanoSeconds (state.responderReceiveConsumeCompleteNs));
        }
      fragment->AddPacketTag (timingTag);
      if (!DispatchSemanticPacket (state.metadata, fragment))
        {
          ++m_pendingResponseDispatchFailuresTotal;
          return false;
        }
    }

  if (state.retryCount > 0)
    {
      ++m_pendingResponseSuccessAfterRetryTotal;
    }
  state.terminalized = true;
  state.enqueued = false;
  return true;
}

void
SesManager::EnqueuePendingResponse (const RequestKey& key)
{
  auto responseIt = m_pendingResponses.find (key);
  if (responseIt == m_pendingResponses.end ())
    {
      return;
    }
  PendingResponseState& state = responseIt->second;
  if (state.enqueued)
    {
      return;
    }

  PendingResponseQueueState& queue = m_pendingResponseQueues[key.peerFep];
  queue.responses.push_back (key);
  state.enqueued = true;
  state.nextRetryMs = NowMs ();
  ++m_pendingResponseEnqueueTotal;
  SchedulePendingResponseDrain (key.peerFep, true);
}

void
SesManager::RemovePendingResponse (const RequestKey& key)
{
  auto responseIt = m_pendingResponses.find (key);
  if (responseIt != m_pendingResponses.end ())
    {
      ReleasePendingResponseBudget (responseIt->second);
    }

  auto queueIt = m_pendingResponseQueues.find (key.peerFep);
  if (queueIt != m_pendingResponseQueues.end ())
    {
      auto& responses = queueIt->second.responses;
      for (auto it = responses.begin (); it != responses.end (); ++it)
        {
          if (*it == key)
            {
              responses.erase (it);
              break;
            }
        }
      if (responses.empty ())
        {
          if (queueIt->second.drainEvent.IsPending ())
            {
              Simulator::Cancel (queueIt->second.drainEvent);
            }
          m_pendingResponseQueues.erase (queueIt);
        }
  }
  m_pendingResponses.erase (key);
}

void
SesManager::SchedulePendingResponseDrain (uint32_t peerFep, bool immediate)
{
  auto queueIt = m_pendingResponseQueues.find (peerFep);
  if (queueIt == m_pendingResponseQueues.end () || queueIt->second.responses.empty ())
    {
      return;
    }
  if (queueIt->second.drainEvent.IsPending ())
    {
      return;
    }
  queueIt->second.drainEvent = immediate
                                   ? Simulator::ScheduleNow (&SesManager::DrainPendingResponseQueue,
                                                             this,
                                                             peerFep)
                                   : Simulator::Schedule (m_processInterval,
                                                          &SesManager::DrainPendingResponseQueue,
                                                          this,
                                                          peerFep);
}

void
SesManager::DrainPendingResponseQueue (uint32_t peerFep)
{
  auto queueIt = m_pendingResponseQueues.find (peerFep);
  if (queueIt == m_pendingResponseQueues.end ())
    {
      return;
    }

  const int64_t now = NowMs ();
  while (!queueIt->second.responses.empty ())
    {
      const RequestKey key = queueIt->second.responses.front ();
      auto responseIt = m_pendingResponses.find (key);
      if (responseIt == m_pendingResponses.end () || responseIt->second.terminalized)
        {
          RemovePendingResponse (key);
          queueIt = m_pendingResponseQueues.find (peerFep);
          if (queueIt == m_pendingResponseQueues.end ())
            {
              return;
            }
          continue;
        }

      PendingResponseState& state = responseIt->second;
      if (state.nextRetryMs > now)
        {
          break;
        }

      if (TryDispatchPendingResponse (key, state))
        {
          RemovePendingResponse (key);
          queueIt = m_pendingResponseQueues.find (peerFep);
          if (queueIt == m_pendingResponseQueues.end ())
            {
              return;
            }
          continue;
        }

      ++m_pendingResponseRetryTotal;
      ++state.retryCount;
      state.nextRetryMs = now + m_processInterval.GetMilliSeconds ();
      break;
    }

  queueIt = m_pendingResponseQueues.find (peerFep);
  if (queueIt != m_pendingResponseQueues.end () && !queueIt->second.responses.empty ())
    {
      SchedulePendingResponseDrain (peerFep, false);
    }
}

void
SesManager::ReleasePendingResponseBudget (PendingResponseState& state)
{
  if (!m_pdsManager || !state.readResponderBudgetReserved)
    {
      return;
    }
  m_pdsManager->ReleaseReadResponderBudget (state.readResponderBudgetPeerFep,
                                           state.readResponderBudgetBytes);
  state.readResponderBudgetReserved = false;
  state.readResponderBudgetPeerFep = 0;
  state.readResponderBudgetBytes = 0;
}

void
SesManager::SendSemanticResponseForRequest (const RequestKey& key,
                                            uint32_t localFep,
                                            uint32_t remoteFep,
                                            uint8_t responseOpcode,
                                            uint8_t returnCode,
                                            uint32_t modifiedLength,
                                            Ptr<Packet> payload,
                                            bool readResponderBudgetReserved,
                                            uint32_t readResponderBudgetPeerFep,
                                            uint32_t readResponderBudgetBytes,
                                            uint64_t responderReceiveConsumeCompleteNs,
                                            uint64_t responseGeneratedNs)
{
  if (!m_pdsManager)
    {
      return;
    }

  PendingResponseState& state = m_pendingResponses[key];
  state = PendingResponseState ();
  state.metadata = Create<ExtendedOperationMetadata> ();
  state.metadata->op_type = responseOpcode == static_cast<uint8_t> (ResponseOpCode::UET_RESPONSE_W_DATA)
                                ? OpType::READ
                                : OpType::SEND;
  state.metadata->job_id = static_cast<uint32_t> (key.jobId);
  state.metadata->messages_id = key.msgId;
  state.metadata->payload.length = modifiedLength;
  state.metadata->delivery_mode = static_cast<uint8_t> (DeliveryMode::RUD);
  state.metadata->expect_response = false;
  state.metadata->SetSourceEndpoint (localFep, 1);
  state.metadata->SetDestinationEndpoint (remoteFep, 1);
  state.responseOpcode = responseOpcode;
  state.returnCode = returnCode;
  state.modifiedLength = modifiedLength;
  state.responseWithData =
      responseOpcode == static_cast<uint8_t> (ResponseOpCode::UET_RESPONSE_W_DATA);
  state.readResponderBudgetReserved = readResponderBudgetReserved;
  state.readResponderBudgetPeerFep = readResponderBudgetPeerFep;
  state.readResponderBudgetBytes = readResponderBudgetBytes;
  state.responderReceiveConsumeCompleteNs = responderReceiveConsumeCompleteNs;
  state.responseGeneratedNs = responseGeneratedNs;
  state.payload = payload ? payload->Copy () : Create<Packet> (1);
  const uint32_t payloadPerPacket = GetPayloadMtu ();
  const uint32_t totalLength =
      state.responseWithData ? modifiedLength : state.payload->GetSize ();
  state.packetCount =
      (!state.responseWithData || totalLength == 0)
          ? 1u
          : std::max<uint32_t> (1u, (totalLength + payloadPerPacket - 1) / payloadPerPacket);

  if (TryDispatchPendingResponse (key, state))
    {
      RemovePendingResponse (key);
      return;
    }

  EnqueuePendingResponse (key);
}

void
SesManager::RecordResponseEvent (uint64_t jobId,
                                 uint16_t msgId,
                                 uint8_t opcode,
                                 uint8_t returnCode,
                                 uint32_t modifiedLength)
{
  ResponseEvent event;
  event.job_id = jobId;
  event.msg_id = msgId;
  event.opcode = opcode;
  event.return_code = returnCode;
  event.modified_length = modifiedLength;
  event.observed_at_ms = NowMs ();
  m_responseHistory.push_back (event);
}

void
SesManager::TerminalizeRequest (const RequestKey& key, RequestTerminalReason reason)
{
  auto it = m_requestStates.find (key);
  if (it == m_requestStates.end ())
    {
      return;
    }
  auto retryIt = m_retryStates.find (key);
  if (retryIt != m_retryStates.end ())
    {
      if (retryIt->second.event.IsPending ())
        {
          Simulator::Cancel (retryIt->second.event);
        }
      m_retryStates.erase (retryIt);
    }
  if (it->second.dispatchEvent.IsPending ())
    {
      Simulator::Cancel (it->second.dispatchEvent);
    }
  RemoveBlockedRequest (key);
  ReleaseDispatchBudget (key, it->second);
  it->second.terminalized = true;
  it->second.terminalReason = reason;
  it->second.terminalizedAtMs = NowMs ();
  it->second.terminalizedAtNs = NowNs ();
  it->second.waitingResponse = false;
  it->second.dispatchStarted = false;
  it->second.pendingPsns = 0;
  it->second.responseDeadlineMs = 0;
  it->second.waitingResponseGeneration = 0;
  if (m_pdsManager &&
      it->second.metadata &&
      it->second.metadata->op_type == OpType::READ &&
      it->second.readResponseTargetRegistered)
    {
      // READ requests reserve requester-side response tracking that must be reclaimed even if the
      // request terminalizes before the full response data path retires the receive context.
      m_pdsManager->UnregisterReadResponseTarget (key.jobId, key.msgId, key.peerFep);
      it->second.readResponseTargetRegistered = false;
    }
  RecordCompletion (key, it->second);
  it->second.countedInSemanticStats = true;
}

void
SesManager::ScheduleRetry (const RequestKey& key, RetryTrigger trigger)
{
  auto requestIt = m_requestStates.find (key);
  if (requestIt == m_requestStates.end () || !requestIt->second.packet || !requestIt->second.metadata)
    {
      NS_LOG_INFO ("SES retry skipped job=" << key.jobId
                   << " msg=" << key.msgId
                   << " peer=" << key.peerFep
                   << " request_state_missing=" << ((requestIt == m_requestStates.end ()) ? "true" : "false"));
      return;
    }

  RequestLifecycleState& requestState = requestIt->second;
  RetryState& retry = m_retryStates[key];
  retry.metadata = CopyObject<ExtendedOperationMetadata> (requestState.metadata);
  retry.packet = requestState.packet->Copy ();
  retry.waitingResponse = false;
  retry.trigger = trigger;
  retry.retryCount = requestState.lastRetryCount + 1;
  retry.scheduledRetryGeneration = requestState.attemptGeneration;
  requestState.lastRetryCount = retry.retryCount;
  requestState.lastRetryTrigger = trigger;
  requestState.retryExhausted = false;
  requestState.waitingResponse = false;
  requestState.dispatchStarted = false;
  requestState.pendingPsns = 0;
  requestState.responseDeadlineMs = 0;
  requestState.waitingResponseGeneration = 0;
  if (trigger == RetryTrigger::RESPONSE_TIMEOUT)
    {
      if (requestState.metadata && requestState.metadata->op_type == OpType::SEND)
        {
          if (requestState.lastResponseGeneration == 0)
            {
              ++m_sendTimeoutWithoutResponseTotal;
            }
          if (requestState.timeoutRetryCount > 0 && requestState.lastResponseGeneration == 0)
            {
              ++m_sendTimeoutRetryWithoutResponseProgressTotal;
              RecordDiagnosticEvent ("SendRetryNoResponseProgress",
                                     "job=" + std::to_string (key.jobId) +
                                     " msg=" + std::to_string (key.msgId) +
                                     " peer=" + std::to_string (key.peerFep) +
                                     " retry_count=" + std::to_string (requestState.lastRetryCount + 1) +
                                     " gen=" + std::to_string (requestState.attemptGeneration));
            }
          RecordDiagnosticEvent ("SendResponseTimeoutRetryScheduled",
                                 "job=" + std::to_string (key.jobId) +
                                 " msg=" + std::to_string (key.msgId) +
                                 " peer=" + std::to_string (key.peerFep) +
                                 " retry_count=" + std::to_string (requestState.lastRetryCount + 1) +
                                 " gen=" + std::to_string (requestState.attemptGeneration) +
                                 " last_response_gen=" + std::to_string (requestState.lastResponseGeneration));
        }
      requestState.timeoutRetryCount++;
    }
  else if (trigger == RetryTrigger::RC_NO_MATCH)
    {
      requestState.noMatchRetryCount++;
    }
  if (retry.retryCount > Max_RTO_Retx_Cnt)
    {
      requestState.retryExhausted = true;
      RecordFailureSnapshot (key,
                             requestState.metadata ? requestState.metadata->op_type : OpType::SEND,
                             static_cast<uint8_t> (ResponseReturnCode::RC_INTERNAL_ERROR),
                             trigger == RetryTrigger::RESPONSE_TIMEOUT ? "response_timeout_exhausted"
                                                                       : "retry_exhausted",
                             trigger == RetryTrigger::RESPONSE_TIMEOUT
                                 ? "response_timeout_retry_exhausted"
                                 : "no_match_retry_exhausted");
      SetRequestCompletionOutcome (key,
                                   static_cast<uint8_t> (ResponseReturnCode::RC_INTERNAL_ERROR),
                                   0);
      TerminalizeRequest (key,
                          trigger == RetryTrigger::RESPONSE_TIMEOUT
                              ? RequestTerminalReason::RESPONSE_TIMEOUT_EXHAUSTED
                              : RequestTerminalReason::RETRY_EXHAUSTED);
      return;
    }
  retry.metadata->is_retry = true;
  retry.nextRetryMs = NowMs () + m_retryTimeout.GetMilliSeconds ();
  NS_LOG_INFO ("SES retry scheduled job=" << key.jobId
               << " msg=" << key.msgId
               << " peer=" << key.peerFep
               << " retry_count=" << retry.retryCount
               << " trigger=" << (trigger == RetryTrigger::RESPONSE_TIMEOUT ? "timeout" : "no_match")
               << " next_retry_ms=" << retry.nextRetryMs);
  if (retry.event.IsPending ())
    {
      Simulator::Cancel (retry.event);
    }
  retry.event = Simulator::Schedule (m_retryTimeout, &SesManager::ExecuteRetry, this, key);
}

void
SesManager::ExecuteRetry (RequestKey key)
{
  auto retryIt = m_retryStates.find (key);
  if (retryIt == m_retryStates.end ())
    {
      return;
    }
  auto requestIt = m_requestStates.find (key);
  if (requestIt != m_requestStates.end () &&
      requestIt->second.attemptGeneration != retryIt->second.scheduledRetryGeneration)
    {
      ++m_staleRetrySkippedTotal;
      m_retryStates.erase (retryIt);
      return;
    }
  Ptr<ExtendedOperationMetadata> metadata = retryIt->second.metadata;
  Ptr<Packet> packet = retryIt->second.packet ? retryIt->second.packet->Copy () : nullptr;
  if (!metadata || !packet)
    {
      return;
    }
  ProcessSendRequest (metadata, packet);

  retryIt = m_retryStates.find (key);
  if (retryIt != m_retryStates.end ())
    {
      retryIt->second.waitingResponse = false;
      requestIt = m_requestStates.find (key);
      if (requestIt != m_requestStates.end ())
        {
          retryIt->second.waitingResponse = requestIt->second.waitingResponse;
          retryIt->second.nextRetryMs = requestIt->second.responseDeadlineMs;
          retryIt->second.scheduledRetryGeneration = requestIt->second.attemptGeneration;
        }
    }
}

bool
SesManager::WritePacketBytesToAddress (Ptr<Packet> packet,
                                       uint64_t baseAddr,
                                       uint32_t offset,
                                       uint32_t maxLength) const
{
  if (!packet || baseAddr == 0)
    {
      return false;
    }
  const std::vector<uint8_t> bytes = CopyPacketBytes (packet);
  if (offset >= maxLength)
    {
      return false;
    }
  const uint32_t copyLen = std::min<uint32_t> (bytes.size (), maxLength - offset);
  std::memcpy (reinterpret_cast<void*> (baseAddr + offset), bytes.data (), copyLen);
  return true;
}

void
SesManager::ArmResponseTimeout (RequestLifecycleState& state)
{
  if (!state.waitingResponse || !state.dispatchStarted)
    {
      state.responseDeadlineMs = 0;
      state.waitingResponseGeneration = 0;
      return;
    }
  if (state.waitingResponseGeneration == 0)
    {
      state.waitingResponseGeneration = state.attemptGeneration;
    }
  state.responseDeadlineMs = NowMs () + m_retryTimeout.GetMilliSeconds ();
}

std::vector<uint8_t>
SesManager::CopyPacketBytes (Ptr<Packet> packet) const
{
  if (!packet)
    {
      return {};
    }
  std::vector<uint8_t> bytes (packet->GetSize ());
  if (!bytes.empty ())
    {
      packet->CopyData (bytes.data (), bytes.size ());
    }
  return bytes;
}

} // namespace ns3
