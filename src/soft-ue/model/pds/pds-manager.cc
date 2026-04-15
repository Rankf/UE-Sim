#include "pds-manager.h"
#include "pds-common.h"
#include "../pdc/pdc-base.h"
#include "../network/soft-ue-net-device.h"
#include "../network/soft-ue-channel.h"
#include "../common/soft-ue-packet-tag.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/assert.h"
#include "ns3/mac48-address.h"
#include "ns3/nstime.h"
#include <algorithm>
#include <cstring>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PdsManager");
NS_OBJECT_ENSURE_REGISTERED (PdsManager);

namespace {

constexpr uint32_t kSemanticHeaderOverhead = 128;
constexpr uint32_t kDefaultDeviceMtu = 1500;

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
PdsManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PdsManager")
    .SetParent<Object> ()
    .SetGroupName ("Soft-Ue")
    .AddConstructor<PdsManager> ()
    ;
  return tid;
}

TypeId
PdsManager::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

PdsManager::PdsManager ()
  : m_sesManager (nullptr),
    m_netDevice (nullptr),
    m_statistics (nullptr),
    m_statisticsEnabled (true),
    m_postedReceives (),
    m_readResponseTargets (),
    m_rxMessageContexts (),
    m_completedSendTombstones (),
    m_completedSendLookupTombstones (),
    m_nextPdcId (0),
    m_maxPdcCount (1024),
    m_maxUnexpectedMessages (64),
    m_maxUnexpectedBytes (1u << 20),
    m_arrivalTrackingCapacity (64),
    m_currentUnexpectedBytes (0),
    m_unexpectedAllocFailures (0),
    m_staleCleanupCount (0),
    m_receiveStateTimeout (MilliSeconds (80)),
    m_payloadMtu (0),
    m_tpdcReadResponseAggressiveDrain (false),
    m_tpdcReadResponseQueuePriority (false),
    m_receivePool (),
    m_sendAdmissionBudget (),
    m_writeBudget (),
    m_readResponderBudget (),
    m_creditGateEnabled (false),
    m_ackControlEnabled (false),
    m_creditRefreshInterval (MilliSeconds (5)),
    m_initialCredits (0),
    m_creditRefreshSent (0),
    m_ackCtrlExtSent (0),
    m_creditGateBlockedCount (0),
    m_sackSent (0),
    m_gapNackSent (0),
    m_pdcIdBitmap (MAX_PDC_ID + 1, false), // Initialize bitmap with all IDs free
    m_freePdcIds (),
    m_state (PDS_IDLE)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("PdsManager created with optimized PDC ID allocation system");
}

PdsManager::~PdsManager ()
{
  NS_LOG_FUNCTION (this);
}

void
PdsManager::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  // Clear components
  m_sesManager = nullptr;
  m_netDevice = nullptr;
  m_statistics = nullptr;

  Object::DoDispose ();
}

void
PdsManager::Initialize (void)
{
  NS_LOG_FUNCTION (this);

  // Only create statistics object if not already created
  if (!m_statistics)
    {
      m_statistics = CreateObject<PdsStatistics> ();
    }

  NS_LOG_INFO ("PDS Manager initialized");
}

void
PdsManager::SetSesManager (Ptr<SesManager> sesManager)
{
  NS_LOG_FUNCTION (this << sesManager);
  m_sesManager = sesManager;
}

Ptr<SesManager>
PdsManager::GetSesManager (void) const
{
  return m_sesManager;
}

void
PdsManager::SetNetDevice (Ptr<SoftUeNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  m_netDevice = device;
}

Ptr<SoftUeNetDevice>
PdsManager::GetNetDevice (void) const
{
  return m_netDevice;
}

