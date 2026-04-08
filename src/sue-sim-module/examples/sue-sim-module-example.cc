/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/soft-ue-module.h"
#include "ns3/sue-sim-module-module.h"

#include <algorithm>
#include <iostream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SueSimModuleExample");

namespace {

void
Step (Time delta)
{
  const EventId stopEvent = Simulator::GetStopEvent ();
  if (stopEvent.IsPending ())
    {
      Simulator::Cancel (stopEvent);
    }
  Simulator::Stop (delta);
  Simulator::Run ();
}

bool
WaitForResponseCode (Ptr<SesManager> ses,
                     uint64_t jobId,
                     uint16_t msgId,
                     uint8_t rc,
                     Time timeout)
{
  const uint32_t iterations = std::max<uint32_t> (1u, timeout.GetMilliSeconds () / 2);
  for (uint32_t i = 0; i < iterations; ++i)
    {
      Step (MilliSeconds (2));
      if (ses->SawReturnCodeFor (jobId, msgId, rc))
        {
          return true;
        }
    }
  return false;
}

} // namespace

int
main (int argc, char* argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  LogComponentEnable ("SueSimModuleExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  SoftUeHelper helper;
  helper.SetDeviceAttribute ("MaxPacketSize", UintegerValue (1024));
  helper.SetDebugSnapshotsEnabled (true);
  helper.SetAuthorizeAllJobs (true);
  NetDeviceContainer devices = helper.Install (nodes);

  Ptr<SoftUeNetDevice> sender = DynamicCast<SoftUeNetDevice> (devices.Get (0));
  Ptr<SoftUeNetDevice> receiver = DynamicCast<SoftUeNetDevice> (devices.Get (1));
  NS_ABORT_MSG_UNLESS (sender && receiver, "soft-ue bridge demo requires SoftUeNetDevice");

  SoftUeObserverHelper observer;
  observer.SetPerformanceLoggingEnabled (true);
  observer.Install (devices);

  const uint64_t sendJobId = 90001;
  const uint16_t sendMsgId = 101;
  const std::vector<uint8_t> payload {'b', 'r', 'i', 'd', 'g', 'e', '-', 'o', 'k'};
  std::vector<uint8_t> recv (payload.size (), 0);

  NS_ABORT_MSG_UNLESS (receiver->GetSesManager ()->PostReceive (sendJobId,
                                                                sendMsgId,
                                                                sender->GetLocalFep (),
                                                                reinterpret_cast<uint64_t> (recv.data ()),
                                                                recv.size ()),
                       "failed to post receive for success path");

  Ptr<ExtendedOperationMetadata> sendMetadata = Create<ExtendedOperationMetadata> ();
  sendMetadata->op_type = OpType::SEND;
  sendMetadata->delivery_mode = static_cast<uint8_t> (DeliveryMode::RUD);
  sendMetadata->job_id = sendJobId;
  sendMetadata->messages_id = sendMsgId;
  sendMetadata->payload.length = payload.size ();
  sendMetadata->som = true;
  sendMetadata->eom = true;
  sendMetadata->expect_response = true;
  sendMetadata->SetSourceEndpoint (sender->GetLocalFep (), 1);
  sendMetadata->SetDestinationEndpoint (receiver->GetLocalFep (), 1);

  Ptr<Packet> sendPacket = Create<Packet> (payload.data (), payload.size ());
  NS_ABORT_MSG_UNLESS (sender->GetSesManager ()->ProcessSendRequest (sendMetadata, sendPacket),
                       "failed to dispatch success-path send");
  NS_ABORT_MSG_UNLESS (WaitForResponseCode (sender->GetSesManager (),
                                            sendJobId,
                                            sendMsgId,
                                            static_cast<uint8_t> (ResponseReturnCode::RC_OK),
                                            MilliSeconds (80)),
                       "success-path send did not converge");

  const uint64_t writeJobId = 90002;
  const uint16_t writeMsgId = 102;
  std::vector<uint8_t> invalidRemote (payload.size (), 0);
  Ptr<ExtendedOperationMetadata> writeMetadata = Create<ExtendedOperationMetadata> ();
  writeMetadata->op_type = OpType::WRITE;
  writeMetadata->delivery_mode = static_cast<uint8_t> (DeliveryMode::RUD);
  writeMetadata->job_id = writeJobId;
  writeMetadata->messages_id = writeMsgId;
  writeMetadata->payload.length = payload.size ();
  writeMetadata->payload.start_addr = reinterpret_cast<uint64_t> (invalidRemote.data ());
  writeMetadata->memory.rkey = 0xdeadbeef;
  writeMetadata->som = true;
  writeMetadata->eom = true;
  writeMetadata->expect_response = true;
  writeMetadata->SetSourceEndpoint (sender->GetLocalFep (), 1);
  writeMetadata->SetDestinationEndpoint (receiver->GetLocalFep (), 1);

  Ptr<Packet> writePacket = Create<Packet> (payload.data (), payload.size ());
  NS_ABORT_MSG_UNLESS (sender->GetSesManager ()->ProcessSendRequest (writeMetadata, writePacket),
                       "failed to dispatch validation-failure write");
  NS_ABORT_MSG_UNLESS (WaitForResponseCode (sender->GetSesManager (),
                                            writeJobId,
                                            writeMsgId,
                                            static_cast<uint8_t> (ResponseReturnCode::RC_INVALID_KEY),
                                            MilliSeconds (80)),
                       "validation-failure write did not return RC_INVALID_KEY");

  Step (MilliSeconds (120));

  const SoftUeProtocolSnapshot senderSnapshot =
      observer.GetLatestProtocolSnapshot (nodes.Get (0)->GetId (), sender->GetIfIndex ());
  const SoftUeFailureSnapshot receiverFailure =
      observer.GetLatestFailureSnapshot (nodes.Get (1)->GetId (), receiver->GetIfIndex ());
  const auto diagnostics =
      observer.GetRecentDiagnosticRecords (nodes.Get (1)->GetId (), receiver->GetIfIndex (), 4);

  std::cout << "soft-ue protocol snapshot: "
            << "node=" << senderSnapshot.node_id
            << " if=" << senderSnapshot.if_index
            << " fep=" << senderSnapshot.local_fep
            << " tx_bytes=" << senderSnapshot.device_stats.totalBytesTransmitted
            << " rx_bytes=" << senderSnapshot.device_stats.totalBytesReceived
            << " retries=" << senderSnapshot.runtime_stats.active_retry_states
            << std::endl;

  std::cout << "soft-ue failure snapshot: "
            << "job=" << receiverFailure.job_id
            << " msg=" << receiverFailure.msg_id
            << " stage=" << receiverFailure.stage
            << " domain=" << receiverFailure.failure_domain
            << " rc=" << static_cast<uint32_t> (receiverFailure.return_code)
            << std::endl;

  if (!diagnostics.empty ())
    {
      const auto& latest = diagnostics.back ();
      std::cout << "soft-ue diagnostic: "
                << latest.name << " detail=" << latest.detail << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}
