#include "soft-ue-channel.h"
#include "soft-ue-net-device.h"
#include "../common/soft-ue-packet-tag.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/mac48-address.h"
#include "ns3/double.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SoftUeChannel");
NS_OBJECT_ENSURE_REGISTERED (SoftUeChannel);

namespace {

uint64_t
MixFlowKey (uint64_t value)
{
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

} // namespace

TypeId
SoftUeChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SoftUeChannel")
    .SetParent<Channel> ()
    .SetGroupName ("Soft-Ue")
    .AddConstructor<SoftUeChannel> ()
    .AddAttribute ("DataRate",
                   "The data rate of the channel",
                   DataRateValue (DataRate ("1Gbps")),
                   MakeDataRateAccessor (&SoftUeChannel::SetDataRate,
                                            &SoftUeChannel::GetDataRate),
                   MakeDataRateChecker ())
    .AddAttribute ("Delay",
                   "The propagation delay of the channel (Data center level latency)",
                   TimeValue (NanoSeconds (100)), // 100ns - Data center internal transmission delay
                   MakeTimeAccessor (&SoftUeChannel::SetDelay,
                                        &SoftUeChannel::GetDelay),
                   MakeTimeChecker ())
    .AddAttribute ("ExtraDelay",
                   "Additional injected propagation delay",
                   TimeValue (NanoSeconds (0)),
                   MakeTimeAccessor (&SoftUeChannel::m_extraDelay),
                   MakeTimeChecker ())
    .AddAttribute ("SerializeTransmissions",
                   "Whether packets share one serialized channel transmission timeline",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SoftUeChannel::m_serializeTransmissions),
                   MakeBooleanChecker ())
    .AddAttribute ("PathCount",
                   "Explicit fabric path count for ECMP multipath mode",
                   UintegerValue (1),
                   MakeUintegerAccessor (&SoftUeChannel::m_pathCount),
                   MakeUintegerChecker<uint32_t> (1, RudRuntimeStats::kMaxFabricPaths))
    .AddAttribute ("PathDataRate",
                   "Explicit fabric per-path data rate",
                   DataRateValue (DataRate ("1Gbps")),
                   MakeDataRateAccessor (&SoftUeChannel::m_pathDataRate),
                   MakeDataRateChecker ())
    .AddAttribute ("PathDelay",
                   "Explicit fabric per-path propagation delay",
                   TimeValue (NanoSeconds (100)),
                   MakeTimeAccessor (&SoftUeChannel::m_pathDelay),
                   MakeTimeChecker ())
    .AddAttribute ("UseEcmpHash",
                   "Whether to use stable ECMP hashing across explicit fabric paths",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SoftUeChannel::m_useEcmpHash),
                   MakeBooleanChecker ())
    .AddAttribute ("DynamicPathSelection",
                   "Whether to adaptively assign new flows to less-loaded explicit fabric paths",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SoftUeChannel::m_dynamicPathSelection),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableEcnObservation",
                   "Whether to count ECN-style queue threshold crossings on explicit paths",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SoftUeChannel::m_enableEcnObservation),
                   MakeBooleanChecker ())
    .AddAttribute ("DropProbability",
                   "Packet drop probability for fault injection",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&SoftUeChannel::m_dropProbability),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("ReorderProbability",
                   "Probability of holding one packet for later flush",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&SoftUeChannel::m_reorderProbability),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("ReorderHoldDelay",
                   "Automatic flush delay for probabilistically reordered packets",
                   TimeValue (NanoSeconds (0)),
                   MakeTimeAccessor (&SoftUeChannel::m_reorderHoldDelay),
                   MakeTimeChecker ())
    .AddTraceSource ("Tx",
                     "Trace source for packet transmission",
                     MakeTraceSourceAccessor (&SoftUeChannel::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx",
                     "Trace source for packet reception",
                     MakeTraceSourceAccessor (&SoftUeChannel::m_rxTrace),
                     "ns3::Packet::TracedCallback")
    ;
  return tid;
}