PdsManager::ReceiveLookupKey
PdsManager::BuildLookupKey (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const
{
  ReceiveLookupKey key;
  key.jobId = jobId;
  key.msgId = msgId;
  key.peerFep = peerFep;
  return key;
}

RxMessageKey
PdsManager::BuildRxMessageKey (OpType opcode,
                               uint64_t jobId,
                               uint16_t msgId,
                               uint32_t srcFep,
                               uint16_t pdcId) const
{
  RxMessageKey key;
  key.opcode = opcode;
  key.jobId = jobId;
  key.msgId = msgId;
  key.srcFep = srcFep;
  key.pdcId = pdcId;
  return key;
}

int64_t
PdsManager::NowMs (void) const
{
  return Simulator::Now ().GetMilliSeconds ();
}

uint32_t
PdsManager::ComputePayloadPerPacket (void) const
{
  if (m_payloadMtu > 0)
    {
      return m_payloadMtu;
    }
  return PayloadCapacityFromDeviceMtu (m_netDevice ? m_netDevice->GetMtu () : kDefaultDeviceMtu);
}

uint32_t
PdsManager::ComputeExpectedChunks (uint32_t totalLength) const
{
  const uint32_t payloadPerPacket = ComputePayloadPerPacket ();
  return totalLength == 0 ? 1u : (totalLength + payloadPerPacket - 1) / payloadPerPacket;
}

uint32_t
PdsManager::ComputeChunkIndex (const SoftUeMetadataTag& metadataTag, uint32_t payloadPerPacket) const
{
  if (payloadPerPacket == 0)
    {
      return 0;
    }
  return metadataTag.GetFragmentOffset () / payloadPerPacket;
}

uint32_t
PdsManager::CountContiguousChunks (const RxMessageContext& context) const
{
  uint32_t contiguous = 0;
  while (contiguous < context.chunk_arrived.size () && context.chunk_arrived[contiguous])
    {
      ++contiguous;
    }
  return contiguous;
}

std::vector<uint8_t>
PdsManager::CopyPacketBytes (Ptr<Packet> packet) const
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

bool
PdsManager::CopyPacketBytesToTarget (Ptr<Packet> packet,
                                     uint64_t baseAddr,
                                     uint32_t offset,
                                     uint32_t maxLength) const
{
  if (!packet || baseAddr == 0 || offset >= maxLength)
    {
      return false;
    }

  const std::vector<uint8_t> bytes = CopyPacketBytes (packet);
  if (bytes.empty ())
    {
      return true;
    }

  const uint32_t copyLen = std::min<uint32_t> (bytes.size (), maxLength - offset);
  std::memcpy (reinterpret_cast<void*> (baseAddr + offset), bytes.data (), copyLen);
  return true;
}

bool
PdsManager::IsContextActive (const RxMessageContext& context) const
{
  return context.present && !context.retired;
}

UnexpectedSendProbe
PdsManager::BuildUnexpectedProbe (const RxMessageContext& context) const
{
  UnexpectedSendProbe probe;
  probe.present = context.present;
  probe.semantic_accepted = context.semantic_accepted;
  probe.buffered_complete = context.buffered_complete;
  probe.matched_to_recv = context.matched_to_recv;
  probe.chunks_done = context.chunks_done;
  probe.expected_chunks = context.expected_chunks;
  probe.completed = context.completed;
  probe.failed = context.failed;
  probe.last_activity_ms = context.last_activity_ms;
  return probe;
}

bool
PdsManager::ReserveArrivalBlock (RxMessageContext* context)
{
  if (!context)
    {
      return false;
    }
  if (context->arrival_reserved)
    {
      return true;
    }
  if (m_receivePool.arrivalBlocksInUse >= m_receivePool.maxArrivalBlocks)
    {
      ++m_receivePool.arrivalAllocFailures;
      return false;
    }
  ++m_receivePool.arrivalBlocksInUse;
  context->arrival_reserved = true;
  return true;
}

bool
PdsManager::ReserveUnexpectedResources (RxMessageContext* context)
{
  if (!context)
    {
      return false;
    }
  if (context->unexpected_reserved)
    {
      return true;
    }
  if (m_receivePool.unexpectedMessagesInUse >= m_receivePool.maxUnexpectedMessages ||
      (m_receivePool.unexpectedBytesInUse + context->total_length) > m_receivePool.maxUnexpectedBytes)
    {
      ++m_receivePool.unexpectedAllocFailures;
      return false;
    }
  ++m_receivePool.unexpectedMessagesInUse;
  m_receivePool.unexpectedBytesInUse += context->total_length;
  context->unexpected_reserved = true;
  return true;
}

bool
PdsManager::ReserveReadTrack (void)
{
  if (m_receivePool.readTracksInUse >= m_receivePool.maxReadTracks)
    {
      ++m_receivePool.readTrackAllocFailures;
      return false;
    }
  ++m_receivePool.readTracksInUse;
  return true;
}

void
PdsManager::ReleaseReadTrack (void)
{
  m_receivePool.readTracksInUse -= std::min<uint32_t> (1u, m_receivePool.readTracksInUse);
}

PdsManager::RudControlState&
PdsManager::GetControlState (uint32_t peerFep)
{
  RudControlState& state = m_controlStates[peerFep];
  if (state.sendCredits == 0 && state.recvCredits == 0 && state.lastRefreshMs == 0)
    {
      state.sendCredits = GetInitialCredits ();
      state.recvCredits = state.sendCredits;
      state.lastRefreshMs = NowMs ();
    }
  return state;
}

uint32_t
PdsManager::GetInitialCredits (void) const
{
  return m_initialCredits != 0 ? m_initialCredits
                               : std::max<uint32_t> (1u, m_receivePool.maxArrivalBlocks / 4u);
}

void
PdsManager::ScheduleCreditRefresh (uint32_t peerFep, uint32_t minCredits)
{
  RudControlState& state = GetControlState (peerFep);
  state.creditRefreshPending = true;
  state.pendingCreditGrant = std::max<uint32_t> (state.pendingCreditGrant, minCredits);
  if (!state.refreshEvent.IsPending ())
    {
      state.refreshEvent =
          Simulator::Schedule (m_creditRefreshInterval, &PdsManager::ExecuteCreditRefresh, this, peerFep);
    }
}

void
PdsManager::ExecuteCreditRefresh (uint32_t peerFep)
{
  RudControlState& state = GetControlState (peerFep);
  state.sendCredits = std::max<uint32_t> (state.sendCredits, std::max (GetInitialCredits (), state.pendingCreditGrant));
  state.creditRefreshPending = false;
  state.creditGateBlocked = false;
  state.pendingCreditGrant = 0;
  state.lastRefreshMs = NowMs ();
  ++m_creditRefreshSent;
  if (m_sesManager)
    {
      m_sesManager->NotifyPeerCreditsAvailable (peerFep);
    }
}

bool
PdsManager::TryConsumeSendCredits (uint32_t peerFep, uint32_t packetCredits)
{
  if (!m_creditGateEnabled)
    {
      return true;
    }

  RudControlState& state = GetControlState (peerFep);
  if (state.sendCredits >= packetCredits)
    {
      state.sendCredits -= packetCredits;
      state.creditGateBlocked = false;
      return true;
    }

  state.creditGateBlocked = true;
  ++m_creditGateBlockedCount;
  ScheduleCreditRefresh (peerFep, packetCredits);
  return false;
}

void
PdsManager::ReturnSendCredits (uint32_t peerFep, uint32_t packetCredits, bool ackControl)
{
  RudControlState& state = GetControlState (peerFep);
  state.sendCredits += packetCredits;
  state.recvCredits += packetCredits;
  state.creditGateBlocked = false;
  state.lastRefreshMs = NowMs ();
  if (ackControl)
    {
      if (m_ackControlEnabled)
        {
          ++m_ackCtrlExtSent;
        }
    }
  else
    {
      ++m_creditRefreshSent;
    }
  if (m_sesManager)
    {
      m_sesManager->NotifyPeerCreditsAvailable (peerFep);
    }
}

void
PdsManager::StoreChunkInContext (RxMessageContext& context,
                                 const SoftUeMetadataTag& metadataTag,
                                 Ptr<Packet> packet)
{
  const uint32_t payloadPerPacket = std::max<uint32_t> (1, context.payload_per_packet);
  const uint32_t chunkIndex = ComputeChunkIndex (metadataTag, payloadPerPacket);
  if (chunkIndex >= context.chunk_arrived.size ())
    {
      return;
    }

  const bool firstArrival = !context.chunk_arrived[chunkIndex];
  const std::vector<uint8_t> bytes = CopyPacketBytes (packet);
  const uint32_t offset = metadataTag.GetFragmentOffset ();
  if (context.payload_buffer.size () < context.total_length)
    {
      context.payload_buffer.resize (context.total_length, 0);
    }
  if (!bytes.empty () && offset < context.payload_buffer.size ())
    {
      const uint32_t copyLen = std::min<uint32_t> (bytes.size (), context.payload_buffer.size () - offset);
      std::copy_n (bytes.begin (), copyLen, context.payload_buffer.begin () + offset);
    }
  if (context.target_base_addr != 0 && context.target_length != 0)
    {
      (void) CopyPacketBytesToTarget (packet,
                                      context.target_base_addr,
                                      offset,
                                      context.target_length);
    }
  if (firstArrival)
    {
      context.chunk_arrived[chunkIndex] = true;
      context.chunks_done++;
    }
  context.buffered_complete = (context.chunks_done >= context.expected_chunks);
  context.last_activity_ms = NowMs ();
}

void
PdsManager::RetireContextResources (RxMessageContext& context)
{
  if (context.retired)
    {
      return;
    }
  if (context.unexpected_reserved)
    {
      m_receivePool.unexpectedMessagesInUse -= std::min<uint32_t> (1u, m_receivePool.unexpectedMessagesInUse);
      m_receivePool.unexpectedBytesInUse -=
          std::min<uint32_t> (m_receivePool.unexpectedBytesInUse, context.total_length);
      context.unexpected_reserved = false;
    }
  if (context.arrival_reserved)
    {
      if (context.mode == RxMessageMode::READ_RESPONSE && m_sesManager)
        {
          m_sesManager->NotifyReadResponseArrivalContextReleased (
              context.job_id,
              context.msg_id,
              context.src_fep,
              static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
        }
      m_receivePool.arrivalBlocksInUse -= std::min<uint32_t> (1u, m_receivePool.arrivalBlocksInUse);
      context.arrival_reserved = false;
    }
  if (context.mode == RxMessageMode::READ_RESPONSE)
    {
      if (context.read_track_reserved)
        {
          ReleaseReadTrack ();
          context.read_track_reserved = false;
        }
    }
  context.payload_buffer.clear ();
  context.chunk_arrived.clear ();
  context.retired = true;
  context.retired_at_ms = NowMs ();
  context.last_activity_ms = context.retired_at_ms;
}

void
PdsManager::TryPromoteUnexpectedToMatched (RxMessageContext& context)
{
  if (context.mode != RxMessageMode::UNEXPECTED_BUFFERED)
    {
      return;
    }

  const ReceiveLookupKey lookup = BuildLookupKey (context.job_id, context.msg_id, context.src_fep);
  auto postedIt = m_postedReceives.find (lookup);
  if (postedIt == m_postedReceives.end () || postedIt->second.baseAddr == 0)
    {
      return;
    }

  context.target_base_addr = postedIt->second.baseAddr;
  context.target_length = postedIt->second.length;
  context.matched_to_recv = true;
  context.mode = RxMessageMode::UNEXPECTED_MATCHED;
  postedIt->second.consumed = false;

  const uint32_t copyLen = std::min<uint32_t> (context.target_length,
                                               static_cast<uint32_t> (context.payload_buffer.size ()));
  if (copyLen != 0)
    {
      std::memcpy (reinterpret_cast<void*> (context.target_base_addr),
                   context.payload_buffer.data (),
                   copyLen);
    }
  if (context.buffered_complete)
    {
      postedIt->second.consumed = true;
    }
}

void
PdsManager::TryRetireCompletedContext (const RxMessageKey& key, RxMessageContext& context)
{
  if (!context.buffered_complete)
    {
      return;
    }

  if (context.mode == RxMessageMode::DIRECT_EXPECTED)
    {
      context.completed = true;
      RetireContextResources (context);
      auto postedIt = m_postedReceives.find (BuildLookupKey (context.job_id, context.msg_id, context.src_fep));
      if (postedIt != m_postedReceives.end ())
        {
          postedIt->second.consumed = true;
        }
      CompletedSendTombstone tombstone;
      tombstone.modifiedLength = context.total_length;
      tombstone.completedAtMs = NowMs ();
      m_completedSendTombstones[key] = tombstone;
      m_completedSendLookupTombstones[BuildLookupKey (context.job_id, context.msg_id, context.src_fep)] = tombstone;
      return;
    }

  if (context.mode == RxMessageMode::UNEXPECTED_MATCHED && context.matched_to_recv)
    {
      context.completed = true;
      auto postedIt = m_postedReceives.find (BuildLookupKey (context.job_id, context.msg_id, context.src_fep));
      if (postedIt != m_postedReceives.end ())
        {
          postedIt->second.consumed = true;
        }
      CompletedSendTombstone tombstone;
      tombstone.modifiedLength = context.total_length;
      tombstone.completedAtMs = NowMs ();
      m_completedSendTombstones[key] = tombstone;
      m_completedSendLookupTombstones[BuildLookupKey (context.job_id, context.msg_id, context.src_fep)] = tombstone;
      RetireContextResources (context);
      return;
    }

  if (context.mode == RxMessageMode::READ_RESPONSE)
    {
      context.completed = true;
      if (m_sesManager)
        {
          m_sesManager->NotifyReadResponseTargetReleased (
              context.job_id,
              context.msg_id,
              context.src_fep,
              static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
        }
      m_readResponseTargets.erase (BuildLookupKey (context.job_id, context.msg_id, context.src_fep));
      RetireContextResources (context);
    }
}

void
PdsManager::PruneCompletedSendTombstones (void)
{
  const int64_t now = NowMs ();
  const int64_t ttl = std::max<int64_t> (500, m_receiveStateTimeout.GetMilliSeconds () * 4);
  for (auto it = m_completedSendTombstones.begin (); it != m_completedSendTombstones.end ();)
    {
      if ((now - it->second.completedAtMs) > ttl)
        {
          it = m_completedSendTombstones.erase (it);
        }
      else
        {
          ++it;
        }
    }

  for (auto it = m_completedSendLookupTombstones.begin (); it != m_completedSendLookupTombstones.end ();)
    {
      if ((now - it->second.completedAtMs) > ttl)
        {
          it = m_completedSendLookupTombstones.erase (it);
        }
      else
        {
          ++it;
        }
    }
}

bool
PdsManager::ProcessSesRequest (const SesPdsRequest& request)
{
  NS_LOG_FUNCTION (this << "Processing SES request");

  // Check state machine - reject if busy or in error state
  if (m_state == PDS_BUSY)
  {
    NS_LOG_WARN ("ProcessSesRequest rejected: PDS manager is busy");
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::RESOURCE_EXHAUSTED);
    }
    return false;
  }

  if (m_state == PDS_ERROR)
  {
    NS_LOG_ERROR ("ProcessSesRequest rejected: PDS manager is in error state");
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::INTERNAL_ERROR);
    }
    return false;
  }

  // Set busy state during processing
  m_state = PDS_BUSY;
  if (m_sesManager)
    m_sesManager->NotifyPause (true);  // Diagnostic hook only; control-plane truth stays in RudControlState.

  // Enhanced request validation
  if (!ValidateSesPdsRequest (request))
  {
    NS_LOG_ERROR ("Invalid SES PDS request received");
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::PROTOCOL_ERROR);
    }
    if (m_sesManager)
      {
        m_sesManager->NotifyPdsErrorEvent (0, static_cast<int> (PdsErrorCode::PROTOCOL_ERROR),
                                           "Invalid SES PDS request");
      }
    m_state = PDS_ERROR;
    return false;
  }

  // Validate network device
  if (!m_netDevice)
  {
    NS_LOG_ERROR ("Network device not available for transmission");
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::RESOURCE_EXHAUSTED);
    }
    if (m_sesManager)
      {
        m_sesManager->NotifyPdsErrorEvent (0, static_cast<int> (PdsErrorCode::RESOURCE_EXHAUSTED),
                                           "Network device not available");
      }
    m_state = PDS_ERROR;
    return false;
  }

  NS_LOG_INFO ("Dispatching packet to FEP " << request.dst_fep << " via PDC");

  // Send through PDC: AllocatePdc + SendPacketThroughPdc (no direct channel->Transmit)
  bool success = DispatchPacket (request);

  if (success)
  {
    m_state = PDS_IDLE;
    if (m_sesManager)
      m_sesManager->NotifyPause (false);  // Diagnostic hook only; control-plane truth stays in RudControlState.
    NS_LOG_DEBUG ("Processed SES request and transmitted packet successfully through PDC");
  }
  else
  {
    m_state = PDS_ERROR;
  }

  return success;
}

