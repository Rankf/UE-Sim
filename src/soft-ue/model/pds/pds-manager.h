#ifndef PDS_MANAGER_H
#define PDS_MANAGER_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/callback.h"
#include "ns3/traced-callback.h"
#include <map>
#include <unordered_map>
#include "pds-common.h"
#include "../ses/ses-manager.h"
#include "../pdc/pdc-base.h"
#include "../pdc/ipdc.h"
#include "../pdc/tpdc.h"

namespace ns3 {

// Forward declaration to avoid circular dependency
class SoftUeNetDevice;

/**
 * @class PdsManager
 * @brief Packet Delivery Sub-layer Manager
 *
 * This class implements the PDS layer management functionality for
 * Ultra Ethernet protocol, handling packet routing and PDC management.
 */
class PdsManager : public Object
{
public:
    /**
     * @brief PDS Manager state enumeration
     */
    enum PdsState
    {
        PDS_IDLE,        ///< PDS manager is idle and ready to process requests
        PDS_BUSY,        ///< PDS manager is busy processing requests
        PDS_ERROR        ///< PDS manager is in error state
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
     * @brief Constructor
     */
    PdsManager ();

    /**
     * @brief Destructor
     */
    virtual ~PdsManager ();

    /**
     * @brief Dispose of this object
     */
    virtual void DoDispose (void) override;

    /**
     * @brief Initialize the manager
     */
    void Initialize (void);

    /**
     * @brief Set the SES manager
     * @param sesManager Pointer to SES manager
     */
    void SetSesManager (Ptr<SesManager> sesManager);

    /**
     * @brief Get the SES manager
     * @return Pointer to SES manager
     */
    Ptr<SesManager> GetSesManager (void) const;

    /**
     * @brief Set the network device
     * @param device Pointer to network device
     */
    void SetNetDevice (Ptr<SoftUeNetDevice> device);

    /**
     * @brief Get the network device
     * @return Pointer to network device
     */
    Ptr<SoftUeNetDevice> GetNetDevice (void) const;

    /**
     * @brief Process SES request
     * @param request SES PDS request
     * @return true if successful
     */
    bool ProcessSesRequest (const SesPdsRequest& request);

    /**
     * @brief Process received packet
     * @param packet Received packet
     * @param sourceEndpoint Source endpoint
     * @param destEndpoint Destination endpoint
     * @return true if successful
     */
    bool ProcessReceivedPacket (Ptr<Packet> packet, uint32_t sourceEndpoint, uint32_t destEndpoint);

    /**
     * @brief Allocate a PDC
     * @param destFep Destination FEP
     * @param tc Traffic class
     * @param dm Delivery mode
     * @param nextHdr Next header
     * @param jobLandingJob Job landing job
     * @param nextJob Next job
     * @return PDC identifier
     */
    uint16_t AllocatePdc (uint32_t destFep, uint8_t tc, uint8_t dm,
                         PDSNextHeader nextHdr, uint16_t jobLandingJob, uint16_t nextJob);

    /**
     * @brief Release a PDC
     * @param pdcId PDC identifier
     * @return true if successful
     */
    bool ReleasePdc (uint16_t pdcId);

    /**
     * @brief Send packet through PDC
     * @param pdcId PDC identifier
     * @param packet Packet to send
     * @param som Start of message flag
     * @param eom End of message flag
     * @return true if successful
     */
    bool SendPacketThroughPdc (uint16_t pdcId, Ptr<Packet> packet, bool som, bool eom);

    /**
     * @brief Dispatch packet to PDC (AllocatePdc + SendPacketThroughPdc)
     * @param request SES PDS request
     * @return true if successful
     */
    bool DispatchPacket (const SesPdsRequest& request);

    /**
     * @brief Get active PDCs count
     * @return Number of active PDCs
     */
    uint32_t GetActivePdcs (void) const;

