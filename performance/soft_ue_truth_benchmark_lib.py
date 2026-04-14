#!/usr/bin/env python3

from __future__ import annotations

import csv
import json
import re
import shutil
import statistics
import subprocess
from collections import defaultdict
from copy import deepcopy
from datetime import datetime, timezone
from itertools import product
from pathlib import Path
from typing import Any

RUNNER_VERSION = "phase4.26-extreme-throughput-ladder-v1"
DEFAULT_PAYLOAD_MTU_BYTES = 2048
HEADLINE_KPI_NAME = "semantic_goodput_mbps"
HEADLINE_KPI_CLASS = "performance_conclusion"
DEVICE_TX_EGRESS_KPI_CLASS = "transport_efficiency_kpi"
RX_DIAGNOSTIC_KPI_CLASS = "diagnostic_only"
REQUIRED_SYSTEM_SCENARIO_MODES = {"soft_ue_truth", "soft_ue_fabric"}
REQUIRED_TRUTH_EXPERIMENT_CLASS = "system_pressure"
REQUIRED_TRUTH_PARAMS = {
    "truthExperimentClass": REQUIRED_TRUTH_EXPERIMENT_CLASS,
    "enableSoftUeObserver": True,
    "enableSoftUeProtocolLogging": True,
    "truthProtocolCsvRequired": True,
}
OPCODE_PREFIXES = {
    "send": "send",
    "write": "write",
    "read": "read",
}

FAILURE_DOMAINS = [
    "validation",
    "resource",
    "control_plane",
    "packet_reliability",
    "message_lifecycle",
]

KPI_GROUPS = {
    "semantic_goodput_mbps": "performance_conclusion",
    "semantic_completion_rate_pct": "performance_conclusion",
    "terminalization_rate_pct": "performance_conclusion",
    "success_latency_p50_us": "performance_conclusion",
    "success_latency_p95_us": "performance_conclusion",
    "success_latency_p99_us": "performance_conclusion",
    "device_tx_egress_mbps": "transport_efficiency_kpi",
    "device_rx_observed_mbps": "diagnostic_only",
    "credit_gate_blocked_total": "pressure_evidence.control_plane",
    "legacy_credit_gate_blocked_total": "pressure_evidence.control_plane",
    "send_admission_blocked_total": "pressure_evidence.control_plane",
    "send_admission_message_limit_blocked_total": "pressure_evidence.control_plane",
    "send_admission_byte_limit_blocked_total": "pressure_evidence.control_plane",
    "send_admission_both_limit_blocked_total": "pressure_evidence.control_plane",
    "write_budget_blocked_total": "pressure_evidence.control_plane",
    "read_responder_blocked_total": "pressure_evidence.control_plane",
    "transport_window_blocked_total": "pressure_evidence.control_plane",
    "credit_refresh_sent_total": "pressure_evidence.control_plane",
    "ack_ctrl_ext_sent_total": "pressure_evidence.control_plane",
    "unexpected_alloc_failures_total": "pressure_evidence.resource",
    "arrival_alloc_failures_total": "pressure_evidence.resource",
    "read_track_alloc_failures_total": "pressure_evidence.resource",
    "peak_tpdc_inflight_packets": "pressure_evidence.packet_reliability",
    "peak_tpdc_pending_sacks": "pressure_evidence.packet_reliability",
    "peak_tpdc_pending_gap_nacks": "pressure_evidence.packet_reliability",
    "sack_sent_total": "pressure_evidence.packet_reliability",
    "gap_nack_sent_total": "pressure_evidence.packet_reliability",
}

PROTOCOL_CUMULATIVE_FIELDS = {
    "TxBytes": "tx_bytes_total",
    "RxBytes": "rx_bytes_total",
    "TxPackets": "tx_packets_total",
    "RxPackets": "rx_packets_total",
    "DroppedPackets": "dropped_packets_total",
    "OpsStartedTotal": "ops_started_total",
    "OpsTerminalTotal": "ops_terminal_total",
    "OpsSuccessTotal": "ops_success_total",
    "OpsFailedTotal": "ops_failed_total",
    "SendSuccessTotal": "send_success_total",
    "WriteSuccessTotal": "write_success_total",
    "ReadSuccessTotal": "read_success_total",
    "SendSuccessBytesTotal": "send_success_bytes_total",
    "WriteSuccessBytesTotal": "write_success_bytes_total",
    "ReadSuccessBytesTotal": "read_success_bytes_total",
    "UnexpectedAllocFailures": "unexpected_alloc_failures_total",
    "ArrivalAllocFailures": "arrival_alloc_failures_total",
    "ReadTrackAllocFailures": "read_track_alloc_failures_total",
    "StaleCleanupCount": "stale_cleanup_total",
    "CreditGateBlocked": "credit_gate_blocked_total",
    "LegacyCreditGateBlocked": "legacy_credit_gate_blocked_total",
    "SendAdmissionBlockedTotal": "send_admission_blocked_total",
    "SendAdmissionMessageLimitBlockedTotal": "send_admission_message_limit_blocked_total",
    "SendAdmissionByteLimitBlockedTotal": "send_admission_byte_limit_blocked_total",
    "SendAdmissionBothLimitBlockedTotal": "send_admission_both_limit_blocked_total",
    "WriteBudgetBlockedTotal": "write_budget_blocked_total",
    "ReadResponderBlockedTotal": "read_responder_blocked_total",
    "TransportWindowBlockedTotal": "transport_window_blocked_total",
    "SendAdmissionMessagesInUsePeak": "send_admission_messages_in_use_peak",
    "SendAdmissionBytesInUsePeak": "send_admission_bytes_in_use_peak",
    "WriteBudgetMessagesInUsePeak": "write_budget_messages_in_use_peak",
    "WriteBudgetBytesInUsePeak": "write_budget_bytes_in_use_peak",
    "ReadResponderMessagesInUsePeak": "read_responder_messages_in_use_peak",
    "ReadResponseBudgetBytesInUsePeak": "read_response_budget_bytes_in_use_peak",
    "BlockedQueuePushTotal": "blocked_queue_push_total",
    "BlockedQueueWakeupTotal": "blocked_queue_wakeup_total",
    "BlockedQueueDispatchTotal": "blocked_queue_dispatch_total",
    "BlockedQueueWaitRecordedTotal": "blocked_queue_wait_recorded_total",
    "BlockedQueueWaitTotalNs": "blocked_queue_wait_total_ns",
    "BlockedQueueWaitPeakNs": "blocked_queue_wait_peak_ns",
    "BlockedQueueWakeupDelayRecordedTotal": "blocked_queue_wakeup_delay_recorded_total",
    "BlockedQueueWakeupDelayTotalNs": "blocked_queue_wakeup_delay_total_ns",
    "BlockedQueueWakeupDelayPeakNs": "blocked_queue_wakeup_delay_peak_ns",
    "SendAdmissionReleaseCount": "send_admission_release_count",
    "SendAdmissionReleaseBytesTotal": "send_admission_release_bytes_total",
    "BlockedQueueRedispatchFailAfterWakeupTotal": "blocked_queue_redispatch_fail_after_wakeup_total",
    "BlockedQueueRedispatchSuccessAfterWakeupTotal": "blocked_queue_redispatch_success_after_wakeup_total",
    "PeerQueueBlockedTotal": "peer_queue_blocked_total",
    "PendingResponseEnqueueTotal": "pending_response_enqueue_total",
    "PendingResponseRetryTotal": "pending_response_retry_total",
    "PendingResponseDispatchFailuresTotal": "pending_response_dispatch_failures_total",
    "PendingResponseSuccessAfterRetryTotal": "pending_response_success_after_retry_total",
    "StaleTimeoutSkippedTotal": "stale_timeout_skipped_total",
    "StaleRetrySkippedTotal": "stale_retry_skipped_total",
    "LateResponseObservedTotal": "late_response_observed_total",
    "SendDuplicateOkAfterTerminalTotal": "send_duplicate_ok_after_terminal_total",
    "SendDispatchStartedTotal": "send_dispatch_started_total",
    "SendResponseOkLiveTotal": "send_response_ok_live_total",
    "SendResponseNonOkLiveTotal": "send_response_nonok_live_total",
    "SendResponseMissingLiveRequestTotal": "send_response_missing_live_request_total",
    "SendResponseAfterTerminalTotal": "send_response_after_terminal_total",
    "SendTimeoutWithoutResponseTotal": "send_timeout_without_response_total",
    "SendTimeoutRetryWithoutResponseProgressTotal": "send_timeout_retry_without_response_progress_total",
    "CreditRefreshSent": "credit_refresh_sent_total",
    "AckCtrlExtSent": "ack_ctrl_ext_sent_total",
    "SackSent": "sack_sent_total",
    "GapNackSent": "gap_nack_sent_total",
    "FabricPathCount": "fabric_path_count",
    "FabricTotalTxBytes": "fabric_total_tx_bytes",
    "FabricTotalTxPackets": "fabric_total_tx_packets",
    "FabricEcnMarkTotal": "fabric_ecn_mark_total",
    "FabricTopHotspotEndpoint": "fabric_top_hotspot_endpoint",
    "FabricDynamicRoutingEnabled": "fabric_dynamic_routing_enabled",
    "FabricDynamicAssignmentTotal": "fabric_dynamic_assignment_total",
    "FabricDynamicPathReuseTotal": "fabric_dynamic_path_reuse_total",
    "FabricActiveFlowAssignmentsPeak": "fabric_active_flow_assignments_peak",
    "FabricPathScoreMinNs": "fabric_path_score_min_ns",
    "FabricPathScoreMaxNs": "fabric_path_score_max_ns",
    "FabricPathScoreMeanNs": "fabric_path_score_mean_ns",
    "FabricPath0TxBytes": "fabric_path0_tx_bytes",
    "FabricPath1TxBytes": "fabric_path1_tx_bytes",
    "FabricPath2TxBytes": "fabric_path2_tx_bytes",
    "FabricPath3TxBytes": "fabric_path3_tx_bytes",
    "FabricPath4TxBytes": "fabric_path4_tx_bytes",
    "FabricPath5TxBytes": "fabric_path5_tx_bytes",
    "FabricPath6TxBytes": "fabric_path6_tx_bytes",
    "FabricPath7TxBytes": "fabric_path7_tx_bytes",
    "FabricPath0FlowCount": "fabric_path0_flow_count",
    "FabricPath1FlowCount": "fabric_path1_flow_count",
    "FabricPath2FlowCount": "fabric_path2_flow_count",
    "FabricPath3FlowCount": "fabric_path3_flow_count",
    "FabricPath4FlowCount": "fabric_path4_flow_count",
    "FabricPath5FlowCount": "fabric_path5_flow_count",
    "FabricPath6FlowCount": "fabric_path6_flow_count",
    "FabricPath7FlowCount": "fabric_path7_flow_count",
}

PROTOCOL_PEAK_FIELDS = {
    "OpsInFlight": "peak_ops_in_flight",
    "ArrivalBlocksInUse": "peak_arrival_blocks_in_use",
    "UnexpectedMsgsInUse": "peak_unexpected_msgs_in_use",
    "UnexpectedBytesInUse": "peak_unexpected_bytes_in_use",
    "ReadTrackInUse": "peak_read_track_in_use",
    "TpdcInflightPackets": "peak_tpdc_inflight_packets",
    "TpdcPendingSacks": "peak_tpdc_pending_sacks",
    "TpdcPendingGapNacks": "peak_tpdc_pending_gap_nacks",
    "BlockedQueueDepthMax": "blocked_queue_depth_max",
    "FabricPath0QueueDepthMax": "fabric_path0_queue_depth_max",
    "FabricPath1QueueDepthMax": "fabric_path1_queue_depth_max",
    "FabricPath2QueueDepthMax": "fabric_path2_queue_depth_max",
    "FabricPath3QueueDepthMax": "fabric_path3_queue_depth_max",
    "FabricPath4QueueDepthMax": "fabric_path4_queue_depth_max",
    "FabricPath5QueueDepthMax": "fabric_path5_queue_depth_max",
    "FabricPath6QueueDepthMax": "fabric_path6_queue_depth_max",
    "FabricPath7QueueDepthMax": "fabric_path7_queue_depth_max",
}

