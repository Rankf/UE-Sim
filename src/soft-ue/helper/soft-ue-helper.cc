#include "soft-ue-helper.h"
#include "../model/network/soft-ue-net-device.h"
#include "../model/network/soft-ue-channel.h"
#include "../model/pds/pds-manager.h"
#include "../model/ses/ses-manager.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/names.h"
#include "ns3/pointer.h"
#include "ns3/mac48-address.h"
#include "ns3/object-factory.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SoftUeHelper");

SoftUeHelper::SoftUeHelper ()
  : m_deviceMaxPacketSize (1500),
    m_rudSemanticMode (true),
    m_unexpectedMessages (64),
    m_unexpectedBytes (1u << 20),
    m_arrivalCapacity (64),
    m_maxReadTracks (64),
    m_retryEnabled (true),
    m_retryTimeout (MilliSeconds (80)),
    m_creditGateEnabled (false),
    m_ackControlEnabled (false),
    m_creditRefreshInterval (MilliSeconds (5)),
    m_initialCredits (0),
    m_payloadMtu (0),
    m_sendAdmissionMessages (64),
    m_sendAdmissionBytes (64ull * 2048ull),
    m_writeBudgetMessages (64),
    m_writeBudgetBytes (64ull * 2048ull),
    m_readResponderMessages (64),
    m_readResponseBytes (64ull * 2048ull),
    m_validationStrictness (ValidationStrictness::STRICT),
    m_debugSnapshotsEnabled (true),
    m_allowAllJobs (true)
{
  NS_LOG_FUNCTION (this);

  // Set default device factory
  m_deviceFactory.SetTypeId ("ns3::SoftUeNetDevice");
  m_channelFactory.SetTypeId ("ns3::SoftUeChannel");

  // Set default device attributes
  m_deviceFactory.Set ("EnableStatistics", BooleanValue (true));
  m_deviceFactory.Set ("MaxPdcCount", UintegerValue (256));
  m_deviceFactory.Set ("ProcessingInterval", TimeValue (NanoSeconds (50))); // Data center processing delay
  m_deviceFactory.Set ("MaxPacketSize", UintegerValue (1500));
}

NetDeviceContainer
SoftUeHelper::Install (NodeContainer nodes)
{
  NS_LOG_FUNCTION (this << nodes.GetN ());

  NetDeviceContainer devices;

  // Create and install Soft-Ue channels
  Ptr<SoftUeChannel> channel = m_channelFactory.Create<SoftUeChannel> ();

  // Install devices on each node
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<SoftUeNetDevice> device = m_deviceFactory.Create<SoftUeNetDevice> ();

      // Set device attributes
      SoftUeConfig config;
      config.address = Mac48Address::Allocate ();
      config.maxPacketSize = static_cast<uint16_t> (m_deviceMaxPacketSize);
      // Keep protocol snapshots observable in short truth/fabric benchmarks.
      config.statsUpdateInterval = MilliSeconds (10);

      // Extract FEP from allocated MAC address to ensure consistency with ExtractFepFromAddress
      uint8_t buffer[6];
      config.address.CopyTo(buffer);
      config.localFep = (static_cast<uint16_t>(buffer[4]) << 8) | buffer[5];

      // Ensure FEP is never zero (minimum value is 1)
      if (config.localFep == 0) {
          config.localFep = i + 1;
      }

      // Initialize the device
      device->Initialize (config);
      if (device->GetSesManager ())
        {
          device->GetSesManager ()->SetRudSemanticMode (m_rudSemanticMode);
          device->GetSesManager ()->ConfigureUnexpectedResources (m_unexpectedMessages,
                                                                  m_unexpectedBytes,
                                                                  m_arrivalCapacity);
          device->GetSesManager ()->ConfigureArrivalTracking (m_arrivalCapacity, m_maxReadTracks);
          device->GetSesManager ()->ConfigureRetry (m_retryEnabled, m_retryTimeout);
          device->GetSesManager ()->SetCreditGateEnabled (m_creditGateEnabled);
          device->GetSesManager ()->SetAckControlEnabled (m_ackControlEnabled);
          device->GetSesManager ()->SetCreditRefreshInterval (m_creditRefreshInterval);
          device->GetSesManager ()->SetInitialCredits (m_initialCredits);
          device->GetSesManager ()->SetPayloadMtu (m_payloadMtu);
          device->GetSesManager ()->ConfigureSemanticBudgets (m_sendAdmissionMessages,
                                                              m_sendAdmissionBytes,
                                                              m_writeBudgetMessages,
                                                              m_writeBudgetBytes,
                                                              m_readResponderMessages,
                                                              m_readResponseBytes);
          device->GetSesManager ()->SetValidationStrictness (m_validationStrictness);
          device->GetSesManager ()->SetDebugSnapshotsEnabled (m_debugSnapshotsEnabled);
          device->GetSesManager ()->SetAuthorizeAllJobs (m_allowAllJobs);
        }
      if (device->GetPdsManager ())
        {
          device->GetPdsManager ()->SetPayloadMtu (m_payloadMtu);
        }

      // Connect device to channel
      device->SetChannel (channel);

      // Add device to node
      node->AddDevice (device);

      devices.Add (device);

      NS_LOG_INFO ("Installed Soft-Ue device on node " << i <<
                   " with FEP ID " << config.localFep <<
                   " and MAC address " << config.address);
    }

  // Connect channel to devices
  channel->Connect (devices);

  NS_LOG_INFO ("Installed " << devices.GetN () << " Soft-Ue devices on " << nodes.GetN () << " nodes");
  return devices;
}