    /**
     * @brief Get total active PDC count
     * @return Total number of active PDCs
     */
    uint32_t GetTotalActivePdcCount (void) const;
    uint32_t GetActiveTpdcCount (void) const;
    uint32_t GetTpdcInflightPackets (void) const;
    uint32_t GetTpdcOutOfOrderPackets (void) const;
    uint32_t GetTpdcPendingSacks (void) const;
    uint32_t GetTpdcPendingGapNacks (void) const;
    bool RegisterPostedReceive (uint64_t jobId,
                                uint16_t msgId,
                                uint32_t srcFep,
                                uint64_t baseAddr,
                                uint32_t length);
    bool RegisterReadResponseTarget (uint64_t jobId,
                                     uint16_t msgId,
                                     uint32_t peerFep,
                                     uint64_t baseAddr,
                                     uint32_t length);
    bool UnregisterReadResponseTarget (uint64_t jobId,
                                       uint16_t msgId,
                                       uint32_t peerFep);
    PdcRxSemanticResult HandleIncomingSendPacket (uint16_t pdcId,
                                                  const SoftUeHeaderTag& headerTag,
                                                  const SoftUeMetadataTag& metadataTag,
                                                  Ptr<Packet> packet);
    PdcRxSemanticResult HandleIncomingReadResponsePacket (uint16_t pdcId,
                                                          const SoftUeHeaderTag& headerTag,
                                                          const SoftUeMetadataTag& metadataTag,
                                                          Ptr<Packet> packet);
    RxMessageProbe QueryRxMessageProbe (uint64_t jobId,
                                        uint16_t msgId,
                                        uint32_t srcFep,
                                        OpType opcode = OpType::SEND) const;
    UnexpectedSendProbe QueryUnexpectedSendProbe (uint64_t jobId,
                                                  uint16_t msgId,
                                                  uint32_t srcFep) const;
    RudResourceStats GetRudResourceStats (void) const;
    void FillRudRuntimeStats (RudRuntimeStats* stats) const;
    std::vector<TpdcSessionProgressRecord> GetTpdcSessionProgressRecords (void) const;
    bool HasPendingReadResponseState (void) const;
    void ConfigureReceiveSemantics (uint32_t maxUnexpectedMessages,
                                    uint32_t maxUnexpectedBytes,
                                    uint32_t arrivalTrackingCapacity,
                                    Time retryTimeout);
    void ConfigureSemanticBudgets (uint32_t sendAdmissionMessages,
                                   uint64_t sendAdmissionBytes,
                                   uint32_t writeBudgetMessages,
                                   uint64_t writeBudgetBytes,
                                   uint32_t readResponderMessages,
                                   uint64_t readResponseBytes);
    void ConfigureTpdcReadResponseScheduling (bool aggressiveDrain,
                                              bool queuePriority);
    void ConfigureControlPlane (bool creditGateEnabled,
                                bool ackControlEnabled,
                                Time creditRefreshInterval,
                                uint32_t maxReadTracks,
                                uint32_t initialCredits);
    void SetPayloadMtu (uint32_t payloadMtu);
    bool TryReserveSendAdmission (uint32_t peerFep, uint32_t payloadBytes);
    void ReleaseSendAdmission (uint32_t peerFep, uint32_t payloadBytes);
    bool TryReserveWriteBudget (uint32_t peerFep, uint32_t payloadBytes);
    void ReleaseWriteBudget (uint32_t peerFep, uint32_t payloadBytes);
    bool TryReserveReadResponderBudget (uint32_t peerFep, uint32_t payloadBytes);
    void ReleaseReadResponderBudget (uint32_t peerFep, uint32_t payloadBytes);
    bool TryConsumeSendCredits (uint32_t peerFep, uint32_t packetCredits);
    void ReturnSendCredits (uint32_t peerFep, uint32_t packetCredits, bool ackControl);
    void PruneReceiveState (void);
    void ResetReceiveState (void);

    /**
     * @brief Handle PDC error
     * @param pdcId PDC identifier
     * @param error Error code
     * @param errorDetails Error details
     */
    void HandlePdcError (uint16_t pdcId, PdsErrorCode error, const std::string& errorDetails);

    // Statistics collection methods

    /**
     * @brief Get PDS statistics
     * @return Pointer to PDS statistics object
     */
    Ptr<PdsStatistics> GetStatistics (void) const;

    /**
     * @brief Reset all statistics
     */
    void ResetStatistics (void);

    /**
     * @brief Get statistics as formatted string
     * @return Formatted statistics string
     */
    std::string GetStatisticsString (void) const;

    /**
     * @brief Enable/disable statistics collection
     * @param enabled Enable statistics collection
     */
    void SetStatisticsEnabled (bool enabled);

    /**
     * @brief Check if statistics collection is enabled
     * @return true if statistics collection is enabled
     */
    bool IsStatisticsEnabled (void) const;

    /**
     * @brief Get current PDS state
     * @return Current PDS state
     */
    PdsState GetState (void) const;

    /**
     * @brief Check if PDS manager is busy
     * @return true if PDS manager is busy
     */
    bool IsBusy (void) const;

    /**
     * @brief Check if PDS manager is in error state
     * @return true if PDS manager is in error state
     */
    bool IsError (void) const;

    /**
     * @brief Reset PDS manager to idle state
     */
    void Reset (void);

private:
    struct ReceivePdcKey
    {
        uint32_t sourceFep{0};
        uint16_t pdcId{0};

        bool operator== (const ReceivePdcKey& other) const
        {
            return sourceFep == other.sourceFep && pdcId == other.pdcId;
        }
    };

