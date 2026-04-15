#include "tpdc.h"
#include "../common/soft-ue-packet-tag.h"
#include "../network/soft-ue-net-device.h"
#include "../ses/ses-manager.h"
#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include <algorithm>
#include <cmath>

namespace ns3 {

namespace {

void
SetTransportAcceptTimestamp (Ptr<Packet> packet, Time acceptTime, bool queuedForSendWindow)
{
  if (!packet)
    {
      return;
    }
  SoftUeTransportTimingTag timingTag;
  packet->PeekPacketTag (timingTag);
  packet->RemovePacketTag (timingTag);
  timingTag.SetTransportAcceptTime (acceptTime);
  timingTag.SetQueuedForSendWindow (queuedForSendWindow);
  packet->AddPacketTag (timingTag);
}

void
SetTransportTransmitTimestamp (Ptr<Packet> packet, Time transmitTime)
{
  if (!packet)
    {
      return;
    }
  SoftUeTransportTimingTag timingTag;
  packet->PeekPacketTag (timingTag);
  packet->RemovePacketTag (timingTag);
  timingTag.SetTransportTransmitTime (transmitTime);
  packet->AddPacketTag (timingTag);
}

} // namespace

NS_LOG_COMPONENT_DEFINE ("Tpdc");
NS_OBJECT_ENSURE_REGISTERED (Tpdc);

TypeId
Tpdc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Tpdc")
    .SetParent<PdcBase> ()
    .SetGroupName ("Soft-Ue")
    .AddConstructor<Tpdc> ();
  return tid;
}

TypeId
Tpdc::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

Tpdc::Tpdc ()
  : PdcBase (),
    m_nextSendSequence (1),
    m_nextReceiveSequence (1),
    m_sendWindowBase (1),
    m_receiveWindowBase (1),
    m_highestObservedSequence (0),
    m_ackInterval (MilliSeconds (1)),
    m_pendingSelectiveAckCount (0),
    m_pendingGapNackCount (0),
    m_currentRto (MilliSeconds (30)),
    m_currentRtt (Seconds (0)),
    m_rttVariance (Seconds (0)),
    m_txDataPacketsTotal (0),
    m_txControlPacketsTotal (0),
    m_rxDataPacketsTotal (0),
    m_rxControlPacketsTotal (0),
    m_ackSentTotal (0),
    m_ackReceivedTotal (0),
    m_gapNackReceivedTotal (0),
    m_lastAckSequence (0),
    m_sendBufferSizeMax (0),
    m_ackAdvanceEventsTotal (0)
{
  NS_LOG_FUNCTION (this);
  m_tpdcConfig = TpdcConfig ();
  m_tpdcStatistics = TpdcStatistics ();
}

Tpdc::~Tpdc ()
{
  NS_LOG_FUNCTION (this);
}

void
Tpdc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  if (m_ackEventId.IsPending ())
    {
      Simulator::Cancel (m_ackEventId);
    }

  ClearSendBuffer ();
  ClearReceiveBuffer ();
  while (!m_sendQueue.empty ())
    {
      m_sendQueue.pop_front ();
    }
  while (!m_ackQueue.empty ())
    {
      m_ackQueue.pop ();
    }

  PdcBase::DoDispose ();
}

bool
Tpdc::Initialize (const PdcConfig& config)
{
  NS_LOG_FUNCTION (this << "Initializing TPDC " << config.pdcId);

  if (!PdcBase::Initialize (config))
    {
      return false;
    }

  m_tpdcConfig = TpdcConfig ();
  m_tpdcConfig.pdcId = config.pdcId;
  m_tpdcConfig.localFep = config.localFep;
  m_tpdcConfig.remoteFep = config.remoteFep;
  m_tpdcConfig.tc = config.tc;
  m_tpdcConfig.type = PdcType::TPDC;
  m_tpdcConfig.deliveryMode = config.deliveryMode;
  m_tpdcConfig.maxPacketSize = config.maxPacketSize;
  m_tpdcConfig.rtoPdcContext = config.rtoPdcContext;
  m_tpdcConfig.rtoCccContext = config.rtoCccContext;
  m_tpdcConfig.rtoInitial = config.rtoInitial;
  m_tpdcConfig.rtoMax = config.rtoMax;
  m_tpdcConfig.detailedLogging = config.detailedLogging;
  m_tpdcConfig.sequenceNumber = config.sequenceNumber;

  if (config.type == PdcType::TPDC)
    {
      const TpdcConfig& typedConfig = static_cast<const TpdcConfig&> (config);
      m_tpdcConfig = typedConfig;
    }

  m_nextSendSequence = 1;
  m_nextReceiveSequence = 1;
  m_sendWindowBase = 1;
  m_receiveWindowBase = 1;
  m_highestObservedSequence = 0;
  m_pendingSelectiveAckCount = 0;
  m_pendingGapNackCount = 0;
  m_currentRto = m_tpdcConfig.initialRto;
  m_txDataPacketsTotal = 0;
  m_txControlPacketsTotal = 0;
  m_rxDataPacketsTotal = 0;
  m_rxControlPacketsTotal = 0;
  m_ackSentTotal = 0;
  m_ackReceivedTotal = 0;
  m_gapNackReceivedTotal = 0;
  m_lastAckSequence = 0;
  m_sendBufferSizeMax = 0;
  m_ackAdvanceEventsTotal = 0;
  m_gapNackObservedNsBySeq.clear ();
  m_readGapContextByMissingSeq.clear ();
  m_gapNackObservedNsBySeq.clear ();

  NS_LOG_INFO ("TPDC " << m_tpdcConfig.pdcId << " initialized successfully");
  return true;
}