RUN_SUMMARY_FIELDS = [
    "run_id",
    "status",
    "variant_name",
    "workload_name",
    "tuning_name",
    "timeout_budget_name",
    "timeout_budget_ms",
    "resource_tier_name",
    "truthPressureProfile",
    "truthFlowCount",
    "truthPayloadBytes",
    "device_mtu_bytes",
    "payload_mtu_bytes",
    "repetition",
    "nXpus",
    "portsPerXpu",
    "truthOpsPerFlow",
    "truth_link_data_rate",
    "fabric_topology_mode",
    "fabric_endpoint_mode",
    "fabric_path_count_cfg",
    "fabric_path_data_rate",
    "fabric_traffic_pattern",
    "fabric_dynamic_routing_enabled_cfg",
    "truth_op_spacing_ns",
    "simulationTime",
    "drain_window_seconds",
    "headline_kpi_name",
    "headline_kpi_class",
    "device_tx_egress_kpi_class",
    "device_rx_observed_kpi_class",
    "protocol_rows",
    "completion_rows",
    "completion_success_rows",
    "failure_rows",
    "diagnostic_rows",
    "status_reason",
    "ops_started_total",
    "ops_terminal_total",
    "ops_success_total",
    "ops_failed_total",
    "send_success_total",
    "write_success_total",
    "read_success_total",
    "tx_bytes_total",
    "rx_bytes_total",
    "tx_packets_total",
    "rx_packets_total",
    "dropped_packets_total",
    "semantic_goodput_mbps",
    "injection_ceiling_mbps",
    "semantic_goodput_utilization_pct",
    "semantic_completion_rate_pct",
    "terminalization_rate_pct",
    "ops_terminal_total_final",
    "ops_success_total_final",
    "ops_failed_total_final",
    "semantic_completion_rate_pct_final",
    "terminalization_rate_pct_final",
    "drain_completed",
    "drain_timeout_ns",
    "final_ops_in_flight",
    "final_active_retry_states",
    "final_active_read_response_states",
    "final_read_track_in_use",
    "final_tpdc_pending_sacks",
    "final_tpdc_pending_gap_nacks",
    "success_latency_p50_us",
    "success_latency_p95_us",
    "success_latency_p99_us",
    "send_stage_sample_count",
    "send_stage_dispatch_p50_ns",
    "send_stage_dispatch_p95_ns",
    "send_stage_dispatch_p99_ns",
    "send_stage_dispatch_wait_for_admission_p50_ns",
    "send_stage_dispatch_wait_for_admission_p95_ns",
    "send_stage_dispatch_wait_for_admission_p99_ns",
    "send_stage_dispatch_after_admission_to_first_send_p50_ns",
    "send_stage_dispatch_after_admission_to_first_send_p95_ns",
    "send_stage_dispatch_after_admission_to_first_send_p99_ns",
    "send_stage_dispatch_attempt_count_mean",
    "send_stage_dispatch_attempt_count_p95",
    "send_stage_dispatch_budget_block_count_mean",
    "send_stage_dispatch_budget_block_count_p95",
    "send_stage_blocked_queue_enqueue_count_mean",
    "send_stage_blocked_queue_enqueue_count_p95",
    "send_stage_blocked_queue_redispatch_count_mean",
    "send_stage_blocked_queue_redispatch_count_p95",
    "send_stage_blocked_queue_wait_total_p50_ns",
    "send_stage_blocked_queue_wait_total_p95_ns",
    "send_stage_blocked_queue_wait_peak_p50_ns",
    "send_stage_blocked_queue_wait_peak_p95_ns",
    "send_stage_admission_release_seen_count_mean",
    "send_stage_admission_release_seen_count_p95",
    "send_stage_blocked_queue_wakeup_count_mean",
    "send_stage_blocked_queue_wakeup_count_p95",
    "send_stage_redispatch_fail_after_wakeup_count_mean",
    "send_stage_redispatch_fail_after_wakeup_count_p95",
    "send_stage_redispatch_success_after_wakeup_count_mean",
    "send_stage_redispatch_success_after_wakeup_count_p95",
    "send_stage_admission_release_to_wakeup_total_p50_ns",
    "send_stage_admission_release_to_wakeup_total_p95_ns",
    "send_stage_admission_release_to_wakeup_peak_p50_ns",
    "send_stage_admission_release_to_wakeup_peak_p95_ns",
    "send_stage_wakeup_to_redispatch_total_p50_ns",
    "send_stage_wakeup_to_redispatch_total_p95_ns",
    "send_stage_wakeup_to_redispatch_peak_p50_ns",
    "send_stage_wakeup_to_redispatch_peak_p95_ns",
    "send_stage_inflight_p50_ns",
    "send_stage_inflight_p95_ns",
    "send_stage_inflight_p99_ns",
    "send_stage_receive_consume_p50_ns",
    "send_stage_receive_consume_p95_ns",
    "send_stage_receive_consume_p99_ns",
    "send_stage_closeout_p50_ns",
    "send_stage_closeout_p95_ns",
    "send_stage_closeout_p99_ns",
    "send_stage_end_to_end_p50_ns",
    "send_stage_end_to_end_p95_ns",
    "send_stage_end_to_end_p99_ns",
    "send_stage_dispatch_mean_ns",
    "send_stage_inflight_mean_ns",
    "send_stage_receive_consume_mean_ns",
    "send_stage_closeout_mean_ns",
    "send_stage_end_to_end_mean_ns",
    "read_stage_sample_count",
    "read_stage_responder_budget_generate_p50_ns",
    "read_stage_responder_budget_generate_p95_ns",
    "read_stage_responder_budget_generate_p99_ns",
    "read_stage_pending_response_queue_dispatch_p50_ns",
    "read_stage_pending_response_queue_dispatch_p95_ns",
    "read_stage_pending_response_queue_dispatch_p99_ns",
    "read_stage_response_first_packet_tx_p50_ns",
    "read_stage_response_first_packet_tx_p95_ns",
    "read_stage_response_first_packet_tx_p99_ns",
    "read_stage_network_return_visibility_p50_ns",
    "read_stage_network_return_visibility_p95_ns",
    "read_stage_network_return_visibility_p99_ns",
    "read_stage_first_response_visible_p50_ns",
    "read_stage_first_response_visible_p95_ns",
    "read_stage_first_response_visible_p99_ns",
    "read_stage_reassembly_complete_p50_ns",
    "read_stage_reassembly_complete_p95_ns",
    "read_stage_reassembly_complete_p99_ns",
    "read_stage_terminal_p50_ns",
    "read_stage_terminal_p95_ns",
    "read_stage_terminal_p99_ns",
    "read_stage_end_to_end_p50_ns",
    "read_stage_end_to_end_p95_ns",
    "read_stage_end_to_end_p99_ns",
    "read_stage_responder_budget_generate_mean_ns",
    "read_stage_pending_response_queue_dispatch_mean_ns",
    "read_stage_response_first_packet_tx_mean_ns",
    "read_stage_network_return_visibility_mean_ns",
    "read_stage_first_response_visible_mean_ns",
    "read_stage_reassembly_complete_mean_ns",
    "read_stage_terminal_mean_ns",
    "read_stage_end_to_end_mean_ns",
    "read_pre_admission_sample_count",
    "read_pre_context_allocated_retry_count",
    "read_pre_terminal_resource_exhaust_count",
    "read_pre_first_packet_no_context_count_total",
    "read_pre_arrival_reserve_fail_count_total",
    "read_pre_target_to_first_no_context_p50_ns",
    "read_pre_target_to_first_no_context_p95_ns",
    "read_pre_target_to_arrival_reserved_p50_ns",
    "read_pre_target_to_arrival_reserved_p95_ns",
    "read_pre_first_no_context_to_arrival_reserved_p50_ns",
    "read_pre_first_no_context_to_arrival_reserved_p95_ns",
    "read_pre_arrival_reserved_to_first_response_visible_p50_ns",
    "read_pre_arrival_reserved_to_first_response_visible_p95_ns",
    "read_pre_target_hold_to_release_p50_ns",
    "read_pre_target_hold_to_release_p95_ns",
    "read_pre_arrival_hold_to_release_p50_ns",
    "read_pre_arrival_hold_to_release_p95_ns",
    "read_pre_target_to_terminal_resource_exhaust_p50_ns",
    "read_pre_target_to_terminal_resource_exhaust_p95_ns",
    "read_pre_first_no_context_to_terminal_resource_exhaust_p50_ns",
    "read_pre_first_no_context_to_terminal_resource_exhaust_p95_ns",
    "read_pre_failure_target_registered_only_count",
    "read_pre_failure_first_packet_no_context_only_count",
    "read_pre_failure_arrival_reserved_before_visible_count",
    "read_pre_failure_context_allocated_retry_count",
    "read_pre_failure_dominant_stage",
    "read_recovery_sample_count",
    "read_recovery_detect_to_nack_p50_ns",
    "read_recovery_detect_to_nack_p95_ns",
    "read_recovery_detect_to_nack_p99_ns",
    "read_recovery_nack_to_retransmit_p50_ns",
    "read_recovery_nack_to_retransmit_p95_ns",
    "read_recovery_nack_to_retransmit_p99_ns",
    "read_recovery_retransmit_to_first_visible_p50_ns",
    "read_recovery_retransmit_to_first_visible_p95_ns",
    "read_recovery_retransmit_to_first_visible_p99_ns",
    "read_recovery_first_visible_to_unblocked_p50_ns",
    "read_recovery_first_visible_to_unblocked_p95_ns",
    "read_recovery_first_visible_to_unblocked_p99_ns",
    "read_recovery_detect_to_unblocked_p50_ns",
    "read_recovery_detect_to_unblocked_p95_ns",
    "read_recovery_detect_to_unblocked_p99_ns",
    "read_recovery_detect_to_nack_mean_ns",
    "read_recovery_nack_to_retransmit_mean_ns",
    "read_recovery_retransmit_to_first_visible_mean_ns",
    "read_recovery_first_visible_to_unblocked_mean_ns",
    "read_recovery_detect_to_unblocked_mean_ns",
    "read_recovery_tracked_count",
    "read_recovery_failure_tracked_count",
    "read_recovery_failure_gap_detect_only_count",
    "read_recovery_failure_nack_sent_only_count",
    "read_recovery_failure_nack_observed_only_count",
    "read_recovery_failure_retransmit_only_count",
    "read_recovery_failure_first_visible_only_count",
    "read_recovery_failure_reassembly_before_terminal_count",
    "read_recovery_failure_detect_to_nack_p50_ns",
    "read_recovery_failure_detect_to_nack_p95_ns",
    "read_recovery_failure_nack_to_observed_at_sender_p50_ns",
    "read_recovery_failure_nack_to_observed_at_sender_p95_ns",
    "read_recovery_failure_observed_at_sender_to_retransmit_p50_ns",
    "read_recovery_failure_observed_at_sender_to_retransmit_p95_ns",
    "read_recovery_failure_retransmit_to_terminal_p50_ns",
    "read_recovery_failure_retransmit_to_terminal_p95_ns",
    "read_recovery_failure_detect_to_terminal_p50_ns",
    "read_recovery_failure_detect_to_terminal_p95_ns",
    "read_recovery_failure_dominant_stage",
    "device_tx_egress_mbps",
    "device_rx_observed_mbps",
    "send_success_completions_total",
    "write_success_completions_total",
    "read_success_completions_total",
    "send_success_payload_bytes_total",
    "write_success_payload_bytes_total",
    "read_success_payload_bytes_total",
    "send_semantic_goodput_mbps",
    "write_semantic_goodput_mbps",
    "read_semantic_goodput_mbps",
    "unexpected_alloc_failures_total",
    "arrival_alloc_failures_total",
    "read_track_alloc_failures_total",
    "stale_cleanup_total",
    "credit_gate_blocked_total",
    "blocked_queue_push_total",
    "blocked_queue_wakeup_total",
    "blocked_queue_dispatch_total",
    "blocked_queue_depth_max",
    "send_admission_release_count",
    "send_admission_release_bytes_total",
    "blocked_queue_redispatch_fail_after_wakeup_total",
    "blocked_queue_redispatch_success_after_wakeup_total",
    "peer_queue_blocked_total",
    "pending_response_enqueue_total",
    "pending_response_retry_total",
    "pending_response_dispatch_failures_total",
    "pending_response_success_after_retry_total",
    "stale_timeout_skipped_total",
    "stale_retry_skipped_total",
    "late_response_observed_total",
    "send_duplicate_ok_after_terminal_total",
    "send_dispatch_started_total",
    "send_response_ok_live_total",
    "send_response_nonok_live_total",
    "send_response_missing_live_request_total",
    "send_response_after_terminal_total",
    "send_timeout_without_response_total",
    "send_timeout_retry_without_response_progress_total",
    "credit_refresh_sent_total",
    "ack_ctrl_ext_sent_total",
    "sack_sent_total",
    "gap_nack_sent_total",
    "fabric_path_count",
    "fabric_total_tx_bytes",
    "fabric_total_tx_packets",
    "fabric_ecn_mark_total",
    "fabric_top_hotspot_endpoint",
    "fabric_dynamic_routing_enabled",
    "fabric_dynamic_assignment_total",
    "fabric_dynamic_path_reuse_total",
    "fabric_active_flow_assignments_peak",
    "fabric_path_score_min_ns",
    "fabric_path_score_max_ns",
    "fabric_path_score_mean_ns",
    "fabric_total_offered_mbps",
    "fabric_total_goodput_mbps",
    "fabric_total_capacity_gbps",
    "fabric_utilization_pct",
    "semantic_link_utilization_pct",
    "fabric_path_skew_ratio",
    "fabric_path0_tx_bytes",
    "fabric_path1_tx_bytes",
    "fabric_path2_tx_bytes",
    "fabric_path3_tx_bytes",
    "fabric_path4_tx_bytes",
    "fabric_path5_tx_bytes",
    "fabric_path6_tx_bytes",
    "fabric_path7_tx_bytes",
    "fabric_path0_queue_depth_max",
    "fabric_path1_queue_depth_max",
    "fabric_path2_queue_depth_max",
    "fabric_path3_queue_depth_max",
    "fabric_path4_queue_depth_max",
    "fabric_path5_queue_depth_max",
    "fabric_path6_queue_depth_max",
    "fabric_path7_queue_depth_max",
    "fabric_path0_flow_count",
    "fabric_path1_flow_count",
    "fabric_path2_flow_count",
    "fabric_path3_flow_count",
    "fabric_path4_flow_count",
    "fabric_path5_flow_count",
    "fabric_path6_flow_count",
    "fabric_path7_flow_count",
    "peak_arrival_blocks_in_use",
    "peak_unexpected_msgs_in_use",
    "peak_unexpected_bytes_in_use",
    "peak_read_track_in_use",
    "peak_tpdc_inflight_packets",
    "peak_tpdc_pending_sacks",
    "peak_tpdc_pending_gap_nacks",
    "validation_failures",
    "resource_failures",
    "control_plane_failures",
    "packet_reliability_failures",
    "message_lifecycle_failures",
    "stdout_path",
    "stderr_path",
    "protocol_artifact",
    "completion_artifact",
    "send_stage_latency_artifact",
    "read_stage_latency_artifact",
    "read_recovery_latency_artifact",
    "failure_artifact",
    "diagnostic_artifact",
    "peer_pair_progress_artifact",
    "tpdc_session_progress_artifact",
    "fabric_path_progress_artifact",
    "top_timeout_pair",
    "top_no_match_pair",
    "top_late_response_pair",
    "top_peer_queue_pair",
    "top_live_response_pair",
    "top_stuck_tpdc_session",
    "top_stuck_tpdc_reason",
    "tpdc_sessions_with_no_ack_progress",
    "tpdc_sessions_with_ack_but_no_closeout",
]

COMPARISON_METRICS = [
    "semantic_goodput_mbps",
    "injection_ceiling_mbps",
    "semantic_goodput_utilization_pct",
    "semantic_completion_rate_pct",
    "terminalization_rate_pct",
    "semantic_completion_rate_pct_final",
    "terminalization_rate_pct_final",
    "success_latency_p50_us",
    "success_latency_p95_us",
    "success_latency_p99_us",
    "send_stage_dispatch_p50_ns",
    "send_stage_dispatch_p95_ns",
    "send_stage_dispatch_p99_ns",
    "send_stage_dispatch_wait_for_admission_p50_ns",
    "send_stage_dispatch_wait_for_admission_p95_ns",
    "send_stage_dispatch_wait_for_admission_p99_ns",
    "send_stage_dispatch_after_admission_to_first_send_p50_ns",
    "send_stage_dispatch_after_admission_to_first_send_p95_ns",
    "send_stage_dispatch_after_admission_to_first_send_p99_ns",
    "send_stage_admission_release_seen_count_mean",
    "send_stage_blocked_queue_wakeup_count_mean",
    "send_stage_redispatch_fail_after_wakeup_count_mean",
    "send_stage_redispatch_success_after_wakeup_count_mean",
    "send_stage_admission_release_to_wakeup_total_p95_ns",
    "send_stage_wakeup_to_redispatch_total_p95_ns",
    "send_stage_inflight_p50_ns",
    "send_stage_inflight_p95_ns",
    "send_stage_inflight_p99_ns",
    "send_stage_receive_consume_p50_ns",
    "send_stage_receive_consume_p95_ns",
    "send_stage_receive_consume_p99_ns",
    "send_stage_closeout_p50_ns",
    "send_stage_closeout_p95_ns",
    "send_stage_closeout_p99_ns",
    "send_stage_end_to_end_p50_ns",
    "send_stage_end_to_end_p95_ns",
    "send_stage_end_to_end_p99_ns",
    "read_stage_responder_budget_generate_p50_ns",
    "read_stage_responder_budget_generate_p95_ns",
    "read_stage_responder_budget_generate_p99_ns",
    "read_stage_pending_response_queue_dispatch_p50_ns",
    "read_stage_pending_response_queue_dispatch_p95_ns",
    "read_stage_pending_response_queue_dispatch_p99_ns",
    "read_stage_response_first_packet_tx_p50_ns",
    "read_stage_response_first_packet_tx_p95_ns",
    "read_stage_response_first_packet_tx_p99_ns",
    "read_stage_network_return_visibility_p50_ns",
    "read_stage_network_return_visibility_p95_ns",
    "read_stage_network_return_visibility_p99_ns",
    "read_stage_first_response_visible_p50_ns",
    "read_stage_first_response_visible_p95_ns",
    "read_stage_first_response_visible_p99_ns",
    "read_stage_reassembly_complete_p50_ns",
    "read_stage_reassembly_complete_p95_ns",
    "read_stage_reassembly_complete_p99_ns",
    "read_stage_terminal_p50_ns",
    "read_stage_terminal_p95_ns",
    "read_stage_terminal_p99_ns",
    "read_stage_end_to_end_p50_ns",
    "read_stage_end_to_end_p95_ns",
    "read_stage_end_to_end_p99_ns",
    "device_tx_egress_mbps",
    "device_rx_observed_mbps",
    "send_success_completions_total",
    "write_success_completions_total",
    "read_success_completions_total",
    "send_semantic_goodput_mbps",
    "write_semantic_goodput_mbps",
    "read_semantic_goodput_mbps",
    "dropped_packets_total",
    "credit_gate_blocked_total",
    "blocked_queue_push_total",
    "blocked_queue_wakeup_total",
    "blocked_queue_dispatch_total",
    "blocked_queue_depth_max",
    "send_admission_release_count",
    "send_admission_release_bytes_total",
    "blocked_queue_redispatch_fail_after_wakeup_total",
    "blocked_queue_redispatch_success_after_wakeup_total",
    "pending_response_enqueue_total",
    "pending_response_retry_total",
    "pending_response_dispatch_failures_total",
    "pending_response_success_after_retry_total",
    "stale_timeout_skipped_total",
    "stale_retry_skipped_total",
    "late_response_observed_total",
    "send_dispatch_started_total",
    "send_response_ok_live_total",
    "send_response_nonok_live_total",
    "send_response_missing_live_request_total",
    "send_response_after_terminal_total",
    "send_timeout_without_response_total",
    "send_timeout_retry_without_response_progress_total",
    "fabric_total_tx_bytes",
    "fabric_total_tx_packets",
    "fabric_ecn_mark_total",
    "fabric_utilization_pct",
    "fabric_total_capacity_gbps",
    "semantic_link_utilization_pct",
    "fabric_dynamic_assignment_total",
    "fabric_dynamic_path_reuse_total",
    "fabric_active_flow_assignments_peak",
    "fabric_path_score_min_ns",
    "fabric_path_score_max_ns",
    "fabric_path_score_mean_ns",
    "fabric_path_skew_ratio",
    "tpdc_sessions_with_no_ack_progress",
    "tpdc_sessions_with_ack_but_no_closeout",
    "credit_refresh_sent_total",
    "ack_ctrl_ext_sent_total",
    "unexpected_alloc_failures_total",
    "arrival_alloc_failures_total",
    "read_track_alloc_failures_total",
    "peak_arrival_blocks_in_use",
    "peak_unexpected_msgs_in_use",
    "peak_unexpected_bytes_in_use",
    "peak_read_track_in_use",
    "peak_tpdc_inflight_packets",
    "peak_tpdc_pending_sacks",
    "peak_tpdc_pending_gap_nacks",
    "sack_sent_total",
    "gap_nack_sent_total",
    "validation_failures",
    "resource_failures",
    "control_plane_failures",
    "packet_reliability_failures",
    "message_lifecycle_failures",
]

COMPARISON_FIELDS = [
    "variant_name",
    "workload_name",
    "tuning_name",
    "timeout_budget_name",
    "timeout_budget_ms",
    "resource_tier_name",
    "truth_op_spacing_ns",
    "device_mtu_bytes",
    "payload_mtu_bytes",
    "fabric_path_count_cfg",
    "fabric_path_data_rate",
    "truthFlowCount",
    "truthPayloadBytes",
    "truthPressureProfile",
    "run_count",
    "pass_count",
    "pass_rate_pct",
    "drain_completed_rate_pct",
]
for metric in COMPARISON_METRICS:
    COMPARISON_FIELDS.extend(
        [
            f"{metric}_mean",
            f"{metric}_min",
            f"{metric}_max",
            f"{metric}_stdev",
        ]
    )
COMPARISON_FIELDS.extend(
    [
        "semantic_goodput_mbps_delta_vs_baseline_pct",
    ]
)

DEFAULT_OUTPUTS = {
    "expanded_matrix_json": "expanded-matrix.json",
    "summary_csv": "benchmark-summary.csv",
    "comparison_csv": "profile-comparison.csv",
    "report_md": "benchmark-report.md",
    "metadata_json": "benchmark-metadata.json",
}

DEFAULT_CONFIG = {
    "name": "soft-ue-truth-benchmark-matrix",
    "build_before_run": False,
    "repetitions": 1,
    "rng_run_base": 1,
    "defaults": {},
    "profile_overrides": {},
    "variant_overrides": {},
    "workload_overrides": {},
    "tuning_overrides": {},
    "timeout_overrides": {},
    "resource_overrides": {},
    "matrix": {},
    "drain_window_seconds": 0.0,
    "outputs": deepcopy(DEFAULT_OUTPUTS),
}

LOG_PATTERNS = {
    "protocol": re.compile(r"^soft-ue protocol log:\s*(.+)$", re.MULTILINE),
    "completion": re.compile(r"^soft-ue completion log:\s*(.+)$", re.MULTILINE),
    "failure": re.compile(r"^soft-ue failure log:\s*(.+)$", re.MULTILINE),
    "diagnostic": re.compile(r"^soft-ue diagnostic log:\s*(.+)$", re.MULTILINE),
    "tpdc_session_progress": re.compile(r"^soft-ue tpdc session progress log:\s*(.+)$", re.MULTILINE),
}

SEND_DIAGNOSTIC_DETAIL_RE = re.compile(r"job=(\d+)\s+msg=(\d+)\s+peer=(\d+)")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def load_config(path: Path) -> dict[str, Any]:
    raw = json.loads(path.read_text(encoding="utf-8"))
    config = deepcopy(DEFAULT_CONFIG)
    config.update({k: v for k, v in raw.items() if k in config and k not in {"outputs"}})
    config["defaults"] = deepcopy(raw.get("defaults", {}))
    config["profile_overrides"] = deepcopy(raw.get("profile_overrides", {}))
    config["variant_overrides"] = deepcopy(raw.get("variant_overrides", {}))
    config["workload_overrides"] = deepcopy(raw.get("workload_overrides", {}))
    config["tuning_overrides"] = deepcopy(raw.get("tuning_overrides", {}))
    config["timeout_overrides"] = deepcopy(raw.get("timeout_overrides", {}))
    config["resource_overrides"] = deepcopy(raw.get("resource_overrides", {}))
    config["matrix"] = deepcopy(raw.get("matrix", {}))
    config["outputs"] = deepcopy(DEFAULT_OUTPUTS)
    config["outputs"].update(raw.get("outputs", {}))
    return config