SoftUeChannel::SoftUeChannel ()
  : m_dataRate ("1Gbps"),
    m_pathDataRate ("1Gbps"),
    m_delay (NanoSeconds (100)), // Data center internal transmission delay
    m_pathDelay (NanoSeconds (100)),
    m_extraDelay (NanoSeconds (0)),
    m_reorderHoldDelay (NanoSeconds (0)),
    m_dropProbability (0.0),
    m_reorderProbability (0.0),
    m_rng (CreateObject<UniformRandomVariable> ()),
    m_hasHeldTransmission (false),
    m_dropNextRequests (0),
    m_dropNextResponses (0),
    m_holdNextResponse (false),
    m_serializeTransmissions (false),
    m_nextTxAvailable (Seconds (0)),
    m_pathCount (1),
    m_useEcmpHash (false),
    m_dynamicPathSelection (false),
    m_enableEcnObservation (false),
    m_dynamicAssignmentTotal (0),
    m_dynamicPathReuseTotal (0),
    m_activeFlowAssignmentsPeak (0),
    m_pathScoreSampleTotalNs (0),
    m_pathScoreSampleCount (0),
    m_pathScoreMinNs (0),
    m_pathScoreMaxNs (0)
{
  NS_LOG_FUNCTION (this);
}

SoftUeChannel::~SoftUeChannel ()
{
  NS_LOG_FUNCTION (this);
}

void
SoftUeChannel::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  if (m_heldFlushEvent.IsPending ())
    {
      m_heldFlushEvent.Cancel ();
    }
  m_devices.clear ();
  m_hasHeldTransmission = false;
  m_dropNextRequests = 0;
  m_dropNextResponses = 0;
  m_holdNextResponse = false;
  m_nextTxAvailable = Seconds (0);
  m_pathStates.clear ();
  m_flowPathAssignments.clear ();
  m_hotspotBytesByDest.clear ();
  m_dynamicAssignmentTotal = 0;
  m_dynamicPathReuseTotal = 0;
  m_activeFlowAssignmentsPeak = 0;
  m_pathScoreSampleTotalNs = 0;
  m_pathScoreSampleCount = 0;
  m_pathScoreMinNs = 0;
  m_pathScoreMaxNs = 0;
  m_rng = nullptr;
  Channel::DoDispose ();
}

void
SoftUeChannel::SetDataRate (DataRate dataRate)
{
  NS_LOG_FUNCTION (this << dataRate);
  m_dataRate = dataRate;
}

DataRate
SoftUeChannel::GetDataRate (void) const
{
  return m_dataRate;
}

void
SoftUeChannel::SetDelay (Time delay)
{
  NS_LOG_FUNCTION (this << delay);
  m_delay = delay;
}

Time
SoftUeChannel::GetDelay (void) const
{
  return m_delay;
}

void
SoftUeChannel::SetPropagationDelay (Time delay)
{
  NS_LOG_FUNCTION (this << delay);
  SetDelay (delay);
}

Time
SoftUeChannel::GetPropagationDelay (void) const
{
  return GetDelay ();
}

void
SoftUeChannel::Attach (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (this << device);

  // Check if device is already attached
  for (uint32_t i = 0; i < m_devices.size (); ++i)
    {
      if (m_devices[i] == device)
        {
          NS_LOG_DEBUG ("Device already attached to channel");
          return;
        }
    }

  // Add device to channel with limit check
  static const std::size_t MAX_DEVICES_PER_CHANNEL = 1000; // Reasonable limit
  if (m_devices.size () >= MAX_DEVICES_PER_CHANNEL)
  {
    NS_LOG_ERROR ("Cannot attach device: maximum device count per channel reached ("
                  << MAX_DEVICES_PER_CHANNEL << ")");
    return;
  }

  m_devices.push_back (device);
  NS_LOG_INFO ("Attached device " << device << " to Soft-Ue channel (total devices: "
               << m_devices.size () << ")");
}

