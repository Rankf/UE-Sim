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
 * @file             soft-ue-packet-tag.cc
 * @brief            ns-3 Packet Tag Implementation for Soft-UE Protocol Information
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-10
 * @copyright        Apache License Version 2.0
 *
 * @details
 * This file implements the packet tag classes used to attach Soft-UE protocol
 * information to ns-3 packets for transport through the ns-3 simulation framework.
 */

#include "soft-ue-packet-tag.h"
#include "ns3/log.h"
#include "ns3/type-id.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SoftUePacketTag");

// ============================================================================
// SoftUeHeaderTag Implementation
// ============================================================================

NS_OBJECT_ENSURE_REGISTERED (SoftUeHeaderTag);

TypeId
SoftUeHeaderTag::GetTypeId (void)
{
    NS_LOG_FUNCTION_NOARGS ();
    static TypeId tid = TypeId ("ns3::SoftUeHeaderTag")
        .SetParent<Tag> ()
        .SetGroupName ("SoftUe")
        .AddConstructor<SoftUeHeaderTag> ();
    return tid;
}

TypeId
SoftUeHeaderTag::GetInstanceTypeId (void) const
{
    NS_LOG_FUNCTION (this);
    return GetTypeId ();
}

uint32_t
SoftUeHeaderTag::GetSerializedSize (void) const
{
    NS_LOG_FUNCTION (this);
    return sizeof (PDSType) + sizeof (uint16_t) + sizeof (uint32_t) * 4;
}

void
SoftUeHeaderTag::Serialize (TagBuffer i) const
{
    NS_LOG_FUNCTION (this << &i);
    i.WriteU8 ((uint8_t)m_pdsType);
    i.WriteU16 (m_pdcId);
    i.WriteU32 (m_psn);
    i.WriteU32 (m_sourceEndpoint);
    i.WriteU32 (m_destEndpoint);
    i.WriteU32 (m_jobId);
}

void
SoftUeHeaderTag::Deserialize (TagBuffer i)
{
    NS_LOG_FUNCTION (this << &i);
    m_pdsType = (PDSType)i.ReadU8 ();
    m_pdcId = i.ReadU16 ();
    m_psn = i.ReadU32 ();
    m_sourceEndpoint = i.ReadU32 ();
    m_destEndpoint = i.ReadU32 ();
    m_jobId = i.ReadU32 ();
}

void
SoftUeHeaderTag::Print (std::ostream &os) const
{
    NS_LOG_FUNCTION (this << &os);
    os << "SoftUeHeaderTag [pdsType=" << (uint32_t)m_pdsType
       << ", pdcId=" << m_pdcId
       << ", psn=" << m_psn
       << ", src=" << m_sourceEndpoint
       << ", dst=" << m_destEndpoint
       << ", jobId=" << m_jobId << "]";
}

SoftUeHeaderTag::SoftUeHeaderTag ()
    : m_pdsType (PDSType::RESERVED), m_pdcId (0), m_psn (0),
      m_sourceEndpoint (0), m_destEndpoint (0), m_jobId (0)
{
    NS_LOG_FUNCTION (this);
}

SoftUeHeaderTag::SoftUeHeaderTag (PDSType pdsType, uint16_t pdcId, uint32_t psn)
    : m_pdsType (pdsType), m_pdcId (pdcId), m_psn (psn),
      m_sourceEndpoint (0), m_destEndpoint (0), m_jobId (0)
{
    NS_LOG_FUNCTION (this << (uint32_t)pdsType << pdcId << psn);
}

PDSType
SoftUeHeaderTag::GetPdsType (void) const
{
    NS_LOG_FUNCTION (this);
    return m_pdsType;
}

void
SoftUeHeaderTag::SetPdsType (PDSType pdsType)
{
    NS_LOG_FUNCTION (this << (uint32_t)pdsType);
    m_pdsType = pdsType;
}

uint16_t
SoftUeHeaderTag::GetPdcId (void) const
{
    NS_LOG_FUNCTION (this);
    return m_pdcId;
}

void
SoftUeHeaderTag::SetPdcId (uint16_t pdcId)
{
    NS_LOG_FUNCTION (this << pdcId);
    m_pdcId = pdcId;
}

