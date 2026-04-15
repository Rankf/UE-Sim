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
 * @file             transport-layer.h
 * @brief            Ultra Ethernet Transport Layer Protocol Definitions
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-07
 * @copyright        Apache License Version 2.0
 *
 * @details
 * This file contains protocol definitions and data structures for the
 * Ultra Ethernet transport layer, adapted for ns-3 simulation.
 */

#ifndef SOFT_UE_TRANSPORT_LAYER_H
#define SOFT_UE_TRANSPORT_LAYER_H

#include <stdint.h>
#include <array>
#include <string>
#include <vector>
#include <ns3/object.h>
#include <ns3/packet.h>

namespace ns3 {

//=============================================================================
// Configuration Constants
//=============================================================================

#define MAX_MTU 4096                           // Maximum transmission unit
#define MAX_QUEUE_SIZE 512                      // Maximum queue size
#define MAX_PDC 512                            // Maximum number of PDCs per type
#define Base_RTO 100                           // Base retransmission timeout (ms)
#define Pend_Timeout 100                       // Pending timeout (ms)
#define Close_Thresh 4                         // Connection close threshold

// UET protocol configuration
#define UET_Over_UDP 1                         // UET runs over UDP
#define UDP_Dest_Port 2887                     // UDP port for UET protocol
#define Limit_PSN_Range 1                      // Limit PSN range for security
#define Default_MPR 8                          // Default MPR value

// Retransmission parameters
#define RTO_Init_Time 4                        // Initial RTO multiplier
#define Max_RTO_Retx_Cnt 5                     // Maximum retransmission count
#define NACK_Retx_Time 4                       // NACK retransmission delay
#define Max_NACK_Retx_Cnt 5                    // Maximum NACK retransmission count

//=============================================================================
// Enumeration Types
//=============================================================================

/**
 * @brief Operation types for Ultra Ethernet
 */
enum class OpType : uint8_t
{
    SEND = 1,          // Standard send operation
    READ = 2,          // RMA read operation
    WRITE = 3,         // RMA write operation
    DEFERRABLE = 4     // Deferred send operation
};

/**
 * @brief PDS header type enumeration
 */
enum class PDSHeaderType : uint8_t
{
    ENTROPY_HEADER,    // Entropy header for path selection
    RUOD_REQ_HEADER,   // RUOD request header
    RUOD_ACK_HEADER,   // RUOD acknowledgment header
    RUOD_CP_HEADER,    // RUOD control packet header
    NACK_HEADER        // Negative acknowledgment header
};

/**
 * @brief PDS packet type enumeration
 */
enum class PDSType : uint8_t
{
    RESERVED,          // Reserved type
    TSS,              // UET encryption header
    RUD_REQ,          // RUD request (Reliable Unordered Delivery)
    ROD_REQ,          // ROD request (Reliable Ordered Delivery)
    RUDI_REQ,         // RUDI request (Reliable Unordered Delivery with Immediate response)
    RUDI_RESP,        // RUDI response
    UUD_REQ,          // UUD request (Unreliable Unordered Delivery)
    ACK,              // Acknowledgment packet
    ACK_CC,           // Congestion control acknowledgment packet
    NACK,             // Negative acknowledgment packet
    CP                // Control packet
};

/**
 * @brief PDS next header type enumeration
 */
enum class PDSNextHeader : uint8_t
{
    UET_HDR_REQUEST_SMALL,      // Small request header
    UET_HDR_REQUEST_MEDIUM,     // Medium request header
    UET_HDR_REQUEST_STD,        // Standard request header
    UET_HDR_RESPONSE,           // Response header
    UET_HDR_RESPONSE_DATA,      // Response header with data
    UET_HDR_RESPONSE_DATA_SMALL,// Response header with small data
    PAYLOAD,                    // Raw payload data
    UET_HDR_NONE,               // No header
    PDS_NEXT_HEADER_ROCE = 0x10  // RoCE (RDMA over Converged Ethernet) header
};


/**
 * @brief PDC type enumeration
 */
enum class PdcType : uint8_t
{
    IPDC,              // Unreliable PDC
    TPDC               // Reliable PDC
};

/**
 * @brief SES header type enumeration
 */
enum class SESHeaderType : uint8_t
{
    STANDARD_HEADER,
    OPTIMIZED_HEADER,
    SMALL_MESSAGE_RMA_HEADER,
    SEMANTIC_RESPONSE_HEADER,
    SEMANTIC_RESPONSE_WITH_DATA_HEADER,
    OPTIMIZED_RESPONSE_WITH_DATA_HEADER
};

/**
 * @brief Response return codes
 */
enum class ResponseReturnCode : uint8_t
{
    RC_OK = 0x00,                    // Normal operation
    RC_PARTIAL_WRITE = 0x01,         // Partial data write
    RC_NO_MATCH = 0x02,              // Tagged send no matching buffer
    RC_INVALID_OP = 0x03,            // Invalid operation code
    RC_NO_BUFFER = 0x10,             // No available receive buffer
    RC_INVALID_KEY = 0x11,           // RMA memory key invalid/expired
    RC_ACCESS_DENIED = 0x12,         // JobID no access permission
    RC_ADDR_UNREACHABLE = 0x13,      // Target address unreachable
    RC_SECURITY_DOMAIN_MISMATCH = 0x20, // Security domain verification failed
    RC_INTEGRITY_CHECK_FAIL = 0x21, // Packet integrity check failed
    RC_REPLAY_DETECTED = 0x22,       // Replay attack detected
    RC_INTERNAL_ERROR = 0x30,        // SES internal state abnormal
    RC_RESOURCE_EXHAUST = 0x31,      // System resource exhausted
    RC_PROTOCOL_ERROR = 0x32         // Protocol violation
};

/**
 * @brief Response operation codes
 */
enum class ResponseOpCode : uint8_t
{
    UET_RESPONSE = 0x00,             // General response
    UET_DEFAULT_RESPONSE = 0x01,     // Default success response
    UET_RESPONSE_W_DATA = 0x02,      // Response with data
    UET_NO_RESPONSE = 0x03,          // Empty response
    UET_NACK = 0x04                  // Error response
};

/**
 * @brief NACK codes for error handling
 */
enum class NackCode : uint8_t
{
    SEQ_GAP = 0x01,          // Sequence gap detected
    RESOURCE = 0x02,         // Resource unavailable
    ACCESS_DENIED = 0x03,    // Access denied
    INVALID_OPCODE = 0x04,   // Invalid operation code
    CHECKSUM = 0x05,         // Checksum error
    TTL_EXCEEDED = 0x06,     // TTL exceeded
    PROTOCOL = 0x07          // Protocol error
};

enum class ValidationStrictness : uint8_t
{
    RELAXED = 0x00,
    STRICT = 0x01,
};

//=============================================================================
// Core Data Structures
//=============================================================================

/**
 * @brief Operation metadata for SES layer
 */
struct OperationMetadata
{
    OpType op_type;                        // Operation type
    uint8_t delivery_mode;                 // Delivery mode (DeliveryMode enum value)