bool
PdsManager::ProcessReceivedPacket (Ptr<Packet> packet, uint32_t sourceEndpoint, uint32_t destEndpoint)
{
  NS_LOG_FUNCTION (this << "Processing received packet");

  if (!packet || !m_netDevice)
  {
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::INVALID_PACKET);
    }
    return false;
  }

  // Parse PDS header to get pdc_id (do not remove yet - PDC expects full packet with header)
  if (packet->GetSize () < 7)  // PDSHeader minimal size
  {
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::INVALID_PACKET);
    }
    return false;
  }

  PDSHeader pdsHeader;
  packet->PeekHeader (pdsHeader);
  uint16_t pdcId = pdsHeader.GetPdcId ();
  const bool reliable = pdsHeader.IsReliable ();
  SoftUeMetadataTag metadataTag;
  const bool hasMetadataTag = packet->PeekPacketTag (metadataTag);
  SoftUeTpdcControlTag controlTag;
  const bool hasTpdcControl = packet->PeekPacketTag (controlTag);

  NS_LOG_INFO ("[UEC-E2E] [PDS] ProcessReceivedPacket: parse pdc_id=" << pdcId
               << ", dispatch by pdc_id (rx-side PDC)");

  // Ensure we have a PDC for this pdc_id on receive side (passive creation)
  if (!EnsureReceivePdc (pdcId, sourceEndpoint, reliable))
  {
    NS_LOG_WARN ("ProcessReceivedPacket: could not ensure PDC " << pdcId << ", delivering anyway");
  }

  // TPDC control packets advance the sender-side outgoing session; data packets
  // belong to passive receive-side sessions keyed by (sourceFep, pdcId).
  Ptr<PdcBase> pdc = hasTpdcControl ? GetPdc (pdcId) : GetReceivePdc (sourceEndpoint, pdcId);
  if (!pdc)
    {
      pdc = GetReceivePdc (sourceEndpoint, pdcId);
    }
  if (pdc && hasTpdcControl)
  {
    const uint8_t flags = controlTag.GetFlags ();
    if ((flags & SOFT_UE_TPDC_CTRL_SACK) != 0)
      {
        ++m_sackSent;
      }
    if ((flags & SOFT_UE_TPDC_CTRL_GAP_NACK) != 0)
      {
        ++m_gapNackSent;
      }
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementReceivedPackets ();
      m_statistics->RecordBytesReceived (packet->GetSize ());
      m_statistics->IncrementErrors (PdsErrorCode::SUCCESS);
    }
    return pdc->HandleReceivedPacket (packet, sourceEndpoint);
  }

  // B2: Receive path through PDC; pass copy so RemoveHeader below does not affect PDC queue
  if (pdc)
  {
    Ptr<Packet> pdcCopy = packet->Copy ();
    if (pdcCopy)
      (void) pdc->HandleReceivedPacket (pdcCopy, sourceEndpoint);
  }

  // Remove PDS header to get payload for upper layer
  packet->RemoveHeader (pdsHeader);

  // E2E log: fragment i/N (optional: Tx k/N) receive-side full path marker
  SoftUeFragmentTag fragTag;
  if (packet->PeekPacketTag (fragTag))
    {
      NS_LOG_INFO ("============================================================");
      SoftUeTransactionTag txTag;
      if (packet->PeekPacketTag (txTag) && txTag.GetTotalTransactions () > 0)
        NS_LOG_INFO (" [UEC-E2E] Tx " << txTag.GetTransactionIndex () << " Frag " << fragTag.GetFragmentIndex () << " recv");
      else
        NS_LOG_INFO (" [UEC-E2E] Frag " << fragTag.GetFragmentIndex () << " recv");
      NS_LOG_INFO ("============================================================");
    }

  // Update PDS statistics (with latency when SoftUeTimingTag present)
  if (m_statistics && m_statisticsEnabled)
  {
    SoftUeTimingTag timingTag;
    if (packet->PeekPacketTag (timingTag))
    {
      Time sendTime = timingTag.GetTimestamp ();
      double latencyNs = (Simulator::Now () - sendTime).GetNanoSeconds ();
      m_statistics->RecordPacketReception (packet->GetSize (), latencyNs);
    }
    else
    {
      m_statistics->IncrementReceivedPackets ();
      m_statistics->RecordBytesReceived (packet->GetSize ());
    }
    m_statistics->IncrementErrors (PdsErrorCode::SUCCESS);
  }

  NS_LOG_DEBUG ("Processed received packet - pdc_id=" << pdcId << " payload size=" << packet->GetSize ());

  // Receive path: PDS -> SES request/response dispatch
  if (m_sesManager)
    {
      if (hasMetadataTag && metadataTag.IsResponse ())
        {
          PdcSesResponse rsp;
          rsp.pdc_id = pdcId;
          rsp.rx_pkt_handle = 0;
          rsp.packet = packet;
          rsp.pkt_len = static_cast<uint16_t> (packet->GetSize ());
          if (m_sesManager->ProcessReceiveResponse (rsp))
            {
              return true;
            }
        }
      else
        {
          PdcSesRequest req;
          req.pdc_id = pdcId;
          req.rx_pkt_handle = 0;
          req.packet = packet;
          req.pkt_len = static_cast<uint16_t> (packet->GetSize ());
          req.next_hdr = PDSNextHeader::PAYLOAD;
          req.orig_pdcid = pdcId;
          req.orig_psn = pdsHeader.GetSequenceNumber ();
          NS_LOG_INFO ("[UEC-E2E] [PDS] Strip PDS header -> SesManager::ProcessReceiveRequest (rx via SES -> app)");
          if (m_sesManager->ProcessReceiveRequest (req))
            {
              return true;
            }
        }
      // If SES declined, fall back to direct delivery
    }

  NS_LOG_INFO ("[UEC-E2E] [PDS] Strip PDS header -> DeliverReceivedPacket to application");
  m_netDevice->DeliverReceivedPacket (packet);
  return true;
}

bool
PdsManager::RegisterPostedReceive (uint64_t jobId,
                                   uint16_t msgId,
                                   uint32_t srcFep,
                                   uint64_t baseAddr,
                                   uint32_t length)
{
  PruneReceiveState ();
  const ReceiveLookupKey lookup = BuildLookupKey (jobId, msgId, srcFep);
  PostedReceiveBinding& posted = m_postedReceives[lookup];
  posted.baseAddr = baseAddr;
  posted.length = length;
  posted.consumed = false;

  for (auto& entry : m_rxMessageContexts)
    {
      RxMessageContext& context = entry.second;
      if (!IsContextActive (context) ||
          context.opcode != OpType::SEND ||
          context.job_id != jobId ||
          context.msg_id != msgId ||
          context.src_fep != srcFep)
        {
          continue;
        }
      TryPromoteUnexpectedToMatched (context);
      TryRetireCompletedContext (entry.first, context);
    }
  return true;
}