uint32_t
SoftUeHeaderTag::GetPsn (void) const
{
    NS_LOG_FUNCTION (this);
    return m_psn;
}

void
SoftUeHeaderTag::SetPsn (uint32_t psn)
{
    NS_LOG_FUNCTION (this << psn);
    m_psn = psn;
}

uint32_t
SoftUeHeaderTag::GetSourceEndpoint (void) const
{
    NS_LOG_FUNCTION (this);
    return m_sourceEndpoint;
}

void
SoftUeHeaderTag::SetSourceEndpoint (uint32_t endpoint)
{
    NS_LOG_FUNCTION (this << endpoint);
    m_sourceEndpoint = endpoint;
}

uint32_t
SoftUeHeaderTag::GetDestinationEndpoint (void) const
{
    NS_LOG_FUNCTION (this);
    return m_destEndpoint;
}

void
SoftUeHeaderTag::SetDestinationEndpoint (uint32_t endpoint)
{
    NS_LOG_FUNCTION (this << endpoint);
    m_destEndpoint = endpoint;
}

uint32_t
SoftUeHeaderTag::GetJobId (void) const
{
    NS_LOG_FUNCTION (this);
    return m_jobId;
}

void
SoftUeHeaderTag::SetJobId (uint32_t jobId)
{
    NS_LOG_FUNCTION (this << jobId);
    m_jobId = jobId;
}

// ============================================================================
// SoftUeMetadataTag Implementation
// ============================================================================

NS_OBJECT_ENSURE_REGISTERED (SoftUeMetadataTag);

TypeId
SoftUeMetadataTag::GetTypeId (void)
{
    NS_LOG_FUNCTION_NOARGS ();
    static TypeId tid = TypeId ("ns3::SoftUeMetadataTag")
        .SetParent<Tag> ()
        .SetGroupName ("SoftUe")
        .AddConstructor<SoftUeMetadataTag> ();
    return tid;
}

TypeId
SoftUeMetadataTag::GetInstanceTypeId (void) const
{
    NS_LOG_FUNCTION (this);
    return GetTypeId ();
}

uint32_t
SoftUeMetadataTag::GetSerializedSize (void) const
{
    NS_LOG_FUNCTION (this);
    return sizeof (uint32_t) * 5 +
           sizeof (uint16_t) +
           sizeof (uint8_t) * 4 +
           sizeof (uint64_t) * 2;
}

void
SoftUeMetadataTag::Serialize (TagBuffer i) const
{
    NS_LOG_FUNCTION (this << &i);
    i.WriteU32 ((uint32_t)m_opType);
    i.WriteU32 (m_messageId);
    i.WriteU16 (m_resourceIndex);
    i.WriteU8 (m_reliable ? 1 : 0);
    i.WriteU32 (m_requestLength);
    i.WriteU32 (m_fragmentOffset);
    i.WriteU64 (m_remoteAddress);
    i.WriteU64 (m_remoteKey);
    i.WriteU8 (m_flags);
    i.WriteU8 (m_responseOpCode);
    i.WriteU8 (m_returnCode);
    i.WriteU32 (m_modifiedLength);
}

void
SoftUeMetadataTag::Deserialize (TagBuffer i)
{
    NS_LOG_FUNCTION (this << &i);
    m_opType = (OpType)i.ReadU32 ();
    m_messageId = i.ReadU32 ();
    m_resourceIndex = i.ReadU16 ();
    m_reliable = (i.ReadU8 () != 0);
    m_requestLength = i.ReadU32 ();
    m_fragmentOffset = i.ReadU32 ();
    m_remoteAddress = i.ReadU64 ();
    m_remoteKey = i.ReadU64 ();
    m_flags = i.ReadU8 ();
    m_responseOpCode = i.ReadU8 ();
    m_returnCode = i.ReadU8 ();
    m_modifiedLength = i.ReadU32 ();
}