bool
SoftUeChannel::IsAttached (Ptr<NetDevice> device) const
{
  for (uint32_t i = 0; i < m_devices.size (); ++i)
    {
      if (m_devices[i] == device)
        {
          return true;
        }
    }
  return false;
}

std::size_t
SoftUeChannel::GetNDevices (void) const
{
  return m_devices.size ();
}

Ptr<NetDevice>
SoftUeChannel::GetDevice (std::size_t i) const
{
  NS_ASSERT (i < m_devices.size ());
  return m_devices[i];
}

std::size_t
SoftUeChannel::GetDevice (Ptr<NetDevice> device) const
{
  for (std::size_t i = 0; i < m_devices.size (); ++i)
    {
      if (m_devices[i] == device)
        {
          return i;
        }
    }
  return m_devices.size (); // Not found
}

void
SoftUeChannel::Connect (NetDeviceContainer devices)
{
  NS_LOG_FUNCTION (this << devices.GetN ());

  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Attach (devices.Get (i));
    }
}

void
SoftUeChannel::FlushHeldPacket (void)
{
  if (!m_hasHeldTransmission)
    {
      return;
    }
  ReleaseHeldPacket (NanoSeconds (1));
}

void
SoftUeChannel::DropNextRequests (uint32_t count)
{
  m_dropNextRequests += count;
}

void
SoftUeChannel::DropNextResponses (uint32_t count)
{
  m_dropNextResponses += count;
}

void
SoftUeChannel::HoldNextResponse (void)
{
  m_holdNextResponse = true;
}

void
SoftUeChannel::ClearDeterministicFaultPlan (void)
{
  m_dropNextRequests = 0;
  m_dropNextResponses = 0;
  m_holdNextResponse = false;
}

void
SoftUeChannel::FillFabricRuntimeStats (RudRuntimeStats* stats) const
{
  if (stats == nullptr)
    {
      return;
    }
  const_cast<SoftUeChannel*> (this)->EnsurePathStateSize ();
  stats->fabric_path_count = m_pathCount;
  uint64_t totalBytes = 0;
  uint64_t totalPackets = 0;
  uint64_t totalEcnMarks = 0;
  for (std::size_t pathIdx = 0; pathIdx < m_pathStates.size () &&
                               pathIdx < RudRuntimeStats::kMaxFabricPaths;
       ++pathIdx)
    {
      const PathState& state = m_pathStates[pathIdx];
      totalBytes += state.txBytes;
      totalPackets += state.txPackets;
      totalEcnMarks += state.ecnMarks;
      stats->fabric_path_tx_bytes[pathIdx] = state.txBytes;
      stats->fabric_path_queue_depth_max[pathIdx] = state.queueDepthMax;
      stats->fabric_path_flow_count[pathIdx] =
          static_cast<uint32_t> (state.flowKeys.size ());
      stats->fabric_path_queue_depth_mean_milli[pathIdx] =
          state.queueDepthSampleCount == 0
              ? 0
              : static_cast<uint32_t> ((1000ull * state.queueDepthSampleTotal) /
                                       state.queueDepthSampleCount);
    }
  stats->fabric_total_tx_bytes = totalBytes;
  stats->fabric_total_tx_packets = totalPackets;
  stats->fabric_ecn_mark_total = totalEcnMarks;
  stats->fabric_dynamic_routing_enabled = m_dynamicPathSelection ? 1u : 0u;
  stats->fabric_dynamic_assignment_total = m_dynamicAssignmentTotal;
  stats->fabric_dynamic_path_reuse_total = m_dynamicPathReuseTotal;
  stats->fabric_active_flow_assignments_peak = m_activeFlowAssignmentsPeak;
  stats->fabric_path_score_min_ns = m_pathScoreSampleCount > 0 ? m_pathScoreMinNs : 0ull;
  stats->fabric_path_score_max_ns = m_pathScoreSampleCount > 0 ? m_pathScoreMaxNs : 0ull;
  stats->fabric_path_score_mean_ns =
      m_pathScoreSampleCount == 0 ? 0ull : (m_pathScoreSampleTotalNs / m_pathScoreSampleCount);

  uint32_t hotspotEndpoint = 0;
  uint64_t hotspotBytes = 0;
  for (const auto& [destFep, bytes] : m_hotspotBytesByDest)
    {
      if (bytes > hotspotBytes)
        {
          hotspotEndpoint = destFep;
          hotspotBytes = bytes;
        }
    }
  stats->fabric_top_hotspot_endpoint = hotspotEndpoint;
}