bool
PdsManager::RegisterReadResponseTarget (uint64_t jobId,
                                        uint16_t msgId,
                                        uint32_t peerFep,
                                        uint64_t baseAddr,
                                        uint32_t length)
{
  PruneReceiveState ();
  const ReceiveLookupKey lookup = BuildLookupKey (jobId, msgId, peerFep);
  ReadResponseTarget target;
  target.baseAddr = baseAddr;
  target.length = length;
  target.registeredAtMs = NowMs ();
  m_readResponseTargets[lookup] = target;
  if (m_sesManager)
    {
      m_sesManager->NotifyReadResponseTargetRegistered (
          jobId,
          msgId,
          peerFep,
          static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
    }
  return true;
}

bool
PdsManager::UnregisterReadResponseTarget (uint64_t jobId,
                                          uint16_t msgId,
                                          uint32_t peerFep)
{
  const ReceiveLookupKey lookup = BuildLookupKey (jobId, msgId, peerFep);
  auto targetIt = m_readResponseTargets.find (lookup);
  if (targetIt == m_readResponseTargets.end ())
    {
      return false;
    }
  if (m_sesManager)
    {
      m_sesManager->NotifyReadResponseTargetReleased (
          jobId,
          msgId,
          peerFep,
          static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
    }
  m_readResponseTargets.erase (targetIt);
  return true;
}

PdcRxSemanticResult
PdsManager::HandleIncomingSendPacket (uint16_t pdcId,
                                      const SoftUeHeaderTag& headerTag,
                                      const SoftUeMetadataTag& metadataTag,
                                      Ptr<Packet> packet)
{
  PdcRxSemanticResult result;
  PruneReceiveState ();

  const uint64_t jobId = headerTag.GetJobId ();
  const uint16_t msgId = static_cast<uint16_t> (metadataTag.GetMessageId ());
  const uint32_t srcFep = headerTag.GetSourceEndpoint ();
  const uint32_t dstFep = headerTag.GetDestinationEndpoint ();

  const RxMessageKey key = BuildRxMessageKey (OpType::SEND,
                                              jobId,
                                              msgId,
                                              srcFep,
                                              pdcId);
  const ReceiveLookupKey lookup = BuildLookupKey (jobId, msgId, srcFep);

  auto tombstoneIt = m_completedSendTombstones.find (key);
  if (tombstoneIt != m_completedSendTombstones.end ())
    {
      result.handled = true;
      result.response_ready = true;
      result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_DEFAULT_RESPONSE);
      result.return_code = static_cast<uint8_t> (ResponseReturnCode::RC_OK);
      result.modified_length = tombstoneIt->second.modifiedLength;
      result.delivery_complete = true;
      result.semantic_accepted = true;
      return result;
    }
  auto lookupTombstoneIt = m_completedSendLookupTombstones.find (lookup);
  if (lookupTombstoneIt != m_completedSendLookupTombstones.end ())
    {
      result.handled = true;
      result.response_ready = true;
      result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_DEFAULT_RESPONSE);
      result.return_code = static_cast<uint8_t> (ResponseReturnCode::RC_OK);
      result.modified_length = lookupTombstoneIt->second.modifiedLength;
      result.delivery_complete = true;
      result.semantic_accepted = true;
      return result;
    }

  RxMessageContext& context = m_rxMessageContexts[key];
  if (!context.present || context.retired)
    {
      context = RxMessageContext ();
      context.present = true;
      context.opcode = OpType::SEND;
      context.src_fep = srcFep;
      context.dst_fep = dstFep;
      context.job_id = jobId;
      context.msg_id = msgId;
      context.pdc_id = pdcId;
      context.total_length = metadataTag.GetRequestLength ();
      context.payload_per_packet = ComputePayloadPerPacket ();
      context.expected_chunks = ComputeExpectedChunks (context.total_length);
      context.chunk_arrived.assign (context.expected_chunks, false);
      context.created_at_ms = NowMs ();
      context.last_activity_ms = context.created_at_ms;
    }

  auto postedIt = m_postedReceives.find (lookup);
  const bool havePostedRecv = (postedIt != m_postedReceives.end () && postedIt->second.baseAddr != 0);
  if (havePostedRecv && context.mode == RxMessageMode::DIRECT_EXPECTED &&
      context.target_base_addr == 0)
    {
      if (!context.arrival_reserved && !ReserveArrivalBlock (&context))
        {
          result.handled = true;
          result.response_ready = true;
          result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_NACK);
          result.return_code = static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST);
          context.failed = true;
          RetireContextResources (context);
          return result;
        }
      context.target_base_addr = postedIt->second.baseAddr;
      context.target_length = postedIt->second.length;
    }

  if (!havePostedRecv &&
      context.mode == RxMessageMode::DIRECT_EXPECTED &&
      !context.semantic_accepted &&
      context.target_base_addr == 0)
    {
      if (!context.arrival_reserved && !ReserveArrivalBlock (&context))
        {
          result.handled = true;
          result.response_ready = true;
          result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_NACK);
          result.return_code =
              (m_receivePool.maxUnexpectedMessages == 0 || m_receivePool.maxUnexpectedBytes == 0)
                  ? static_cast<uint8_t> (ResponseReturnCode::RC_NO_MATCH)
                  : static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST);
          context.failed = true;
          RetireContextResources (context);
          return result;
        }
      if (!ReserveUnexpectedResources (&context))
        {
          result.handled = true;
          result.response_ready = true;
          result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_NACK);
          result.return_code = static_cast<uint8_t> (ResponseReturnCode::RC_NO_MATCH);
          context.failed = true;
          RetireContextResources (context);
          return result;
        }

      context.mode = RxMessageMode::UNEXPECTED_BUFFERED;
      context.semantic_accepted = true;
      context.payload_buffer.assign (context.total_length, 0);
      result.response_ready = true;
      result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_DEFAULT_RESPONSE);
      result.return_code = static_cast<uint8_t> (ResponseReturnCode::RC_OK);
      result.modified_length = context.total_length;
      result.semantic_accepted = true;
      context.response_sent = true;
    }
  else if (havePostedRecv && !context.semantic_accepted)
    {
      context.mode = RxMessageMode::DIRECT_EXPECTED;
      context.semantic_accepted = true;
    }

  if (context.mode == RxMessageMode::DIRECT_EXPECTED)
    {
      (void) CopyPacketBytesToTarget (packet,
                                      context.target_base_addr,
                                      metadataTag.GetFragmentOffset (),
                                      std::max<uint32_t> (1, context.target_length));
    }

  StoreChunkInContext (context, metadataTag, packet);
  if (context.mode == RxMessageMode::UNEXPECTED_BUFFERED ||
      context.mode == RxMessageMode::UNEXPECTED_MATCHED)
    {
      TryPromoteUnexpectedToMatched (context);
    }
  TryRetireCompletedContext (key, context);

  if (context.mode == RxMessageMode::DIRECT_EXPECTED &&
      context.completed &&
      !context.response_sent)
    {
      result.response_ready = true;
      result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_DEFAULT_RESPONSE);
      result.return_code = static_cast<uint8_t> (ResponseReturnCode::RC_OK);
      result.modified_length = context.total_length;
      context.response_sent = true;
    }

  result.handled = true;
  result.semantic_accepted = context.semantic_accepted;
  result.matched_to_recv = context.matched_to_recv;
  result.delivery_complete = context.completed;
  if (context.completed)
    {
      result.receive_consume_complete_ns =
          static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ());
    }
  result.failed = context.failed;
  return result;
}