void
SoftUeMetadataTag::Print (std::ostream &os) const
{
    NS_LOG_FUNCTION (this << &os);
    os << "SoftUeMetadataTag [opType=" << (uint32_t)m_opType
       << ", messageId=" << m_messageId
       << ", resourceIndex=" << m_resourceIndex
       << ", reliable=" << (m_reliable ? "true" : "false")
       << ", requestLength=" << m_requestLength
       << ", fragmentOffset=" << m_fragmentOffset
       << ", remoteAddress=" << m_remoteAddress
       << ", remoteKey=" << m_remoteKey
       << ", flags=" << static_cast<uint32_t> (m_flags)
       << ", responseOpCode=" << static_cast<uint32_t> (m_responseOpCode)
       << ", returnCode=" << static_cast<uint32_t> (m_returnCode)
       << ", modifiedLength=" << m_modifiedLength << "]";
}

SoftUeMetadataTag::SoftUeMetadataTag ()
    : m_opType (OpType::SEND),
      m_messageId (0),
      m_resourceIndex (0),
      m_reliable (false),
      m_requestLength (0),
      m_fragmentOffset (0),
      m_remoteAddress (0),
      m_remoteKey (0),
      m_flags (0),
      m_responseOpCode (0),
      m_returnCode (0),
      m_modifiedLength (0)
{
    NS_LOG_FUNCTION (this);
}

SoftUeMetadataTag::SoftUeMetadataTag (const OperationMetadata& metadata)
    : m_opType (metadata.op_type), m_messageId (metadata.messages_id),
      m_resourceIndex (metadata.res_index), m_reliable (metadata.delivery_mode == 0),
      m_requestLength (static_cast<uint32_t> (metadata.payload.length)),
      m_fragmentOffset (0),
      m_remoteAddress (metadata.op_type == OpType::READ ? metadata.payload.imm_data
                                                        : metadata.payload.start_addr),
      m_remoteKey (metadata.memory.rkey),
      m_flags (metadata.is_retry ? SOFT_UE_METADATA_RETRY : 0),
      m_responseOpCode (0),
      m_returnCode (0),
      m_modifiedLength (0)
{
    NS_LOG_FUNCTION (this << (uint32_t)m_opType << m_messageId << m_resourceIndex << m_reliable);
}

OpType
SoftUeMetadataTag::GetOperationType (void) const
{
    NS_LOG_FUNCTION (this);
    return m_opType;
}

void
SoftUeMetadataTag::SetOperationType (OpType opType)
{
    NS_LOG_FUNCTION (this << (uint32_t)opType);
    m_opType = opType;
}

uint32_t
SoftUeMetadataTag::GetMessageId (void) const
{
    NS_LOG_FUNCTION (this);
    return m_messageId;
}

void
SoftUeMetadataTag::SetMessageId (uint32_t messageId)
{
    NS_LOG_FUNCTION (this << messageId);
    m_messageId = messageId;
}

uint16_t
SoftUeMetadataTag::GetResourceIndex (void) const
{
    NS_LOG_FUNCTION (this);
    return m_resourceIndex;
}

void
SoftUeMetadataTag::SetResourceIndex (uint16_t resourceIndex)
{
    NS_LOG_FUNCTION (this << resourceIndex);
    m_resourceIndex = resourceIndex;
}

bool
SoftUeMetadataTag::IsReliable (void) const
{
    NS_LOG_FUNCTION (this);
    return m_reliable;
}

void
SoftUeMetadataTag::SetReliable (bool reliable)
{
    NS_LOG_FUNCTION (this << reliable);
    m_reliable = reliable;
}

uint32_t
SoftUeMetadataTag::GetRequestLength (void) const
{
    return m_requestLength;
}

void
SoftUeMetadataTag::SetRequestLength (uint32_t requestLength)
{
    m_requestLength = requestLength;
}

uint32_t
SoftUeMetadataTag::GetFragmentOffset (void) const
{
    return m_fragmentOffset;
}

void
SoftUeMetadataTag::SetFragmentOffset (uint32_t fragmentOffset)
{
    m_fragmentOffset = fragmentOffset;
}

uint64_t
SoftUeMetadataTag::GetRemoteAddress (void) const
{
    return m_remoteAddress;
}

void
SoftUeMetadataTag::SetRemoteAddress (uint64_t remoteAddress)
{
    m_remoteAddress = remoteAddress;
}