void
SoftUeChannel::Transmit (Ptr<Packet> packet, Ptr<NetDevice> src, uint32_t sourceFep, uint32_t destFep)
{
  NS_LOG_FUNCTION (this << packet << src << sourceFep << destFep);

  if (!packet || !src)
    {
      NS_LOG_WARN ("Invalid packet or source device");
      return;
    }

  // Trace transmission
  NS_LOG_INFO ("[UEC-E2E] [Channel] (5) Transmit: FEP " << sourceFep << " -> FEP " << destFep
               << " size=" << packet->GetSize () << " B (delivered after delay)");
  m_txTrace (packet, sourceFep, destFep);
  const bool isResponse = IsResponsePacket (packet);
  EnsurePathStateSize ();

  // Transmit to all destination devices
  bool delayReserved = false;
  Time reservedDelay = Seconds (0);
  for (uint32_t i = 0; i < m_devices.size (); ++i)
    {
      Ptr<NetDevice> dest = m_devices[i];
      if (dest != src) // Don't transmit back to source
        {
          // For unicast transmission, only send to specific destination
          if (destFep == 0 || destFep == GetDestinationFepForDevice (dest))
            {
              if (!isResponse && m_dropNextRequests > 0)
                {
                  --m_dropNextRequests;
                  continue;
                }
              if (isResponse && m_dropNextResponses > 0)
                {
                  --m_dropNextResponses;
                  continue;
                }
              if (m_rng && m_rng->GetValue () < m_dropProbability)
                {
                  continue;
                }
              Time receiveDelay = Seconds (0);
              if (m_pathCount > 1 && m_useEcmpHash)
                {
                  if (!delayReserved)
                    {
                      reservedDelay = ReserveExplicitPathAndCalculateDelay (packet, sourceFep, destFep);
                      delayReserved = true;
                    }
                  receiveDelay = reservedDelay;
                }
              else
                {
                  receiveDelay = CalculateScheduleDelay (packet);
                }
              if (!m_hasHeldTransmission && isResponse && m_holdNextResponse)
                {
                  m_holdNextResponse = false;
                  m_hasHeldTransmission = true;
                  m_heldTransmission.packet = packet->Copy ();
                  m_heldTransmission.dest = dest;
                  m_heldTransmission.sourceFep = sourceFep;
                  m_heldTransmission.destFep = destFep;
                  m_heldTransmission.delay = receiveDelay;
                  if (m_reorderHoldDelay.IsStrictlyPositive () && !m_heldFlushEvent.IsPending ())
                    {
                      m_heldFlushEvent =
                          Simulator::Schedule (m_reorderHoldDelay,
                                               &SoftUeChannel::ReleaseHeldPacket,
                                               this,
                                               m_heldTransmission.delay);
                    }
                }
              else if (!m_hasHeldTransmission && m_rng && m_rng->GetValue () < m_reorderProbability)
                {
                  m_hasHeldTransmission = true;
                  m_heldTransmission.packet = packet->Copy ();
                  m_heldTransmission.dest = dest;
                  m_heldTransmission.sourceFep = sourceFep;
                  m_heldTransmission.destFep = destFep;
                  m_heldTransmission.delay = receiveDelay;
                  if (m_reorderHoldDelay.IsStrictlyPositive () && !m_heldFlushEvent.IsPending ())
                    {
                      m_heldFlushEvent =
                          Simulator::Schedule (m_reorderHoldDelay,
                                               &SoftUeChannel::ReleaseHeldPacket,
                                               this,
                                               m_heldTransmission.delay);
                    }
                }
              else
                {
                  ScheduleReceive (packet->Copy (), dest, sourceFep, destFep, receiveDelay);
                }
            }
        }
    }
}