PdcRxSemanticResult
PdsManager::HandleIncomingReadResponsePacket (uint16_t pdcId,
                                              const SoftUeHeaderTag& headerTag,
                                              const SoftUeMetadataTag& metadataTag,
                                              Ptr<Packet> packet)
{
  PdcRxSemanticResult result;
  PruneReceiveState ();

  const uint64_t jobId = headerTag.GetJobId ();
  const uint16_t msgId = static_cast<uint16_t> (metadataTag.GetMessageId ());
  const uint32_t srcFep = headerTag.GetSourceEndpoint ();

  const ReceiveLookupKey lookup = BuildLookupKey (jobId, msgId, srcFep);
  auto targetIt = m_readResponseTargets.find (lookup);
  if (targetIt == m_readResponseTargets.end ())
    {
      return result;
    }

  const RxMessageKey key = BuildRxMessageKey (OpType::READ,
                                              jobId,
                                              msgId,
                                              srcFep,
                                              pdcId);
  RxMessageContext& context = m_rxMessageContexts[key];
  const bool noActiveContext = (!context.present || context.retired);
  if (noActiveContext && m_sesManager)
    {
      m_sesManager->NotifyReadResponseFirstPacketNoContext (
          jobId,
          msgId,
          srcFep,
          static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
    }
  if (!context.present || context.retired)
    {
      context = RxMessageContext ();
      context.present = true;
      context.opcode = OpType::READ;
      context.src_fep = srcFep;
      context.dst_fep = headerTag.GetDestinationEndpoint ();
      context.job_id = jobId;
      context.msg_id = msgId;
      context.pdc_id = pdcId;
      context.total_length = metadataTag.GetModifiedLength ();
      context.payload_per_packet = ComputePayloadPerPacket ();
      context.expected_chunks = ComputeExpectedChunks (context.total_length);
      context.chunk_arrived.assign (context.expected_chunks, false);
      context.mode = RxMessageMode::READ_RESPONSE;
      context.semantic_accepted = true;
      context.target_base_addr = targetIt->second.baseAddr;
      context.target_length = targetIt->second.length;
      context.created_at_ms = NowMs ();
      context.last_activity_ms = context.created_at_ms;
      if (!ReserveReadTrack ())
        {
          result.handled = true;
          result.response_ready = true;
          result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_NACK);
          result.return_code = static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST);
          context.failed = true;
          RetireContextResources (context);
          return result;
        }
      context.read_track_reserved = true;
      if (!ReserveArrivalBlock (&context))
        {
          if (m_sesManager)
            {
              m_sesManager->NotifyReadResponseArrivalBlockReserveFailed (
                  jobId,
                  msgId,
                  srcFep,
                  static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
            }
          result.handled = true;
          result.response_ready = true;
          result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_NACK);
          result.return_code = static_cast<uint8_t> (ResponseReturnCode::RC_RESOURCE_EXHAUST);
          context.failed = true;
          RetireContextResources (context);
          return result;
        }
      if (m_sesManager)
        {
          m_sesManager->NotifyReadResponseArrivalBlockReserved (
              jobId,
              msgId,
              srcFep,
              static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
        }
    }

  const uint32_t contiguousBefore = CountContiguousChunks (context);
  const uint32_t payloadPerPacket = std::max<uint32_t> (1, context.payload_per_packet);
  const uint32_t chunkIndex = ComputeChunkIndex (metadataTag, payloadPerPacket);
  const bool firstArrival =
      chunkIndex < context.chunk_arrived.size () && !context.chunk_arrived[chunkIndex];
  if (!context.recovery_gap_tracked &&
      !context.recovery_gap_completed &&
      firstArrival &&
      chunkIndex > contiguousBefore)
    {
      context.recovery_gap_tracked = true;
      context.recovery_missing_chunk_index = contiguousBefore;
      if (m_sesManager)
        {
          m_sesManager->NotifyReadResponseGapDetected (
              jobId,
              msgId,
              srcFep,
              contiguousBefore,
              static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
        }
    }

  StoreChunkInContext (context, metadataTag, packet);
  if (context.recovery_gap_tracked &&
      !context.recovery_visible_recorded &&
      firstArrival &&
      chunkIndex == context.recovery_missing_chunk_index)
    {
      SoftUeRecoveryTag recoveryTag;
      if (packet && packet->PeekPacketTag (recoveryTag))
        {
          if (m_sesManager)
            {
              m_sesManager->NotifyReadResponseRecoveryVisible (
                  jobId,
                  msgId,
                  srcFep,
                  chunkIndex,
                  static_cast<uint64_t> (recoveryTag.GetGapNackSentTime ().GetNanoSeconds ()),
                  static_cast<uint64_t> (recoveryTag.GetRetransmitTxTime ().GetNanoSeconds ()),
                  static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
            }
          context.recovery_visible_recorded = true;
        }
    }
  const uint32_t contiguousAfter = CountContiguousChunks (context);
  if (context.recovery_gap_tracked &&
      !context.recovery_gap_completed &&
      firstArrival &&
      contiguousAfter > context.recovery_missing_chunk_index)
    {
      if (m_sesManager)
        {
          m_sesManager->NotifyReadResponseReassemblyUnblocked (
              jobId,
              msgId,
              srcFep,
              context.recovery_missing_chunk_index,
              static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
        }
      context.recovery_gap_completed = true;
    }
  TryRetireCompletedContext (key, context);

  result.handled = true;
  result.delivery_complete = context.completed;
  result.modified_length = context.total_length;
  result.response_opcode = static_cast<uint8_t> (ResponseOpCode::UET_RESPONSE_W_DATA);
  result.return_code = static_cast<uint8_t> (ResponseReturnCode::RC_OK);
  return result;
}

RxMessageProbe
PdsManager::QueryRxMessageProbe (uint64_t jobId,
                                 uint16_t msgId,
                                 uint32_t srcFep,
                                 OpType opcode) const
{
  RxMessageProbe probe;
  for (const auto& entry : m_rxMessageContexts)
    {
      const RxMessageContext& context = entry.second;
      if (!IsContextActive (context) ||
          context.job_id != jobId ||
          context.msg_id != msgId ||
          context.src_fep != srcFep ||
          context.opcode != opcode)
        {
          continue;
        }
      probe.present = context.present;
      probe.mode = context.mode;
      probe.opcode = context.opcode;
      probe.chunks_done = context.chunks_done;
      probe.expected_chunks = context.expected_chunks;
      probe.semantic_accepted = context.semantic_accepted;
      probe.buffered_complete = context.buffered_complete;
      probe.matched_to_recv = context.matched_to_recv;
      probe.completed = context.completed;
      probe.failed = context.failed;
      probe.last_activity_ms = context.last_activity_ms;
      return probe;
    }
  return probe;
}

UnexpectedSendProbe
PdsManager::QueryUnexpectedSendProbe (uint64_t jobId, uint16_t msgId, uint32_t srcFep) const
{
  for (const auto& entry : m_rxMessageContexts)
    {
      const RxMessageContext& context = entry.second;
      if (context.opcode != OpType::SEND ||
          !IsContextActive (context) ||
          context.job_id != jobId ||
          context.msg_id != msgId ||
          context.src_fep != srcFep ||
          (context.mode != RxMessageMode::UNEXPECTED_BUFFERED &&
           context.mode != RxMessageMode::UNEXPECTED_MATCHED))
        {
          continue;
        }
      return BuildUnexpectedProbe (context);
    }
  return UnexpectedSendProbe ();
}

RudResourceStats
PdsManager::GetRudResourceStats (void) const
{
  RudResourceStats stats;
  stats.arrival_blocks_in_use = m_receivePool.arrivalBlocksInUse;
  stats.unexpected_msgs_in_use = m_receivePool.unexpectedMessagesInUse;
  stats.max_unexpected_msgs = m_receivePool.maxUnexpectedMessages;
  stats.max_unexpected_bytes = m_receivePool.maxUnexpectedBytes;
  stats.max_arrival_blocks = m_receivePool.maxArrivalBlocks;
  stats.max_read_tracks = m_receivePool.maxReadTracks;
  stats.unexpected_bytes_in_use = m_receivePool.unexpectedBytesInUse;
  stats.unexpected_alloc_failures = m_receivePool.unexpectedAllocFailures;
  stats.arrival_alloc_failures = m_receivePool.arrivalAllocFailures;
  stats.read_track_in_use = m_receivePool.readTracksInUse;
  stats.read_track_alloc_failures = m_receivePool.readTrackAllocFailures;
  stats.stale_cleanup_count = m_staleCleanupCount;
  return stats;
}

void
PdsManager::FillRudRuntimeStats (RudRuntimeStats* stats) const
{
  if (!stats)
    {
      return;
    }

  stats->active_read_response_states = static_cast<uint32_t> (m_readResponseTargets.size ());
  for (const auto& entry : m_rxMessageContexts)
    {
      const RxMessageContext& context = entry.second;
      if (!IsContextActive (context))
        {
          continue;
        }
      if (context.mode == RxMessageMode::UNEXPECTED_BUFFERED ||
          context.mode == RxMessageMode::UNEXPECTED_MATCHED)
        {
          stats->unexpected_buffered_in_use++;
          if (context.semantic_accepted)
            {
              stats->unexpected_semantic_accepted_in_use++;
            }
          if (!context.buffered_complete)
            {
              stats->unexpected_partial_in_use++;
            }
        }
      stats->tpdc_out_of_order_packets += 0;
    }
  stats->credit_refresh_sent += m_creditRefreshSent;
  stats->ack_ctrl_ext_sent += m_ackCtrlExtSent;
  stats->legacy_credit_gate_blocked += m_creditGateBlockedCount;
  stats->send_admission_blocked_total += m_sendAdmissionBudget.blockedCount;
  stats->send_admission_message_limit_blocked_total += m_sendAdmissionBudget.blockedByMessages;
  stats->send_admission_byte_limit_blocked_total += m_sendAdmissionBudget.blockedByBytes;
  stats->send_admission_both_limit_blocked_total += m_sendAdmissionBudget.blockedByBoth;
  stats->write_budget_blocked_total += m_writeBudget.blockedCount;
  stats->read_responder_blocked_total += m_readResponderBudget.blockedCount;
  stats->credit_gate_blocked += m_creditGateBlockedCount +
                                m_sendAdmissionBudget.blockedCount +
                                m_writeBudget.blockedCount +
                                m_readResponderBudget.blockedCount;
  stats->send_admission_messages_in_use_peak =
      std::max (stats->send_admission_messages_in_use_peak, m_sendAdmissionBudget.messagesInUsePeak);
  stats->send_admission_bytes_in_use_peak =
      std::max<uint64_t> (stats->send_admission_bytes_in_use_peak, m_sendAdmissionBudget.bytesInUsePeak);
  stats->send_admission_release_count += m_sendAdmissionBudget.releaseCount;
  stats->send_admission_release_bytes_total += m_sendAdmissionBudget.releaseBytesTotal;
  stats->write_budget_messages_in_use_peak =
      std::max (stats->write_budget_messages_in_use_peak, m_writeBudget.messagesInUsePeak);
  stats->write_budget_bytes_in_use_peak =
      std::max<uint64_t> (stats->write_budget_bytes_in_use_peak, m_writeBudget.bytesInUsePeak);
  stats->read_responder_messages_in_use_peak =
      std::max (stats->read_responder_messages_in_use_peak, m_readResponderBudget.messagesInUsePeak);
  stats->read_response_budget_bytes_in_use_peak =
      std::max<uint64_t> (stats->read_response_budget_bytes_in_use_peak, m_readResponderBudget.bytesInUsePeak);
  stats->sack_sent += m_sackSent;
  stats->gap_nack_sent += m_gapNackSent;
}

std::vector<TpdcSessionProgressRecord>
PdsManager::GetTpdcSessionProgressRecords (void) const
{
  std::vector<TpdcSessionProgressRecord> records;
  records.reserve (m_pdcs.size () + m_receivePdcs.size ());
  for (const auto& entry : m_pdcs)
    {
      Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
      if (!tpdc)
        {
          continue;
        }
      records.push_back (tpdc->GetSessionProgressRecord ());
    }
  for (const auto& entry : m_receivePdcs)
    {
      Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
      if (!tpdc)
        {
          continue;
        }
      records.push_back (tpdc->GetSessionProgressRecord ());
    }
  return records;
}

bool
PdsManager::HasPendingReadResponseState (void) const
{
  return !m_readResponseTargets.empty ();
}

void
PdsManager::ConfigureReceiveSemantics (uint32_t maxUnexpectedMessages,
                                       uint32_t maxUnexpectedBytes,
                                       uint32_t arrivalTrackingCapacity,
                                       Time retryTimeout)
{
  m_maxUnexpectedMessages = maxUnexpectedMessages;
  m_maxUnexpectedBytes = maxUnexpectedBytes;
  m_arrivalTrackingCapacity = arrivalTrackingCapacity;
  m_receiveStateTimeout = retryTimeout;
  m_receivePool.maxUnexpectedMessages = maxUnexpectedMessages;
  m_receivePool.maxUnexpectedBytes = maxUnexpectedBytes;
  m_receivePool.maxArrivalBlocks = arrivalTrackingCapacity;
}

void
PdsManager::ConfigureSemanticBudgets (uint32_t sendAdmissionMessages,
                                      uint64_t sendAdmissionBytes,
                                      uint32_t writeBudgetMessages,
                                      uint64_t writeBudgetBytes,
                                      uint32_t readResponderMessages,
                                      uint64_t readResponseBytes)
{
  m_sendAdmissionBudget.messageLimit = sendAdmissionMessages;
  m_sendAdmissionBudget.byteLimit = sendAdmissionBytes;
  m_writeBudget.messageLimit = writeBudgetMessages;
  m_writeBudget.byteLimit = writeBudgetBytes;
  m_readResponderBudget.messageLimit = readResponderMessages;
  m_readResponderBudget.byteLimit = readResponseBytes;
}

void
PdsManager::ConfigureControlPlane (bool creditGateEnabled,
                                   bool ackControlEnabled,
                                   Time creditRefreshInterval,
                                   uint32_t maxReadTracks,
                                   uint32_t initialCredits)
{
  m_creditGateEnabled = creditGateEnabled;
  m_ackControlEnabled = ackControlEnabled;
  m_creditRefreshInterval = creditRefreshInterval;
  m_receivePool.maxReadTracks = maxReadTracks;
  m_initialCredits = initialCredits;
}

void
PdsManager::ConfigureTpdcReadResponseScheduling (bool aggressiveDrain,
                                                 bool queuePriority)
{
  m_tpdcReadResponseAggressiveDrain = aggressiveDrain;
  m_tpdcReadResponseQueuePriority = queuePriority;
}

void
PdsManager::SetPayloadMtu (uint32_t payloadMtu)
{
  m_payloadMtu = payloadMtu;
}

bool
PdsManager::TryReserveSendAdmission (uint32_t peerFep, uint32_t payloadBytes)
{
  const bool messagesOk =
      (m_sendAdmissionBudget.messageLimit == 0) ||
      (m_sendAdmissionBudget.messagesInUse + 1 <= m_sendAdmissionBudget.messageLimit);
  const bool bytesOk =
      (m_sendAdmissionBudget.byteLimit == 0) ||
      (m_sendAdmissionBudget.bytesInUse + payloadBytes <= m_sendAdmissionBudget.byteLimit);
  if (!messagesOk || !bytesOk)
    {
      ++m_sendAdmissionBudget.blockedCount;
      if (!messagesOk && !bytesOk)
        {
          ++m_sendAdmissionBudget.blockedByBoth;
        }
      else if (!messagesOk)
        {
          ++m_sendAdmissionBudget.blockedByMessages;
        }
      else if (!bytesOk)
        {
          ++m_sendAdmissionBudget.blockedByBytes;
        }
      GetControlState (peerFep).creditGateBlocked = true;
      return false;
    }

  ++m_sendAdmissionBudget.messagesInUse;
  m_sendAdmissionBudget.bytesInUse += payloadBytes;
  m_sendAdmissionBudget.messagesInUsePeak =
      std::max (m_sendAdmissionBudget.messagesInUsePeak, m_sendAdmissionBudget.messagesInUse);
  m_sendAdmissionBudget.bytesInUsePeak =
      std::max (m_sendAdmissionBudget.bytesInUsePeak, m_sendAdmissionBudget.bytesInUse);
  return true;
}

void
PdsManager::ReleaseSendAdmission (uint32_t peerFep, uint32_t payloadBytes)
{
  if (m_sendAdmissionBudget.messagesInUse > 0)
    {
      --m_sendAdmissionBudget.messagesInUse;
    }
  ++m_sendAdmissionBudget.releaseCount;
  m_sendAdmissionBudget.releaseBytesTotal += payloadBytes;
  m_sendAdmissionBudget.bytesInUse =
      (m_sendAdmissionBudget.bytesInUse >= payloadBytes) ? (m_sendAdmissionBudget.bytesInUse - payloadBytes) : 0;
  GetControlState (peerFep).creditGateBlocked = false;
  if (m_sesManager)
    {
      m_sesManager->NotifyPeerSendAdmissionReleased (peerFep, payloadBytes);
    }
}

bool
PdsManager::TryReserveWriteBudget (uint32_t peerFep, uint32_t payloadBytes)
{
  const bool messagesOk =
      (m_writeBudget.messageLimit == 0) ||
      (m_writeBudget.messagesInUse + 1 <= m_writeBudget.messageLimit);
  const bool bytesOk =
      (m_writeBudget.byteLimit == 0) ||
      (m_writeBudget.bytesInUse + payloadBytes <= m_writeBudget.byteLimit);
  if (!messagesOk || !bytesOk)
    {
      ++m_writeBudget.blockedCount;
      GetControlState (peerFep).creditGateBlocked = true;
      return false;
    }

  ++m_writeBudget.messagesInUse;
  m_writeBudget.bytesInUse += payloadBytes;
  m_writeBudget.messagesInUsePeak = std::max (m_writeBudget.messagesInUsePeak, m_writeBudget.messagesInUse);
  m_writeBudget.bytesInUsePeak = std::max (m_writeBudget.bytesInUsePeak, m_writeBudget.bytesInUse);
  return true;
}

void
PdsManager::ReleaseWriteBudget (uint32_t peerFep, uint32_t payloadBytes)
{
  if (m_writeBudget.messagesInUse > 0)
    {
      --m_writeBudget.messagesInUse;
    }
  m_writeBudget.bytesInUse =
      (m_writeBudget.bytesInUse >= payloadBytes) ? (m_writeBudget.bytesInUse - payloadBytes) : 0;
  GetControlState (peerFep).creditGateBlocked = false;
  if (m_sesManager)
    {
      m_sesManager->NotifyPeerCreditsAvailable (peerFep);
    }
}

bool
PdsManager::TryReserveReadResponderBudget (uint32_t peerFep, uint32_t payloadBytes)
{
  const bool messagesOk =
      (m_readResponderBudget.messageLimit == 0) ||
      (m_readResponderBudget.messagesInUse + 1 <= m_readResponderBudget.messageLimit);
  const bool bytesOk =
      (m_readResponderBudget.byteLimit == 0) ||
      (m_readResponderBudget.bytesInUse + payloadBytes <= m_readResponderBudget.byteLimit);
  if (!messagesOk || !bytesOk)
    {
      ++m_readResponderBudget.blockedCount;
      GetControlState (peerFep).creditGateBlocked = true;
      return false;
    }

  ++m_readResponderBudget.messagesInUse;
  m_readResponderBudget.bytesInUse += payloadBytes;
  m_readResponderBudget.messagesInUsePeak =
      std::max (m_readResponderBudget.messagesInUsePeak, m_readResponderBudget.messagesInUse);
  m_readResponderBudget.bytesInUsePeak =
      std::max (m_readResponderBudget.bytesInUsePeak, m_readResponderBudget.bytesInUse);
  return true;
}

void
PdsManager::ReleaseReadResponderBudget (uint32_t peerFep, uint32_t payloadBytes)
{
  if (m_readResponderBudget.messagesInUse > 0)
    {
      --m_readResponderBudget.messagesInUse;
    }
  m_readResponderBudget.bytesInUse =
      (m_readResponderBudget.bytesInUse >= payloadBytes) ? (m_readResponderBudget.bytesInUse - payloadBytes) : 0;
  GetControlState (peerFep).creditGateBlocked = false;
  if (m_sesManager)
    {
      m_sesManager->NotifyPeerCreditsAvailable (peerFep);
    }
}

void
PdsManager::PruneReceiveState (void)
{
  PruneCompletedSendTombstones ();
  const int64_t now = NowMs ();
  const int64_t ttl = std::max<int64_t> (500, m_receiveStateTimeout.GetMilliSeconds () * 4);

  for (auto it = m_rxMessageContexts.begin (); it != m_rxMessageContexts.end ();)
    {
      RxMessageContext& context = it->second;
      if (!context.retired && (now - context.last_activity_ms) >= ttl)
        {
          context.failed = !context.completed;
          if (context.mode == RxMessageMode::READ_RESPONSE)
            {
              if (m_sesManager)
                {
                  m_sesManager->NotifyReadResponseTargetReleased (
                      context.job_id,
                      context.msg_id,
                      context.src_fep,
                      static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
                }
              m_readResponseTargets.erase (BuildLookupKey (context.job_id, context.msg_id, context.src_fep));
            }
          RetireContextResources (context);
          ++m_staleCleanupCount;
        }

      if (context.retired && (now - context.retired_at_ms) >= ttl)
        {
          it = m_rxMessageContexts.erase (it);
        }
      else
        {
          ++it;
        }
    }

  for (auto it = m_postedReceives.begin (); it != m_postedReceives.end ();)
    {
      if (it->second.consumed)
        {
          it = m_postedReceives.erase (it);
        }
      else
        {
          ++it;
        }
    }
}

void
PdsManager::ResetReceiveState (void)
{
  for (auto& entry : m_controlStates)
    {
      if (entry.second.refreshEvent.IsPending ())
        {
          Simulator::Cancel (entry.second.refreshEvent);
        }
    }
  m_postedReceives.clear ();
  m_readResponseTargets.clear ();
  m_rxMessageContexts.clear ();
  m_completedSendTombstones.clear ();
  m_completedSendLookupTombstones.clear ();
  m_receivePdcs.clear ();
  m_currentUnexpectedBytes = 0;
  m_receivePool.unexpectedMessagesInUse = 0;
  m_receivePool.unexpectedBytesInUse = 0;
  m_receivePool.arrivalBlocksInUse = 0;
  m_receivePool.readTracksInUse = 0;
  m_sendAdmissionBudget.messagesInUse = 0;
  m_sendAdmissionBudget.bytesInUse = 0;
  m_writeBudget.messagesInUse = 0;
  m_writeBudget.bytesInUse = 0;
  m_readResponderBudget.messagesInUse = 0;
  m_readResponderBudget.bytesInUse = 0;
  m_controlStates.clear ();
}

bool
PdsManager::EnsureReceivePdc (uint16_t pdcId, uint32_t sourceFep, bool reliable)
{
  NS_LOG_FUNCTION (this << "Ensure receive PDC " << pdcId << " remoteFep=" << sourceFep);

  const ReceivePdcKey receiveKey{sourceFep, pdcId};
  auto it = m_receivePdcs.find (receiveKey);
  if (it != m_receivePdcs.end ())
  {
    return true;
  }

  if (m_pdcs.size () + m_receivePdcs.size () >= m_maxPdcCount)
  {
    NS_LOG_WARN ("EnsureReceivePdc: max PDC count reached");
    return false;
  }

  if (pdcId > MAX_PDC_ID)
  {
    return false;
  }

  Ptr<PdcBase> pdc;
  if (reliable)
  {
    Ptr<Tpdc> tpdc = Create<Tpdc> ();
    TpdcConfig tpdcConfig;
    tpdcConfig.pdcId = pdcId;
    tpdcConfig.localFep = m_netDevice->GetLocalFep ();
    tpdcConfig.remoteFep = sourceFep;
    tpdcConfig.maxPacketSize = m_netDevice ? m_netDevice->GetMtu () : 1500;
    tpdcConfig.deliveryMode = DeliveryMode::RUD;
    tpdcConfig.initialRto = MilliSeconds (30);
    tpdcConfig.maxRto = MilliSeconds (120);
    tpdcConfig.ackTimeout = MilliSeconds (1);
    tpdcConfig.readResponseAggressiveDrain = m_tpdcReadResponseAggressiveDrain;
    tpdcConfig.readResponseQueuePriority = m_tpdcReadResponseQueuePriority;
    pdc = tpdc;
    if (!pdc)
    {
      return false;
    }
    pdc->SetNetDevice (m_netDevice);
    if (!pdc->Initialize (tpdcConfig))
    {
      return false;
    }
  }
  else
  {
    pdc = Create<Ipdc> ();
    if (!pdc)
    {
      return false;
    }
    pdc->SetNetDevice (m_netDevice);
    PdcConfig config;
    config.pdcId = pdcId;
    config.localFep = m_netDevice->GetLocalFep ();
    config.remoteFep = sourceFep;
    config.maxPacketSize = m_netDevice ? m_netDevice->GetMtu () : 1500;
    config.deliveryMode = DeliveryMode::RUD;
    if (!pdc->Initialize (config))
    {
      return false;
    }
  }
  pdc->Activate ();

  m_receivePdcs[receiveKey] = pdc;

  if (m_statistics && m_statisticsEnabled)
  {
    m_statistics->IncrementPdcCreations ();
  }

  NS_LOG_DEBUG ("Created receive PDC " << pdcId << " for remote FEP " << sourceFep);
  return true;
}

uint16_t
PdsManager::AllocatePdc (uint32_t destFep, uint8_t tc, uint8_t dm,
                         PDSNextHeader nextHdr, uint16_t jobLandingJob, uint16_t nextJob)
{
  NS_LOG_FUNCTION (this << "Allocating PDC for destFep=" << destFep <<
                  " tc=" << static_cast<int> (tc) << " dm=" << static_cast<int> (dm));

  (void) nextHdr;
  (void) jobLandingJob;
  (void) nextJob;
  const bool reliable = (dm == static_cast<uint8_t> (DeliveryMode::RUD) ||
                         dm == static_cast<uint8_t> (DeliveryMode::ROD));
  if (reliable)
  {
    const SessionKey key{destFep, tc, dm};
    auto existing = m_outgoingSessions.find (key);
    if (existing != m_outgoingSessions.end ())
    {
      Ptr<PdcBase> active = GetPdc (existing->second);
      if (active)
      {
        return existing->second;
      }
      m_outgoingSessions.erase (existing);
    }
  }

  // Check if we have reached maximum PDC count
  if (m_pdcs.size () >= m_maxPdcCount)
  {
    NS_LOG_ERROR ("Maximum PDC count reached: " << m_maxPdcCount);
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::PDC_FULL);
    }
    return UINT16_MAX;  // Return sentinel value for failure
  }

  // Optimized PDC ID allocation - O(1) average case
  // Use a flag to track whether we found a valid ID (since ID 0 is now valid)
  bool foundId = false;
  uint16_t pdcId = 0;

  // First try to use a recently freed ID from the queue
  if (!m_freePdcIds.empty ())
  {
    pdcId = m_freePdcIds.front ();
    m_freePdcIds.pop ();
    foundId = true;
    NS_LOG_DEBUG ("Reusing freed PDC ID: " << pdcId);
  }
  else
  {
    // Find next available ID using bitmap for O(1) lookup
    while (m_nextPdcId <= MAX_PDC_ID)
    {
      if (!m_pdcIdBitmap[m_nextPdcId])
      {
        pdcId = m_nextPdcId;
        m_nextPdcId++;
        foundId = true;
        break;
      }
      m_nextPdcId++;
    }

    // If we wrapped around and still didn't find a free ID, check if any IDs are available
    if (!foundId && m_pdcs.size () < m_maxPdcCount)
    {
      // Search from beginning for any free ID
      for (uint16_t id = 0; id <= MAX_PDC_ID && id < m_maxPdcCount; ++id)
      {
        if (!m_pdcIdBitmap[id])
        {
          pdcId = id;
          m_nextPdcId = id + 1;
          foundId = true;
          break;
        }
      }
    }

    if (!foundId)
    {
      NS_LOG_ERROR ("No free PDC IDs available (max: " << m_maxPdcCount << ")");
      if (m_statistics && m_statisticsEnabled)
      {
        m_statistics->IncrementErrors (PdsErrorCode::PDC_FULL);
      }
      return UINT16_MAX;  // Return sentinel value for failure
    }
  }

  // Mark the ID as used in bitmap
  m_pdcIdBitmap[pdcId] = true;

  Ptr<PdcBase> pdc;
  if (reliable)
  {
    Ptr<Tpdc> tpdc = Create<Tpdc> ();
    TpdcConfig tpdcConfig;
    tpdcConfig.pdcId = pdcId;
    tpdcConfig.localFep = m_netDevice->GetLocalFep ();
    tpdcConfig.remoteFep = destFep;
    tpdcConfig.tc = tc;
    tpdcConfig.deliveryMode = static_cast<DeliveryMode> (dm);
    tpdcConfig.maxPacketSize = m_netDevice ? m_netDevice->GetMtu () : 1500;
    tpdcConfig.initialRto = MilliSeconds (30);
    tpdcConfig.maxRto = MilliSeconds (120);
    tpdcConfig.ackTimeout = MilliSeconds (1);
    tpdcConfig.maxRetransmissions = Max_RTO_Retx_Cnt + 1;
    tpdcConfig.readResponseAggressiveDrain = m_tpdcReadResponseAggressiveDrain;
    tpdcConfig.readResponseQueuePriority = m_tpdcReadResponseQueuePriority;
    pdc = tpdc;
    if (!pdc)
    {
      NS_LOG_ERROR ("Failed to create PDC");
      m_pdcIdBitmap[pdcId] = false;
      return UINT16_MAX;
    }
    pdc->SetNetDevice (m_netDevice);
    if (!pdc->Initialize (tpdcConfig))
    {
      NS_LOG_ERROR ("Failed to initialize PDC " << pdcId);
      m_pdcIdBitmap[pdcId] = false;
      return UINT16_MAX;
    }
  }
  else
  {
    pdc = Create<Ipdc> ();
    if (!pdc)
    {
      NS_LOG_ERROR ("Failed to create PDC");
      m_pdcIdBitmap[pdcId] = false;
      return UINT16_MAX;
    }
    pdc->SetNetDevice (m_netDevice);
    PdcConfig config;
    config.pdcId = pdcId;
    config.localFep = m_netDevice->GetLocalFep ();
    config.remoteFep = destFep;
    config.tc = tc;
    config.deliveryMode = static_cast<DeliveryMode> (dm);
    config.maxPacketSize = m_netDevice ? m_netDevice->GetMtu () : 1500;
    if (!pdc->Initialize (config))
    {
      NS_LOG_ERROR ("Failed to initialize PDC " << pdcId);
      m_pdcIdBitmap[pdcId] = false;
      return UINT16_MAX;
    }
  }
  pdc->Activate ();

  // Add to PDC container
  m_pdcs[pdcId] = pdc;
  m_nextPdcId = pdcId + 1;
  if (reliable)
  {
    m_outgoingSessions[SessionKey{destFep, tc, dm}] = pdcId;
  }

  if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementPdcCreations ();
    }

  NS_LOG_INFO ("Successfully allocated PDC " << pdcId);
  return pdcId;
}

bool
PdsManager::ReleasePdc (uint16_t pdcId)
{
  NS_LOG_FUNCTION (this << "Releasing PDC " << pdcId);

  // Find and remove PDC from container
  auto it = m_pdcs.find (pdcId);
  if (it == m_pdcs.end ())
  {
    NS_LOG_WARN ("PDC " << pdcId << " not found for release");
    return false;
  }

  // Remove PDC from container and mark ID as available for reuse
  uint16_t freedPdcId = pdcId;
  m_pdcs.erase (it);
  for (auto sessionIt = m_outgoingSessions.begin (); sessionIt != m_outgoingSessions.end (); )
  {
    if (sessionIt->second == pdcId)
    {
      sessionIt = m_outgoingSessions.erase (sessionIt);
    }
    else
    {
      ++sessionIt;
    }
  }

  // Mark ID as free in bitmap and add to reuse queue for O(1) allocation
  if (freedPdcId <= MAX_PDC_ID)
  {
    m_pdcIdBitmap[freedPdcId] = false;
    m_freePdcIds.push (freedPdcId);
    NS_LOG_DEBUG ("PDC ID " << freedPdcId << " marked as available for reuse");
  }

  if (m_statistics && m_statisticsEnabled)
  {
    m_statistics->IncrementPdcDestructions ();
  }

  NS_LOG_INFO ("Released PDC " << pdcId);
  return true;
}

bool
PdsManager::SendPacketThroughPdc (uint16_t pdcId, Ptr<Packet> packet, bool som, bool eom)
{
  NS_LOG_FUNCTION (this << "Sending packet through PDC " << pdcId);

  if (!packet)
  {
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::INVALID_PDC);
    }
    return false;
  }

  // Get PDC from container
  Ptr<PdcBase> pdc = GetPdc (pdcId);
  if (!pdc)
  {
    NS_LOG_ERROR ("PDC " << pdcId << " not found");
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::INVALID_PDC);
    }
    return false;
  }

  NS_LOG_INFO ("[UEC-E2E] [PDS] SendPacketThroughPdc pdc_id=" << pdcId << " -> PDC instance send");

  // Send packet through PDC
  bool success = pdc->SendPacket (packet, som, eom);
  if (!success)
  {
    NS_LOG_ERROR ("Failed to send packet through PDC " << pdcId);
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::PROTOCOL_ERROR);
    }
    return false;
  }

  // Increment sent packets count
  if (m_statistics && m_statisticsEnabled)
  {
    m_statistics->IncrementSentPackets ();
  }

  NS_LOG_INFO ("Packet sent successfully through PDC " << pdcId);
  return true;
}