uint64_t
SoftUeMetadataTag::GetRemoteKey (void) const
{
    return m_remoteKey;
}

void
SoftUeMetadataTag::SetRemoteKey (uint64_t remoteKey)
{
    m_remoteKey = remoteKey;
}

uint8_t
SoftUeMetadataTag::GetFlags (void) const
{
    return m_flags;
}

void
SoftUeMetadataTag::SetFlags (uint8_t flags)
{
    m_flags = flags;
}

bool
SoftUeMetadataTag::IsResponse (void) const
{
    return (m_flags & SOFT_UE_METADATA_RESPONSE) != 0;
}

void
SoftUeMetadataTag::SetIsResponse (bool isResponse)
{
    if (isResponse)
    {
        m_flags |= SOFT_UE_METADATA_RESPONSE;
    }
    else
    {
        m_flags &= ~SOFT_UE_METADATA_RESPONSE;
    }
}

bool
SoftUeMetadataTag::IsRetry (void) const
{
    return (m_flags & SOFT_UE_METADATA_RETRY) != 0;
}

void
SoftUeMetadataTag::SetIsRetry (bool isRetry)
{
    if (isRetry)
    {
        m_flags |= SOFT_UE_METADATA_RETRY;
    }
    else
    {
        m_flags &= ~SOFT_UE_METADATA_RETRY;
    }
}

uint8_t
SoftUeMetadataTag::GetResponseOpCode (void) const
{
    return m_responseOpCode;
}

void
SoftUeMetadataTag::SetResponseOpCode (uint8_t opcode)
{
    m_responseOpCode = opcode;
}

uint8_t
SoftUeMetadataTag::GetReturnCode (void) const
{
    return m_returnCode;
}

void
SoftUeMetadataTag::SetReturnCode (uint8_t returnCode)
{
    m_returnCode = returnCode;
}

uint32_t
SoftUeMetadataTag::GetModifiedLength (void) const
{
    return m_modifiedLength;
}

void
SoftUeMetadataTag::SetModifiedLength (uint32_t modifiedLength)
{
    m_modifiedLength = modifiedLength;
}

// ============================================================================
// SoftUeTpdcControlTag Implementation
// ============================================================================

NS_OBJECT_ENSURE_REGISTERED (SoftUeTpdcControlTag);

TypeId
SoftUeTpdcControlTag::GetTypeId (void)
{
    NS_LOG_FUNCTION_NOARGS ();
    static TypeId tid = TypeId ("ns3::SoftUeTpdcControlTag")
        .SetParent<Tag> ()
        .SetGroupName ("SoftUe")
        .AddConstructor<SoftUeTpdcControlTag> ();
    return tid;
}

TypeId
SoftUeTpdcControlTag::GetInstanceTypeId (void) const
{
    NS_LOG_FUNCTION (this);
    return GetTypeId ();
}

uint32_t
SoftUeTpdcControlTag::GetSerializedSize (void) const
{
    NS_LOG_FUNCTION (this);
    return sizeof (uint8_t) + sizeof (uint32_t) * 3;
}

void
SoftUeTpdcControlTag::Serialize (TagBuffer i) const
{
    NS_LOG_FUNCTION (this << &i);
    i.WriteU8 (m_flags);
    i.WriteU32 (m_cumulativeAck);
    i.WriteU32 (m_selectiveAck);
    i.WriteU32 (m_gapNack);
}

void
SoftUeTpdcControlTag::Deserialize (TagBuffer i)
{
    NS_LOG_FUNCTION (this << &i);
    m_flags = i.ReadU8 ();
    m_cumulativeAck = i.ReadU32 ();
    m_selectiveAck = i.ReadU32 ();
    m_gapNack = i.ReadU32 ();
}

void
SoftUeTpdcControlTag::Print (std::ostream &os) const
{
    NS_LOG_FUNCTION (this << &os);
    os << "SoftUeTpdcControlTag [flags=" << static_cast<uint32_t> (m_flags)
       << ", cumulativeAck=" << m_cumulativeAck
       << ", selectiveAck=" << m_selectiveAck
       << ", gapNack=" << m_gapNack << "]";
}