void
SoftUeChannel::ReleaseHeldPacket (Time scheduleDelay)
{
  if (!m_hasHeldTransmission)
    {
      return;
    }
  if (m_heldFlushEvent.IsPending ())
    {
      m_heldFlushEvent.Cancel ();
    }
  const Time receiveDelay =
      (m_pathCount > 1 && m_useEcmpHash)
          ? ReserveExplicitPathAndCalculateDelay (m_heldTransmission.packet,
                                                  m_heldTransmission.sourceFep,
                                                  m_heldTransmission.destFep)
          : (m_serializeTransmissions ? CalculateScheduleDelay (m_heldTransmission.packet) : scheduleDelay);
  ScheduleReceive (m_heldTransmission.packet,
                   m_heldTransmission.dest,
                   m_heldTransmission.sourceFep,
                   m_heldTransmission.destFep,
                   receiveDelay);
  m_hasHeldTransmission = false;
}

void
SoftUeChannel::ScheduleReceive (Ptr<Packet> packet, Ptr<NetDevice> dest, uint32_t sourceFep, uint32_t destFep, Time delay)
{
  NS_LOG_FUNCTION (this << packet << dest << sourceFep << destFep << delay);

  Ptr<SoftUeNetDevice> softUeDevice = DynamicCast<SoftUeNetDevice> (dest);
  if (softUeDevice && softUeDevice->GetNode ())
    {
      Simulator::ScheduleWithContext (softUeDevice->GetNode ()->GetId (),
                                      delay,
                                      &SoftUeChannel::ReceivePacket,
                                      this,
                                      packet,
                                      dest,
                                      sourceFep,
                                      destFep);
      return;
    }

  Simulator::Schedule (delay, &SoftUeChannel::ReceivePacket, this, packet, dest, sourceFep, destFep);
}

void
SoftUeChannel::ReceivePacket (Ptr<Packet> packet, Ptr<NetDevice> dest, uint32_t sourceFep, uint32_t destFep)
{
  NS_LOG_FUNCTION (this << packet << dest << sourceFep << destFep);

  // Check if destination device is a SoftUeNetDevice
  Ptr<SoftUeNetDevice> softUeDevice = DynamicCast<SoftUeNetDevice> (dest);
  if (!softUeDevice)
    {
      NS_LOG_DEBUG ("Destination device is not a SoftUeNetDevice");
      return;
    }

  // Trace reception
  NS_LOG_INFO ("[UEC-E2E] [Channel] (6) ReceivePacket: FEP " << sourceFep << " -> FEP " << destFep
               << " delivered, device will ReceivePacket");
  m_rxTrace (packet, sourceFep, destFep);

  // Forward packet to device for processing
  softUeDevice->ReceivePacket (packet, sourceFep, destFep);
}

Time
SoftUeChannel::CalculateTransmissionTime (Ptr<Packet> packet, const DataRate& dataRate) const
{
  if (!packet)
    {
      return Seconds (0);
    }

  // Calculate transmission time based on data rate and packet size
  uint32_t packetSize = packet->GetSize () * 8; // Convert to bits
  double dataRateBps = dataRate.GetBitRate ();

  if (dataRateBps > 0)
    {
      return Seconds (packetSize / dataRateBps);
    }
  else
    {
      return Seconds (0); // Instantaneous transmission
    }
}