bool
Tpdc::SendPacket (Ptr<Packet> packet, bool som, bool eom)
{
  NS_LOG_FUNCTION (this << packet << som << eom);

  if (!packet || !IsActive ())
    {
      HandleError (PdsErrorCode::INVALID_PACKET, "Null packet or inactive TPDC");
      return false;
    }

  if (!ValidatePacket (packet, true))
    {
      HandleError (PdsErrorCode::INVALID_PACKET, "Packet validation failed");
      return false;
    }

  return BufferPacketForSending (packet, som, eom);
}

bool
Tpdc::HandleReceivedPacket (Ptr<Packet> packet, uint32_t sourceFep)
{
  NS_LOG_FUNCTION (this << packet << sourceFep);

  if (!ValidateAndRecordReceivedPacket (packet, sourceFep))
    {
      return false;
    }

  PDSHeader header;
  packet->PeekHeader (header);

  SoftUeTpdcControlTag controlTag;
  if (packet->PeekPacketTag (controlTag))
    {
      SoftUeRecoveryTag recoveryTag;
      const Time gapNackSentTime =
          packet->PeekPacketTag (recoveryTag) ? recoveryTag.GetGapNackSentTime () : Time::Max ();
      Acknowledgment ack;
      ack.ackSequence = controlTag.GetCumulativeAck ();
      ack.receiveWindow = m_tpdcConfig.receiveWindowSize;
      ack.cumulative = (controlTag.GetFlags () & SOFT_UE_TPDC_CTRL_ACK) != 0;
      if ((controlTag.GetFlags () & SOFT_UE_TPDC_CTRL_GAP_NACK) != 0 &&
          controlTag.GetGapNack () != 0)
        {
          ack.nackList.push_back (controlTag.GetGapNack ());
        }

      bool handled = ProcessReceivedAcknowledgment (ack, gapNackSentTime);
      if ((controlTag.GetFlags () & SOFT_UE_TPDC_CTRL_SACK) != 0 &&
          controlTag.GetSelectiveAck () != 0)
        {
          auto it = m_sendBuffer.find (controlTag.GetSelectiveAck ());
          if (it != m_sendBuffer.end ())
            {
              it->second.acknowledged = true;
              handled = true;
            }
          CleanupAcknowledgedPackets ();
          ProcessSendQueue ();
        }

      return handled;
    }

  return BufferPacketForReceiving (packet, header.GetSequenceNumber ());
}

TpdcStatistics
Tpdc::GetTpdcStatistics (void) const
{
  TpdcStatistics stats = m_tpdcStatistics;
  stats.currentSendBufferSize = static_cast<uint32_t> (m_sendBuffer.size ());
  stats.currentReceiveBufferSize = static_cast<uint32_t> (m_receiveBuffer.size ());
  stats.averageRto = m_currentRto.GetMilliSeconds ();
  stats.averageRoundTripTime = m_currentRtt;
  return stats;
}

void
Tpdc::ResetTpdcStatistics (void)
{
  NS_LOG_FUNCTION (this);
  m_tpdcStatistics = TpdcStatistics ();
  m_txDataPacketsTotal = 0;
  m_txControlPacketsTotal = 0;
  m_rxDataPacketsTotal = 0;
  m_rxControlPacketsTotal = 0;
  m_ackSentTotal = 0;
  m_ackReceivedTotal = 0;
  m_gapNackReceivedTotal = 0;
  m_lastAckSequence = 0;
  m_sendBufferSizeMax = 0;
  m_ackAdvanceEventsTotal = 0;
}

uint32_t
Tpdc::GetSendBufferSize (void) const
{
  return static_cast<uint32_t> (m_sendBuffer.size ());
}

uint32_t
Tpdc::GetReceiveBufferSize (void) const
{
  return static_cast<uint32_t> (m_receiveBuffer.size ());
}

TpdcConfig
Tpdc::GetTpdcConfiguration (void) const
{
  return m_tpdcConfig;
}

bool
Tpdc::UpdateTpdcConfiguration (const TpdcConfig& config)
{
  NS_LOG_FUNCTION (this << "Updating TPDC configuration");

  if (config.pdcId != m_tpdcConfig.pdcId)
    {
      NS_LOG_ERROR ("Cannot change PDC ID after initialization");
      return false;
    }

  m_tpdcConfig = config;
  m_currentRto = config.initialRto;
  return true;
}

