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
 * @file             soft-ue-channel.h
 * @brief            Soft-Ue Channel Implementation
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-07
 * @copyright        Apache License Version 2.0
 *
 * @details
 * This file contains the Soft-Ue channel that provides connectivity between
 * Soft-Ue network devices for Ultra Ethernet protocol transmission.
 */

#ifndef SOFT_UE_CHANNEL_H
#define SOFT_UE_CHANNEL_H

#include "ns3/channel.h"
#include "ns3/net-device.h"
#include "ns3/net-device-container.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"
#include "../common/transport-layer.h"
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ns3 {

/**
 * @class SoftUeChannel
 * @brief Soft-Ue Channel for Ultra Ethernet protocol transmission
 *
 * The SoftUeChannel provides a broadcast-like medium for Soft-Ue network
 * devices, simulating the physical layer characteristics of Ultra Ethernet
 * networks including propagation delay and bandwidth constraints.
 */
class SoftUeChannel : public Channel
{
public:
    /**
     * @brief Get the type ID for this class
     * @return TypeId
     */
    static TypeId GetTypeId (void);

    /**
     * @brief Constructor
     */
    SoftUeChannel ();

    /**
     * @brief Destructor
     */
    virtual ~SoftUeChannel ();

    /**
     * @brief Set the data rate of the channel
     * @param dataRate Data rate
     */
    void SetDataRate (DataRate dataRate);

    /**
     * @brief Get the data rate of the channel
     * @return Data rate
     */
    DataRate GetDataRate (void) const;

    /**
     * @brief Set the propagation delay
     * @param delay Propagation delay
     */
    void SetDelay (Time delay);

    /**
     * @brief Get the propagation delay
     * @return Propagation delay
     */
    Time GetDelay (void) const;

    /**
     * @brief Connect devices to this channel
     * @param devices NetDevice container with devices to connect
     */
    void Connect (NetDeviceContainer devices);
    void FlushHeldPacket (void);
    void DropNextRequests (uint32_t count = 1);
    void DropNextResponses (uint32_t count = 1);
    void HoldNextResponse (void);
    void ClearDeterministicFaultPlan (void);
    void FillFabricRuntimeStats (RudRuntimeStats* stats) const;

    // Channel interface implementation
    virtual void SetPropagationDelay (Time delay);
    virtual Time GetPropagationDelay (void) const;
    virtual void Attach (Ptr<NetDevice> device);
    virtual bool IsAttached (Ptr<NetDevice> device) const;
    virtual std::size_t GetNDevices (void) const override;
    virtual Ptr<NetDevice> GetDevice (std::size_t i) const override;
    virtual std::size_t GetDevice (Ptr<NetDevice> device) const;

    // Soft-Ue specific methods

    /**
     * @brief Transmit packet from source device to all other devices
     * @param packet Packet to transmit
     * @param src Source device
     * @param sourceFep Source fabric endpoint
     * @param destFep Destination fabric endpoint (0 for broadcast)
     */
    void Transmit (Ptr<Packet> packet, Ptr<NetDevice> src, uint32_t sourceFep,
                  uint32_t destFep = 0);

    // Traced callbacks
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_txTrace;        ///< Transmit trace (packet, srcFep, destFep)
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_rxTrace;        ///< Receive trace (packet, srcFep, destFep)

protected:
    /**
     * @brief DoDispose method for cleanup
     */
    virtual void DoDispose (void) override;

private:
    struct HeldTransmission
    {
        Ptr<Packet> packet;
        Ptr<NetDevice> dest;
        uint32_t sourceFep{0};
        uint32_t destFep{0};
        Time delay{Seconds (0)};
    };

    struct PathState
    {
        Time nextTxAvailable{Seconds (0)};
        uint64_t txBytes{0};
        uint64_t txPackets{0};
        uint32_t queueDepthMax{0};
        uint64_t queueDepthSampleTotal{0};
        uint64_t queueDepthSampleCount{0};
        uint64_t ecnMarks{0};
        std::deque<Time> finishTimes;
        std::unordered_set<uint64_t> flowKeys;
    };

    struct ScoreSnapshot
    {
        uint64_t scoreNs{0};
        uint32_t queueDepth{0};
    };

    DataRate m_dataRate;                           ///< Channel data rate
    DataRate m_pathDataRate;                       ///< Explicit fabric per-path data rate
    Time m_delay;                                   ///< Propagation delay
    Time m_pathDelay;                               ///< Explicit fabric per-path delay
    Time m_extraDelay;                              ///< Additional injected propagation delay
    Time m_reorderHoldDelay;                        ///< Automatic flush delay for reordered packet
    std::vector<Ptr<NetDevice>> m_devices;         ///< Connected devices
    double m_dropProbability;                       ///< Packet drop probability
    double m_reorderProbability;                    ///< Hold-one-packet reorder probability
    Ptr<UniformRandomVariable> m_rng;               ///< Random source for channel faults
    bool m_hasHeldTransmission;                     ///< Whether a held transmission exists
    HeldTransmission m_heldTransmission;            ///< Deferred packet for reordering
    uint32_t m_dropNextRequests;                    ///< Deterministic request drops remaining
    uint32_t m_dropNextResponses;                   ///< Deterministic response drops remaining
    bool m_holdNextResponse;                        ///< Hold the next response packet once
    EventId m_heldFlushEvent;                       ///< Automatic release event for held packet
    bool m_serializeTransmissions;                  ///< Whether all packets share one serialized tx timeline
    Time m_nextTxAvailable;                         ///< Next time the shared transmitter becomes available
    uint32_t m_pathCount;                           ///< Explicit fabric path count
    bool m_useEcmpHash;                             ///< Whether to hash flows across paths
    bool m_dynamicPathSelection;                    ///< Whether to adaptively assign new flows to less-loaded paths
    bool m_enableEcnObservation;                    ///< Whether to count ECN-style queue threshold crossings
    std::vector<PathState> m_pathStates;            ///< Per-path serialized state and counters
    std::unordered_map<uint64_t, uint32_t> m_flowPathAssignments; ///< Sticky flow-to-path assignments
    std::unordered_map<uint32_t, uint64_t> m_hotspotBytesByDest; ///< Bytes sent toward each destination FEP
    uint32_t m_dynamicAssignmentTotal;              ///< Number of new adaptive path assignments
    uint32_t m_dynamicPathReuseTotal;               ///< Number of packets that reused an existing adaptive assignment
    uint32_t m_activeFlowAssignmentsPeak;           ///< Peak sticky flow assignment count
    uint64_t m_pathScoreSampleTotalNs;              ///< Sum of selected path scores for diagnostics
    uint64_t m_pathScoreSampleCount;                ///< Number of selected path score samples
    uint64_t m_pathScoreMinNs;                      ///< Minimum selected path score
    uint64_t m_pathScoreMaxNs;                      ///< Maximum selected path score

    void ReceivePacket (Ptr<Packet> packet, Ptr<NetDevice> dest, uint32_t sourceFep,
                       uint32_t destFep);
    void ScheduleReceive (Ptr<Packet> packet, Ptr<NetDevice> dest, uint32_t sourceFep,
                          uint32_t destFep, Time delay);
    void ReleaseHeldPacket (Time scheduleDelay);
    Time CalculateTransmissionTime (Ptr<Packet> packet, const DataRate& dataRate) const;
    Time CalculateScheduleDelay (Ptr<Packet> packet) const;
    uint32_t SelectPathIndex (Ptr<Packet> packet, uint32_t sourceFep, uint32_t destFep) const;
    uint32_t SelectAdaptivePathIndex (Ptr<Packet> packet, uint32_t sourceFep, uint32_t destFep);
    Time ReserveExplicitPathAndCalculateDelay (Ptr<Packet> packet, uint32_t sourceFep, uint32_t destFep);
    void EnsurePathStateSize (void);
    void RefreshPathState (PathState& state, Time now);
    ScoreSnapshot CapturePathScore (PathState& state, Time now, Time txTime) const;
    uint64_t BuildFlowKey (Ptr<Packet> packet, uint32_t sourceFep, uint32_t destFep) const;
    uint32_t GetDestinationFepForDevice (Ptr<NetDevice> device) const;
    bool IsResponsePacket (Ptr<Packet> packet) const;
    bool IsGapNackControlPacket (Ptr<Packet> packet) const;
};

} // namespace ns3

#endif /* SOFT_UE_CHANNEL_H */