SoftUeTpdcControlTag::SoftUeTpdcControlTag ()
    : m_flags (0),
      m_cumulativeAck (0),
      m_selectiveAck (0),
      m_gapNack (0)
{
    NS_LOG_FUNCTION (this);
}

uint8_t
SoftUeTpdcControlTag::GetFlags (void) const
{
    return m_flags;
}

void
SoftUeTpdcControlTag::SetFlags (uint8_t flags)
{
    m_flags = flags;
}

uint32_t
SoftUeTpdcControlTag::GetCumulativeAck (void) const
{
    return m_cumulativeAck;
}

void
SoftUeTpdcControlTag::SetCumulativeAck (uint32_t ack)
{
    m_cumulativeAck = ack;
}

uint32_t
SoftUeTpdcControlTag::GetSelectiveAck (void) const
{
    return m_selectiveAck;
}

void
SoftUeTpdcControlTag::SetSelectiveAck (uint32_t ack)
{
    m_selectiveAck = ack;
}

uint32_t
SoftUeTpdcControlTag::GetGapNack (void) const
{
    return m_gapNack;
}

void
SoftUeTpdcControlTag::SetGapNack (uint32_t nack)
{
    m_gapNack = nack;
}

// ============================================================================
// SoftUeTimingTag Implementation
// ============================================================================

NS_OBJECT_ENSURE_REGISTERED (SoftUeTimingTag);

TypeId
SoftUeTimingTag::GetTypeId (void)
{
    NS_LOG_FUNCTION_NOARGS ();
    static TypeId tid = TypeId ("ns3::SoftUeTimingTag")
        .SetParent<Tag> ()
        .SetGroupName ("SoftUe")
        .AddConstructor<SoftUeTimingTag> ();
    return tid;
}

TypeId
SoftUeTimingTag::GetInstanceTypeId (void) const
{
    NS_LOG_FUNCTION (this);
    return GetTypeId ();
}

uint32_t
SoftUeTimingTag::GetSerializedSize (void) const
{
    NS_LOG_FUNCTION (this);
    return 3 * sizeof (int64_t); // Three timestamps
}

void
SoftUeTimingTag::Serialize (TagBuffer i) const
{
    NS_LOG_FUNCTION (this << &i);
    i.WriteU64 ((int64_t)m_timestamp.GetNanoSeconds ());
    i.WriteU64 ((int64_t)m_expectedDeliveryTime.GetNanoSeconds ());
    i.WriteU64 ((int64_t)m_detailTimestamp.GetNanoSeconds ());
}

void
SoftUeTimingTag::Deserialize (TagBuffer i)
{
    NS_LOG_FUNCTION (this << &i);
    m_timestamp = NanoSeconds (i.ReadU64 ());
    m_expectedDeliveryTime = NanoSeconds (i.ReadU64 ());
    m_detailTimestamp = NanoSeconds (i.ReadU64 ());
}

void
SoftUeTimingTag::Print (std::ostream &os) const
{
    NS_LOG_FUNCTION (this << &os);
    os << "SoftUeTimingTag [timestamp=" << m_timestamp.GetNanoSeconds ()
       << "ns, expected=" << m_expectedDeliveryTime.GetNanoSeconds ()
       << "ns, detail=" << m_detailTimestamp.GetNanoSeconds () << "ns]";
}

SoftUeTimingTag::SoftUeTimingTag ()
    : m_timestamp (Simulator::Now ()), m_expectedDeliveryTime (Time::Max ()), m_detailTimestamp (Time::Max ())
{
    NS_LOG_FUNCTION (this);
}

SoftUeTimingTag::SoftUeTimingTag (Time timestamp)
    : m_timestamp (timestamp), m_expectedDeliveryTime (Time::Max ()), m_detailTimestamp (Time::Max ())
{
    NS_LOG_FUNCTION (this << timestamp.GetNanoSeconds ());
}

Time
SoftUeTimingTag::GetTimestamp (void) const
{
    NS_LOG_FUNCTION (this);
    return m_timestamp;
}