bool
PdsManager::DispatchPacket (const SesPdsRequest& request)
{
  NS_LOG_FUNCTION (this << "Dispatching packet to PDC");

  if (m_sesManager)
    m_sesManager->NotifyPause (true);  // Diagnostic hook only; control-plane truth stays in RudControlState.

  // Allocate a new PDC for this request
  uint16_t pdcId = AllocatePdc (request.dst_fep, request.tc, request.mode,
                               request.next_hdr, 0, 0);
  if (pdcId == UINT16_MAX)
  {
    if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (PdsErrorCode::PDC_FULL);
    }
    if (m_sesManager)
      {
        m_sesManager->NotifyPdsErrorEvent (0, static_cast<int> (PdsErrorCode::PDC_FULL),
                                           "PDC allocation failed");
      }
    return false;
  }

  NS_LOG_INFO ("[UEC-E2E] [PDS] AllocatePdc pdc_id=" << pdcId
               << " -> SendPacketThroughPdc (via PDC)");

  // Send packet through PDC
  bool success = SendPacketThroughPdc (pdcId, request.packet, request.som, request.eom);
  if (success && m_sesManager)
    {
      m_sesManager->NotifyTxResponse (pdcId);  // Diagnostic hook only; control-plane truth stays in RudControlState.
      m_sesManager->NotifyEagerSize (1372);    // Diagnostic hook only; control-plane truth stays in RudControlState.
      m_sesManager->NotifyPause (false);       // Diagnostic hook only; control-plane truth stays in RudControlState.
    }
  if (!success && m_sesManager)
    m_sesManager->NotifyPause (false);
  if (!success)
  {
    NS_LOG_ERROR ("Failed to send packet through PDC " << pdcId);
  }

  return success;
}