Time
SoftUeChannel::CalculateScheduleDelay (Ptr<Packet> packet) const
{
  const Time txTime = CalculateTransmissionTime (packet, m_dataRate);
  if (!m_serializeTransmissions)
    {
      return m_delay + m_extraDelay + txTime;
    }

  const Time now = Simulator::Now ();
  const Time startTx = std::max (now, m_nextTxAvailable);
  const Time finishTx = startTx + txTime;
  const_cast<SoftUeChannel*> (this)->m_nextTxAvailable = finishTx;
  return (finishTx - now) + m_delay + m_extraDelay;
}

uint32_t
SoftUeChannel::SelectPathIndex (Ptr<Packet> packet, uint32_t sourceFep, uint32_t destFep) const
{
  if (m_dynamicPathSelection)
    {
      return const_cast<SoftUeChannel*> (this)->SelectAdaptivePathIndex (packet, sourceFep, destFep);
    }
  if (m_pathCount <= 1 || !m_useEcmpHash)
    {
      return 0;
    }
  const uint64_t flowKey = MixFlowKey (BuildFlowKey (packet, sourceFep, destFep));
  return static_cast<uint32_t> (flowKey % m_pathCount);
}

uint32_t
SoftUeChannel::SelectAdaptivePathIndex (Ptr<Packet> packet, uint32_t sourceFep, uint32_t destFep)
{
  if (m_pathCount <= 1)
    {
      return 0;
    }

  EnsurePathStateSize ();
  const uint64_t flowKey = BuildFlowKey (packet, sourceFep, destFep);
  const auto existing = m_flowPathAssignments.find (flowKey);
  if (existing != m_flowPathAssignments.end ())
    {
      ++m_dynamicPathReuseTotal;
      return existing->second;
    }

  const Time now = Simulator::Now ();
  const Time txTime = CalculateTransmissionTime (packet, m_pathDataRate);
  uint64_t bestScore = 0;
  bool hasBest = false;
  std::vector<uint32_t> bestPaths;

  for (uint32_t pathIndex = 0; pathIndex < m_pathCount; ++pathIndex)
    {
      PathState& state = m_pathStates[pathIndex];
      RefreshPathState (state, now);
      const ScoreSnapshot score = CapturePathScore (state, now, txTime);
      if (!hasBest || score.scoreNs < bestScore)
        {
          bestScore = score.scoreNs;
          bestPaths.clear ();
          bestPaths.push_back (pathIndex);
          hasBest = true;
        }
      else if (score.scoreNs == bestScore)
        {
          bestPaths.push_back (pathIndex);
        }
    }

  uint32_t selectedPath = 0;
  if (!bestPaths.empty ())
    {
      if (bestPaths.size () == 1 || !m_useEcmpHash)
        {
          selectedPath = bestPaths.front ();
        }
      else
        {
          const uint64_t tieBreak = MixFlowKey (flowKey);
          selectedPath = bestPaths[static_cast<std::size_t> (tieBreak % bestPaths.size ())];
        }
    }

  m_flowPathAssignments[flowKey] = selectedPath;
  ++m_dynamicAssignmentTotal;
  m_activeFlowAssignmentsPeak =
      std::max<uint32_t> (m_activeFlowAssignmentsPeak,
                          static_cast<uint32_t> (m_flowPathAssignments.size ()));
  m_pathScoreSampleTotalNs += bestScore;
  ++m_pathScoreSampleCount;
  if (m_pathScoreSampleCount == 1)
    {
      m_pathScoreMinNs = bestScore;
      m_pathScoreMaxNs = bestScore;
    }
  else
    {
      m_pathScoreMinNs = std::min<uint64_t> (m_pathScoreMinNs, bestScore);
      m_pathScoreMaxNs = std::max<uint64_t> (m_pathScoreMaxNs, bestScore);
    }
  return selectedPath;
}