void
SoftUeTimingTag::SetTimestamp (Time timestamp)
{
    NS_LOG_FUNCTION (this << timestamp.GetNanoSeconds ());
    m_timestamp = timestamp;
}

Time
SoftUeTimingTag::GetExpectedDeliveryTime (void) const
{
    NS_LOG_FUNCTION (this);
    return m_expectedDeliveryTime;
}

void
SoftUeTimingTag::SetExpectedDeliveryTime (Time time)
{
    NS_LOG_FUNCTION (this << time.GetNanoSeconds ());
    m_expectedDeliveryTime = time;
}

Time
SoftUeTimingTag::GetAuxTimestamp (void) const
{
    NS_LOG_FUNCTION (this);
    return m_expectedDeliveryTime;
}

void
SoftUeTimingTag::SetAuxTimestamp (Time time)
{
    NS_LOG_FUNCTION (this << time.GetNanoSeconds ());
    m_expectedDeliveryTime = time;
}

Time
SoftUeTimingTag::GetDetailTimestamp (void) const
{
    NS_LOG_FUNCTION (this);
    return m_detailTimestamp;
}

void
SoftUeTimingTag::SetDetailTimestamp (Time time)
{
    NS_LOG_FUNCTION (this << time.GetNanoSeconds ());
    m_detailTimestamp = time;
}

// ============================================================================
// SoftUeRecoveryTag Implementation
// ============================================================================

NS_OBJECT_ENSURE_REGISTERED (SoftUeRecoveryTag);

TypeId
SoftUeRecoveryTag::GetTypeId (void)
{
    NS_LOG_FUNCTION_NOARGS ();
    static TypeId tid = TypeId ("ns3::SoftUeRecoveryTag")
        .SetParent<Tag> ()
        .SetGroupName ("SoftUe")
        .AddConstructor<SoftUeRecoveryTag> ();
    return tid;
}

TypeId
SoftUeRecoveryTag::GetInstanceTypeId (void) const
{
    NS_LOG_FUNCTION (this);
    return GetTypeId ();
}

uint32_t
SoftUeRecoveryTag::GetSerializedSize (void) const
{
    NS_LOG_FUNCTION (this);
    return 2 * sizeof (int64_t);
}

void
SoftUeRecoveryTag::Serialize (TagBuffer i) const
{
    NS_LOG_FUNCTION (this << &i);
    i.WriteU64 ((int64_t)m_gapNackSentTime.GetNanoSeconds ());
    i.WriteU64 ((int64_t)m_retransmitTxTime.GetNanoSeconds ());
}

void
SoftUeRecoveryTag::Deserialize (TagBuffer i)
{
    NS_LOG_FUNCTION (this << &i);
    m_gapNackSentTime = NanoSeconds (i.ReadU64 ());
    m_retransmitTxTime = NanoSeconds (i.ReadU64 ());
}

void
SoftUeRecoveryTag::Print (std::ostream &os) const
{
    NS_LOG_FUNCTION (this << &os);
    os << "SoftUeRecoveryTag [gapNackSent=" << m_gapNackSentTime.GetNanoSeconds ()
       << "ns, retransmitTx=" << m_retransmitTxTime.GetNanoSeconds () << "ns]";
}

SoftUeRecoveryTag::SoftUeRecoveryTag ()
    : m_gapNackSentTime (Time::Max ()),
      m_retransmitTxTime (Time::Max ())
{
    NS_LOG_FUNCTION (this);
}

Time
SoftUeRecoveryTag::GetGapNackSentTime (void) const
{
    NS_LOG_FUNCTION (this);
    return m_gapNackSentTime;
}

void
SoftUeRecoveryTag::SetGapNackSentTime (Time time)
{
    NS_LOG_FUNCTION (this << time.GetNanoSeconds ());
    m_gapNackSentTime = time;
}

Time
SoftUeRecoveryTag::GetRetransmitTxTime (void) const
{
    NS_LOG_FUNCTION (this);
    return m_retransmitTxTime;
}

void
SoftUeRecoveryTag::SetRetransmitTxTime (Time time)
{
    NS_LOG_FUNCTION (this << time.GetNanoSeconds ());
    m_retransmitTxTime = time;
}