bool
Tpdc::SendAcknowledgment (const Acknowledgment& ack)
{
  NS_LOG_FUNCTION (this << ack.ackSequence);

  uint8_t flags = SOFT_UE_TPDC_CTRL_ACK;
  uint32_t selectiveAck = 0;
  uint32_t gapNack = 0;
  if (!m_receiveBuffer.empty ())
    {
      flags |= SOFT_UE_TPDC_CTRL_SACK;
      selectiveAck = std::min_element (
        m_receiveBuffer.begin (),
        m_receiveBuffer.end (),
        [] (const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; })->first;
    }
  if (!ack.nackList.empty ())
    {
      flags |= SOFT_UE_TPDC_CTRL_GAP_NACK;
      gapNack = ack.nackList.front ();
    }

  if (!SendControlPacket (flags, ack.ackSequence, selectiveAck, gapNack))
    {
      return false;
    }

  m_tpdcStatistics.acknowledgmentsSent++;
  ++m_ackSentTotal;
  if (ack.cumulative)
    {
      m_tpdcStatistics.cumulativeAcksSent++;
    }
  m_pendingSelectiveAckCount = static_cast<uint32_t> (m_receiveBuffer.size ());
  m_pendingGapNackCount = ack.nackList.empty () ? 0u : 1u;
  m_ackTrace (ack);
  return true;
}