Time
SoftUeChannel::ReserveExplicitPathAndCalculateDelay (Ptr<Packet> packet,
                                                     uint32_t sourceFep,
                                                     uint32_t destFep)
{
  EnsurePathStateSize ();
  const uint32_t pathIndex = SelectPathIndex (packet, sourceFep, destFep);
  PathState& state = m_pathStates[pathIndex];
  const Time now = Simulator::Now ();
  RefreshPathState (state, now);
  const uint32_t queueDepth = static_cast<uint32_t> (state.finishTimes.size ());
  state.queueDepthMax = std::max (state.queueDepthMax, queueDepth);
  state.queueDepthSampleTotal += queueDepth;
  state.queueDepthSampleCount++;
  if (m_enableEcnObservation && queueDepth >= 8)
    {
      state.ecnMarks++;
    }

  const Time txTime = CalculateTransmissionTime (packet, m_pathDataRate);
  const Time startTx = std::max (now, state.nextTxAvailable);
  const Time finishTx = startTx + txTime;
  state.nextTxAvailable = finishTx;
  state.finishTimes.push_back (finishTx);
  state.txBytes += packet ? packet->GetSize () : 0u;
  state.txPackets += 1u;
  state.flowKeys.insert (BuildFlowKey (packet, sourceFep, destFep));
  if (destFep != 0)
    {
      m_hotspotBytesByDest[destFep] += packet ? packet->GetSize () : 0u;
    }
  return (finishTx - now) + m_pathDelay + m_extraDelay;
}

void
SoftUeChannel::EnsurePathStateSize (void)
{
  const uint32_t effectiveCount = std::max<uint32_t> (1u, m_pathCount);
  if (m_pathStates.size () != effectiveCount)
    {
      m_pathStates.resize (effectiveCount);
    }
}

void
SoftUeChannel::RefreshPathState (PathState& state, Time now)
{
  while (!state.finishTimes.empty () && state.finishTimes.front () <= now)
    {
      state.finishTimes.pop_front ();
    }
}

SoftUeChannel::ScoreSnapshot
SoftUeChannel::CapturePathScore (PathState& state, Time now, Time txTime) const
{
  const uint32_t queueDepth = static_cast<uint32_t> (state.finishTimes.size ());
  const Time backlog = state.nextTxAvailable > now ? (state.nextTxAvailable - now) : Seconds (0);
  const uint64_t txNs = static_cast<uint64_t> (std::max<int64_t> (0, txTime.GetNanoSeconds ()));
  const uint64_t scoreNs =
      static_cast<uint64_t> (std::max<int64_t> (0, backlog.GetNanoSeconds ())) +
      (static_cast<uint64_t> (queueDepth) * txNs);
  return {scoreNs, queueDepth};
}

uint64_t
SoftUeChannel::BuildFlowKey (Ptr<Packet> packet, uint32_t sourceFep, uint32_t destFep) const
{
  SoftUeMetadataTag metadataTag;
  const uint64_t opBits =
      (packet && packet->PeekPacketTag (metadataTag))
          ? static_cast<uint64_t> (metadataTag.GetOperationType ())
          : 0ull;
  const uint64_t rawKey =
      (static_cast<uint64_t> (sourceFep) << 32) ^
      static_cast<uint64_t> (destFep) ^
      (opBits << 56);
  return rawKey;
}

uint32_t
SoftUeChannel::GetDestinationFepForDevice (Ptr<NetDevice> device) const
{
  // For SoftUeNetDevice, extract the FEP ID from the MAC address
  Ptr<SoftUeNetDevice> softUeDevice = DynamicCast<SoftUeNetDevice> (device);
  if (softUeDevice)
    {
      SoftUeConfig config = softUeDevice->GetConfiguration ();
      return config.localFep;
    }

  // For other devices, use device index + 1 as default
  return GetDevice (device) + 1;
}

bool
SoftUeChannel::IsResponsePacket (Ptr<Packet> packet) const
{
  if (!packet)
    {
      return false;
    }
  SoftUeMetadataTag metadataTag;
  return packet->PeekPacketTag (metadataTag) && metadataTag.IsResponse ();
}

} // namespace ns3