    // Memory region information
    struct {
        uint64_t rkey;                    // Registered memory key
        bool idempotent_safe;             // Idempotent operation safety flag
    } memory;

    // Data payload information
    struct {
        uint64_t start_addr;              // Data start address
        size_t length;                    // Data length
        uint64_t imm_data;                // Immediate data (optional)
    } payload;

    uint32_t s_pid_on_fep;                // Source endpoint process ID
    uint32_t t_pid_on_fep;                // Target endpoint process ID
    uint32_t job_id;                      // Job identifier
    uint16_t res_index;                   // Resource index
    uint32_t messages_id;                 // Message identifier

    // Operation flags
    bool relative;                        // Relative addressing flag
    bool use_optimized_header;            // Optimized header usage flag
    bool has_imm_data;                    // Immediate data presence flag
    bool som;                             // Start-of-message flag
    bool eom;                             // End-of-message flag
    bool expect_response;                 // Semantic response expected
    bool is_retry;                        // Whole-message retry request flag

    // Constructor
    OperationMetadata()
        : op_type(OpType::SEND)
        , delivery_mode(0)
        , memory({0, false})
        , payload({0, 0, 0})
        , s_pid_on_fep(0)
        , t_pid_on_fep(0)
        , job_id(0)
        , res_index(0)
        , messages_id(0)
        , relative(false)
        , use_optimized_header(false)
        , has_imm_data(false)
        , som(true)
        , eom(true)
        , expect_response(true)
        , is_retry(false)
    {}
};

/**
 * @brief UET Address structure
 */
struct UETAddress
{
    uint8_t version;                      // Address format version
    uint16_t flags;                       // Valid field flag bits