void
PdsManager::HandlePdcError (uint16_t pdcId, PdsErrorCode error, const std::string& errorDetails)
{
  NS_LOG_FUNCTION (this << "Handling PDC error for PDC " << pdcId <<
                  " error=" << static_cast<int> (error) << " details=" << errorDetails);

  if (m_statistics && m_statisticsEnabled)
    {
      m_statistics->IncrementErrors (error);
    }

  if (m_sesManager)
    {
      m_sesManager->NotifyPdsErrorEvent (pdcId, static_cast<int> (error), errorDetails);
    }
  NS_LOG_WARN ("PDC error handled for PDC " << pdcId << ": " << errorDetails);
}

Ptr<PdcBase>
PdsManager::GetPdc (uint16_t pdcId) const
{
  auto it = m_pdcs.find (pdcId);
  if (it != m_pdcs.end ())
  {
    return it->second;
  }
  return nullptr;
}

Ptr<PdcBase>
PdsManager::GetReceivePdc (uint32_t sourceFep, uint16_t pdcId) const
{
  const ReceivePdcKey receiveKey{sourceFep, pdcId};
  auto it = m_receivePdcs.find (receiveKey);
  if (it != m_receivePdcs.end ())
    {
      return it->second;
    }
  return nullptr;
}

uint32_t
PdsManager::GetActivePdcs (void) const
{
  return static_cast<uint32_t> (m_pdcs.size () + m_receivePdcs.size ());
}