bool
Tpdc::ProcessReceivedAcknowledgment (const Acknowledgment& ack, Time gapNackSentTime)
{
  NS_LOG_FUNCTION (this << ack.ackSequence);

  m_tpdcStatistics.acknowledgmentsReceived++;
  ++m_rxControlPacketsTotal;
  ++m_ackReceivedTotal;
  if (!ack.nackList.empty ())
    {
      ++m_gapNackReceivedTotal;
    }
  m_lastAckSequence = ack.ackSequence;

  if (ack.ackSequence + 1 < m_sendWindowBase)
    {
      m_tpdcStatistics.duplicateAcksReceived++;
      return false;
    }

  bool advanced = false;
  const uint32_t newBase = std::max (m_sendWindowBase, ack.ackSequence + 1);
  if (newBase > m_sendWindowBase)
    {
      ++m_ackAdvanceEventsTotal;
    }
  for (uint32_t seq = m_sendWindowBase; seq < newBase; ++seq)
    {
      auto it = m_sendBuffer.find (seq);
      if (it != m_sendBuffer.end () && !it->second.acknowledged)
        {
          it->second.acknowledged = true;
          const Time measuredRtt = Simulator::Now () - it->second.timestamp;
          UpdateRtoEstimate (measuredRtt);
          m_rttTrace (m_currentRtt, measuredRtt);
          advanced = true;
        }
    }
  m_sendWindowBase = newBase;

  for (uint32_t nackSeq : ack.nackList)
    {
      auto it = m_sendBuffer.find (nackSeq);
      if (it != m_sendBuffer.end () && !it->second.acknowledged)
        {
          SoftUeMetadataTag metadataTag;
          SoftUeHeaderTag headerTag;
          const bool hasMetadataTag = it->second.packet && it->second.packet->PeekPacketTag (metadataTag);
          const bool hasHeaderTag = it->second.packet && it->second.packet->PeekPacketTag (headerTag);
          const bool isReadResponse =
              hasMetadataTag &&
              metadataTag.GetOperationType () == OpType::READ &&
              metadataTag.GetResponseOpCode () ==
                  static_cast<uint8_t> (ResponseOpCode::UET_RESPONSE_W_DATA);
          if (gapNackSentTime != Time::Max () &&
              m_gapNackObservedNsBySeq.find (nackSeq) == m_gapNackObservedNsBySeq.end ())
            {
              m_gapNackObservedNsBySeq[nackSeq] =
                  static_cast<uint64_t> (gapNackSentTime.GetNanoSeconds ());
            }
          Ptr<SesManager> ses = GetNetDevice () ? GetNetDevice ()->GetSesManager () : GetSesManager ();
          if (isReadResponse && ses)
            {
              ses->NotifyReadResponseGapNackObservedAtSender (
                  hasHeaderTag ? headerTag.GetJobId () : 0,
                  static_cast<uint16_t> (metadataTag.GetMessageId ()),
                  hasHeaderTag ? headerTag.GetDestinationEndpoint () : 0,
                  nackSeq,
                  static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
              ses->NotifyExternalDiagnosticEvent (
                  "TpdcReadGapNackObservedAtSender",
                  "job=" + std::to_string (hasHeaderTag ? headerTag.GetJobId () : 0) +
                      " msg=" + std::to_string (metadataTag.GetMessageId ()) +
                      " peer=" + std::to_string (hasHeaderTag ? headerTag.GetDestinationEndpoint () : 0) +
                      " missing_seq=" + std::to_string (nackSeq) +
                      " observed_at_ns=" + std::to_string (Simulator::Now ().GetNanoSeconds ()) +
                      " gap_nack_sent_at_ns=" +
                      std::to_string (gapNackSentTime != Time::Max ()
                                          ? gapNackSentTime.GetNanoSeconds ()
                                          : 0));
            }
          RetransmitPacket (nackSeq);
        }
    }

  CleanupAcknowledgedPackets ();
  ProcessSendQueue ();
  UpdateSendWindow ();
  return advanced || !ack.nackList.empty ();
}

uint32_t
Tpdc::ForceRetransmission (void)
{
  NS_LOG_FUNCTION (this);

  uint32_t retransmitted = 0;
  for (auto& pair : m_sendBuffer)
    {
      if (!pair.second.acknowledged)
        {
          RetransmitPacket (pair.first);
          ++retransmitted;
        }
    }
  return retransmitted;
}

uint32_t
Tpdc::ClearSendBuffer (void)
{
  const uint32_t count = static_cast<uint32_t> (m_sendBuffer.size ());
  for (auto& entry : m_sendBuffer)
    {
      CancelTimeout (entry.second);
    }
  m_sendBuffer.clear ();
  m_gapNackObservedNsBySeq.clear ();
  m_readGapContextByMissingSeq.clear ();
  return count;
}

uint32_t
Tpdc::ClearReceiveBuffer (void)
{
  const uint32_t count = static_cast<uint32_t> (m_receiveBuffer.size ());
  m_receiveBuffer.clear ();
  m_pendingSelectiveAckCount = 0;
  m_pendingGapNackCount = 0;
  return count;
}

Time
Tpdc::GetCurrentRtt (void) const
{
  return m_currentRtt;
}

Time
Tpdc::GetCurrentRto (void) const
{
  return m_currentRto;
}

uint32_t
Tpdc::GetInflightPacketCount (void) const
{
  return static_cast<uint32_t> (m_sendBuffer.size ());
}

uint32_t
Tpdc::GetOutOfOrderPacketCount (void) const
{
  return static_cast<uint32_t> (m_receiveBuffer.size ());
}

uint32_t
Tpdc::GetPendingSelectiveAckCount (void) const
{
  return m_pendingSelectiveAckCount;
}

uint32_t
Tpdc::GetPendingGapNackCount (void) const
{
  return m_pendingGapNackCount;
}

TpdcSessionProgressRecord
Tpdc::GetSessionProgressRecord (void) const
{
  TpdcSessionProgressRecord record;
  record.timestamp_ns = Simulator::Now ().GetNanoSeconds ();
  record.local_fep = GetLocalFep ();
  record.remote_fep = GetRemoteFep ();
  record.pdc_id = m_tpdcConfig.pdcId;
  record.tx_data_packets_total = m_txDataPacketsTotal;
  record.tx_control_packets_total = m_txControlPacketsTotal;
  record.rx_data_packets_total = m_rxDataPacketsTotal;
  record.rx_control_packets_total = m_rxControlPacketsTotal;
  record.ack_sent_total = m_ackSentTotal;
  record.ack_received_total = m_ackReceivedTotal;
  record.gap_nack_received_total = m_gapNackReceivedTotal;
  record.last_ack_sequence = m_lastAckSequence;
  record.send_window_base = m_sendWindowBase;
  record.next_send_sequence = m_nextSendSequence;
  record.send_buffer_size_current = static_cast<uint32_t> (m_sendBuffer.size ());
  record.send_buffer_size_max = m_sendBufferSizeMax;
  record.retransmissions_total = static_cast<uint32_t> (m_tpdcStatistics.retransmissions);
  record.rto_timeouts_total = static_cast<uint32_t> (m_tpdcStatistics.retransmissionTimeouts);
  record.ack_advance_events_total = m_ackAdvanceEventsTotal;
  return record;
}

bool
Tpdc::ValidatePacket (Ptr<Packet> packet, bool isSend) const
{
  if (!PdcBase::ValidatePacket (packet, isSend))
    {
      return false;
    }

  return packet->GetSize () <= static_cast<uint32_t> (m_tpdcConfig.maxPacketSize) + 64u;
}

bool
Tpdc::HandleError (PdsErrorCode error, const std::string& details)
{
  NS_LOG_FUNCTION (this << static_cast<int> (error) << details);
  m_tpdcStatistics.errors++;
  return PdcBase::HandleError (error, details);
}

bool
Tpdc::BufferPacketForSending (Ptr<Packet> packet, bool som, bool eom)
{
  if (m_sendBuffer.size () >= m_tpdcConfig.maxSendBufferSize)
    {
      m_tpdcStatistics.sendBufferOverflows++;
      HandleError (PdsErrorCode::PDC_FULL, "Send buffer overflow");
      return false;
    }

  if (!IsInSendWindow (m_nextSendSequence))
    {
      Ptr<Packet> queuedPacket = packet->Copy ();
      SetTransportAcceptTimestamp (queuedPacket, Simulator::Now (), true);
      BufferedPacket buffered (queuedPacket, som, eom, 0);
      const bool isReadResponse = IsReadResponsePacket (queuedPacket);
      if (isReadResponse && m_tpdcConfig.readResponseQueuePriority)
        {
          m_sendQueue.push_front (buffered);
        }
      else
        {
          m_sendQueue.push_back (buffered);
        }
      return true;
    }

  const uint32_t seq = GenerateSequenceNumber ();
  Ptr<Packet> acceptedPacket = packet->Copy ();
  SetTransportAcceptTimestamp (acceptedPacket, Simulator::Now (), false);
  BufferedPacket buffered (acceptedPacket, som, eom, seq);
  m_sendBuffer[seq] = buffered;

  if (!TransmitPacket (m_sendBuffer[seq]))
    {
      m_sendBuffer.erase (seq);
      return false;
    }

  ArmTimeout (seq);
  UpdateStatistics ();
  return true;
}

bool
Tpdc::IsReadResponsePacket (Ptr<Packet> packet) const
{
  SoftUeMetadataTag metadataTag;
  return packet &&
         packet->PeekPacketTag (metadataTag) &&
         metadataTag.GetOperationType () == OpType::READ &&
         metadataTag.GetResponseOpCode () ==
             static_cast<uint8_t> (ResponseOpCode::UET_RESPONSE_W_DATA);
}

bool
Tpdc::BufferPacketForReceiving (Ptr<Packet> packet, uint32_t seq)
{
  ++m_rxDataPacketsTotal;
  PruneResolvedReadGapContexts ();
  if (seq < m_nextReceiveSequence)
    {
      PdcBase::UpdateStatistics (false, packet);
      m_packetRxTrace (packet, m_tpdcConfig.pdcId);
      ScheduleAcknowledgment (false);
      return true;
    }

  if (!IsInReceiveWindow (seq))
    {
      m_tpdcStatistics.outOfOrderPackets++;
      return false;
    }

  PdcBase::UpdateStatistics (false, packet);
  m_packetRxTrace (packet, m_tpdcConfig.pdcId);

  m_highestObservedSequence = std::max (m_highestObservedSequence, seq);

  if (seq == m_nextReceiveSequence)
    {
      ++m_nextReceiveSequence;
      while (m_receiveBuffer.erase (m_nextReceiveSequence) > 0)
        {
          ++m_nextReceiveSequence;
        }
      m_receiveWindowBase = m_nextReceiveSequence;
    }
  else
    {
      if (m_receiveBuffer.size () >= m_tpdcConfig.maxReceiveBufferSize)
        {
          m_tpdcStatistics.receiveBufferOverflows++;
          HandleError (PdsErrorCode::PDC_FULL, "Receive buffer overflow");
          return false;
        }
      if (m_receiveBuffer.find (seq) == m_receiveBuffer.end ())
        {
          m_receiveBuffer.emplace (seq, BufferedPacket (packet->Copy (), false, false, seq));
          m_tpdcStatistics.outOfOrderPackets++;
        }
    }

  m_pendingSelectiveAckCount = static_cast<uint32_t> (m_receiveBuffer.size ());
  m_pendingGapNackCount = (m_nextReceiveSequence <= m_highestObservedSequence &&
                           !m_receiveBuffer.empty ()) ? 1u : 0u;
  // When we observe a gap, emit ACK/NACK immediately so the sender can
  // recover the missing packet without waiting for the periodic ACK tick.
  const bool hasGap = (m_nextReceiveSequence <= m_highestObservedSequence &&
                       !m_receiveBuffer.empty ());
  if (hasGap)
    {
      SoftUeMetadataTag metadataTag;
      SoftUeHeaderTag headerTag;
      const bool hasMetadataTag = packet && packet->PeekPacketTag (metadataTag);
      const bool hasHeaderTag = packet && packet->PeekPacketTag (headerTag);
      const bool isReadResponse =
          hasMetadataTag &&
          metadataTag.GetOperationType () == OpType::READ &&
          metadataTag.GetResponseOpCode () ==
              static_cast<uint8_t> (ResponseOpCode::UET_RESPONSE_W_DATA);
      const uint32_t missingSeq = m_nextReceiveSequence;
      if (isReadResponse &&
          m_readGapContextByMissingSeq.find (missingSeq) == m_readGapContextByMissingSeq.end ())
        {
          ReadGapDiagnosticContext context;
          context.jobId = hasHeaderTag ? headerTag.GetJobId () : 0;
          context.msgId = static_cast<uint16_t> (metadataTag.GetMessageId ());
          context.peerFep = hasHeaderTag ? headerTag.GetSourceEndpoint () : m_tpdcConfig.remoteFep;
          context.missingSeq = missingSeq;
          context.observedSeq = seq;
          context.gapDetectedAtNs = static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ());
          m_readGapContextByMissingSeq[missingSeq] = context;
          Ptr<SesManager> ses = GetNetDevice () ? GetNetDevice ()->GetSesManager () : GetSesManager ();
          if (ses)
            {
              ses->NotifyExternalDiagnosticEvent (
                  "TpdcReadGapDetected",
                  "job=" + std::to_string (context.jobId) +
                      " msg=" + std::to_string (context.msgId) +
                      " peer=" + std::to_string (context.peerFep) +
                      " missing_seq=" + std::to_string (context.missingSeq) +
                      " observed_seq=" + std::to_string (context.observedSeq) +
                      " detected_at_ns=" + std::to_string (context.gapDetectedAtNs));
            }
        }
    }
  ScheduleAcknowledgment (hasGap);
  UpdateReceiveWindow ();
  return true;
}