    // Capability identifiers
    struct {
        bool ai_base : 1;                // AI basic profile support
        bool ai_full : 1;                // AI full profile support
        bool hpc : 1;                    // HPC profile support
    } capabilities;

    uint16_t pid_on_fep;                 // Process ID on endpoint
    struct {
        uint64_t low;                    // Low 64 bits of fabric address
        uint64_t high;                   // High 64 bits of fabric address
    } fabric_addr;
    uint16_t start_res_index;            // Starting resource index
    uint16_t num_res_indices;            // Number of resource indices
    uint32_t initiator_id;               // Initiator ID
};

/**
 * @brief Memory region structure
 */
struct MemoryRegion
{
    uint64_t start_addr;                 // Memory region start address
    size_t length;                       // Memory region length
};

/**
 * @brief MSN (Message Sequence Number) entry for tracking
 */
struct MSNEntry
{
    uint64_t last_psn;                   // Last received packet sequence number
    uint64_t expected_len;               // Message expected total length
    uint32_t pdc_id;                     // Associated PDC
};

/**
 * @brief NACK payload structure
 */
struct NackPayload
{
    uint8_t nack_code;                   // NACK type enumeration value
    uint64_t expected_psn;               // Expected PSN (for SEQ_GAP)
    uint32_t current_window;             // Current receive window size
};

//=============================================================================
// Inter-layer Communication Structures
//=============================================================================

/**
 * @brief Request structure from PDC to SES layer
 */
struct PdcSesRequest
{
    uint16_t pdc_id;                     // PDC identifier
    uint16_t rx_pkt_handle;              // Receive packet handle
    Ptr<Packet> packet;                  // Actual packet data
    uint16_t pkt_len;                    // Packet length
    PDSNextHeader next_hdr;              // Next header type
    uint16_t orig_pdcid;                 // Original destination PDCID
    uint32_t orig_psn;                   // Original packet sequence number
};

/**
 * @brief Response structure from PDC to SES layer
 */
struct PdcSesResponse
{
    uint16_t pdc_id;                     // PDC identifier
    uint16_t rx_pkt_handle;              // Receive packet handle
    Ptr<Packet> packet;                  // Response packet data
    uint16_t pkt_len;                    // Packet length
};

/**
 * @brief Request structure from SES layer to PDS
 */
struct SesPdsRequest
{
    uint32_t src_fep;                    // Source FEP
    uint32_t dst_fep;                    // Destination FEP
    uint8_t mode;                        // Transmission mode
    uint16_t rod_context;                // ROD context
    PDSNextHeader next_hdr;              // Next header type
    uint8_t tc;                          // Traffic control category
    bool lock_pdc;                       // PDC lock flag
    uint16_t tx_pkt_handle;              // Transmit packet handle
    Ptr<Packet> packet;                  // Packet data
    uint16_t pkt_len;                    // Packet length
    uint32_t tss_context;                // TSS context
    uint16_t rsv_pdc_context;            // Reserved PDC context
    uint16_t rsv_ccc_context;            // Reserved CCC context
    bool som;                            // Start of Message flag
    bool eom;                            // End of Message flag
};

/**
 * @brief Response structure from SES layer to PDC
 */
struct SesPdsResponse
{
    uint16_t pdc_id;                     // PDC identifier
    uint32_t src_fep;                    // Source FEP
    uint32_t dst_fep;                    // Destination FEP
    uint16_t rx_pkt_handle;              // Receive packet handle
    bool gtd_del;                        // Guaranteed delivery flag
    bool ses_nack;                       // SES layer NACK flag
    NackPayload nack_payload;            // NACK information payload
    Ptr<Packet> packet;                  // Response packet data
    uint16_t rsp_len;                    // Response length
};

enum class RequestTerminalReason : uint8_t
{
    NONE = 0x00,
    RC_OK_RESPONSE = 0x01,
    RC_NO_MATCH_TERMINAL = 0x02,
    RETRY_EXHAUSTED = 0x03,
    INVALID_REQUEST = 0x04,
    PROTOCOL_ERROR = 0x05,
    RESPONSE_TIMEOUT_EXHAUSTED = 0x06,
};

struct RequestTerminalProbe
{
    bool present{false};
    bool retry_present{false};
    bool terminalized{false};
    RequestTerminalReason reason{RequestTerminalReason::NONE};
    int64_t terminalized_at_ms{0};
};

struct RequestTxProbe
{
    bool present{false};
    bool has_tx_pkt_map_entries{false};
    bool has_tx_pkt_buffer_entries{false};
    uint32_t oldest_pending_psn{0};
    uint32_t pending_psn_count{0};
    bool pending_control_only{false};
    bool pending_data_only{false};
    int64_t last_tx_progress_ms{0};
};

struct UnexpectedSendProbe
{
    bool present{false};
    bool semantic_accepted{false};
    bool buffered_complete{false};
    bool matched_to_recv{false};
    uint32_t chunks_done{0};
    uint32_t expected_chunks{0};
    bool completed{false};
    bool failed{false};
    int64_t last_activity_ms{0};
};

struct SendRetryProbe
{
    bool present{false};
    bool waiting_response{false};
    uint16_t retry_count{0};
    int64_t next_retry_ms{0};
    bool timeout_armed{false};
    bool last_trigger_timeout{false};
    bool last_trigger_no_match{false};
    bool exhausted{false};
};

struct RudResourceStats
{
    uint32_t arrival_blocks_in_use{0};
    uint32_t unexpected_msgs_in_use{0};
    uint32_t max_unexpected_msgs{0};
    uint32_t max_unexpected_bytes{0};
    uint32_t max_arrival_blocks{0};
    uint32_t max_read_tracks{0};
    uint32_t unexpected_bytes_in_use{0};
    uint32_t unexpected_alloc_failures{0};
    uint32_t arrival_alloc_failures{0};
    uint32_t read_track_in_use{0};
    uint32_t read_track_alloc_failures{0};
    uint32_t stale_cleanup_count{0};
};

struct RudRuntimeStats
{
    static constexpr std::size_t kMaxFabricPaths = 8;
    uint32_t active_retry_states{0};
    uint32_t active_read_response_states{0};
    uint32_t unexpected_buffered_in_use{0};
    uint32_t unexpected_semantic_accepted_in_use{0};
    uint32_t unexpected_partial_in_use{0};
    uint32_t tpdc_inflight_packets{0};
    uint32_t tpdc_out_of_order_packets{0};
    uint32_t tpdc_pending_sacks{0};
    uint32_t tpdc_pending_gap_nacks{0};
    uint32_t active_tpdc_sessions{0};
    uint32_t credit_refresh_sent{0};
    uint32_t ack_ctrl_ext_sent{0};
    uint32_t credit_gate_blocked{0};
    uint32_t legacy_credit_gate_blocked{0};
    uint32_t send_admission_blocked_total{0};
    uint32_t send_admission_message_limit_blocked_total{0};
    uint32_t send_admission_byte_limit_blocked_total{0};
    uint32_t send_admission_both_limit_blocked_total{0};
    uint32_t write_budget_blocked_total{0};
    uint32_t read_responder_blocked_total{0};
    uint32_t transport_window_blocked_total{0};
    uint32_t send_admission_messages_in_use_peak{0};
    uint64_t send_admission_bytes_in_use_peak{0};
    uint32_t write_budget_messages_in_use_peak{0};
    uint64_t write_budget_bytes_in_use_peak{0};
    uint32_t read_responder_messages_in_use_peak{0};
    uint64_t read_response_budget_bytes_in_use_peak{0};
    uint32_t blocked_queue_push_total{0};
    uint32_t blocked_queue_wakeup_total{0};
    uint32_t blocked_queue_dispatch_total{0};
    uint32_t blocked_queue_depth_max{0};
    uint32_t blocked_queue_wait_recorded_total{0};
    uint64_t blocked_queue_wait_total_ns{0};
    uint64_t blocked_queue_wait_peak_ns{0};
    uint32_t blocked_queue_wakeup_delay_recorded_total{0};
    uint64_t blocked_queue_wakeup_delay_total_ns{0};
    uint64_t blocked_queue_wakeup_delay_peak_ns{0};
    uint32_t send_admission_release_count{0};
    uint64_t send_admission_release_bytes_total{0};
    uint32_t blocked_queue_redispatch_fail_after_wakeup_total{0};
    uint32_t blocked_queue_redispatch_success_after_wakeup_total{0};
    uint32_t peer_queue_blocked_total{0};
    uint32_t pending_response_enqueue_total{0};
    uint32_t pending_response_retry_total{0};
    uint32_t pending_response_dispatch_failures_total{0};
    uint32_t pending_response_success_after_retry_total{0};
    uint32_t stale_timeout_skipped_total{0};
    uint32_t stale_retry_skipped_total{0};
    uint32_t late_response_observed_total{0};
    uint32_t send_duplicate_ok_after_terminal_total{0};
    uint32_t send_dispatch_started_total{0};
    uint32_t send_response_ok_live_total{0};
    uint32_t send_response_nonok_live_total{0};
    uint32_t send_response_missing_live_request_total{0};
    uint32_t send_response_after_terminal_total{0};
    uint32_t send_timeout_without_response_total{0};
    uint32_t send_timeout_retry_without_response_progress_total{0};
    uint32_t sack_sent{0};
    uint32_t gap_nack_sent{0};
    uint32_t fabric_path_count{0};
    uint64_t fabric_total_tx_bytes{0};
    uint64_t fabric_total_tx_packets{0};
    uint64_t fabric_ecn_mark_total{0};
    uint32_t fabric_top_hotspot_endpoint{0};
    uint32_t fabric_dynamic_routing_enabled{0};
    uint32_t fabric_dynamic_assignment_total{0};
    uint32_t fabric_dynamic_path_reuse_total{0};
    uint32_t fabric_active_flow_assignments_peak{0};
    uint64_t fabric_path_score_min_ns{0};
    uint64_t fabric_path_score_max_ns{0};
    uint64_t fabric_path_score_mean_ns{0};
    std::array<uint64_t, kMaxFabricPaths> fabric_path_tx_bytes{};
    std::array<uint32_t, kMaxFabricPaths> fabric_path_queue_depth_max{};
    std::array<uint32_t, kMaxFabricPaths> fabric_path_queue_depth_mean_milli{};
    std::array<uint32_t, kMaxFabricPaths> fabric_path_flow_count{};
};

struct TpdcSessionProgressRecord
{
    uint64_t timestamp_ns{0};
    uint32_t node_id{0};
    uint32_t if_index{0};
    uint32_t local_fep{0};
    uint32_t remote_fep{0};
    uint16_t pdc_id{0};
    uint32_t tx_data_packets_total{0};
    uint32_t tx_control_packets_total{0};
    uint32_t rx_data_packets_total{0};
    uint32_t rx_control_packets_total{0};
    uint32_t ack_sent_total{0};
    uint32_t ack_received_total{0};
    uint32_t gap_nack_received_total{0};
    uint32_t last_ack_sequence{0};
    uint32_t send_window_base{0};
    uint32_t next_send_sequence{0};
    uint32_t send_buffer_size_current{0};
    uint32_t send_buffer_size_max{0};
    uint32_t retransmissions_total{0};
    uint32_t rto_timeouts_total{0};
    uint32_t ack_advance_events_total{0};
};

struct ResponseEvent
{
    uint64_t job_id{0};
    uint16_t msg_id{0};
    uint8_t opcode{0};
    uint8_t return_code{0};
    uint32_t modified_length{0};
    int64_t observed_at_ms{0};
};

struct SoftUeStats
{
    uint64_t totalBytesReceived{0};
    uint64_t totalBytesTransmitted{0};
    uint64_t totalPacketsReceived{0};
    uint64_t totalPacketsTransmitted{0};
    uint64_t droppedPackets{0};
    uint64_t activePdcCount{0};
    double averageLatency{0.0};
    double throughput{0.0};
    int64_t lastActivityNs{0};
};

struct SoftUeSemanticStats
{
    uint64_t ops_started_total{0};
    uint64_t ops_terminal_total{0};
    uint64_t ops_success_total{0};
    uint64_t ops_failed_total{0};
    uint64_t ops_in_flight{0};
    uint64_t send_started_total{0};
    uint64_t write_started_total{0};
    uint64_t read_started_total{0};
    uint64_t send_success_total{0};
    uint64_t write_success_total{0};
    uint64_t read_success_total{0};
    uint64_t send_success_bytes_total{0};
    uint64_t write_success_bytes_total{0};
    uint64_t read_success_bytes_total{0};
    uint64_t success_latency_samples{0};
    uint64_t success_latency_mean_ns{0};
    uint64_t success_latency_max_ns{0};
};

struct SoftUeDiagnosticRecord
{
    uint64_t timestamp_ns{0};
    uint32_t node_id{0};
    uint32_t if_index{0};
    uint32_t local_fep{0};
    std::string name;
    std::string detail;
};

struct SoftUeCompletionRecord
{
    uint64_t timestamp_ns{0};
    uint32_t node_id{0};
    uint32_t if_index{0};
    uint32_t local_fep{0};
    uint64_t job_id{0};
    uint16_t msg_id{0};
    uint32_t peer_fep{0};
    OpType opcode{OpType::SEND};
    bool success{false};
    uint8_t return_code{static_cast<uint8_t> (ResponseReturnCode::RC_OK)};
    std::string failure_domain;
    RequestTerminalReason terminal_reason{RequestTerminalReason::NONE};
    uint32_t payload_bytes{0};
    uint64_t latency_ns{0};
    uint16_t retry_count{0};
    bool send_stage_valid{false};
    uint64_t send_stage_dispatch_ns{0};
    uint64_t send_stage_dispatch_wait_for_admission_ns{0};
    uint64_t send_stage_dispatch_after_admission_to_first_send_ns{0};
    uint64_t send_stage_inflight_ns{0};
    uint64_t send_stage_receive_consume_ns{0};
    uint64_t send_stage_closeout_ns{0};
    uint64_t send_stage_end_to_end_ns{0};
    uint32_t send_stage_dispatch_attempt_count{0};
    uint32_t send_stage_dispatch_budget_block_count{0};
    uint32_t send_stage_blocked_queue_enqueue_count{0};
    uint32_t send_stage_blocked_queue_redispatch_count{0};
    uint64_t send_stage_blocked_queue_wait_total_ns{0};
    uint64_t send_stage_blocked_queue_wait_peak_ns{0};
    uint32_t send_stage_admission_release_seen_count{0};
    uint32_t send_stage_blocked_queue_wakeup_count{0};
    uint32_t send_stage_redispatch_fail_after_wakeup_count{0};
    uint32_t send_stage_redispatch_success_after_wakeup_count{0};
    uint64_t send_stage_admission_release_to_wakeup_total_ns{0};
    uint64_t send_stage_admission_release_to_wakeup_peak_ns{0};
    uint64_t send_stage_wakeup_to_redispatch_total_ns{0};
    uint64_t send_stage_wakeup_to_redispatch_peak_ns{0};
    bool read_stage_valid{false};
    uint64_t read_stage_responder_budget_generate_ns{0};
    uint64_t read_stage_pending_response_queue_dispatch_ns{0};
    uint64_t read_stage_response_first_packet_tx_ns{0};
    uint64_t read_stage_network_return_visibility_ns{0};
    uint64_t read_stage_network_queue_serialization_ns{0};
    uint64_t read_stage_tpdc_transport_send_serialization_ns{0};
    uint64_t read_stage_return_path_data_fragment_delay_ns{0};
    bool read_stage_tpdc_send_window_queued{false};
    uint64_t read_stage_network_reorder_hold_ns{0};
    uint64_t read_stage_requester_visibility_ns{0};
    uint64_t read_stage_first_response_visible_ns{0};
    uint64_t read_stage_reassembly_complete_ns{0};
    uint64_t read_stage_terminal_ns{0};
    uint64_t read_stage_end_to_end_ns{0};
    bool read_pre_admission_tracked{false};
    bool read_pre_context_allocated_retry{false};
    bool read_pre_terminal_resource_exhaust{false};
    uint32_t read_pre_first_packet_no_context_count{0};
    uint32_t read_pre_arrival_reserve_fail_count{0};
    uint64_t read_pre_target_registered_at_ns{0};
    uint64_t read_pre_first_packet_no_context_at_ns{0};
    uint64_t read_pre_arrival_block_reserved_at_ns{0};
    uint64_t read_pre_context_allocated_retry_at_ns{0};
    uint64_t read_pre_target_released_at_ns{0};
    uint64_t read_pre_arrival_context_released_at_ns{0};
    uint64_t read_pre_target_to_first_no_context_ns{0};
    uint64_t read_pre_target_to_arrival_reserved_ns{0};
    uint64_t read_pre_first_no_context_to_arrival_reserved_ns{0};
    uint64_t read_pre_arrival_reserved_to_first_response_visible_ns{0};
    uint64_t read_pre_target_hold_to_release_ns{0};
    uint64_t read_pre_arrival_hold_to_release_ns{0};
    uint64_t read_pre_target_to_terminal_resource_exhaust_ns{0};
    uint64_t read_pre_first_no_context_to_terminal_resource_exhaust_ns{0};
    bool read_recovery_valid{false};
    bool read_recovery_tracked{false};
    uint32_t read_recovery_missing_chunk_index{0};
    uint32_t read_recovery_missing_transport_seq{0};
    uint64_t read_recovery_gap_detected_at_ns{0};
    uint64_t read_recovery_gap_nack_sent_at_ns{0};
    uint64_t read_recovery_gap_nack_observed_at_sender_ns{0};
    uint64_t read_recovery_missing_fragment_retransmit_tx_at_ns{0};
    uint64_t read_recovery_missing_fragment_first_visible_at_ns{0};
    uint64_t read_recovery_reassembly_unblocked_at_ns{0};
    uint32_t read_recovery_gap_nack_sent_count{0};
    uint32_t read_recovery_gap_nack_observed_at_sender_count{0};
    uint32_t read_recovery_missing_fragment_retransmit_count{0};
    uint64_t read_recovery_detect_to_nack_ns{0};
    uint64_t read_recovery_nack_to_observed_at_sender_ns{0};
    uint64_t read_recovery_observed_at_sender_to_retransmit_ns{0};
    uint64_t read_recovery_nack_to_retransmit_ns{0};
    uint64_t read_recovery_retransmit_to_first_visible_ns{0};
    uint64_t read_recovery_first_visible_to_unblocked_ns{0};
    uint64_t read_recovery_detect_to_unblocked_ns{0};
    uint64_t read_recovery_retransmit_to_terminal_ns{0};
    uint64_t read_recovery_detect_to_terminal_ns{0};
};

struct SoftUeProtocolSnapshot
{
    uint64_t timestamp_ns{0};
    uint32_t node_id{0};
    uint32_t if_index{0};
    uint32_t local_fep{0};
    SoftUeStats device_stats;
    SoftUeSemanticStats semantic_stats;
    RudResourceStats resource_stats;
    RudRuntimeStats runtime_stats;
};

struct SoftUeFailureSnapshot
{
    uint64_t timestamp_ns{0};
    uint32_t node_id{0};
    uint32_t if_index{0};
    uint32_t local_fep{0};
    uint64_t job_id{0};
    uint16_t msg_id{0};
    uint32_t peer_fep{0};
    OpType opcode{OpType::SEND};
    std::string stage;
    std::string failure_domain;
    uint8_t return_code{static_cast<uint8_t> (ResponseReturnCode::RC_OK)};
    RequestTerminalProbe terminal_probe;
    RequestTxProbe tx_probe;
    UnexpectedSendProbe unexpected_probe;
    SendRetryProbe retry_probe;
    RudResourceStats resource_stats;
    RudRuntimeStats runtime_stats;
    std::string diagnostic_text;
};

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert operation type to string
 * @param op The operation type
 * @return String representation
 */
inline std::string OperationTypeToString(OpType op)
{
    switch (op) {
        case OpType::SEND: return "SEND";
        case OpType::READ: return "READ";
        case OpType::WRITE: return "WRITE";
        case OpType::DEFERRABLE: return "DEFERRABLE";
        default: return "UNKNOWN";
    }
}

inline std::string RequestTerminalReasonToString(RequestTerminalReason reason)
{
    switch (reason) {
        case RequestTerminalReason::NONE: return "NONE";
        case RequestTerminalReason::RC_OK_RESPONSE: return "RC_OK_RESPONSE";
        case RequestTerminalReason::RC_NO_MATCH_TERMINAL: return "RC_NO_MATCH_TERMINAL";
        case RequestTerminalReason::RETRY_EXHAUSTED: return "RETRY_EXHAUSTED";
        case RequestTerminalReason::INVALID_REQUEST: return "INVALID_REQUEST";
        case RequestTerminalReason::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
        case RequestTerminalReason::RESPONSE_TIMEOUT_EXHAUSTED: return "RESPONSE_TIMEOUT_EXHAUSTED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert PDC type to string
 * @param type The PDC type
 * @return String representation
 */
inline std::string PdcTypeToString(PdcType type)
{
    switch (type) {
        case PdcType::IPDC: return "IPDC";
        case PdcType::TPDC: return "TPDC";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Check if PDC ID is IPDC type
 * @param pdc_id The PDC ID to check
 * @return true if IPDC type
 */
inline bool IsIpdc(uint16_t pdc_id)
{
    return pdc_id < MAX_PDC;
}

/**
 * @brief Check if PDC ID is TPDC type
 * @param pdc_id The PDC ID to check
 * @return true if TPDC type
 */
inline bool IsTpdc(uint16_t pdc_id)
{
    return pdc_id >= MAX_PDC;
}

/**
 * @brief Get PDC type from ID
 * @param pdc_id The PDC ID
 * @return PDC type
 */
inline PdcType GetPdcType(uint16_t pdc_id)
{
    return IsIpdc(pdc_id) ? PdcType::IPDC : PdcType::TPDC;
}

} // namespace ns3

#endif /* SOFT_UE_TRANSPORT_LAYER_H */