def validate_truth_benchmark_config(config: dict[str, Any]) -> None:
    def validate_system_scenario_value(value: Any, context: str) -> None:
        if value not in REQUIRED_SYSTEM_SCENARIO_MODES:
            allowed = ", ".join(sorted(REQUIRED_SYSTEM_SCENARIO_MODES))
            raise ValueError(f"{context} must set systemScenarioMode to one of {{{allowed}}}, got {value!r}")

    def validate_override_group(group_name: str, overrides_by_name: dict[str, dict[str, Any]]) -> None:
        for override_name, overrides in overrides_by_name.items():
            if "systemScenarioMode" in overrides:
                validate_system_scenario_value(
                    overrides["systemScenarioMode"],
                    f"benchmark {group_name}[{override_name!r}]",
                )
            if (
                "truthExperimentClass" in overrides
                and overrides["truthExperimentClass"] != REQUIRED_TRUTH_EXPERIMENT_CLASS
            ):
                raise ValueError(
                    f"benchmark {group_name}[{override_name!r}] must set truthExperimentClass={REQUIRED_TRUTH_EXPERIMENT_CLASS!r}, got {overrides['truthExperimentClass']!r}"
                )

    default_system_scenario = config["defaults"].get("systemScenarioMode")
    if default_system_scenario is not None:
        validate_system_scenario_value(default_system_scenario, "benchmark defaults")
    default_truth_experiment = config["defaults"].get("truthExperimentClass")
    if default_truth_experiment is not None and default_truth_experiment != REQUIRED_TRUTH_EXPERIMENT_CLASS:
        raise ValueError(
            f"benchmark defaults must set truthExperimentClass={REQUIRED_TRUTH_EXPERIMENT_CLASS!r}, got {default_truth_experiment!r}"
        )
    for profile, overrides in config["profile_overrides"].items():
        if "systemScenarioMode" in overrides:
            validate_system_scenario_value(
                overrides["systemScenarioMode"],
                f"benchmark profile_overrides[{profile!r}]",
            )
        if (
            "truthExperimentClass" in overrides
            and overrides["truthExperimentClass"] != REQUIRED_TRUTH_EXPERIMENT_CLASS
        ):
            raise ValueError(
                f"benchmark profile_overrides[{profile!r}] must set truthExperimentClass={REQUIRED_TRUTH_EXPERIMENT_CLASS!r}, got {overrides['truthExperimentClass']!r}"
            )
    validate_override_group("variant_overrides", config["variant_overrides"])
    validate_override_group("workload_overrides", config["workload_overrides"])
    validate_override_group("tuning_overrides", config["tuning_overrides"])
    validate_override_group("timeout_overrides", config["timeout_overrides"])
    validate_override_group("resource_overrides", config["resource_overrides"])

    matrix = config["matrix"]
    for variant_name in matrix.get("variant", []):
        if variant_name not in config["variant_overrides"]:
            raise ValueError(f"benchmark matrix.variant includes unknown variant {variant_name!r}")
    for workload_name in matrix.get("workload", []):
        if workload_name not in config["workload_overrides"]:
            raise ValueError(f"benchmark matrix.workload includes unknown workload {workload_name!r}")
    for tuning_name in matrix.get("tuning", []):
        if tuning_name not in config["tuning_overrides"]:
            raise ValueError(f"benchmark matrix.tuning includes unknown tuning {tuning_name!r}")
    for timeout_budget_name in matrix.get("timeout_budget", []):
        if timeout_budget_name not in config["timeout_overrides"]:
            raise ValueError(
                f"benchmark matrix.timeout_budget includes unknown timeout budget {timeout_budget_name!r}"
            )
    for resource_tier_name in matrix.get("resource_tier", []):
        if resource_tier_name not in config["resource_overrides"]:
            raise ValueError(
                f"benchmark matrix.resource_tier includes unknown resource tier {resource_tier_name!r}"
            )


def apply_required_truth_params(params: dict[str, Any]) -> dict[str, Any]:
    normalized = deepcopy(params)
    normalized.update(REQUIRED_TRUTH_PARAMS)
    return normalized


def value_to_cli(value: Any) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value)


def sanitize_run_id(part: str) -> str:
    safe = re.sub(r"[^A-Za-z0-9._-]+", "-", part)
    safe = safe.strip("-")
    return safe or "run"


def expand_matrix(config: dict[str, Any], filter_profiles: set[str] | None = None) -> list[dict[str, Any]]:
    matrix = config["matrix"]
    profiles = list(matrix.get("truthPressureProfile", []))
    if filter_profiles:
        profiles = [profile for profile in profiles if profile in filter_profiles]
    variants = list(matrix.get("variant", [])) or ["default"]
    workloads = list(matrix.get("workload", [])) or ["default"]
    tunings = list(matrix.get("tuning", [])) or ["default"]
    timeout_budgets = list(matrix.get("timeout_budget", [])) or ["default"]
    resource_tiers = list(matrix.get("resource_tier", [])) or ["default"]
    flow_counts = list(matrix.get("truthFlowCount", [])) or [None]
    payloads = list(matrix.get("truthPayloadBytes", [])) or [None]
    repetitions = int(config["repetitions"])
    rng_run = int(config["rng_run_base"])
    runs: list[dict[str, Any]] = []

    for profile, variant_name, workload_name, tuning_name, timeout_budget_name, resource_tier_name, flow_count, payload_bytes in product(
        profiles,
        variants,
        workloads,
        tunings,
        timeout_budgets,
        resource_tiers,
        flow_counts,
        payloads,
    ):
        for repetition in range(1, repetitions + 1):
            params = deepcopy(config["defaults"])
            params.update(deepcopy(config["profile_overrides"].get(profile, {})))
            if variant_name != "default":
                params.update(deepcopy(config["variant_overrides"].get(variant_name, {})))
            if workload_name != "default":
                params.update(deepcopy(config["workload_overrides"].get(workload_name, {})))
            if tuning_name != "default":
                params.update(deepcopy(config["tuning_overrides"].get(tuning_name, {})))
            if timeout_budget_name != "default":
                params.update(deepcopy(config["timeout_overrides"].get(timeout_budget_name, {})))
            if resource_tier_name != "default":
                params.update(deepcopy(config["resource_overrides"].get(resource_tier_name, {})))
            params["truthPressureProfile"] = profile
            if flow_count is not None:
                params["truthFlowCount"] = flow_count
            if payload_bytes is not None:
                params["truthPayloadBytes"] = payload_bytes
            params["RngRun"] = rng_run
            params = apply_required_truth_params(params)
            effective_flow_count = int(params.get("truthFlowCount", 0))
            effective_payload_bytes = int(params.get("truthPayloadBytes", 0))
            if (
                variant_name == "default"
                and workload_name == "default"
                and tuning_name == "default"
                and timeout_budget_name == "default"
                and resource_tier_name == "default"
            ):
                run_id = sanitize_run_id(
                    f"profile-{profile}__flows-{effective_flow_count}__payload-{effective_payload_bytes}__rep-{repetition:02d}"
                )
            else:
                run_id = sanitize_run_id(
                    "variant-{variant}__workload-{workload}__tuning-{tuning}__timeout-{timeout}__resource-{resource}__profile-{profile}__flows-{flows}__payload-{payload}__rep-{rep:02d}".format(
                        variant=variant_name,
                        workload=workload_name,
                        tuning=tuning_name,
                        timeout=timeout_budget_name,
                        resource=resource_tier_name,
                        profile=profile,
                        flows=effective_flow_count,
                        payload=effective_payload_bytes,
                        rep=repetition,
                    )
                )
            runs.append(
                {
                    "run_id": run_id,
                    "repetition": repetition,
                    "rng_run": rng_run,
                    "variant_name": variant_name,
                    "workload_name": workload_name,
                    "tuning_name": tuning_name,
                    "timeout_budget_name": timeout_budget_name,
                    "resource_tier_name": resource_tier_name,
                    "params": params,
                }
            )
            rng_run += 1
    return runs


def build_ns3_run_command(params: dict[str, Any]) -> list[str]:
    ns3_args = [f"--{key}={value_to_cli(params[key])}" for key in sorted(params.keys())]
    return ["./ns3", "run", "SUE-Sim " + " ".join(ns3_args)]


def build_benchmark_command(params: dict[str, Any], drain_window_seconds: float) -> list[str]:
    command_params = deepcopy(params)
    if drain_window_seconds > 0.0:
        command_params["simulationTime"] = float(command_params.get("simulationTime", 0.0)) + drain_window_seconds
    return build_ns3_run_command(command_params)


def parse_log_paths(text: str) -> dict[str, Path | None]:
    parsed: dict[str, Path | None] = {}
    for key, pattern in LOG_PATTERNS.items():
        match = pattern.search(text)
        if not match:
            parsed[key] = None
            continue
        candidate = match.group(1).strip()
        parsed[key] = None if candidate == "(not generated)" else Path(candidate)
    return parsed


def resolve_artifact_path(repo: Path, path: Path | None) -> Path | None:
    if path is None:
        return None
    return path if path.is_absolute() else (repo / path)


def read_csv_rows(path: Path | None) -> list[dict[str, str]]:
    if path is None or not path.exists():
        return []
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def to_int(value: str | None) -> int:
    if value is None or value == "":
        return 0
    return int(float(value))


def parse_data_rate_mbps(value: str | None) -> float:
    if value is None:
        return 0.0
    text = str(value).strip().lower()
    match = re.fullmatch(r"([0-9]+(?:\.[0-9]+)?)\s*([kmgt]?)(?:bps)?", text)
    if not match:
        return 0.0
    magnitude = float(match.group(1))
    unit = match.group(2)
    scale = {
        "": 1.0 / 1_000_000.0,
        "k": 1.0 / 1_000.0,
        "m": 1.0,
        "g": 1_000.0,
        "t": 1_000_000.0,
    }[unit]
    return magnitude * scale