NetDeviceContainer
SoftUeHelper::Install (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << node);

  // Create a node container with the single node
  NodeContainer nodes;
  nodes.Add (node);

  return Install (nodes);
}

void
SoftUeHelper::SetDeviceAttribute (std::string name, const AttributeValue& value)
{
  NS_LOG_FUNCTION (this << name << &value);
  m_deviceFactory.Set (name, value);
  if (name == "MaxPacketSize")
    {
      const auto* maxPacketSize = dynamic_cast<const UintegerValue*> (&value);
      if (maxPacketSize)
        {
          m_deviceMaxPacketSize = maxPacketSize->Get ();
        }
    }
}

void
SoftUeHelper::SetChannelAttribute (std::string name, const AttributeValue& value)
{
  m_channelFactory.Set (name, value);
}

void
SoftUeHelper::SetRudSemanticMode (bool enabled)
{
  m_rudSemanticMode = enabled;
}

void
SoftUeHelper::ConfigureUnexpectedResources (uint32_t maxMessages, uint32_t maxBytes, uint32_t arrivalCapacity)
{
  m_unexpectedMessages = maxMessages;
  m_unexpectedBytes = maxBytes;
  m_arrivalCapacity = arrivalCapacity;
}

void
SoftUeHelper::ConfigureRetry (bool enabled, Time retryTimeout)
{
  m_retryEnabled = enabled;
  m_retryTimeout = retryTimeout;
}

void
SoftUeHelper::SetCreditGateEnabled (bool enabled)
{
  m_creditGateEnabled = enabled;
}

void
SoftUeHelper::SetAckControlEnabled (bool enabled)
{
  m_ackControlEnabled = enabled;
}

void
SoftUeHelper::SetCreditRefreshInterval (Time interval)
{
  m_creditRefreshInterval = interval;
}

void
SoftUeHelper::SetInitialCredits (uint32_t initialCredits)
{
  m_initialCredits = initialCredits;
}

void
SoftUeHelper::SetPayloadMtu (uint32_t payloadMtu)
{
  m_payloadMtu = payloadMtu;
}

void
SoftUeHelper::ConfigureArrivalTracking (uint32_t maxArrivalBlocks, uint32_t maxReadTracks)
{
  m_arrivalCapacity = maxArrivalBlocks;
  m_maxReadTracks = maxReadTracks;
}

void
SoftUeHelper::ConfigureSemanticBudgets (uint32_t sendAdmissionMessages,
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
}

void
SoftUeHelper::SetValidationStrictness (ValidationStrictness strictness)
{
  m_validationStrictness = strictness;
}

void
SoftUeHelper::SetDebugSnapshotsEnabled (bool enabled)
{
  m_debugSnapshotsEnabled = enabled;
}

void
SoftUeHelper::SetAuthorizeAllJobs (bool allowAll)
{
  m_allowAllJobs = allowAll;
}

} // namespace ns3