uint32_t
PdsManager::GetTotalActivePdcCount (void) const
{
  return GetActivePdcs ();
}

uint32_t
PdsManager::GetActiveTpdcCount (void) const
{
  uint32_t count = 0;
  for (const auto& entry : m_pdcs)
  {
    if (DynamicCast<Tpdc> (entry.second))
    {
      ++count;
    }
  }
  for (const auto& entry : m_receivePdcs)
  {
    if (DynamicCast<Tpdc> (entry.second))
    {
      ++count;
    }
  }
  return count;
}

uint32_t
PdsManager::GetTpdcInflightPackets (void) const
{
  uint32_t total = 0;
  for (const auto& entry : m_pdcs)
  {
    Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
    if (tpdc)
    {
      total += tpdc->GetInflightPacketCount ();
    }
  }
  for (const auto& entry : m_receivePdcs)
  {
    Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
    if (tpdc)
    {
      total += tpdc->GetInflightPacketCount ();
    }
  }
  return total;
}

uint32_t
PdsManager::GetTpdcOutOfOrderPackets (void) const
{
  uint32_t total = 0;
  for (const auto& entry : m_pdcs)
  {
    Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
    if (tpdc)
    {
      total += tpdc->GetOutOfOrderPacketCount ();
    }
  }
  for (const auto& entry : m_receivePdcs)
  {
    Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
    if (tpdc)
    {
      total += tpdc->GetOutOfOrderPacketCount ();
    }
  }
  return total;
}

uint32_t
PdsManager::GetTpdcPendingSacks (void) const
{
  uint32_t total = 0;
  for (const auto& entry : m_pdcs)
  {
    Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
    if (tpdc)
    {
      total += tpdc->GetPendingSelectiveAckCount ();
    }
  }
  for (const auto& entry : m_receivePdcs)
  {
    Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
    if (tpdc)
    {
      total += tpdc->GetPendingSelectiveAckCount ();
    }
  }
  return total;
}

uint32_t
PdsManager::GetTpdcPendingGapNacks (void) const
{
  uint32_t total = 0;
  for (const auto& entry : m_pdcs)
  {
    Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
    if (tpdc)
    {
      total += tpdc->GetPendingGapNackCount ();
    }
  }
  for (const auto& entry : m_receivePdcs)
  {
    Ptr<Tpdc> tpdc = DynamicCast<Tpdc> (entry.second);
    if (tpdc)
    {
      total += tpdc->GetPendingGapNackCount ();
    }
  }
  return total;
}

Ptr<PdsStatistics>
PdsManager::GetStatistics (void) const
{
  return m_statistics;
}

void
PdsManager::ResetStatistics (void)
{
  if (m_statistics)
  {
    m_statistics->Reset ();
  }
  m_creditRefreshSent = 0;
  m_ackCtrlExtSent = 0;
  m_creditGateBlockedCount = 0;
  m_sackSent = 0;
  m_gapNackSent = 0;
  m_sendAdmissionBudget.blockedCount = 0;
  m_sendAdmissionBudget.blockedByMessages = 0;
  m_sendAdmissionBudget.blockedByBytes = 0;
  m_sendAdmissionBudget.blockedByBoth = 0;
  m_sendAdmissionBudget.messagesInUsePeak = 0;
  m_sendAdmissionBudget.bytesInUsePeak = 0;
  m_sendAdmissionBudget.releaseCount = 0;
  m_sendAdmissionBudget.releaseBytesTotal = 0;
  m_writeBudget.blockedCount = 0;
  m_writeBudget.messagesInUsePeak = 0;
  m_writeBudget.bytesInUsePeak = 0;
  m_readResponderBudget.blockedCount = 0;
  m_readResponderBudget.messagesInUsePeak = 0;
  m_readResponderBudget.bytesInUsePeak = 0;
}

std::string
PdsManager::GetStatisticsString (void) const
{
  if (m_statistics)
    {
      return m_statistics->GetStatistics ();
    }
  return "Statistics not available";
}

void
PdsManager::SetStatisticsEnabled (bool enabled)
{
  m_statisticsEnabled = enabled;
}

bool
PdsManager::IsStatisticsEnabled (void) const
{
  return m_statisticsEnabled;
}

PdsManager::PdsState
PdsManager::GetState (void) const
{
  return m_state;
}

bool
PdsManager::IsBusy (void) const
{
  return m_state == PDS_BUSY;
}

bool
PdsManager::IsError (void) const
{
  return m_state == PDS_ERROR;
}

void
PdsManager::Reset (void)
{
  NS_LOG_FUNCTION (this);

  // Reset state
  m_state = PDS_IDLE;

  NS_LOG_INFO ("PdsManager reset to IDLE state");
}

bool
PdsManager::ValidateSesPdsRequest (const SesPdsRequest& request) const
{
  NS_LOG_FUNCTION (this);

  // Check if packet exists
  if (!request.packet)
  {
    NS_LOG_WARN ("SES PDS request has null packet");
    return false;
  }

  // Validate packet size
  if (request.packet->GetSize () == 0)
  {
    NS_LOG_WARN ("SES PDS request has empty packet");
    return false;
  }

  // Validate packet size against reasonable limit (e.g., 64KB)
  const uint32_t MAX_PACKET_SIZE = 65536;
  if (request.packet->GetSize () > MAX_PACKET_SIZE)
  {
    NS_LOG_WARN ("SES PDS request packet size " << request.packet->GetSize ()
                 << " exceeds maximum allowed size " << MAX_PACKET_SIZE);
    return false;
  }

  // Validate destination FEP address
  if (request.dst_fep == 0)
  {
    NS_LOG_WARN ("SES PDS request has invalid destination FEP address (0)");
    return false;
  }

  // Validate source FEP address
  if (request.src_fep == 0)
  {
    NS_LOG_WARN ("SES PDS request has invalid source FEP address (0)");
    return false;
  }

  // Validate mode field
  if (request.mode > 7)  // Assuming 3-bit mode field
  {
    NS_LOG_WARN ("SES PDS request has invalid mode: " << request.mode);
    return false;
  }

  // Validate traffic class
  if (request.tc > 255)  // 8-bit traffic class
  {
    NS_LOG_WARN ("SES PDS request has invalid traffic class: " << request.tc);
    return false;
  }

  // Validate packet length consistency
  if (request.pkt_len != request.packet->GetSize ())
  {
    NS_LOG_WARN ("SES PDS request packet length mismatch: header says "
                 << request.pkt_len << ", actual packet size is "
                 << request.packet->GetSize ());
    return false;
  }

  return true;
}

} // namespace ns3