    struct ReceivePdcKeyHash
    {
        std::size_t operator() (const ReceivePdcKey& key) const
        {
            std::size_t seed = std::hash<uint32_t>{}(key.sourceFep);
            seed ^= static_cast<std::size_t> (key.pdcId) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    /**
     * @brief Get PDC by identifier
     * @param pdcId PDC identifier
     * @return Pointer to PDC
     */
    Ptr<PdcBase> GetPdc (uint16_t pdcId) const;
    Ptr<PdcBase> GetReceivePdc (uint32_t sourceFep, uint16_t pdcId) const;

    /**
     * @brief Validate SES PDS request
     * @param request SES PDS request to validate
     * @return true if request is valid
     */
    bool ValidateSesPdsRequest (const SesPdsRequest& request) const;

    /**
     * @brief Ensure a PDC exists for receive (passive creation when packet arrives with unknown pdc_id)
     * @param pdcId PDC identifier from packet header
     * @param sourceFep Source FEP (remote endpoint)
     * @return true if PDC exists or was created
     */
    bool EnsureReceivePdc (uint16_t pdcId, uint32_t sourceFep, bool reliable);

    struct ReceiveLookupKey
    {
        uint64_t jobId{0};
        uint16_t msgId{0};
        uint32_t peerFep{0};

        bool operator== (const ReceiveLookupKey& other) const
        {
            return jobId == other.jobId &&
                   msgId == other.msgId &&
                   peerFep == other.peerFep;
        }
    };

    struct ReceiveLookupKeyHash
    {
        std::size_t operator() (const ReceiveLookupKey& key) const
        {
            std::size_t seed = std::hash<uint64_t>{}(key.jobId);
            seed ^= static_cast<std::size_t> (key.msgId) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<uint32_t>{}(key.peerFep) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    struct PostedReceiveBinding
    {
        uint64_t baseAddr{0};
        uint32_t length{0};
        bool consumed{false};
    };

    struct ReadResponseTarget
    {
        uint64_t baseAddr{0};
        uint32_t length{0};
        int64_t registeredAtMs{0};
    };

    struct CompletedSendTombstone
    {
        uint32_t modifiedLength{0};
        int64_t completedAtMs{0};
    };

    struct RudReceiveResourcePool
    {
        uint32_t maxUnexpectedMessages{64};
        uint32_t maxUnexpectedBytes{1u << 20};
        uint32_t maxArrivalBlocks{64};
        uint32_t maxReadTracks{64};
        uint32_t unexpectedMessagesInUse{0};
        uint32_t unexpectedBytesInUse{0};
        uint32_t arrivalBlocksInUse{0};
        uint32_t readTracksInUse{0};
        uint32_t unexpectedAllocFailures{0};
        uint32_t arrivalAllocFailures{0};
        uint32_t readTrackAllocFailures{0};
    };

    struct RudControlState
    {
        uint32_t sendCredits{0};
        uint32_t recvCredits{0};
        bool creditGateBlocked{false};
        bool creditRefreshPending{false};
        bool ackControlPending{false};
        uint32_t pendingCreditGrant{0};
        int64_t lastRefreshMs{0};
        EventId refreshEvent;
    };

    struct SemanticBudgetState
    {
        uint32_t messageLimit{0};
        uint64_t byteLimit{0};
        uint32_t messagesInUse{0};
        uint64_t bytesInUse{0};
        uint32_t blockedCount{0};
        uint32_t blockedByMessages{0};
        uint32_t blockedByBytes{0};
        uint32_t blockedByBoth{0};
        uint32_t messagesInUsePeak{0};
        uint64_t bytesInUsePeak{0};
        uint32_t releaseCount{0};
        uint64_t releaseBytesTotal{0};
    };

    ReceiveLookupKey BuildLookupKey (uint64_t jobId, uint16_t msgId, uint32_t peerFep) const;
    RxMessageKey BuildRxMessageKey (OpType opcode,
                                    uint64_t jobId,
                                    uint16_t msgId,
                                    uint32_t srcFep,
                                    uint16_t pdcId) const;
    int64_t NowMs (void) const;
    uint32_t ComputePayloadPerPacket (void) const;
    uint32_t ComputeExpectedChunks (uint32_t totalLength) const;
    uint32_t ComputeChunkIndex (const SoftUeMetadataTag& metadataTag, uint32_t payloadPerPacket) const;
    uint32_t CountContiguousChunks (const RxMessageContext& context) const;
    std::vector<uint8_t> CopyPacketBytes (Ptr<Packet> packet) const;
    bool CopyPacketBytesToTarget (Ptr<Packet> packet,
                                  uint64_t baseAddr,
                                  uint32_t offset,
                                  uint32_t maxLength) const;
    void StoreChunkInContext (RxMessageContext& context,
                              const SoftUeMetadataTag& metadataTag,
                              Ptr<Packet> packet);
    void TryPromoteUnexpectedToMatched (RxMessageContext& context);
    void RetireContextResources (RxMessageContext& context);
    void TryRetireCompletedContext (const RxMessageKey& key, RxMessageContext& context);
    void PruneCompletedSendTombstones (void);
    bool IsContextActive (const RxMessageContext& context) const;
    UnexpectedSendProbe BuildUnexpectedProbe (const RxMessageContext& context) const;
    bool ReserveArrivalBlock (RxMessageContext* context);
    bool ReserveUnexpectedResources (RxMessageContext* context);
    bool ReserveReadTrack (void);
    void ReleaseReadTrack (void);
    RudControlState& GetControlState (uint32_t peerFep);
    uint32_t GetInitialCredits (void) const;
    void ScheduleCreditRefresh (uint32_t peerFep, uint32_t minCredits);
    void ExecuteCreditRefresh (uint32_t peerFep);

    struct SessionKey
    {
        uint32_t destFep{0};
        uint8_t tc{0};
        uint8_t deliveryMode{0};

        bool operator< (const SessionKey& other) const
        {
            if (destFep != other.destFep)
            {
                return destFep < other.destFep;
            }
            if (tc != other.tc)
            {
                return tc < other.tc;
            }
            return deliveryMode < other.deliveryMode;
        }
    };

    // Member variables
    Ptr<SesManager> m_sesManager;                        ///< Associated SES manager
    Ptr<SoftUeNetDevice> m_netDevice;                    ///< Network device
    Ptr<PdsStatistics> m_statistics;                     ///< Statistics collection
    bool m_statisticsEnabled;                            ///< Statistics collection enabled flag

    // PDC Management
    std::map<uint16_t, Ptr<PdcBase>> m_pdcs;             ///< Active outgoing PDCs indexed by PDC ID
    std::unordered_map<ReceivePdcKey, Ptr<PdcBase>, ReceivePdcKeyHash> m_receivePdcs; ///< Passive receive-side PDCs keyed by (sourceFep,pdcId)
    std::map<SessionKey, uint16_t> m_outgoingSessions;   ///< Reused reliable sessions
    std::unordered_map<ReceiveLookupKey, PostedReceiveBinding, ReceiveLookupKeyHash> m_postedReceives;
    std::unordered_map<ReceiveLookupKey, ReadResponseTarget, ReceiveLookupKeyHash> m_readResponseTargets;
    std::unordered_map<RxMessageKey, RxMessageContext, RxMessageKeyHash> m_rxMessageContexts;
    std::unordered_map<RxMessageKey, CompletedSendTombstone, RxMessageKeyHash> m_completedSendTombstones;
    std::unordered_map<ReceiveLookupKey, CompletedSendTombstone, ReceiveLookupKeyHash> m_completedSendLookupTombstones;
    std::unordered_map<uint32_t, RudControlState> m_controlStates;
    uint16_t m_nextPdcId;                                ///< Next available PDC ID
    uint32_t m_maxPdcCount;                              ///< Maximum PDC count
    uint32_t m_maxUnexpectedMessages;                    ///< Max active unexpected contexts
    uint32_t m_maxUnexpectedBytes;                       ///< Max bytes reserved for unexpected contexts
    uint32_t m_arrivalTrackingCapacity;                  ///< Max active arrival bitmaps
    uint32_t m_currentUnexpectedBytes;                   ///< Bytes reserved by active unexpected contexts
    uint32_t m_unexpectedAllocFailures;                  ///< Unexpected allocation failures
    uint32_t m_staleCleanupCount;                        ///< Number of stale contexts reaped
    Time m_receiveStateTimeout;                          ///< Context cleanup timeout baseline
    uint32_t m_payloadMtu;                               ///< Explicit semantic payload MTU (0 = derive)
    bool m_tpdcReadResponseAggressiveDrain;
    bool m_tpdcReadResponseQueuePriority;
    RudReceiveResourcePool m_receivePool;
    SemanticBudgetState m_sendAdmissionBudget;
    SemanticBudgetState m_writeBudget;
    SemanticBudgetState m_readResponderBudget;
    bool m_creditGateEnabled;
    bool m_ackControlEnabled;
    Time m_creditRefreshInterval;
    uint32_t m_initialCredits;
    uint32_t m_creditRefreshSent;
    uint32_t m_ackCtrlExtSent;
    uint32_t m_creditGateBlockedCount;
    uint32_t m_sackSent;
    uint32_t m_gapNackSent;

    // Performance optimization: track free PDC IDs using a bitmap and queue
    static const uint16_t MAX_PDC_ID = 65535;             ///< Maximum PDC ID value
    std::vector<bool> m_pdcIdBitmap;                      ///< Bitmap for quick free ID lookup
    std::queue<uint16_t> m_freePdcIds;                    ///< Queue of recently freed PDC IDs

    // State management
    PdsState m_state;                                    ///< Current PDS manager state
};

} // namespace ns3

#endif /* PDS_MANAGER_H */
