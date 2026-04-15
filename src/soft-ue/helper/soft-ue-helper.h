#ifndef SOFT_UE_HELPER_H
#define SOFT_UE_HELPER_H

#include <ns3/object-factory.h>
#include <ns3/node-container.h>
#include <ns3/net-device-container.h>
#include <ns3/nstime.h>
#include "../model/common/transport-layer.h"

namespace ns3 {

/**
 * @brief Soft-UE Helper class
 */
class SoftUeHelper
{
public:
    /**
     * Default constructor
     */
    SoftUeHelper ();

    /**
     * Install Soft-UE devices on nodes
     * @param nodes Node container
     * @return NetDeviceContainer with installed devices
     */
    NetDeviceContainer Install (NodeContainer nodes);

    /**
     * Install Soft-UE device on a single node
     * @param node Single node
     * @return NetDeviceContainer with installed device
     */
    NetDeviceContainer Install (Ptr<Node> node);

    /**
     * Set an attribute on the devices to be created
     * @param name Attribute name
     * @param value Attribute value
     */
    void SetDeviceAttribute (std::string name, const AttributeValue& value);
    void SetChannelAttribute (std::string name, const AttributeValue& value);
    void SetRudSemanticMode (bool enabled);
    void ConfigureUnexpectedResources (uint32_t maxMessages, uint32_t maxBytes, uint32_t arrivalCapacity);
    void ConfigureRetry (bool enabled, Time retryTimeout);
    void SetCreditGateEnabled (bool enabled);
    void SetAckControlEnabled (bool enabled);
    void SetCreditRefreshInterval (Time interval);
    void SetInitialCredits (uint32_t initialCredits);
    void SetPayloadMtu (uint32_t payloadMtu);
    void ConfigureArrivalTracking (uint32_t maxArrivalBlocks, uint32_t maxReadTracks);
    void ConfigureSemanticBudgets (uint32_t sendAdmissionMessages,
                                   uint64_t sendAdmissionBytes,
                                   uint32_t writeBudgetMessages,
                                   uint64_t writeBudgetBytes,
                                   uint32_t readResponderMessages,
                                   uint64_t readResponseBytes);
    void ConfigureTpdcReadResponseScheduling (bool aggressiveDrain,
                                              bool queuePriority);
    void SetValidationStrictness (ValidationStrictness strictness);
    void SetDebugSnapshotsEnabled (bool enabled);
    void SetAuthorizeAllJobs (bool allowAll);

private:
    ObjectFactory m_deviceFactory; ///< Device factory
    ObjectFactory m_channelFactory; ///< Channel factory
    uint32_t m_deviceMaxPacketSize;
    bool m_rudSemanticMode;
    uint32_t m_unexpectedMessages;
    uint32_t m_unexpectedBytes;
    uint32_t m_arrivalCapacity;
    uint32_t m_maxReadTracks;
    bool m_retryEnabled;
    Time m_retryTimeout;
    bool m_creditGateEnabled;
    bool m_ackControlEnabled;
    Time m_creditRefreshInterval;
    uint32_t m_initialCredits;
    uint32_t m_payloadMtu;
    uint32_t m_sendAdmissionMessages;
    uint64_t m_sendAdmissionBytes;
    uint32_t m_writeBudgetMessages;
    uint64_t m_writeBudgetBytes;
    uint32_t m_readResponderMessages;
    uint64_t m_readResponseBytes;
    bool m_tpdcReadResponseAggressiveDrain;
    bool m_tpdcReadResponseQueuePriority;
    ValidationStrictness m_validationStrictness;
    bool m_debugSnapshotsEnabled;
    bool m_allowAllJobs;
};

} // namespace ns3

#endif /* SOFT_UE_HELPER_H */