void
Tpdc::ProcessSendQueue (void)
{
  while (!m_sendQueue.empty () && IsInSendWindow (m_nextSendSequence))
    {
      std::deque<BufferedPacket>::iterator selected = m_sendQueue.begin ();
      if (m_tpdcConfig.readResponseAggressiveDrain)
        {
          for (auto it = m_sendQueue.begin (); it != m_sendQueue.end (); ++it)
            {
              if (IsReadResponsePacket (it->packet))
                {
                  selected = it;
                  break;
                }
            }
        }
      BufferedPacket buffered = *selected;
      m_sendQueue.erase (selected);

      const uint32_t seq = GenerateSequenceNumber ();
      buffered.sequenceNumber = seq;
      buffered.timestamp = Simulator::Now ();
      SetTransportAcceptTimestamp (buffered.packet, buffered.timestamp, true);
      m_sendBuffer[seq] = buffered;

      if (TransmitPacket (m_sendBuffer[seq]))
        {
          ArmTimeout (seq);
        }
      else
        {
          m_sendBuffer.erase (seq);
          break;
        }
    }

  UpdateStatistics ();
}

void
Tpdc::ProcessReceiveQueue (void)
{
  UpdateReceiveWindow ();
}

void
Tpdc::RetransmitPacket (uint32_t sequenceNumber)
{
  auto it = m_sendBuffer.find (sequenceNumber);
  if (it == m_sendBuffer.end () || it->second.acknowledged)
    {
      return;
    }

  BufferedPacket& packet = it->second;
  SoftUeMetadataTag metadataTag;
  const bool hasMetadataTag = packet.packet && packet.packet->PeekPacketTag (metadataTag);
  const bool isReadResponse =
      hasMetadataTag &&
      metadataTag.GetOperationType () == OpType::READ &&
      metadataTag.GetResponseOpCode () ==
          static_cast<uint8_t> (ResponseOpCode::UET_RESPONSE_W_DATA);
  if (isReadResponse)
    {
      auto recoveryIt = m_gapNackObservedNsBySeq.find (sequenceNumber);
      if (recoveryIt != m_gapNackObservedNsBySeq.end ())
        {
          packet.recoveryGapNackSentTime = NanoSeconds (recoveryIt->second);
          packet.recoveryRetransmitTxTime = Simulator::Now ();
        }
      SoftUeHeaderTag headerTag;
      const bool hasHeaderTag = packet.packet && packet.packet->PeekPacketTag (headerTag);
      Ptr<SesManager> ses = GetNetDevice () ? GetNetDevice ()->GetSesManager () : GetSesManager ();
      if (ses)
        {
          ses->NotifyReadResponseGapRetransmitTx (
              hasHeaderTag ? headerTag.GetJobId () : 0,
              static_cast<uint16_t> (metadataTag.GetMessageId ()),
              hasHeaderTag ? headerTag.GetDestinationEndpoint () : 0,
              sequenceNumber,
              static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
          ses->NotifyExternalDiagnosticEvent (
              "TpdcReadGapRetransmitTx",
              "job=" + std::to_string (hasHeaderTag ? headerTag.GetJobId () : 0) +
                  " msg=" + std::to_string (metadataTag.GetMessageId ()) +
                  " peer=" + std::to_string (hasHeaderTag ? headerTag.GetDestinationEndpoint () : 0) +
                  " missing_seq=" + std::to_string (sequenceNumber) +
                  " retransmit_tx_at_ns=" +
                  std::to_string (Simulator::Now ().GetNanoSeconds ()));
        }
    }
  if (packet.retransmissionCount >= m_tpdcConfig.maxRetransmissions)
    {
      HandleError (PdsErrorCode::PROTOCOL_ERROR, "Maximum retransmissions exceeded");
      return;
    }

  packet.retransmissionCount++;
  packet.lastRetransmission = Simulator::Now ();
  m_tpdcStatistics.retransmissions++;
  if (packet.retransmissionCount == 1)
    {
      m_tpdcStatistics.fastRetransmissions++;
    }

  if (TransmitPacket (packet))
    {
      ArmTimeout (sequenceNumber);
      m_retransmissionTrace (sequenceNumber, packet.retransmissionCount);
    }
}

bool
Tpdc::TransmitPacket (const BufferedPacket& bufferedPacket)
{
  Ptr<Packet> packet = bufferedPacket.packet ? bufferedPacket.packet->Copy () : nullptr;
  if (!packet || !GetNetDevice ())
    {
      return false;
    }

  SetTransportTransmitTimestamp (packet, Simulator::Now ());

  if (bufferedPacket.recoveryGapNackSentTime != Time::Max () ||
      bufferedPacket.recoveryRetransmitTxTime != Time::Max ())
    {
      SoftUeRecoveryTag recoveryTag;
      if (bufferedPacket.recoveryGapNackSentTime != Time::Max ())
        {
          recoveryTag.SetGapNackSentTime (bufferedPacket.recoveryGapNackSentTime);
        }
      if (bufferedPacket.recoveryRetransmitTxTime != Time::Max ())
        {
          recoveryTag.SetRetransmitTxTime (bufferedPacket.recoveryRetransmitTxTime);
        }
      packet->AddPacketTag (recoveryTag);
    }

  PDSHeader header = CreatePdsHeader (packet, bufferedPacket.som, bufferedPacket.eom);
  header.SetSequenceNumber (bufferedPacket.sequenceNumber);
  header.SetReliable (true);
  packet->AddHeader (header);

  PdcBase::UpdateStatistics (true, packet);
  m_packetTxTrace (packet, m_tpdcConfig.pdcId);
  ++m_txDataPacketsTotal;
  return GetNetDevice ()->TransmitToChannel (packet,
                                             m_tpdcConfig.localFep,
                                             m_tpdcConfig.remoteFep);
}

void
Tpdc::ScheduleAcknowledgment (bool immediate)
{
  if (immediate)
    {
      if (m_ackEventId.IsPending ())
        {
          Simulator::Cancel (m_ackEventId);
        }
      m_ackEventId = Simulator::ScheduleNow (&Tpdc::SendPendingAcknowledgments, this);
      return;
    }

  if (!m_ackEventId.IsPending ())
    {
      m_ackEventId = Simulator::Schedule (m_ackInterval,
                                          &Tpdc::SendPendingAcknowledgments,
                                          this);
    }
}

void
Tpdc::SendPendingAcknowledgments (void)
{
  SendAcknowledgment (CreateAcknowledgment ());
}

void
Tpdc::UpdateSendWindow (void)
{
  UpdateStatistics ();
}

void
Tpdc::UpdateReceiveWindow (void)
{
  UpdateStatistics ();
}

void
Tpdc::UpdateRtoEstimate (Time measuredRtt)
{
  if (measuredRtt.IsZero ())
    {
      return;
    }

  if (m_currentRtt.IsZero ())
    {
      m_currentRtt = measuredRtt;
      m_rttVariance = measuredRtt / 2;
    }
  else
    {
      const double error = measuredRtt.GetSeconds () - m_currentRtt.GetSeconds ();
      const double newRtt = m_currentRtt.GetSeconds () + error * 0.125;
      const double newVariance = m_rttVariance.GetSeconds () * 0.75 + std::abs (error) * 0.25;
      m_currentRtt = Seconds (newRtt);
      m_rttVariance = Seconds (newVariance);
    }

  m_currentRto = m_currentRtt + Seconds (std::max (m_rttVariance.GetSeconds () * 4.0,
                                                   m_tpdcConfig.initialRto.GetSeconds ()));
  if (m_currentRto < m_tpdcConfig.initialRto)
    {
      m_currentRto = m_tpdcConfig.initialRto;
    }
  if (m_currentRto > m_tpdcConfig.maxRto)
    {
      m_currentRto = m_tpdcConfig.maxRto;
    }
}

void
Tpdc::HandleRtoTimeout (uint32_t sequenceNumber)
{
  m_tpdcStatistics.retransmissionTimeouts++;
  RetransmitPacket (sequenceNumber);
}

void
Tpdc::HandleAckTimeout (void)
{
  SendPendingAcknowledgments ();
}

bool
Tpdc::IsInSendWindow (uint32_t sequence) const
{
  return sequence >= m_sendWindowBase &&
         sequence < m_sendWindowBase + m_tpdcConfig.sendWindowSize;
}

bool
Tpdc::IsInReceiveWindow (uint32_t sequence) const
{
  return sequence >= m_receiveWindowBase &&
         sequence < m_receiveWindowBase + m_tpdcConfig.receiveWindowSize;
}

void
Tpdc::CleanupAcknowledgedPackets (void)
{
  for (auto it = m_sendBuffer.begin (); it != m_sendBuffer.end (); )
    {
      if (it->second.acknowledged)
        {
          m_gapNackObservedNsBySeq.erase (it->first);
          CancelTimeout (it->second);
          it = m_sendBuffer.erase (it);
        }
      else
        {
          ++it;
        }
    }
}

Acknowledgment
Tpdc::CreateAcknowledgment (void)
{
  Acknowledgment ack;
  ack.ackSequence = (m_nextReceiveSequence > 0) ? (m_nextReceiveSequence - 1) : 0;
  ack.receiveWindow = m_tpdcConfig.receiveWindowSize;
  ack.cumulative = true;
  if (m_nextReceiveSequence <= m_highestObservedSequence && !m_receiveBuffer.empty ())
    {
      ack.nackList.push_back (m_nextReceiveSequence);
    }
  return ack;
}

void
Tpdc::PruneResolvedReadGapContexts (void)
{
  for (auto it = m_readGapContextByMissingSeq.begin (); it != m_readGapContextByMissingSeq.end ();)
    {
      if (it->first < m_nextReceiveSequence)
        {
          it = m_readGapContextByMissingSeq.erase (it);
        }
      else
        {
          ++it;
        }
    }
}

uint32_t
Tpdc::GenerateSequenceNumber (void)
{
  return m_nextSendSequence++;
}

void
Tpdc::UpdateStatistics (void)
{
  m_tpdcStatistics.currentSendBufferSize = static_cast<uint32_t> (m_sendBuffer.size ());
  m_tpdcStatistics.currentReceiveBufferSize = static_cast<uint32_t> (m_receiveBuffer.size ());
  m_sendBufferSizeMax = std::max (m_sendBufferSizeMax,
                                  static_cast<uint32_t> (m_sendBuffer.size ()));
}

bool
Tpdc::SendControlPacket (uint8_t flags,
                         uint32_t cumulativeAck,
                         uint32_t selectiveAck,
                         uint32_t gapNack)
{
  if (!GetNetDevice ())
    {
      return false;
    }

  Ptr<Packet> packet = Create<Packet> ();
  SoftUeTpdcControlTag controlTag;
  controlTag.SetFlags (flags);
  controlTag.SetCumulativeAck (cumulativeAck);
  controlTag.SetSelectiveAck (selectiveAck);
  controlTag.SetGapNack (gapNack);
  packet->AddPacketTag (controlTag);
  if ((flags & SOFT_UE_TPDC_CTRL_GAP_NACK) != 0 && gapNack != 0)
    {
      SoftUeRecoveryTag recoveryTag;
      recoveryTag.SetGapNackSentTime (Simulator::Now ());
      packet->AddPacketTag (recoveryTag);
      auto ctxIt = m_readGapContextByMissingSeq.find (gapNack);
      Ptr<SesManager> ses = GetNetDevice () ? GetNetDevice ()->GetSesManager () : GetSesManager ();
      if (ctxIt != m_readGapContextByMissingSeq.end () && ses)
        {
          ses->NotifyReadResponseGapNackSent (ctxIt->second.jobId,
                                              ctxIt->second.msgId,
                                              ctxIt->second.peerFep,
                                              gapNack,
                                              static_cast<uint64_t> (Simulator::Now ().GetNanoSeconds ()));
          ses->NotifyExternalDiagnosticEvent (
              "TpdcReadGapNackSent",
              "job=" + std::to_string (ctxIt->second.jobId) +
                  " msg=" + std::to_string (ctxIt->second.msgId) +
                  " peer=" + std::to_string (ctxIt->second.peerFep) +
                  " missing_seq=" + std::to_string (gapNack) +
                  " gap_detected_at_ns=" + std::to_string (ctxIt->second.gapDetectedAtNs) +
                  " gap_nack_sent_at_ns=" + std::to_string (Simulator::Now ().GetNanoSeconds ()));
        }
    }

  PDSHeader header = CreatePdsHeader (packet, false, false);
  header.SetSequenceNumber (0);
  header.SetReliable (true);
  packet->AddHeader (header);

  PdcBase::UpdateStatistics (true, packet);
  ++m_txControlPacketsTotal;
  return GetNetDevice ()->TransmitToChannel (packet,
                                             m_tpdcConfig.localFep,
                                             m_tpdcConfig.remoteFep);
}

void
Tpdc::ArmTimeout (uint32_t sequenceNumber)
{
  auto it = m_sendBuffer.find (sequenceNumber);
  if (it == m_sendBuffer.end ())
    {
      return;
    }

  CancelTimeout (it->second);
  it->second.timeoutEvent = Simulator::Schedule (m_currentRto,
                                                 &Tpdc::HandleRtoTimeout,
                                                 this,
                                                 sequenceNumber);
}

void
Tpdc::CancelTimeout (BufferedPacket& packet)
{
  if (packet.timeoutEvent.IsPending ())
    {
      Simulator::Cancel (packet.timeoutEvent);
    }
  if (packet.rtoTimer)
    {
      packet.rtoTimer->Cancel ();
    }
}

} // namespace ns3