def percentile_or_zero(values: list[float], percentile: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return round(ordered[0], 6)
    rank = (len(ordered) - 1) * percentile
    lower = int(rank)
    upper = min(lower + 1, len(ordered) - 1)
    weight = rank - lower
    return round(ordered[lower] * (1.0 - weight) + ordered[upper] * weight, 6)


def canonicalize_opcode(value: str | None) -> str:
    return (value or "").strip().lower()


def filter_rows_by_time_ns(rows: list[dict[str, str]], end_time_ns: int | None) -> list[dict[str, str]]:
    if end_time_ns is None:
        return list(rows)
    return [row for row in rows if to_int(row.get("TimeNs")) <= end_time_ns]


def aggregate_protocol_rows(rows: list[dict[str, str]], simulation_time: float) -> dict[str, Any]:
    result = {field: 0 for field in PROTOCOL_CUMULATIVE_FIELDS.values()}
    result.update({field: 0 for field in PROTOCOL_PEAK_FIELDS.values()})
    result["protocol_rows"] = len(rows)
    fabric_global_fields = {
        "fabric_path_count",
        "fabric_total_tx_bytes",
        "fabric_total_tx_packets",
        "fabric_ecn_mark_total",
        "fabric_top_hotspot_endpoint",
        "fabric_dynamic_routing_enabled",
        "fabric_dynamic_assignment_total",
        "fabric_dynamic_path_reuse_total",
        "fabric_active_flow_assignments_peak",
        "fabric_path_score_min_ns",
        "fabric_path_score_max_ns",
        "fabric_path_score_mean_ns",
        "fabric_path0_tx_bytes",
        "fabric_path1_tx_bytes",
        "fabric_path2_tx_bytes",
        "fabric_path3_tx_bytes",
        "fabric_path4_tx_bytes",
        "fabric_path5_tx_bytes",
        "fabric_path6_tx_bytes",
        "fabric_path7_tx_bytes",
        "fabric_path0_flow_count",
        "fabric_path1_flow_count",
        "fabric_path2_flow_count",
        "fabric_path3_flow_count",
        "fabric_path4_flow_count",
        "fabric_path5_flow_count",
        "fabric_path6_flow_count",
        "fabric_path7_flow_count",
    }
    fabric_peak_fields = {
        "fabric_path0_queue_depth_max",
        "fabric_path1_queue_depth_max",
        "fabric_path2_queue_depth_max",
        "fabric_path3_queue_depth_max",
        "fabric_path4_queue_depth_max",
        "fabric_path5_queue_depth_max",
        "fabric_path6_queue_depth_max",
        "fabric_path7_queue_depth_max",
    }

    final_by_device: dict[tuple[str, str], dict[str, str]] = {}
    for row in rows:
        key = (row.get("NodeId", ""), row.get("IfIndex", ""))
        final_by_device[key] = row
        for source_name, target_name in PROTOCOL_PEAK_FIELDS.items():
            result[target_name] = max(result[target_name], to_int(row.get(source_name)))

    for row in final_by_device.values():
        for source_name, target_name in PROTOCOL_CUMULATIVE_FIELDS.items():
            value = to_int(row.get(source_name))
            if target_name in fabric_global_fields:
                result[target_name] = max(result[target_name], value)
            else:
                result[target_name] += value
    for row in rows:
        for source_name, target_name in PROTOCOL_PEAK_FIELDS.items():
            if target_name in fabric_peak_fields:
                result[target_name] = max(result[target_name], to_int(row.get(source_name)))

    if simulation_time > 0:
        result["device_tx_egress_mbps"] = round((result["tx_bytes_total"] * 8.0) / simulation_time / 1_000_000.0, 6)
        result["device_rx_observed_mbps"] = round(
            (result["rx_bytes_total"] * 8.0) / simulation_time / 1_000_000.0,
            6,
        )
        result["fabric_total_offered_mbps"] = round(
            (result["fabric_total_tx_bytes"] * 8.0) / simulation_time / 1_000_000.0,
            6,
        )
    else:
        result["device_tx_egress_mbps"] = 0.0
        result["device_rx_observed_mbps"] = 0.0
        result["fabric_total_offered_mbps"] = 0.0
    result["fabric_total_goodput_mbps"] = 0.0
    result["fabric_utilization_pct"] = 0.0
    result["fabric_path_skew_ratio"] = 0.0
    nonzero_path_bytes = [
        float(result[f"fabric_path{path_idx}_tx_bytes"])
        for path_idx in range(8)
        if float(result[f"fabric_path{path_idx}_tx_bytes"]) > 0.0
    ]
    if nonzero_path_bytes:
        mean_nonzero = statistics.fmean(nonzero_path_bytes)
        result["fabric_path_skew_ratio"] = round(max(nonzero_path_bytes) / mean_nonzero, 6) if mean_nonzero > 0.0 else 0.0
    return result


def aggregate_completion_rows(rows: list[dict[str, str]], simulation_time: float) -> dict[str, Any]:
    result: dict[str, Any] = {
        "completion_rows": len(rows),
        "completion_success_rows": 0,
        "completion_success_payload_bytes_total": 0,
        "semantic_goodput_mbps": 0.0,
        "success_latency_p50_us": 0.0,
        "success_latency_p95_us": 0.0,
        "success_latency_p99_us": 0.0,
    }
    for prefix in OPCODE_PREFIXES.values():
        result[f"{prefix}_success_completions_total"] = 0
        result[f"{prefix}_success_payload_bytes_total"] = 0
        result[f"{prefix}_semantic_goodput_mbps"] = 0.0
    success_latencies_us: list[float] = []
    for row in rows:
        success = to_int(row.get("Success")) != 0
        if not success:
            continue
        opcode = canonicalize_opcode(row.get("Opcode"))
        result["completion_success_rows"] += 1
        payload_bytes = to_int(row.get("PayloadBytes"))
        result["completion_success_payload_bytes_total"] += payload_bytes
        success_latencies_us.append(to_int(row.get("LatencyNs")) / 1000.0)
        if opcode in OPCODE_PREFIXES:
            prefix = OPCODE_PREFIXES[opcode]
            result[f"{prefix}_success_completions_total"] += 1
            result[f"{prefix}_success_payload_bytes_total"] += payload_bytes

    if simulation_time > 0:
        result["semantic_goodput_mbps"] = round(
            (result["completion_success_payload_bytes_total"] * 8.0) / simulation_time / 1_000_000.0,
            6,
        )
        for prefix in OPCODE_PREFIXES.values():
            result[f"{prefix}_semantic_goodput_mbps"] = round(
                (result[f"{prefix}_success_payload_bytes_total"] * 8.0) / simulation_time / 1_000_000.0,
                6,
            )
    result["success_latency_p50_us"] = percentile_or_zero(success_latencies_us, 0.50)
    result["success_latency_p95_us"] = percentile_or_zero(success_latencies_us, 0.95)
    result["success_latency_p99_us"] = percentile_or_zero(success_latencies_us, 0.99)
    return result


def build_send_stage_latency(rows: list[dict[str, str]]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for row in rows:
        if canonicalize_opcode(row.get("Opcode")) != "send":
            continue
        if to_int(row.get("Success")) == 0 or to_int(row.get("SendStageValid")) == 0:
            continue
        result.append(
            {
                "job_id": to_int(row.get("JobId")),
                "msg_id": to_int(row.get("MsgId")),
                "peer_fep": to_int(row.get("PeerFep")),
                "payload_bytes": to_int(row.get("PayloadBytes")),
                "dispatch_ns": to_int(row.get("SendStageDispatchNs")),
                "dispatch_wait_for_admission_ns": to_int(row.get("SendStageDispatchWaitForAdmissionNs")),
                "dispatch_after_admission_to_first_send_ns": to_int(
                    row.get("SendStageDispatchAfterAdmissionToFirstSendNs")
                ),
                "dispatch_attempt_count": to_int(row.get("SendStageDispatchAttemptCount")),
                "dispatch_budget_block_count": to_int(row.get("SendStageDispatchBudgetBlockCount")),
                "blocked_queue_enqueue_count": to_int(row.get("SendStageBlockedQueueEnqueueCount")),
                "blocked_queue_redispatch_count": to_int(row.get("SendStageBlockedQueueRedispatchCount")),
                "blocked_queue_wait_total_ns": to_int(row.get("SendStageBlockedQueueWaitTotalNs")),
                "blocked_queue_wait_peak_ns": to_int(row.get("SendStageBlockedQueueWaitPeakNs")),
                "admission_release_seen_count": to_int(row.get("SendStageAdmissionReleaseSeenCount")),
                "blocked_queue_wakeup_count": to_int(row.get("SendStageBlockedQueueWakeupCount")),
                "redispatch_fail_after_wakeup_count": to_int(
                    row.get("SendStageRedispatchFailAfterWakeupCount")
                ),
                "redispatch_success_after_wakeup_count": to_int(
                    row.get("SendStageRedispatchSuccessAfterWakeupCount")
                ),
                "admission_release_to_wakeup_total_ns": to_int(
                    row.get("SendStageAdmissionReleaseToWakeupTotalNs")
                ),
                "admission_release_to_wakeup_peak_ns": to_int(
                    row.get("SendStageAdmissionReleaseToWakeupPeakNs")
                ),
                "wakeup_to_redispatch_total_ns": to_int(row.get("SendStageWakeupToRedispatchTotalNs")),
                "wakeup_to_redispatch_peak_ns": to_int(row.get("SendStageWakeupToRedispatchPeakNs")),
                "inflight_ns": to_int(row.get("SendStageInflightNs")),
                "receive_consume_ns": to_int(row.get("SendStageReceiveConsumeNs")),
                "closeout_ns": to_int(row.get("SendStageCloseoutNs")),
                "end_to_end_ns": to_int(row.get("SendStageEndToEndNs")),
            }
        )
    return result


def build_read_stage_latency(rows: list[dict[str, str]]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for row in rows:
        if canonicalize_opcode(row.get("Opcode")) != "read":
            continue
        if to_int(row.get("Success")) == 0 or to_int(row.get("ReadStageValid")) == 0:
            continue
        result.append(
            {
                "job_id": to_int(row.get("JobId")),
                "msg_id": to_int(row.get("MsgId")),
                "peer_fep": to_int(row.get("PeerFep")),
                "payload_bytes": to_int(row.get("PayloadBytes")),
                "responder_budget_generate_ns": to_int(row.get("ReadStageResponderBudgetGenerateNs")),
                "pending_response_queue_dispatch_ns": to_int(
                    row.get("ReadStagePendingResponseQueueDispatchNs")
                ),
                "response_first_packet_tx_ns": to_int(row.get("ReadStageResponseFirstPacketTxNs")),
                "network_return_visibility_ns": to_int(
                    row.get("ReadStageNetworkReturnVisibilityNs")
                ),
                "first_response_visible_ns": to_int(row.get("ReadStageFirstResponseVisibleNs")),
                "reassembly_complete_ns": to_int(row.get("ReadStageReassemblyCompleteNs")),
                "terminal_ns": to_int(row.get("ReadStageTerminalNs")),
                "end_to_end_ns": to_int(row.get("ReadStageEndToEndNs")),
            }
        )
    return result


def build_read_recovery_latency(rows: list[dict[str, str]]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for row in rows:
        if canonicalize_opcode(row.get("Opcode")) != "read":
            continue
        if to_int(row.get("Success")) == 0 or to_int(row.get("ReadRecoveryValid")) == 0:
            continue
        result.append(
            {
                "job_id": to_int(row.get("JobId")),
                "msg_id": to_int(row.get("MsgId")),
                "peer_fep": to_int(row.get("PeerFep")),
                "payload_bytes": to_int(row.get("PayloadBytes")),
                "missing_chunk_index": to_int(row.get("ReadRecoveryMissingChunkIndex")),
                "gap_detected_at_ns": to_int(row.get("ReadRecoveryGapDetectedAtNs")),
                "gap_nack_sent_at_ns": to_int(row.get("ReadRecoveryGapNackSentAtNs")),
                "missing_fragment_retransmit_tx_at_ns": to_int(
                    row.get("ReadRecoveryMissingFragmentRetransmitTxAtNs")
                ),
                "missing_fragment_first_visible_at_ns": to_int(
                    row.get("ReadRecoveryMissingFragmentFirstVisibleAtNs")
                ),
                "reassembly_unblocked_at_ns": to_int(row.get("ReadRecoveryReassemblyUnblockedAtNs")),
                "detect_to_nack_ns": to_int(row.get("ReadRecoveryDetectToNackNs")),
                "nack_to_retransmit_ns": to_int(row.get("ReadRecoveryNackToRetransmitNs")),
                "retransmit_to_first_visible_ns": to_int(
                    row.get("ReadRecoveryRetransmitToFirstVisibleNs")
                ),
                "first_visible_to_unblocked_ns": to_int(
                    row.get("ReadRecoveryFirstVisibleToUnblockedNs")
                ),
                "detect_to_unblocked_ns": to_int(row.get("ReadRecoveryDetectToUnblockedNs")),
            }
        )
    return result


def build_read_recovery_progress(rows: list[dict[str, str]]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for row in rows:
        if canonicalize_opcode(row.get("Opcode")) != "read":
            continue
        if to_int(row.get("ReadRecoveryTracked")) == 0:
            continue
        result.append(
            {
                "job_id": to_int(row.get("JobId")),
                "msg_id": to_int(row.get("MsgId")),
                "peer_fep": to_int(row.get("PeerFep")),
                "success": to_int(row.get("Success")) != 0,
                "failure_domain": str(row.get("FailureDomain") or ""),
                "terminal_reason": str(row.get("TerminalReason") or ""),
                "missing_chunk_index": to_int(row.get("ReadRecoveryMissingChunkIndex")),
                "missing_transport_seq": to_int(row.get("ReadRecoveryMissingTransportSeq")),
                "gap_detected_at_ns": to_int(row.get("ReadRecoveryGapDetectedAtNs")),
                "gap_nack_sent_at_ns": to_int(row.get("ReadRecoveryGapNackSentAtNs")),
                "gap_nack_observed_at_sender_ns": to_int(
                    row.get("ReadRecoveryGapNackObservedAtSenderNs")
                ),
                "missing_fragment_retransmit_tx_at_ns": to_int(
                    row.get("ReadRecoveryMissingFragmentRetransmitTxAtNs")
                ),
                "missing_fragment_first_visible_at_ns": to_int(
                    row.get("ReadRecoveryMissingFragmentFirstVisibleAtNs")
                ),
                "reassembly_unblocked_at_ns": to_int(
                    row.get("ReadRecoveryReassemblyUnblockedAtNs")
                ),
                "gap_nack_sent_count": to_int(row.get("ReadRecoveryGapNackSentCount")),
                "gap_nack_observed_at_sender_count": to_int(
                    row.get("ReadRecoveryGapNackObservedAtSenderCount")
                ),
                "missing_fragment_retransmit_count": to_int(
                    row.get("ReadRecoveryMissingFragmentRetransmitCount")
                ),
                "detect_to_nack_ns": to_int(row.get("ReadRecoveryDetectToNackNs")),
                "nack_to_observed_at_sender_ns": to_int(
                    row.get("ReadRecoveryNackToObservedAtSenderNs")
                ),
                "observed_at_sender_to_retransmit_ns": to_int(
                    row.get("ReadRecoveryObservedAtSenderToRetransmitNs")
                ),
                "nack_to_retransmit_ns": to_int(row.get("ReadRecoveryNackToRetransmitNs")),
                "retransmit_to_first_visible_ns": to_int(
                    row.get("ReadRecoveryRetransmitToFirstVisibleNs")
                ),
                "first_visible_to_unblocked_ns": to_int(
                    row.get("ReadRecoveryFirstVisibleToUnblockedNs")
                ),
                "detect_to_unblocked_ns": to_int(row.get("ReadRecoveryDetectToUnblockedNs")),
                "retransmit_to_terminal_ns": to_int(
                    row.get("ReadRecoveryRetransmitToTerminalNs")
                ),
                "detect_to_terminal_ns": to_int(row.get("ReadRecoveryDetectToTerminalNs")),
            }
        )
    return result


def build_read_pre_admission_rows(rows: list[dict[str, str]]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for row in rows:
        if canonicalize_opcode(row.get("Opcode")) != "read":
            continue
        if to_int(row.get("ReadPreAdmissionTracked")) == 0:
            continue
        result.append(
            {
                "job_id": to_int(row.get("JobId")),
                "msg_id": to_int(row.get("MsgId")),
                "peer_fep": to_int(row.get("PeerFep")),
                "success": to_int(row.get("Success")) != 0,
                "failure_domain": str(row.get("FailureDomain") or ""),
                "terminal_reason": str(row.get("TerminalReason") or ""),
                "context_allocated_retry": to_int(row.get("ReadPreContextAllocatedRetry")) != 0,
                "terminal_resource_exhaust": to_int(row.get("ReadPreTerminalResourceExhaust")) != 0,
                "first_packet_no_context_count": to_int(row.get("ReadPreFirstPacketNoContextCount")),
                "arrival_reserve_fail_count": to_int(row.get("ReadPreArrivalReserveFailCount")),
                "target_registered_at_ns": to_int(row.get("ReadPreTargetRegisteredAtNs")),
                "first_packet_no_context_at_ns": to_int(row.get("ReadPreFirstPacketNoContextAtNs")),
                "arrival_block_reserved_at_ns": to_int(row.get("ReadPreArrivalBlockReservedAtNs")),
                "context_allocated_retry_at_ns": to_int(row.get("ReadPreContextAllocatedRetryAtNs")),
                "target_released_at_ns": to_int(row.get("ReadPreTargetReleasedAtNs")),
                "arrival_context_released_at_ns": to_int(
                    row.get("ReadPreArrivalContextReleasedAtNs")
                ),
                "target_to_first_no_context_ns": to_int(
                    row.get("ReadPreTargetToFirstNoContextNs")
                ),
                "target_to_arrival_reserved_ns": to_int(
                    row.get("ReadPreTargetToArrivalReservedNs")
                ),
                "first_no_context_to_arrival_reserved_ns": to_int(
                    row.get("ReadPreFirstNoContextToArrivalReservedNs")
                ),
                "arrival_reserved_to_first_response_visible_ns": to_int(
                    row.get("ReadPreArrivalReservedToFirstResponseVisibleNs")
                ),
                "target_hold_to_release_ns": to_int(row.get("ReadPreTargetHoldToReleaseNs")),
                "arrival_hold_to_release_ns": to_int(row.get("ReadPreArrivalHoldToReleaseNs")),
                "target_to_terminal_resource_exhaust_ns": to_int(
                    row.get("ReadPreTargetToTerminalResourceExhaustNs")
                ),
                "first_no_context_to_terminal_resource_exhaust_ns": to_int(
                    row.get("ReadPreFirstNoContextToTerminalResourceExhaustNs")
                ),
            }
        )
    return result


def aggregate_send_stage_rows(rows: list[dict[str, str]]) -> dict[str, Any]:
    stage_rows = build_send_stage_latency(rows)
    dispatch_values = [float(item["dispatch_ns"]) for item in stage_rows]
    dispatch_wait_values = [float(item["dispatch_wait_for_admission_ns"]) for item in stage_rows]
    dispatch_after_values = [float(item["dispatch_after_admission_to_first_send_ns"]) for item in stage_rows]
    dispatch_attempt_values = [float(item["dispatch_attempt_count"]) for item in stage_rows]
    dispatch_block_values = [float(item["dispatch_budget_block_count"]) for item in stage_rows]
    blocked_enqueue_values = [float(item["blocked_queue_enqueue_count"]) for item in stage_rows]
    blocked_redispatch_values = [float(item["blocked_queue_redispatch_count"]) for item in stage_rows]
    blocked_wait_total_values = [float(item["blocked_queue_wait_total_ns"]) for item in stage_rows]
    blocked_wait_peak_values = [float(item["blocked_queue_wait_peak_ns"]) for item in stage_rows]
    release_seen_values = [float(item["admission_release_seen_count"]) for item in stage_rows]
    wakeup_count_values = [float(item["blocked_queue_wakeup_count"]) for item in stage_rows]
    redispatch_fail_values = [float(item["redispatch_fail_after_wakeup_count"]) for item in stage_rows]
    redispatch_success_values = [float(item["redispatch_success_after_wakeup_count"]) for item in stage_rows]
    release_to_wakeup_total_values = [float(item["admission_release_to_wakeup_total_ns"]) for item in stage_rows]
    release_to_wakeup_peak_values = [float(item["admission_release_to_wakeup_peak_ns"]) for item in stage_rows]
    wakeup_to_redispatch_total_values = [float(item["wakeup_to_redispatch_total_ns"]) for item in stage_rows]
    wakeup_to_redispatch_peak_values = [float(item["wakeup_to_redispatch_peak_ns"]) for item in stage_rows]
    inflight_values = [float(item["inflight_ns"]) for item in stage_rows]
    receive_consume_values = [float(item["receive_consume_ns"]) for item in stage_rows]
    closeout_values = [float(item["closeout_ns"]) for item in stage_rows]
    end_to_end_values = [float(item["end_to_end_ns"]) for item in stage_rows]
    return {
        "send_stage_sample_count": len(stage_rows),
        "send_stage_dispatch_p50_ns": percentile_or_zero(dispatch_values, 0.50),
        "send_stage_dispatch_p95_ns": percentile_or_zero(dispatch_values, 0.95),
        "send_stage_dispatch_p99_ns": percentile_or_zero(dispatch_values, 0.99),
        "send_stage_dispatch_wait_for_admission_p50_ns": percentile_or_zero(dispatch_wait_values, 0.50),
        "send_stage_dispatch_wait_for_admission_p95_ns": percentile_or_zero(dispatch_wait_values, 0.95),
        "send_stage_dispatch_wait_for_admission_p99_ns": percentile_or_zero(dispatch_wait_values, 0.99),
        "send_stage_dispatch_after_admission_to_first_send_p50_ns": percentile_or_zero(dispatch_after_values, 0.50),
        "send_stage_dispatch_after_admission_to_first_send_p95_ns": percentile_or_zero(dispatch_after_values, 0.95),
        "send_stage_dispatch_after_admission_to_first_send_p99_ns": percentile_or_zero(dispatch_after_values, 0.99),
        "send_stage_dispatch_attempt_count_mean": round(statistics.fmean(dispatch_attempt_values), 6)
        if dispatch_attempt_values
        else 0.0,
        "send_stage_dispatch_attempt_count_p95": percentile_or_zero(dispatch_attempt_values, 0.95),
        "send_stage_dispatch_budget_block_count_mean": round(statistics.fmean(dispatch_block_values), 6)
        if dispatch_block_values
        else 0.0,
        "send_stage_dispatch_budget_block_count_p95": percentile_or_zero(dispatch_block_values, 0.95),
        "send_stage_blocked_queue_enqueue_count_mean": round(statistics.fmean(blocked_enqueue_values), 6)
        if blocked_enqueue_values
        else 0.0,
        "send_stage_blocked_queue_enqueue_count_p95": percentile_or_zero(blocked_enqueue_values, 0.95),
        "send_stage_blocked_queue_redispatch_count_mean": round(statistics.fmean(blocked_redispatch_values), 6)
        if blocked_redispatch_values
        else 0.0,
        "send_stage_blocked_queue_redispatch_count_p95": percentile_or_zero(blocked_redispatch_values, 0.95),
        "send_stage_blocked_queue_wait_total_p50_ns": percentile_or_zero(blocked_wait_total_values, 0.50),
        "send_stage_blocked_queue_wait_total_p95_ns": percentile_or_zero(blocked_wait_total_values, 0.95),
        "send_stage_blocked_queue_wait_peak_p50_ns": percentile_or_zero(blocked_wait_peak_values, 0.50),
        "send_stage_blocked_queue_wait_peak_p95_ns": percentile_or_zero(blocked_wait_peak_values, 0.95),
        "send_stage_admission_release_seen_count_mean": round(statistics.fmean(release_seen_values), 6)
        if release_seen_values
        else 0.0,
        "send_stage_admission_release_seen_count_p95": percentile_or_zero(release_seen_values, 0.95),
        "send_stage_blocked_queue_wakeup_count_mean": round(statistics.fmean(wakeup_count_values), 6)
        if wakeup_count_values
        else 0.0,
        "send_stage_blocked_queue_wakeup_count_p95": percentile_or_zero(wakeup_count_values, 0.95),
        "send_stage_redispatch_fail_after_wakeup_count_mean": round(statistics.fmean(redispatch_fail_values), 6)
        if redispatch_fail_values
        else 0.0,
        "send_stage_redispatch_fail_after_wakeup_count_p95": percentile_or_zero(redispatch_fail_values, 0.95),
        "send_stage_redispatch_success_after_wakeup_count_mean": round(
            statistics.fmean(redispatch_success_values), 6
        )
        if redispatch_success_values
        else 0.0,
        "send_stage_redispatch_success_after_wakeup_count_p95": percentile_or_zero(
            redispatch_success_values, 0.95
        ),
        "send_stage_admission_release_to_wakeup_total_p50_ns": percentile_or_zero(
            release_to_wakeup_total_values, 0.50
        ),
        "send_stage_admission_release_to_wakeup_total_p95_ns": percentile_or_zero(
            release_to_wakeup_total_values, 0.95
        ),
        "send_stage_admission_release_to_wakeup_peak_p50_ns": percentile_or_zero(
            release_to_wakeup_peak_values, 0.50
        ),
        "send_stage_admission_release_to_wakeup_peak_p95_ns": percentile_or_zero(
            release_to_wakeup_peak_values, 0.95
        ),
        "send_stage_wakeup_to_redispatch_total_p50_ns": percentile_or_zero(
            wakeup_to_redispatch_total_values, 0.50
        ),
        "send_stage_wakeup_to_redispatch_total_p95_ns": percentile_or_zero(
            wakeup_to_redispatch_total_values, 0.95
        ),
        "send_stage_wakeup_to_redispatch_peak_p50_ns": percentile_or_zero(
            wakeup_to_redispatch_peak_values, 0.50
        ),
        "send_stage_wakeup_to_redispatch_peak_p95_ns": percentile_or_zero(
            wakeup_to_redispatch_peak_values, 0.95
        ),
        "send_stage_inflight_p50_ns": percentile_or_zero(inflight_values, 0.50),
        "send_stage_inflight_p95_ns": percentile_or_zero(inflight_values, 0.95),
        "send_stage_inflight_p99_ns": percentile_or_zero(inflight_values, 0.99),
        "send_stage_receive_consume_p50_ns": percentile_or_zero(receive_consume_values, 0.50),
        "send_stage_receive_consume_p95_ns": percentile_or_zero(receive_consume_values, 0.95),
        "send_stage_receive_consume_p99_ns": percentile_or_zero(receive_consume_values, 0.99),
        "send_stage_closeout_p50_ns": percentile_or_zero(closeout_values, 0.50),
        "send_stage_closeout_p95_ns": percentile_or_zero(closeout_values, 0.95),
        "send_stage_closeout_p99_ns": percentile_or_zero(closeout_values, 0.99),
        "send_stage_end_to_end_p50_ns": percentile_or_zero(end_to_end_values, 0.50),
        "send_stage_end_to_end_p95_ns": percentile_or_zero(end_to_end_values, 0.95),
        "send_stage_end_to_end_p99_ns": percentile_or_zero(end_to_end_values, 0.99),
        "send_stage_dispatch_mean_ns": round(statistics.fmean(dispatch_values), 6) if dispatch_values else 0.0,
        "send_stage_inflight_mean_ns": round(statistics.fmean(inflight_values), 6) if inflight_values else 0.0,
        "send_stage_receive_consume_mean_ns": round(statistics.fmean(receive_consume_values), 6)
        if receive_consume_values
        else 0.0,
        "send_stage_closeout_mean_ns": round(statistics.fmean(closeout_values), 6) if closeout_values else 0.0,
        "send_stage_end_to_end_mean_ns": round(statistics.fmean(end_to_end_values), 6) if end_to_end_values else 0.0,
    }


def aggregate_read_stage_rows(rows: list[dict[str, str]]) -> dict[str, Any]:
    stage_rows = build_read_stage_latency(rows)
    responder_generate_values = [float(item["responder_budget_generate_ns"]) for item in stage_rows]
    pending_dispatch_values = [float(item["pending_response_queue_dispatch_ns"]) for item in stage_rows]
    first_packet_tx_values = [float(item["response_first_packet_tx_ns"]) for item in stage_rows]
    network_visibility_values = [float(item["network_return_visibility_ns"]) for item in stage_rows]
    first_visible_values = [float(item["first_response_visible_ns"]) for item in stage_rows]
    reassembly_values = [float(item["reassembly_complete_ns"]) for item in stage_rows]
    terminal_values = [float(item["terminal_ns"]) for item in stage_rows]
    end_to_end_values = [float(item["end_to_end_ns"]) for item in stage_rows]
    return {
        "read_stage_sample_count": len(stage_rows),
        "read_stage_responder_budget_generate_p50_ns": percentile_or_zero(responder_generate_values, 0.50),
        "read_stage_responder_budget_generate_p95_ns": percentile_or_zero(responder_generate_values, 0.95),
        "read_stage_responder_budget_generate_p99_ns": percentile_or_zero(responder_generate_values, 0.99),
        "read_stage_pending_response_queue_dispatch_p50_ns": percentile_or_zero(pending_dispatch_values, 0.50),
        "read_stage_pending_response_queue_dispatch_p95_ns": percentile_or_zero(pending_dispatch_values, 0.95),
        "read_stage_pending_response_queue_dispatch_p99_ns": percentile_or_zero(pending_dispatch_values, 0.99),
        "read_stage_response_first_packet_tx_p50_ns": percentile_or_zero(first_packet_tx_values, 0.50),
        "read_stage_response_first_packet_tx_p95_ns": percentile_or_zero(first_packet_tx_values, 0.95),
        "read_stage_response_first_packet_tx_p99_ns": percentile_or_zero(first_packet_tx_values, 0.99),
        "read_stage_network_return_visibility_p50_ns": percentile_or_zero(network_visibility_values, 0.50),
        "read_stage_network_return_visibility_p95_ns": percentile_or_zero(network_visibility_values, 0.95),
        "read_stage_network_return_visibility_p99_ns": percentile_or_zero(network_visibility_values, 0.99),
        "read_stage_first_response_visible_p50_ns": percentile_or_zero(first_visible_values, 0.50),
        "read_stage_first_response_visible_p95_ns": percentile_or_zero(first_visible_values, 0.95),
        "read_stage_first_response_visible_p99_ns": percentile_or_zero(first_visible_values, 0.99),
        "read_stage_reassembly_complete_p50_ns": percentile_or_zero(reassembly_values, 0.50),
        "read_stage_reassembly_complete_p95_ns": percentile_or_zero(reassembly_values, 0.95),
        "read_stage_reassembly_complete_p99_ns": percentile_or_zero(reassembly_values, 0.99),
        "read_stage_terminal_p50_ns": percentile_or_zero(terminal_values, 0.50),
        "read_stage_terminal_p95_ns": percentile_or_zero(terminal_values, 0.95),
        "read_stage_terminal_p99_ns": percentile_or_zero(terminal_values, 0.99),
        "read_stage_end_to_end_p50_ns": percentile_or_zero(end_to_end_values, 0.50),
        "read_stage_end_to_end_p95_ns": percentile_or_zero(end_to_end_values, 0.95),
        "read_stage_end_to_end_p99_ns": percentile_or_zero(end_to_end_values, 0.99),
        "read_stage_responder_budget_generate_mean_ns": round(statistics.fmean(responder_generate_values), 6)
        if responder_generate_values
        else 0.0,
        "read_stage_pending_response_queue_dispatch_mean_ns": round(statistics.fmean(pending_dispatch_values), 6)
        if pending_dispatch_values
        else 0.0,
        "read_stage_response_first_packet_tx_mean_ns": round(statistics.fmean(first_packet_tx_values), 6)
        if first_packet_tx_values
        else 0.0,
        "read_stage_network_return_visibility_mean_ns": round(statistics.fmean(network_visibility_values), 6)
        if network_visibility_values
        else 0.0,
        "read_stage_first_response_visible_mean_ns": round(statistics.fmean(first_visible_values), 6)
        if first_visible_values
        else 0.0,
        "read_stage_reassembly_complete_mean_ns": round(statistics.fmean(reassembly_values), 6)
        if reassembly_values
        else 0.0,
        "read_stage_terminal_mean_ns": round(statistics.fmean(terminal_values), 6)
        if terminal_values
        else 0.0,
        "read_stage_end_to_end_mean_ns": round(statistics.fmean(end_to_end_values), 6)
        if end_to_end_values
        else 0.0,
    }


def aggregate_read_recovery_rows(rows: list[dict[str, str]]) -> dict[str, Any]:
    recovery_rows = build_read_recovery_latency(rows)
    detect_to_nack_values = [float(item["detect_to_nack_ns"]) for item in recovery_rows]
    nack_to_retransmit_values = [float(item["nack_to_retransmit_ns"]) for item in recovery_rows]
    retransmit_to_first_visible_values = [
        float(item["retransmit_to_first_visible_ns"]) for item in recovery_rows
    ]
    first_visible_to_unblocked_values = [
        float(item["first_visible_to_unblocked_ns"]) for item in recovery_rows
    ]
    detect_to_unblocked_values = [float(item["detect_to_unblocked_ns"]) for item in recovery_rows]
    return {
        "read_recovery_sample_count": len(recovery_rows),
        "read_recovery_detect_to_nack_p50_ns": percentile_or_zero(detect_to_nack_values, 0.50),
        "read_recovery_detect_to_nack_p95_ns": percentile_or_zero(detect_to_nack_values, 0.95),
        "read_recovery_detect_to_nack_p99_ns": percentile_or_zero(detect_to_nack_values, 0.99),
        "read_recovery_nack_to_retransmit_p50_ns": percentile_or_zero(
            nack_to_retransmit_values, 0.50
        ),
        "read_recovery_nack_to_retransmit_p95_ns": percentile_or_zero(
            nack_to_retransmit_values, 0.95
        ),
        "read_recovery_nack_to_retransmit_p99_ns": percentile_or_zero(
            nack_to_retransmit_values, 0.99
        ),
        "read_recovery_retransmit_to_first_visible_p50_ns": percentile_or_zero(
            retransmit_to_first_visible_values, 0.50
        ),
        "read_recovery_retransmit_to_first_visible_p95_ns": percentile_or_zero(
            retransmit_to_first_visible_values, 0.95
        ),
        "read_recovery_retransmit_to_first_visible_p99_ns": percentile_or_zero(
            retransmit_to_first_visible_values, 0.99
        ),
        "read_recovery_first_visible_to_unblocked_p50_ns": percentile_or_zero(
            first_visible_to_unblocked_values, 0.50
        ),
        "read_recovery_first_visible_to_unblocked_p95_ns": percentile_or_zero(
            first_visible_to_unblocked_values, 0.95
        ),
        "read_recovery_first_visible_to_unblocked_p99_ns": percentile_or_zero(
            first_visible_to_unblocked_values, 0.99
        ),
        "read_recovery_detect_to_unblocked_p50_ns": percentile_or_zero(
            detect_to_unblocked_values, 0.50
        ),
        "read_recovery_detect_to_unblocked_p95_ns": percentile_or_zero(
            detect_to_unblocked_values, 0.95
        ),
        "read_recovery_detect_to_unblocked_p99_ns": percentile_or_zero(
            detect_to_unblocked_values, 0.99
        ),
        "read_recovery_detect_to_nack_mean_ns": round(statistics.fmean(detect_to_nack_values), 6)
        if detect_to_nack_values
        else 0.0,
        "read_recovery_nack_to_retransmit_mean_ns": round(
            statistics.fmean(nack_to_retransmit_values), 6
        )
        if nack_to_retransmit_values
        else 0.0,
        "read_recovery_retransmit_to_first_visible_mean_ns": round(
            statistics.fmean(retransmit_to_first_visible_values), 6
        )
        if retransmit_to_first_visible_values
        else 0.0,
        "read_recovery_first_visible_to_unblocked_mean_ns": round(
            statistics.fmean(first_visible_to_unblocked_values), 6
        )
        if first_visible_to_unblocked_values
        else 0.0,
        "read_recovery_detect_to_unblocked_mean_ns": round(
            statistics.fmean(detect_to_unblocked_values), 6
        )
        if detect_to_unblocked_values
        else 0.0,
    }


def classify_read_pre_failure_stage(row: dict[str, Any]) -> str:
    if not row["terminal_resource_exhaust"]:
        return "no-resource-exhaust"
    if row["arrival_block_reserved_at_ns"] > 0 and row["context_allocated_retry"]:
        return "context-allocated-retry-before-visible"
    if row["arrival_block_reserved_at_ns"] > 0:
        return "arrival-reserved-before-visible"
    if row["first_packet_no_context_count"] > 0:
        return "first-packet-no-context"
    return "target-registered-only"


def aggregate_read_pre_admission_rows(rows: list[dict[str, str]]) -> dict[str, Any]:
    pre_rows = build_read_pre_admission_rows(rows)
    retry_count = sum(1 for row in pre_rows if row["context_allocated_retry"])
    terminal_resource_exhaust_count = sum(
        1 for row in pre_rows if row["terminal_resource_exhaust"]
    )
    first_packet_no_context_count_total = sum(
        row["first_packet_no_context_count"] for row in pre_rows
    )
    arrival_reserve_fail_count_total = sum(
        row["arrival_reserve_fail_count"] for row in pre_rows
    )

    target_to_first_no_context_values = [
        float(row["target_to_first_no_context_ns"])
        for row in pre_rows
        if row["target_to_first_no_context_ns"] > 0
    ]
    target_to_arrival_reserved_values = [
        float(row["target_to_arrival_reserved_ns"])
        for row in pre_rows
        if row["target_to_arrival_reserved_ns"] > 0
    ]
    first_no_context_to_arrival_reserved_values = [
        float(row["first_no_context_to_arrival_reserved_ns"])
        for row in pre_rows
        if row["first_no_context_to_arrival_reserved_ns"] > 0
    ]
    arrival_reserved_to_first_visible_values = [
        float(row["arrival_reserved_to_first_response_visible_ns"])
        for row in pre_rows
        if row["arrival_reserved_to_first_response_visible_ns"] > 0
    ]
    target_hold_to_release_values = [
        float(row["target_hold_to_release_ns"])
        for row in pre_rows
        if row["target_hold_to_release_ns"] > 0
    ]
    arrival_hold_to_release_values = [
        float(row["arrival_hold_to_release_ns"])
        for row in pre_rows
        if row["arrival_hold_to_release_ns"] > 0
    ]

    failure_rows = [row for row in pre_rows if row["terminal_resource_exhaust"]]
    failure_stage_counts = {
        "target-registered-only": 0,
        "first-packet-no-context": 0,
        "arrival-reserved-before-visible": 0,
        "context-allocated-retry-before-visible": 0,
    }
    for row in failure_rows:
        stage = classify_read_pre_failure_stage(row)
        if stage in failure_stage_counts:
            failure_stage_counts[stage] += 1

    target_to_terminal_resource_exhaust_values = [
        float(row["target_to_terminal_resource_exhaust_ns"])
        for row in failure_rows
        if row["target_to_terminal_resource_exhaust_ns"] > 0
    ]
    first_no_context_to_terminal_resource_exhaust_values = [
        float(row["first_no_context_to_terminal_resource_exhaust_ns"])
        for row in failure_rows
        if row["first_no_context_to_terminal_resource_exhaust_ns"] > 0
    ]

    dominant_failure_stage = ""
    if any(failure_stage_counts.values()):
        dominant_failure_stage = max(
            failure_stage_counts.items(), key=lambda item: item[1]
        )[0]

    return {
        "read_pre_admission_sample_count": len(pre_rows),
        "read_pre_context_allocated_retry_count": retry_count,
        "read_pre_terminal_resource_exhaust_count": terminal_resource_exhaust_count,
        "read_pre_first_packet_no_context_count_total": first_packet_no_context_count_total,
        "read_pre_arrival_reserve_fail_count_total": arrival_reserve_fail_count_total,
        "read_pre_target_to_first_no_context_p50_ns": percentile_or_zero(
            target_to_first_no_context_values, 0.50
        ),
        "read_pre_target_to_first_no_context_p95_ns": percentile_or_zero(
            target_to_first_no_context_values, 0.95
        ),
        "read_pre_target_to_arrival_reserved_p50_ns": percentile_or_zero(
            target_to_arrival_reserved_values, 0.50
        ),
        "read_pre_target_to_arrival_reserved_p95_ns": percentile_or_zero(
            target_to_arrival_reserved_values, 0.95
        ),
        "read_pre_first_no_context_to_arrival_reserved_p50_ns": percentile_or_zero(
            first_no_context_to_arrival_reserved_values, 0.50
        ),
        "read_pre_first_no_context_to_arrival_reserved_p95_ns": percentile_or_zero(
            first_no_context_to_arrival_reserved_values, 0.95
        ),
        "read_pre_arrival_reserved_to_first_response_visible_p50_ns": percentile_or_zero(
            arrival_reserved_to_first_visible_values, 0.50
        ),
        "read_pre_arrival_reserved_to_first_response_visible_p95_ns": percentile_or_zero(
            arrival_reserved_to_first_visible_values, 0.95
        ),
        "read_pre_target_hold_to_release_p50_ns": percentile_or_zero(
            target_hold_to_release_values, 0.50
        ),
        "read_pre_target_hold_to_release_p95_ns": percentile_or_zero(
            target_hold_to_release_values, 0.95
        ),
        "read_pre_arrival_hold_to_release_p50_ns": percentile_or_zero(
            arrival_hold_to_release_values, 0.50
        ),
        "read_pre_arrival_hold_to_release_p95_ns": percentile_or_zero(
            arrival_hold_to_release_values, 0.95
        ),
        "read_pre_target_to_terminal_resource_exhaust_p50_ns": percentile_or_zero(
            target_to_terminal_resource_exhaust_values, 0.50
        ),
        "read_pre_target_to_terminal_resource_exhaust_p95_ns": percentile_or_zero(
            target_to_terminal_resource_exhaust_values, 0.95
        ),
        "read_pre_first_no_context_to_terminal_resource_exhaust_p50_ns": percentile_or_zero(
            first_no_context_to_terminal_resource_exhaust_values, 0.50
        ),
        "read_pre_first_no_context_to_terminal_resource_exhaust_p95_ns": percentile_or_zero(
            first_no_context_to_terminal_resource_exhaust_values, 0.95
        ),
        "read_pre_failure_target_registered_only_count": failure_stage_counts[
            "target-registered-only"
        ],
        "read_pre_failure_first_packet_no_context_only_count": failure_stage_counts[
            "first-packet-no-context"
        ],
        "read_pre_failure_arrival_reserved_before_visible_count": failure_stage_counts[
            "arrival-reserved-before-visible"
        ],
        "read_pre_failure_context_allocated_retry_count": failure_stage_counts[
            "context-allocated-retry-before-visible"
        ],
        "read_pre_failure_dominant_stage": dominant_failure_stage,
    }


def classify_read_recovery_farthest_stage(row: dict[str, Any]) -> str:
    if int(row.get("reassembly_unblocked_at_ns", 0)) > 0:
        return "reassembly-unblocked"
    if int(row.get("missing_fragment_first_visible_at_ns", 0)) > 0:
        return "missing-fragment-visible"
    if int(row.get("missing_fragment_retransmit_tx_at_ns", 0)) > 0:
        return "retransmit-tx"
    if int(row.get("gap_nack_observed_at_sender_ns", 0)) > 0:
        return "nack-observed-at-sender"
    if int(row.get("gap_nack_sent_at_ns", 0)) > 0:
        return "nack-sent"
    if int(row.get("gap_detected_at_ns", 0)) > 0:
        return "gap-detected"
    return "tracked-no-gap"


def aggregate_read_recovery_failure_rows(rows: list[dict[str, str]]) -> dict[str, Any]:
    progress_rows = build_read_recovery_progress(rows)
    failure_rows = [row for row in progress_rows if not row["success"]]
    failure_stage_counts = {
        "gap-detected": 0,
        "nack-sent": 0,
        "nack-observed-at-sender": 0,
        "retransmit-tx": 0,
        "missing-fragment-visible": 0,
        "reassembly-unblocked": 0,
    }
    for row in failure_rows:
        stage = classify_read_recovery_farthest_stage(row)
        if stage in failure_stage_counts:
            failure_stage_counts[stage] += 1

    detect_to_nack_values = [
        float(row["detect_to_nack_ns"]) for row in failure_rows if int(row["detect_to_nack_ns"]) > 0
    ]
    nack_to_observed_values = [
        float(row["nack_to_observed_at_sender_ns"])
        for row in failure_rows
        if int(row["nack_to_observed_at_sender_ns"]) > 0
    ]
    observed_to_retransmit_values = [
        float(row["observed_at_sender_to_retransmit_ns"])
        for row in failure_rows
        if int(row["observed_at_sender_to_retransmit_ns"]) > 0
    ]
    retransmit_to_terminal_values = [
        float(row["retransmit_to_terminal_ns"])
        for row in failure_rows
        if int(row["retransmit_to_terminal_ns"]) > 0
    ]
    detect_to_terminal_values = [
        float(row["detect_to_terminal_ns"])
        for row in failure_rows
        if int(row["detect_to_terminal_ns"]) > 0
    ]

    dominant_stage = ""
    if any(failure_stage_counts.values()):
        dominant_stage = max(failure_stage_counts.items(), key=lambda item: item[1])[0]

    return {
        "read_recovery_tracked_count": len(progress_rows),
        "read_recovery_failure_tracked_count": len(failure_rows),
        "read_recovery_failure_gap_detect_only_count": failure_stage_counts["gap-detected"],
        "read_recovery_failure_nack_sent_only_count": failure_stage_counts["nack-sent"],
        "read_recovery_failure_nack_observed_only_count": failure_stage_counts[
            "nack-observed-at-sender"
        ],
        "read_recovery_failure_retransmit_only_count": failure_stage_counts["retransmit-tx"],
        "read_recovery_failure_first_visible_only_count": failure_stage_counts[
            "missing-fragment-visible"
        ],
        "read_recovery_failure_reassembly_before_terminal_count": failure_stage_counts[
            "reassembly-unblocked"
        ],
        "read_recovery_failure_detect_to_nack_p50_ns": percentile_or_zero(
            detect_to_nack_values, 0.50
        ),
        "read_recovery_failure_detect_to_nack_p95_ns": percentile_or_zero(
            detect_to_nack_values, 0.95
        ),
        "read_recovery_failure_nack_to_observed_at_sender_p50_ns": percentile_or_zero(
            nack_to_observed_values, 0.50
        ),
        "read_recovery_failure_nack_to_observed_at_sender_p95_ns": percentile_or_zero(
            nack_to_observed_values, 0.95
        ),
        "read_recovery_failure_observed_at_sender_to_retransmit_p50_ns": percentile_or_zero(
            observed_to_retransmit_values, 0.50
        ),
        "read_recovery_failure_observed_at_sender_to_retransmit_p95_ns": percentile_or_zero(
            observed_to_retransmit_values, 0.95
        ),
        "read_recovery_failure_retransmit_to_terminal_p50_ns": percentile_or_zero(
            retransmit_to_terminal_values, 0.50
        ),
        "read_recovery_failure_retransmit_to_terminal_p95_ns": percentile_or_zero(
            retransmit_to_terminal_values, 0.95
        ),
        "read_recovery_failure_detect_to_terminal_p50_ns": percentile_or_zero(
            detect_to_terminal_values, 0.50
        ),
        "read_recovery_failure_detect_to_terminal_p95_ns": percentile_or_zero(
            detect_to_terminal_values, 0.95
        ),
        "read_recovery_failure_dominant_stage": dominant_stage,
    }


def aggregate_final_protocol_state(rows: list[dict[str, str]]) -> dict[str, Any]:
    final_by_device: dict[tuple[str, str], dict[str, str]] = {}
    for row in rows:
        key = (row.get("NodeId", ""), row.get("IfIndex", ""))
        final_by_device[key] = row

    result = {
        "ops_started_total_final": 0,
        "ops_terminal_total_final": 0,
        "ops_success_total_final": 0,
        "ops_failed_total_final": 0,
        "final_ops_in_flight": 0,
        "final_active_retry_states": 0,
        "final_active_read_response_states": 0,
        "final_read_track_in_use": 0,
        "final_tpdc_pending_sacks": 0,
        "final_tpdc_pending_gap_nacks": 0,
        "drain_completed": False,
    }
    for row in final_by_device.values():
        result["ops_started_total_final"] += to_int(row.get("OpsStartedTotal"))
        result["ops_terminal_total_final"] += to_int(row.get("OpsTerminalTotal"))
        result["ops_success_total_final"] += to_int(row.get("OpsSuccessTotal"))
        result["ops_failed_total_final"] += to_int(row.get("OpsFailedTotal"))
        result["final_ops_in_flight"] += to_int(row.get("OpsInFlight"))
        result["final_active_retry_states"] += to_int(row.get("ActiveRetryStates"))
        result["final_active_read_response_states"] += to_int(row.get("ActiveReadResponseStates"))
        result["final_read_track_in_use"] += to_int(row.get("ReadTrackInUse"))
        result["final_tpdc_pending_sacks"] += to_int(row.get("TpdcPendingSacks"))
        result["final_tpdc_pending_gap_nacks"] += to_int(row.get("TpdcPendingGapNacks"))

    started_total = float(result.get("ops_started_total_final", 0))
    if started_total > 0:
        result["semantic_completion_rate_pct_final"] = round(
            (float(result.get("ops_success_total_final", 0)) * 100.0) / started_total,
            6,
        )
        result["terminalization_rate_pct_final"] = round(
            (float(result.get("ops_terminal_total_final", 0)) * 100.0) / started_total,
            6,
        )
    else:
        result["semantic_completion_rate_pct_final"] = 0.0
        result["terminalization_rate_pct_final"] = 0.0

    result["drain_completed"] = (
        result["final_ops_in_flight"] == 0
        and result["final_active_retry_states"] == 0
        and result["final_active_read_response_states"] == 0
        and result["final_read_track_in_use"] == 0
        and result["final_tpdc_pending_sacks"] == 0
        and result["final_tpdc_pending_gap_nacks"] == 0
    )
    return result


def aggregate_failure_rows(rows: list[dict[str, str]]) -> dict[str, Any]:
    result = {"failure_rows": len(rows)}
    for domain in FAILURE_DOMAINS:
        result[f"{domain}_failures"] = 0
    for row in rows:
        domain = row.get("FailureDomain", "")
        if domain in FAILURE_DOMAINS:
            result[f"{domain}_failures"] += 1
    return result


def aggregate_diagnostic_rows(rows: list[dict[str, str]]) -> dict[str, Any]:
    return {"diagnostic_rows": len(rows)}


def build_peer_pair_progress(
    completion_rows: list[dict[str, str]],
    failure_rows: list[dict[str, str]],
    diagnostic_rows: list[dict[str, str]],
) -> dict[str, Any]:
    pairs: dict[tuple[int, int], dict[str, Any]] = {}

    def ensure(local_fep: int, peer_fep: int) -> dict[str, Any]:
        key = (local_fep, peer_fep)
        if key not in pairs:
            pairs[key] = {
                "local_fep": local_fep,
                "peer_fep": peer_fep,
                "success_completions": 0,
                "failed_completions": 0,
                "credit_gate_blocked": 0,
                "peer_queue_blocked": 0,
                "send_unexpected_no_match": 0,
                "response_timeout_exhausted": 0,
                "send_dispatch_started": 0,
                "send_response_closed_live": 0,
                "send_response_after_terminal": 0,
                "send_retry_no_response_progress": 0,
            }
        return pairs[key]

    for row in completion_rows:
        bucket = ensure(to_int(row.get("LocalFep")), to_int(row.get("PeerFep")))
        if to_int(row.get("Success")) != 0:
            bucket["success_completions"] += 1
        else:
            bucket["failed_completions"] += 1

    for row in failure_rows:
        bucket = ensure(to_int(row.get("LocalFep")), to_int(row.get("PeerFep")))
        stage = str(row.get("Stage", ""))
        if stage in {"send_admission_blocked", "write_budget_blocked", "read_response_budget_blocked"}:
            stage = "credit_gate_blocked"
        if stage in bucket:
            bucket[stage] += 1
        if str(row.get("DiagnosticText", "")) == "dispatch_deferred_waiting_for_peer_queue":
            bucket["peer_queue_blocked"] += 1

    for row in diagnostic_rows:
        name = str(row.get("Name", ""))
        if name not in {
            "SendDispatchStarted",
            "SendResponseClosedLiveRequest",
            "SendResponseAfterTerminal",
            "SendRetryNoResponseProgress",
        }:
            continue
        match = SEND_DIAGNOSTIC_DETAIL_RE.search(str(row.get("Detail", "")))
        if not match:
            continue
        bucket = ensure(to_int(row.get("LocalFep")), int(match.group(3)))
        if name == "SendDispatchStarted":
            bucket["send_dispatch_started"] += 1
        elif name == "SendResponseClosedLiveRequest":
            bucket["send_response_closed_live"] += 1
        elif name == "SendResponseAfterTerminal":
            bucket["send_response_after_terminal"] += 1
        elif name == "SendRetryNoResponseProgress":
            bucket["send_retry_no_response_progress"] += 1

    rows = sorted(
        pairs.values(),
        key=lambda item: (
            -int(item["response_timeout_exhausted"]),
            -int(item["send_unexpected_no_match"]),
            -int(item["send_response_after_terminal"]),
            item["local_fep"],
            item["peer_fep"],
        ),
    )

    def top_pair(metric: str) -> str:
        if not rows:
            return ""
        best = max(rows, key=lambda item: (int(item[metric]), -int(item["local_fep"]), -int(item["peer_fep"])))
        if int(best[metric]) <= 0:
            return ""
        return f"{best['local_fep']}->{best['peer_fep']}:{int(best[metric])}"

    return {
        "rows": rows,
        "top_timeout_pair": top_pair("response_timeout_exhausted"),
        "top_no_match_pair": top_pair("send_unexpected_no_match"),
        "top_late_response_pair": top_pair("send_response_after_terminal"),
        "top_peer_queue_pair": top_pair("peer_queue_blocked"),
        "top_live_response_pair": top_pair("send_response_closed_live"),
    }


def build_tpdc_session_progress(rows: list[dict[str, str]]) -> dict[str, Any]:
    latest_by_session: dict[tuple[int, int, int], dict[str, Any]] = {}

    for row in rows:
        local_fep = to_int(row.get("LocalFep"))
        remote_fep = to_int(row.get("RemoteFep"))
        pdc_id = to_int(row.get("PdcId"))
        key = (local_fep, remote_fep, pdc_id)
        time_ns = to_int(row.get("TimeNs"))
        candidate = {
            "time_ns": time_ns,
            "local_fep": local_fep,
            "remote_fep": remote_fep,
            "pdc_id": pdc_id,
            "tx_data_packets_total": to_int(row.get("TxDataPacketsTotal")),
            "tx_control_packets_total": to_int(row.get("TxControlPacketsTotal")),
            "rx_data_packets_total": to_int(row.get("RxDataPacketsTotal")),
            "rx_control_packets_total": to_int(row.get("RxControlPacketsTotal")),
            "ack_sent_total": to_int(row.get("AckSentTotal")),
            "ack_received_total": to_int(row.get("AckReceivedTotal")),
            "gap_nack_received_total": to_int(row.get("GapNackReceivedTotal")),
            "last_ack_sequence": to_int(row.get("LastAckSequence")),
            "send_window_base": to_int(row.get("SendWindowBase")),
            "next_send_sequence": to_int(row.get("NextSendSequence")),
            "send_buffer_size_current": to_int(row.get("SendBufferSizeCurrent")),
            "send_buffer_size_max": to_int(row.get("SendBufferSizeMax")),
            "retransmissions_total": to_int(row.get("RetransmissionsTotal")),
            "rto_timeouts_total": to_int(row.get("RtoTimeoutsTotal")),
            "ack_advance_events_total": to_int(row.get("AckAdvanceEventsTotal")),
        }
        existing = latest_by_session.get(key)
        if existing is None or candidate["time_ns"] >= existing["time_ns"]:
            if existing is not None:
                candidate["send_buffer_size_max"] = max(
                    candidate["send_buffer_size_max"],
                    int(existing.get("send_buffer_size_max", 0)),
                )
            latest_by_session[key] = candidate
        elif existing is not None:
            existing["send_buffer_size_max"] = max(
                int(existing.get("send_buffer_size_max", 0)),
                candidate["send_buffer_size_max"],
            )

    session_rows: list[dict[str, Any]] = []
    for record in latest_by_session.values():
        stuck_inflight = (
            int(record["send_buffer_size_current"]) > 0
            or int(record["rto_timeouts_total"]) > 0
            or int(record["retransmissions_total"]) > 0
        )
        if stuck_inflight and int(record["ack_received_total"]) == 0:
            stuck_reason_hint = "no_ack_seen"
        elif (
            stuck_inflight
            and int(record["ack_received_total"]) > 0
            and int(record["ack_advance_events_total"]) == 0
        ):
            stuck_reason_hint = "ack_seen_no_window_advance"
        elif (
            stuck_inflight
            and int(record["retransmissions_total"]) > 0
            and int(record["ack_advance_events_total"]) == 0
        ):
            stuck_reason_hint = "retransmit_without_ack_progress"
        else:
            stuck_reason_hint = ""
        record["stuck_inflight"] = stuck_inflight
        record["stuck_reason_hint"] = stuck_reason_hint
        session_rows.append(record)

    session_rows.sort(
        key=lambda item: (
            -int(item["send_buffer_size_current"]),
            -int(item["rto_timeouts_total"]),
            -int(item["retransmissions_total"]),
            int(item["local_fep"]),
            int(item["remote_fep"]),
            int(item["pdc_id"]),
        )
    )

    top_stuck = next((row for row in session_rows if bool(row.get("stuck_inflight"))), None)
    return {
        "rows": session_rows,
        "top_stuck_tpdc_session": (
            f"{top_stuck['local_fep']}->{top_stuck['remote_fep']}/pdc{top_stuck['pdc_id']}:"
            f"{top_stuck['send_buffer_size_current']}"
            if top_stuck
            else ""
        ),
        "top_stuck_tpdc_reason": str(top_stuck.get("stuck_reason_hint", "")) if top_stuck else "",
        "tpdc_sessions_with_no_ack_progress": sum(
            1 for row in session_rows if row.get("stuck_reason_hint") == "no_ack_seen"
        ),
        "tpdc_sessions_with_ack_but_no_closeout": sum(
            1
            for row in session_rows
            if int(row.get("ack_received_total", 0)) > 0 and int(row.get("send_buffer_size_current", 0)) > 0
        ),
    }


def validate_summary_consistency(summary: dict[str, Any]) -> str:
    if int(summary.get("completion_rows", 0)) <= 0:
        return "completion_csv_missing_or_empty"
    if int(summary.get("ops_terminal_total", 0)) < int(summary.get("ops_success_total", 0)):
        return "ops_terminal_total_less_than_ops_success_total"
    if int(summary.get("ops_success_total", 0)) != int(summary.get("completion_success_rows", 0)):
        return "ops_success_total_mismatch_completion_success_rows"
    if int(summary.get("send_success_total", 0)) != int(summary.get("send_success_completions_total", 0)):
        return "send_success_total_mismatch_completion_rows"
    if int(summary.get("write_success_total", 0)) != int(summary.get("write_success_completions_total", 0)):
        return "write_success_total_mismatch_completion_rows"
    if int(summary.get("read_success_total", 0)) != int(summary.get("read_success_completions_total", 0)):
        return "read_success_total_mismatch_completion_rows"
    if int(summary.get("send_success_bytes_total", 0)) != int(summary.get("send_success_payload_bytes_total", 0)):
        return "send_success_bytes_total_mismatch_completion_payload"
    if int(summary.get("write_success_bytes_total", 0)) != int(summary.get("write_success_payload_bytes_total", 0)):
        return "write_success_bytes_total_mismatch_completion_payload"
    if int(summary.get("read_success_bytes_total", 0)) != int(summary.get("read_success_payload_bytes_total", 0)):
        return "read_success_bytes_total_mismatch_completion_payload"
    if int(summary.get("ops_terminal_total_final", 0)) < int(summary.get("ops_success_total_final", 0)):
        return "ops_terminal_total_final_less_than_ops_success_total_final"
    if int(summary.get("ops_success_total_final", 0)) < int(summary.get("ops_success_total", 0)):
        return "ops_success_total_final_less_than_window_success_total"
    if float(summary.get("drain_window_seconds", 0.0)) > 0.0 and not bool(summary.get("drain_completed", False)):
        return "drain_not_completed"
    return ""


def copy_artifact(src: Path | None, dst: Path) -> str:
    if src is None or not src.exists():
        return ""
    shutil.copy2(src, dst)
    return str(dst)


def write_csv(path: Path, fieldnames: list[str], rows: list[dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fieldnames})


def mean_or_zero(values: list[float]) -> float:
    return round(statistics.fmean(values), 6) if values else 0.0


def min_or_zero(values: list[float]) -> float:
    return round(min(values), 6) if values else 0.0


def max_or_zero(values: list[float]) -> float:
    return round(max(values), 6) if values else 0.0


def stdev_or_zero(values: list[float]) -> float:
    if len(values) <= 1:
        return 0.0
    return round(statistics.stdev(values), 6)


def get_git_revision(repo: Path) -> str:
    completed = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=str(repo),
        text=True,
        capture_output=True,
        check=False,
    )
    return completed.stdout.strip() if completed.returncode == 0 else "unknown"


def build_profile_comparison(summary_rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, str, str, str, str, int, int, str], list[dict[str, Any]]] = defaultdict(list)
    for row in summary_rows:
        key = (
            str(row.get("variant_name", "default")),
            str(row.get("workload_name", "default")),
            str(row.get("tuning_name", "default")),
            str(row.get("timeout_budget_name", "default")),
            str(row.get("resource_tier_name", "default")),
            int(row["truthFlowCount"]),
            int(row["truthPayloadBytes"]),
            str(row["truthPressureProfile"]),
        )
        grouped[key].append(row)

    comparison_rows: list[dict[str, Any]] = []
    baseline_lookup: dict[tuple[str, str, str, str, str, int, int], float] = {}

    for (variant_name, workload_name, tuning_name, timeout_budget_name, resource_tier_name, flow_count, payload_bytes, profile), rows in sorted(grouped.items()):
        passed_rows = [row for row in rows if row["status"] == "passed"]
        compare_row: dict[str, Any] = {
            "variant_name": variant_name,
            "workload_name": workload_name,
            "tuning_name": tuning_name,
            "timeout_budget_name": timeout_budget_name,
            "timeout_budget_ms": round(float(rows[0].get("timeout_budget_ms", 0.0)), 6),
            "resource_tier_name": resource_tier_name,
            "truth_op_spacing_ns": int(rows[0].get("truth_op_spacing_ns", 0)),
            "device_mtu_bytes": int(rows[0].get("device_mtu_bytes", 0)),
            "payload_mtu_bytes": int(rows[0].get("payload_mtu_bytes", 0)),
            "truthFlowCount": flow_count,
            "truthPayloadBytes": payload_bytes,
            "truthPressureProfile": profile,
            "run_count": len(rows),
            "pass_count": len(passed_rows),
            "pass_rate_pct": round((len(passed_rows) * 100.0 / len(rows)), 6) if rows else 0.0,
            "drain_completed_rate_pct": round(
                (sum(1 for row in rows if bool(row.get("drain_completed", False))) * 100.0 / len(rows)),
                6,
            ) if rows else 0.0,
        }

        for metric in COMPARISON_METRICS:
            values = [float(row.get(metric, 0.0)) for row in passed_rows]
            compare_row[f"{metric}_mean"] = mean_or_zero(values)
            compare_row[f"{metric}_min"] = min_or_zero(values)
            compare_row[f"{metric}_max"] = max_or_zero(values)
            compare_row[f"{metric}_stdev"] = stdev_or_zero(values)

        compare_row["semantic_goodput_mbps_delta_vs_baseline_pct"] = 0.0
        comparison_rows.append(compare_row)
        if profile == "baseline":
            baseline_lookup[(variant_name, workload_name, tuning_name, timeout_budget_name, resource_tier_name, flow_count, payload_bytes)] = float(
                compare_row["semantic_goodput_mbps_mean"]
            )

    for row in comparison_rows:
        baseline = baseline_lookup.get(
            (
                str(row.get("variant_name", "default")),
                str(row.get("workload_name", "default")),
                str(row.get("tuning_name", "default")),
                str(row.get("timeout_budget_name", "default")),
                str(row.get("resource_tier_name", "default")),
                int(row["truthFlowCount"]),
                int(row["truthPayloadBytes"]),
            )
        )
        if baseline is None:
            continue
        if baseline > 0:
            row["semantic_goodput_mbps_delta_vs_baseline_pct"] = round(
                ((float(row["semantic_goodput_mbps_mean"]) - baseline) / baseline) * 100.0,
                6,
            )
    return comparison_rows


def summarize_profile_strength(rows: list[dict[str, Any]], metric_names: list[str]) -> str:
    if not rows:
        return "n/a"
    strongest = max(rows, key=lambda item: sum(float(item[f"{metric}_mean"]) for metric in metric_names))
    return str(strongest["truthPressureProfile"])


def render_report(
    report_path: Path,
    metadata: dict[str, Any],
    comparison_rows: list[dict[str, Any]],
) -> None:
    grouped: dict[tuple[str, str, str, str, int, int], list[dict[str, Any]]] = defaultdict(list)
    by_profile: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in comparison_rows:
        grouped[
            (
                str(row.get("workload_name", "default")),
                str(row.get("tuning_name", "default")),
                str(row.get("timeout_budget_name", "default")),
                str(row.get("resource_tier_name", "default")),
                int(row["truthFlowCount"]),
                int(row["truthPayloadBytes"]),
            )
        ].append(row)
        by_profile[str(row["truthPressureProfile"])].append(row)

    unique_device_mtus = sorted({int(row["device_mtu_bytes"]) for row in comparison_rows}) if comparison_rows else []
    unique_payload_mtus = sorted({int(row["payload_mtu_bytes"]) for row in comparison_rows}) if comparison_rows else []
    unique_variants = sorted({str(row.get("variant_name", "default")) for row in comparison_rows}) if comparison_rows else []
    unique_workloads = sorted({str(row.get("workload_name", "default")) for row in comparison_rows}) if comparison_rows else []
    unique_tunings = sorted({str(row.get("tuning_name", "default")) for row in comparison_rows}) if comparison_rows else []
    unique_timeout_budgets = sorted({str(row.get("timeout_budget_name", "default")) for row in comparison_rows}) if comparison_rows else []
    unique_resource_tiers = sorted({str(row.get("resource_tier_name", "default")) for row in comparison_rows}) if comparison_rows else []

    lines = [
        f"# {metadata['benchmark_name']}",
        "",
        "## Benchmark Identity",
        "",
        f"- Generated at: `{metadata['generated_at_utc']}`",
        f"- Git revision: `{metadata['git_revision']}`",
        f"- Runner version: `{metadata['runner_version']}`",
        f"- Matrix runs: `{metadata['matrix_run_count']}`",
        f"- Repetitions per combination: `{metadata['repetitions']}`",
        f"- Long-run gate enforced: `{metadata['enforce_long_run_gate']}`",
        f"- Drain window seconds: `{float(metadata.get('drain_window_seconds', 0.0)):.3f}`",
        f"- Device MTU bytes: `{', '.join(str(value) for value in unique_device_mtus) if unique_device_mtus else 0}`",
        f"- Payload MTU bytes: `{', '.join(str(value) for value in unique_payload_mtus) if unique_payload_mtus else 0}`",
        f"- Variants: `{', '.join(unique_variants) if unique_variants else 'default'}`",
        f"- Workloads: `{', '.join(unique_workloads) if unique_workloads else 'default'}`",
        f"- Tunings: `{', '.join(unique_tunings) if unique_tunings else 'default'}`",
        f"- Timeout budgets: `{', '.join(unique_timeout_budgets) if unique_timeout_budgets else 'default'}`",
        f"- Resource tiers: `{', '.join(unique_resource_tiers) if unique_resource_tiers else 'default'}`",
        "",
        "## KPI Semantics",
        "",
        f"- `{HEADLINE_KPI_NAME}` is the headline semantic throughput KPI. It is derived from successful requester-side completion payload bytes divided by the workload window `simulationTime`.",
        "- `semantic_completion_rate_pct` and `terminalization_rate_pct` are workload-window rates only.",
        "- `semantic_completion_rate_pct_final` and `terminalization_rate_pct_final` are computed after the optional drain window and should be used for final completion conclusions.",
        "- `injection_ceiling_mbps` is the workload-window semantic injection ceiling implied by flow count, ops per flow, payload bytes, and `simulationTime`.",
        "- `semantic_goodput_utilization_pct` is headline semantic goodput divided by that injection ceiling.",
        "- `success_latency_p50_us/p95_us/p99_us` are computed from successful requester-side completion records only.",
        "- `device_tx_egress_mbps` is a transport-efficiency supporting metric. It is sender-side device egress derived from workload-window protocol bytes divided by workload-window `simulationTime`.",
        "- `device_rx_observed_mbps` is diagnostic-only. It can be zero or under-reported because semantic receive placement may complete inside `PDS/SES` without flowing through `DeliverReceivedPacket()`.",
        "- Per-opcode success counts, payload bytes, and semantic goodput are derived from requester-side completion records and are valid for internal drill-down only after the long-run gate passes.",
        "- Control-plane, resource, packet-reliability, and failure-domain counters are pressure evidence. They explain why a run behaved a certain way; they are not headline performance KPIs.",
        "",
        "## Global Conclusions",
        "",
        f"- Strongest control-plane pressure profile: `{summarize_profile_strength(list(comparison_rows), ['credit_gate_blocked_total', 'credit_refresh_sent_total', 'ack_ctrl_ext_sent_total'])}`",
        f"- Strongest packet-reliability pressure profile: `{summarize_profile_strength(list(comparison_rows), ['peak_tpdc_pending_sacks', 'peak_tpdc_pending_gap_nacks', 'sack_sent_total', 'gap_nack_sent_total'])}`",
        f"- Strongest resource pressure profile: `{summarize_profile_strength(list(comparison_rows), ['unexpected_alloc_failures_total', 'arrival_alloc_failures_total', 'read_track_alloc_failures_total'])}`",
        f"- Highest semantic goodput profile across the matrix: `{summarize_profile_strength(list(comparison_rows), ['semantic_goodput_mbps'])}`",
        "- Not claimed in this phase: receiver-side observed Mbps as a performance KPI, legacy_sue benchmark conclusions, external NIC performance.",
        "- Profiles that fail the long-run gate or run-level consistency checks are excluded from internal performance conclusions.",
        "",
    ]

    if metadata.get("enforce_long_run_gate"):
        gate = metadata.get("long_run_gate", {})
        lines.extend(
            [
                "## Long-Run Gate",
                "",
                f"- Gate status: `{gate.get('status', 'unknown')}`",
                f"- Gate command: `{gate.get('command', './build/soft-ue-truth-pressure-system-test --long')}`",
                "",
            ]
        )

    for (workload_name, tuning_name, timeout_budget_name, resource_tier_name, flow_count, payload_bytes), rows in sorted(grouped.items()):
        rows = sorted(
            rows,
            key=lambda item: (
                str(item.get("timeout_budget_name", "default")),
                str(item.get("resource_tier_name", "default")),
                str(item.get("variant_name", "default")),
                str(item["truthPressureProfile"]),
            ),
        )
        incomplete = any(float(row["pass_rate_pct"]) < 100.0 for row in rows)
        lines.extend(
            [
                f"## workload={workload_name}, tuning={tuning_name}, timeout_budget={timeout_budget_name}, resource_tier={resource_tier_name}, flow_count={flow_count}, payload={payload_bytes}",
                "",
                f"- Benchmark status: `{'incomplete' if incomplete else 'complete'}`",
                "",
                "| variant | tuning | timeout_budget | resource_tier | profile | op_spacing_ns | pass/run | pass_rate_pct | drain_completed_rate_pct | semantic_goodput_mean | injection_ceiling_mean | utilization_pct_mean | window_completion_mean | final_completion_mean | final_terminal_mean | p95_latency_us_mean | credit_gate_mean | pending_sacks_mean |",
                "| --- | --- | --- | --- | --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
            ]
        )
        for row in rows:
            lines.append(
                "| {variant} | {tuning} | {timeout_budget} | {resource_tier} | {profile} | {op_spacing_ns} | {pass_count}/{run_count} | {pass_rate:.2f} | {drain_rate:.2f} | {goodput_mean:.6f} | {ceiling_mean:.6f} | {utilization_mean:.2f} | {completion_mean:.2f} | {completion_final_mean:.2f} | {terminal_final_mean:.2f} | {latency_p95:.3f} | {blocked:.2f} | {sacks:.2f} |".format(
                    variant=row.get("variant_name", "default"),
                    tuning=row.get("tuning_name", "default"),
                    timeout_budget=row.get("timeout_budget_name", "default"),
                    resource_tier=row.get("resource_tier_name", "default"),
                    profile=row["truthPressureProfile"],
                    op_spacing_ns=int(row.get("truth_op_spacing_ns", 0)),
                    pass_count=row["pass_count"],
                    run_count=row["run_count"],
                    pass_rate=float(row["pass_rate_pct"]),
                    drain_rate=float(row.get("drain_completed_rate_pct", 0.0)),
                    goodput_mean=float(row["semantic_goodput_mbps_mean"]),
                    ceiling_mean=float(row["injection_ceiling_mbps_mean"]),
                    utilization_mean=float(row["semantic_goodput_utilization_pct_mean"]),
                    completion_mean=float(row["semantic_completion_rate_pct_mean"]),
                    completion_final_mean=float(row["semantic_completion_rate_pct_final_mean"]),
                    terminal_final_mean=float(row["terminalization_rate_pct_final_mean"]),
                    latency_p95=float(row["success_latency_p95_us_mean"]),
                    blocked=float(row["credit_gate_blocked_total_mean"]),
                    sacks=float(row["peak_tpdc_pending_sacks_mean"]),
                )
            )
        lines.append("")
        sender_rank = sorted(rows, key=lambda item: float(item["semantic_goodput_mbps_mean"]), reverse=True)
        strongest_control = summarize_profile_strength(
            rows,
            ["credit_gate_blocked_total", "credit_refresh_sent_total", "ack_ctrl_ext_sent_total"],
        )
        strongest_packet = summarize_profile_strength(
            rows,
            ["peak_tpdc_pending_sacks", "peak_tpdc_pending_gap_nacks", "sack_sent_total", "gap_nack_sent_total"],
        )
        strongest_resource = summarize_profile_strength(
            rows,
            ["unexpected_alloc_failures_total", "arrival_alloc_failures_total", "read_track_alloc_failures_total"],
        )
        strongest_failure_domain = summarize_profile_strength(
            rows,
            ["validation_failures", "resource_failures", "control_plane_failures", "packet_reliability_failures", "message_lifecycle_failures"],
        )
        lines.extend(
            [
                "- Semantic goodput ranking: `{}`".format(
                    " > ".join(
                        "{variant}:{tuning}:{resource}:{profile}".format(
                            variant=item.get("variant_name", "default"),
                            tuning=item.get("tuning_name", "default"),
                            resource=item.get("resource_tier_name", "default"),
                            profile=item["truthPressureProfile"],
                        )
                        for item in sender_rank
                    )
                ),
                "- Lowest p95 latency profile: `{}`".format(
                    "{variant}:{tuning}:{resource}:{profile}".format(
                        variant=min(rows, key=lambda item: float(item["success_latency_p95_us_mean"])).get("variant_name", "default"),
                        tuning=min(rows, key=lambda item: float(item["success_latency_p95_us_mean"])).get("tuning_name", "default"),
                        resource=min(rows, key=lambda item: float(item["success_latency_p95_us_mean"])).get("resource_tier_name", "default"),
                        profile=min(rows, key=lambda item: float(item["success_latency_p95_us_mean"]))["truthPressureProfile"],
                    )
                ),
                f"- Strongest control-plane pressure: `{strongest_control}`",
                f"- Strongest packet-reliability pressure: `{strongest_packet}`",
                f"- Strongest resource pressure: `{strongest_resource}`",
                f"- Heaviest failure-domain footprint: `{strongest_failure_domain}`",
                "",
                "| variant | tuning | timeout_budget | resource_tier | profile | send_goodput_mean | write_goodput_mean | read_goodput_mean | send_success_mean | write_success_mean | read_success_mean |",
                "| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |",
            ]
        )
        for row in rows:
            lines.append(
                "| {variant} | {tuning} | {timeout_budget} | {resource_tier} | {profile} | {send_gp:.6f} | {write_gp:.6f} | {read_gp:.6f} | {send_ok:.2f} | {write_ok:.2f} | {read_ok:.2f} |".format(
                    variant=row.get("variant_name", "default"),
                    tuning=row.get("tuning_name", "default"),
                    timeout_budget=row.get("timeout_budget_name", "default"),
                    resource_tier=row.get("resource_tier_name", "default"),
                    profile=row["truthPressureProfile"],
                    send_gp=float(row["send_semantic_goodput_mbps_mean"]),
                    write_gp=float(row["write_semantic_goodput_mbps_mean"]),
                    read_gp=float(row["read_semantic_goodput_mbps_mean"]),
                    send_ok=float(row["send_success_completions_total_mean"]),
                    write_ok=float(row["write_success_completions_total_mean"]),
                    read_ok=float(row["read_success_completions_total_mean"]),
                )
            )
        lines.append("")

    lines.extend(
        [
            "## Phase 4.9 Boundary",
            "",
            "- Current phase can support internal truth-backed semantic performance benchmark, profile comparison, repetition-based stability, and pressure evidence attribution.",
            "- `device_rx_observed_mbps` remains diagnostic-only and must not be used for headline performance conclusions.",
            "- Internal performance conclusions require a passing long-run gate across baseline, credit_pressure, lossy, and mixed.",
            "- Next phase should add broader pressure sweeps and any additional external-facing performance surfaces only if needed.",
            "",
        ]
    )

    report_path.write_text("\n".join(lines), encoding="utf-8")


def print_terminal_summary(comparison_rows: list[dict[str, Any]]) -> None:
    if not comparison_rows:
        print("No comparison rows generated.")
        return
    header = (
        "variant".ljust(16)
        + "workload".ljust(14)
        + "tuning".ljust(12)
        + "resource".ljust(20)
        + "timeout".ljust(14)
        + "profile".ljust(18)
        + "flow_count".rjust(12)
        + "payload".rjust(12)
        + "semantic_gp".rjust(16)
        + "window_comp".rjust(12)
        + "final_comp".rjust(12)
        + "final_term".rjust(12)
        + "p95_us".rjust(12)
        + "pass_rate".rjust(12)
        + "credit_gate".rjust(16)
        + "unexpected_alloc".rjust(20)
        + "pending_sacks".rjust(16)
    )
    print(header)
    print("-" * len(header))
    for row in comparison_rows:
        print(
            str(row.get("variant_name", "default")).ljust(16)
            + str(row.get("workload_name", "default")).ljust(14)
            + str(row.get("tuning_name", "default")).ljust(12)
            + str(row.get("resource_tier_name", "default")).ljust(20)
            + str(row.get("timeout_budget_name", "default")).ljust(14)
            + str(row["truthPressureProfile"]).ljust(18)
            + str(row["truthFlowCount"]).rjust(12)
            + str(row["truthPayloadBytes"]).rjust(12)
            + f"{float(row['semantic_goodput_mbps_mean']):.3f}".rjust(16)
            + f"{float(row['semantic_completion_rate_pct_mean']):.1f}".rjust(12)
            + f"{float(row['semantic_completion_rate_pct_final_mean']):.1f}".rjust(12)
            + f"{float(row['terminalization_rate_pct_final_mean']):.1f}".rjust(12)
            + f"{float(row['success_latency_p95_us_mean']):.3f}".rjust(12)
            + f"{float(row['pass_rate_pct']):.1f}".rjust(12)
            + f"{float(row['credit_gate_blocked_total_mean']):.3f}".rjust(16)
            + f"{float(row['unexpected_alloc_failures_total_mean']):.3f}".rjust(20)
            + f"{float(row['peak_tpdc_pending_sacks_mean']):.3f}".rjust(16)
        )


def run_process(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=str(cwd),
        text=True,
        capture_output=True,
        check=False,
    )


def execute_long_run_gate(repo: Path, output_dir: Path) -> dict[str, Any]:
    gate_dir = output_dir / "long-run-gate"
    gate_dir.mkdir(parents=True, exist_ok=True)
    command = ["./build/soft-ue-truth-pressure-system-test", "--long"]
    completed = run_process(command, repo)
    stdout_path = gate_dir / "stdout.log"
    stderr_path = gate_dir / "stderr.log"
    stdout_path.write_text(completed.stdout, encoding="utf-8")
    stderr_path.write_text(completed.stderr, encoding="utf-8")
    result = {
        "command": " ".join(command),
        "status": "passed" if completed.returncode == 0 else "failed",
        "returncode": completed.returncode,
        "stdout_path": str(stdout_path),
        "stderr_path": str(stderr_path),
    }
    (gate_dir / "result.json").write_text(json.dumps(result, indent=2, sort_keys=True), encoding="utf-8")
    if completed.returncode != 0:
        raise RuntimeError(
            "long-run gate failed; refusing to generate internal performance report. "
            f"See {stdout_path} and {stderr_path}."
        )
    return result


def execute_run(repo: Path, run_spec: dict[str, Any], runs_dir: Path, *, drain_window_seconds: float = 0.0) -> dict[str, Any]:
    run_id = str(run_spec["run_id"])
    run_dir = runs_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)

    window_simulation_time = float(run_spec["params"].get("simulationTime", 0.0))
    command = build_benchmark_command(run_spec["params"], drain_window_seconds)
    command_text = " ".join(command)
    (run_dir / "command.txt").write_text(command_text + "\n", encoding="utf-8")

    completed = run_process(command, repo)
    stdout_path = run_dir / "stdout.log"
    stderr_path = run_dir / "stderr.log"
    stdout_path.write_text(completed.stdout, encoding="utf-8")
    stderr_path.write_text(completed.stderr, encoding="utf-8")

    logs = parse_log_paths(completed.stdout + "\n" + completed.stderr)
    protocol_dst = run_dir / "protocol.csv"
    completion_dst = run_dir / "completion.csv"
    failure_dst = run_dir / "failure.csv"
    diagnostic_dst = run_dir / "diagnostic.csv"
    tpdc_session_progress_dst = run_dir / "tpdc-session-progress.csv"

    protocol_artifact = copy_artifact(resolve_artifact_path(repo, logs["protocol"]), protocol_dst)
    completion_artifact = copy_artifact(resolve_artifact_path(repo, logs["completion"]), completion_dst)
    failure_artifact = copy_artifact(resolve_artifact_path(repo, logs["failure"]), failure_dst)
    diagnostic_artifact = copy_artifact(resolve_artifact_path(repo, logs["diagnostic"]), diagnostic_dst)
    tpdc_session_progress_artifact_csv = copy_artifact(
        resolve_artifact_path(repo, logs["tpdc_session_progress"]),
        tpdc_session_progress_dst,
    )

    protocol_rows = read_csv_rows(protocol_dst if protocol_artifact else None)
    completion_rows = read_csv_rows(completion_dst if completion_artifact else None)
    failure_rows = read_csv_rows(failure_dst if failure_artifact else None)
    diagnostic_rows = read_csv_rows(diagnostic_dst if diagnostic_artifact else None)
    send_stage_latency_rows = build_send_stage_latency(completion_rows)
    read_stage_latency_rows = build_read_stage_latency(completion_rows)
    read_recovery_latency_rows = build_read_recovery_latency(completion_rows)
    tpdc_session_progress_rows = read_csv_rows(tpdc_session_progress_dst if tpdc_session_progress_artifact_csv else None)
    peer_pair_progress = build_peer_pair_progress(completion_rows, failure_rows, diagnostic_rows)
    peer_pair_progress_artifact = run_dir / "peer-pair-progress.json"
    peer_pair_progress_artifact.write_text(
        json.dumps(peer_pair_progress, indent=2, sort_keys=True),
        encoding="utf-8",
    )
    tpdc_session_progress = build_tpdc_session_progress(tpdc_session_progress_rows)
    tpdc_session_progress_artifact = run_dir / "tpdc-session-progress.json"
    tpdc_session_progress_artifact.write_text(
        json.dumps(tpdc_session_progress, indent=2, sort_keys=True),
        encoding="utf-8",
    )
    fabric_path_progress_artifact = run_dir / "fabric-path-progress.json"
    send_stage_latency_artifact = run_dir / "send-stage-latency.json"
    send_stage_latency_artifact.write_text(
        json.dumps(send_stage_latency_rows, indent=2, sort_keys=True),
        encoding="utf-8",
    )
    read_stage_latency_artifact = run_dir / "read-stage-latency.json"
    read_stage_latency_artifact.write_text(
        json.dumps(read_stage_latency_rows, indent=2, sort_keys=True),
        encoding="utf-8",
    )
    read_recovery_latency_artifact = run_dir / "read-recovery-latency.json"
    read_recovery_latency_artifact.write_text(
        json.dumps(read_recovery_latency_rows, indent=2, sort_keys=True),
        encoding="utf-8",
    )

    simulation_time = window_simulation_time
    drain_timeout_ns = int(drain_window_seconds * 1_000_000_000.0)
    window_end_ns = int(simulation_time * 1_000_000_000.0) if simulation_time > 0.0 else None
    window_protocol_rows = filter_rows_by_time_ns(protocol_rows, window_end_ns)
    window_completion_rows = filter_rows_by_time_ns(completion_rows, window_end_ns)
    device_mtu_bytes = int(run_spec["params"].get("Mtu", 2500))
    payload_mtu_bytes = int(run_spec["params"].get("PayloadMtu", DEFAULT_PAYLOAD_MTU_BYTES))
    truth_op_spacing_ns = int(run_spec["params"].get("truthOpSpacingNs", 0))
    injection_ceiling_mbps = 0.0
    if simulation_time > 0.0:
        injection_ceiling_mbps = round(
            (
                int(run_spec["params"]["truthFlowCount"])
                * int(run_spec["params"].get("truthOpsPerFlow", 0))
                * int(run_spec["params"]["truthPayloadBytes"])
                * 8.0
            )
            / simulation_time
            / 1_000_000.0,
            6,
        )
    summary = {
        "run_id": run_id,
        "status": "failed",
        "status_reason": "",
        "variant_name": str(run_spec.get("variant_name", "default")),
        "workload_name": str(run_spec.get("workload_name", "default")),
        "tuning_name": str(run_spec.get("tuning_name", "default")),
        "timeout_budget_name": str(run_spec.get("timeout_budget_name", "default")),
        "timeout_budget_ms": round(float(run_spec["params"].get("truthRetryTimeoutNs", 80_000_000)) / 1_000_000.0, 6),
        "resource_tier_name": str(run_spec.get("resource_tier_name", "default")),
        "truthPressureProfile": run_spec["params"]["truthPressureProfile"],
        "truthFlowCount": int(run_spec["params"]["truthFlowCount"]),
        "truthPayloadBytes": int(run_spec["params"]["truthPayloadBytes"]),
        "device_mtu_bytes": device_mtu_bytes,
        "payload_mtu_bytes": payload_mtu_bytes,
        "repetition": int(run_spec["repetition"]),
        "nXpus": int(run_spec["params"].get("nXpus", 0)),
        "portsPerXpu": int(run_spec["params"].get("portsPerXpu", 0)),
        "truthOpsPerFlow": int(run_spec["params"].get("truthOpsPerFlow", 0)),
        "truth_link_data_rate": str(run_spec["params"].get("truthLinkDataRate", "1Gbps")),
        "fabric_topology_mode": str(run_spec["params"].get("fabricTopologyMode", "shared_truth")),
        "fabric_endpoint_mode": str(run_spec["params"].get("fabricEndpointMode", "six_endpoint")),
        "fabric_path_count_cfg": int(run_spec["params"].get("fabricPathCount", 1)),
        "fabric_path_data_rate": str(run_spec["params"].get("fabricPathDataRate", run_spec["params"].get("truthLinkDataRate", "1Gbps"))),
        "fabric_traffic_pattern": str(run_spec["params"].get("fabricTrafficPattern", "all_to_all")),
        "fabric_dynamic_routing_enabled_cfg": bool(run_spec["params"].get("fabricDynamicPathSelection", False)),
        "truth_op_spacing_ns": truth_op_spacing_ns,
        "simulationTime": simulation_time,
        "drain_window_seconds": drain_window_seconds,
        "drain_timeout_ns": drain_timeout_ns,
        "headline_kpi_name": HEADLINE_KPI_NAME,
        "headline_kpi_class": HEADLINE_KPI_CLASS,
        "device_tx_egress_kpi_class": DEVICE_TX_EGRESS_KPI_CLASS,
        "device_rx_observed_kpi_class": RX_DIAGNOSTIC_KPI_CLASS,
        "stdout_path": str(stdout_path),
        "stderr_path": str(stderr_path),
        "protocol_artifact": protocol_artifact,
        "completion_artifact": completion_artifact,
        "send_stage_latency_artifact": str(send_stage_latency_artifact),
        "read_stage_latency_artifact": str(read_stage_latency_artifact),
        "read_recovery_latency_artifact": str(read_recovery_latency_artifact),
        "failure_artifact": failure_artifact,
        "diagnostic_artifact": diagnostic_artifact,
        "peer_pair_progress_artifact": str(peer_pair_progress_artifact),
        "tpdc_session_progress_artifact": str(tpdc_session_progress_artifact),
        "fabric_path_progress_artifact": str(fabric_path_progress_artifact),
        "top_timeout_pair": peer_pair_progress["top_timeout_pair"],
        "top_no_match_pair": peer_pair_progress["top_no_match_pair"],
        "top_late_response_pair": peer_pair_progress["top_late_response_pair"],
        "top_peer_queue_pair": peer_pair_progress["top_peer_queue_pair"],
        "top_live_response_pair": peer_pair_progress["top_live_response_pair"],
        "top_stuck_tpdc_session": tpdc_session_progress["top_stuck_tpdc_session"],
        "top_stuck_tpdc_reason": tpdc_session_progress["top_stuck_tpdc_reason"],
        "tpdc_sessions_with_no_ack_progress": tpdc_session_progress["tpdc_sessions_with_no_ack_progress"],
        "tpdc_sessions_with_ack_but_no_closeout": tpdc_session_progress["tpdc_sessions_with_ack_but_no_closeout"],
        "injection_ceiling_mbps": injection_ceiling_mbps,
    }
    summary.update(aggregate_protocol_rows(window_protocol_rows, simulation_time))
    summary.update(aggregate_completion_rows(window_completion_rows, simulation_time))
    summary.update(aggregate_send_stage_rows(window_completion_rows))
    summary.update(aggregate_read_stage_rows(window_completion_rows))
    summary.update(aggregate_read_pre_admission_rows(window_completion_rows))
    summary.update(aggregate_read_recovery_rows(window_completion_rows))
    summary.update(aggregate_read_recovery_failure_rows(window_completion_rows))
    summary.update(aggregate_final_protocol_state(protocol_rows))
    summary.update(aggregate_failure_rows(failure_rows))
    summary.update(aggregate_diagnostic_rows(diagnostic_rows))
    started_total = float(summary.get("ops_started_total", 0))
    if started_total > 0:
        summary["semantic_completion_rate_pct"] = round(
            (float(summary.get("ops_success_total", 0)) * 100.0) / started_total,
            6,
        )
        summary["terminalization_rate_pct"] = round(
            (float(summary.get("ops_terminal_total", 0)) * 100.0) / started_total,
            6,
        )
    else:
        summary["semantic_completion_rate_pct"] = 0.0
        summary["terminalization_rate_pct"] = 0.0
    summary["semantic_goodput_utilization_pct"] = round(
        (float(summary.get("semantic_goodput_mbps", 0.0)) * 100.0 / injection_ceiling_mbps),
        6,
    ) if injection_ceiling_mbps > 0.0 else 0.0
    fabric_total_capacity_mbps = 0.0
    if summary["fabric_topology_mode"] == "explicit_multipath":
        fabric_total_capacity_mbps = (
            parse_data_rate_mbps(summary["fabric_path_data_rate"]) * float(summary["fabric_path_count_cfg"])
        )
    else:
        fabric_total_capacity_mbps = parse_data_rate_mbps(summary["truth_link_data_rate"])
    summary["fabric_total_goodput_mbps"] = float(summary.get("semantic_goodput_mbps", 0.0))
    summary["fabric_total_capacity_gbps"] = (
        round(fabric_total_capacity_mbps / 1000.0, 6) if fabric_total_capacity_mbps > 0.0 else 0.0
    )
    summary["fabric_utilization_pct"] = (
        round((float(summary.get("fabric_total_offered_mbps", 0.0)) * 100.0) / fabric_total_capacity_mbps, 6)
        if fabric_total_capacity_mbps > 0.0
        else 0.0
    )
    summary["semantic_link_utilization_pct"] = (
        round((float(summary.get("semantic_goodput_mbps", 0.0)) * 100.0) / fabric_total_capacity_mbps, 6)
        if fabric_total_capacity_mbps > 0.0
        else 0.0
    )
    fabric_path_progress = {
        "path_count": int(summary.get("fabric_path_count", 0)),
        "top_hotspot_endpoint": int(summary.get("fabric_top_hotspot_endpoint", 0)),
        "ecn_mark_total": int(summary.get("fabric_ecn_mark_total", 0)),
        "paths": [
            {
                "path_index": path_idx,
                "tx_bytes": int(summary.get(f"fabric_path{path_idx}_tx_bytes", 0)),
                "queue_depth_max": int(summary.get(f"fabric_path{path_idx}_queue_depth_max", 0)),
                "flow_count": int(summary.get(f"fabric_path{path_idx}_flow_count", 0)),
            }
            for path_idx in range(8)
        ],
        "dynamic_routing_enabled": bool(summary.get("fabric_dynamic_routing_enabled", 0)),
        "dynamic_assignment_total": int(summary.get("fabric_dynamic_assignment_total", 0)),
        "dynamic_path_reuse_total": int(summary.get("fabric_dynamic_path_reuse_total", 0)),
        "active_flow_assignments_peak": int(summary.get("fabric_active_flow_assignments_peak", 0)),
        "path_score_min_ns": int(summary.get("fabric_path_score_min_ns", 0)),
        "path_score_max_ns": int(summary.get("fabric_path_score_max_ns", 0)),
        "path_score_mean_ns": int(summary.get("fabric_path_score_mean_ns", 0)),
        "path_skew_ratio": float(summary.get("fabric_path_skew_ratio", 0.0)),
    }
    fabric_path_progress_artifact.write_text(
        json.dumps(fabric_path_progress, indent=2, sort_keys=True),
        encoding="utf-8",
    )

    if completed.returncode != 0:
        summary["status_reason"] = f"run_exit_{completed.returncode}"
    elif len(protocol_rows) <= 0:
        summary["status_reason"] = "protocol_csv_missing_or_empty"
    else:
        summary["status_reason"] = validate_summary_consistency(summary)
    summary["status"] = "passed" if not summary["status_reason"] else "failed"

    (run_dir / "kpi.json").write_text(json.dumps(summary, indent=2, sort_keys=True), encoding="utf-8")
    return summary


def prepare_output_dir(config: dict[str, Any], output_root: Path) -> Path:
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%SZ")
    target = output_root / f"{config['name']}_{timestamp}"
    target.mkdir(parents=True, exist_ok=False)
    (target / "runs").mkdir(parents=True, exist_ok=True)
    return target


def maybe_build(repo: Path) -> None:
    completed = run_process(["./ns3", "build"], repo)
    if completed.returncode != 0:
        raise RuntimeError(
            "ns3 build failed\nSTDOUT:\n{stdout}\nSTDERR:\n{stderr}".format(
                stdout=completed.stdout,
                stderr=completed.stderr,
            )
        )


def build_metadata(
    repo: Path,
    config_path: Path,
    config: dict[str, Any],
    runs: list[dict[str, Any]],
    output_dir: Path,
    *,
    enforce_long_run_gate: bool,
) -> dict[str, Any]:
    return {
        "benchmark_name": config["name"],
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "git_revision": get_git_revision(repo),
        "runner_version": RUNNER_VERSION,
        "config_path": str(config_path),
        "output_dir": str(output_dir),
        "matrix_run_count": len(runs),
        "repetitions": int(config["repetitions"]),
        "defaults": deepcopy(config["defaults"]),
        "profile_overrides": deepcopy(config["profile_overrides"]),
        "variant_overrides": deepcopy(config.get("variant_overrides", {})),
        "workload_overrides": deepcopy(config.get("workload_overrides", {})),
        "tuning_overrides": deepcopy(config.get("tuning_overrides", {})),
        "timeout_overrides": deepcopy(config.get("timeout_overrides", {})),
        "resource_overrides": deepcopy(config.get("resource_overrides", {})),
        "drain_window_seconds": float(config.get("drain_window_seconds", 0.0)),
        "matrix": deepcopy(config["matrix"]),
        "expanded_matrix_file": str(output_dir / config["outputs"]["expanded_matrix_json"]),
        "enforce_long_run_gate": enforce_long_run_gate,
    }


def run_benchmark_matrix(
    config_path: Path,
    output_root: Path,
    *,
    force_build: bool = False,
    dry_run: bool = False,
    filter_profiles: set[str] | None = None,
    enforce_long_run_gate: bool = False,
) -> dict[str, Any]:
    repo = repo_root()
    config = load_config(config_path)
    validate_truth_benchmark_config(config)
    runs = expand_matrix(config, filter_profiles=filter_profiles)
    target_dir = prepare_output_dir(config, output_root)
    runs_dir = target_dir / "runs"
    expanded_matrix_path = target_dir / config["outputs"]["expanded_matrix_json"]
    expanded_matrix_path.write_text(json.dumps(runs, indent=2, sort_keys=True), encoding="utf-8")

    metadata = build_metadata(
        repo,
        config_path,
        config,
        runs,
        target_dir,
        enforce_long_run_gate=enforce_long_run_gate,
    )
    (target_dir / config["outputs"]["metadata_json"]).write_text(
        json.dumps(metadata, indent=2, sort_keys=True),
        encoding="utf-8",
    )

    if force_build or config["build_before_run"]:
        if dry_run:
            print("./ns3 build")
        else:
            maybe_build(repo)

    if dry_run:
        if enforce_long_run_gate:
            print("./build/soft-ue-truth-pressure-system-test --long")
        for run_spec in runs:
            print(" ".join(build_benchmark_command(run_spec["params"], float(config.get("drain_window_seconds", 0.0)))))
        return {
            "output_dir": target_dir,
            "summary_rows": [],
            "comparison_rows": [],
            "runs": runs,
            "metadata": metadata,
        }

    long_run_gate_result: dict[str, Any] | None = None
    if enforce_long_run_gate:
        long_run_gate_result = execute_long_run_gate(repo, target_dir)
        metadata["long_run_gate"] = long_run_gate_result
        (target_dir / config["outputs"]["metadata_json"]).write_text(
            json.dumps(metadata, indent=2, sort_keys=True),
            encoding="utf-8",
        )

    summary_rows = [
        execute_run(
            repo,
            run_spec,
            runs_dir,
            drain_window_seconds=float(config.get("drain_window_seconds", 0.0)),
        )
        for run_spec in runs
    ]
    comparison_rows = build_profile_comparison(summary_rows)

    write_csv(target_dir / config["outputs"]["summary_csv"], RUN_SUMMARY_FIELDS, summary_rows)
    write_csv(target_dir / config["outputs"]["comparison_csv"], COMPARISON_FIELDS, comparison_rows)
    render_report(target_dir / config["outputs"]["report_md"], metadata, comparison_rows)
    print_terminal_summary(comparison_rows)
    return {
        "output_dir": target_dir,
        "summary_rows": summary_rows,
        "comparison_rows": comparison_rows,
        "runs": runs,
        "metadata": metadata,
        "long_run_gate": long_run_gate_result,
    }