// ============================================================================
// SoftUeFragmentTag Implementation
// ============================================================================

NS_OBJECT_ENSURE_REGISTERED (SoftUeFragmentTag);

TypeId
SoftUeFragmentTag::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SoftUeFragmentTag")
        .SetParent<Tag> ()
        .SetGroupName ("SoftUe")
        .AddConstructor<SoftUeFragmentTag> ();
    return tid;
}

TypeId
SoftUeFragmentTag::GetInstanceTypeId (void) const
{
    return GetTypeId ();
}

uint32_t
SoftUeFragmentTag::GetSerializedSize (void) const
{
    return sizeof (uint32_t) * 2;
}

void
SoftUeFragmentTag::Serialize (TagBuffer i) const
{
    i.WriteU32 (m_fragmentIndex);
    i.WriteU32 (m_totalFragments);
}

void
SoftUeFragmentTag::Deserialize (TagBuffer i)
{
    m_fragmentIndex = i.ReadU32 ();
    m_totalFragments = i.ReadU32 ();
}

void
SoftUeFragmentTag::Print (std::ostream &os) const
{
    os << "SoftUeFragmentTag [fragment=" << m_fragmentIndex << "/" << m_totalFragments << "]";
}

SoftUeFragmentTag::SoftUeFragmentTag ()
    : m_fragmentIndex (0), m_totalFragments (0)
{
}

SoftUeFragmentTag::SoftUeFragmentTag (uint32_t fragmentIndex, uint32_t totalFragments)
    : m_fragmentIndex (fragmentIndex), m_totalFragments (totalFragments)
{
}

uint32_t
SoftUeFragmentTag::GetFragmentIndex (void) const
{
    return m_fragmentIndex;
}

void
SoftUeFragmentTag::SetFragmentIndex (uint32_t index)
{
    m_fragmentIndex = index;
}

uint32_t
SoftUeFragmentTag::GetTotalFragments (void) const
{
    return m_totalFragments;
}

void
SoftUeFragmentTag::SetTotalFragments (uint32_t total)
{
    m_totalFragments = total;
}

// ============================================================================
// SoftUeTransactionTag Implementation
// ============================================================================

NS_OBJECT_ENSURE_REGISTERED (SoftUeTransactionTag);

TypeId
SoftUeTransactionTag::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SoftUeTransactionTag")
        .SetParent<Tag> ()
        .SetGroupName ("SoftUe")
        .AddConstructor<SoftUeTransactionTag> ();
    return tid;
}

TypeId
SoftUeTransactionTag::GetInstanceTypeId (void) const
{
    return GetTypeId ();
}

uint32_t
SoftUeTransactionTag::GetSerializedSize (void) const
{
    return sizeof (uint32_t) * 2;
}

void
SoftUeTransactionTag::Serialize (TagBuffer i) const
{
    i.WriteU32 (m_transactionIndex);
    i.WriteU32 (m_totalTransactions);
}

void
SoftUeTransactionTag::Deserialize (TagBuffer i)
{
    m_transactionIndex = i.ReadU32 ();
    m_totalTransactions = i.ReadU32 ();
}

void
SoftUeTransactionTag::Print (std::ostream &os) const
{
    os << "SoftUeTransactionTag [transaction=" << m_transactionIndex << "/" << m_totalTransactions << "]";
}

SoftUeTransactionTag::SoftUeTransactionTag ()
    : m_transactionIndex (0), m_totalTransactions (0)
{
}

SoftUeTransactionTag::SoftUeTransactionTag (uint32_t transactionIndex, uint32_t totalTransactions)
    : m_transactionIndex (transactionIndex), m_totalTransactions (totalTransactions)
{
}

uint32_t
SoftUeTransactionTag::GetTransactionIndex (void) const
{
    return m_transactionIndex;
}

void
SoftUeTransactionTag::SetTransactionIndex (uint32_t index)
{
    m_transactionIndex = index;
}

uint32_t
SoftUeTransactionTag::GetTotalTransactions (void) const
{
    return m_totalTransactions;
}

void
SoftUeTransactionTag::SetTotalTransactions (uint32_t total)
{
    m_totalTransactions = total;
}

} // namespace ns3
