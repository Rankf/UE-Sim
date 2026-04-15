#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from soft_ue_truth_benchmark_lib import get_git_revision, repo_root, run_benchmark_matrix


def load_raw_config(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_csv_rows(path: Path, rows: list[dict[str, Any]]) -> None:
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fieldnames: list[str] = []
    for row in rows:
        for key in row.keys():
            if key not in fieldnames:
                fieldnames.append(key)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def build_stage_config(raw_config: dict[str, Any], *, output_dir: Path) -> Path:
    tuning_overrides: dict[str, dict[str, Any]] = {}
    tuning_names: list[str] = []
    for case in raw_config["diagnose_cases"]:
        case_name = str(case["case_name"])
        tuning_names.append(case_name)
        tuning_overrides[case_name] = {
            "truthArrivalBlocks": int(case["truthArrivalBlocks"]),
            "truthReadTracks": int(case["truthReadTracks"]),
            "truthReadResponderMessages": int(case["truthReadResponderMessages"]),
            "truthReadResponseBytes": int(case["truthReadResponseBytes"]),
            "truthOpsPerFlow": int(case["truthOpsPerFlow"]),
        }
    config = {
        "name": f"{raw_config['name']}-matrix",
        "build_before_run": False,
        "repetitions": int(raw_config.get("repetitions", 1)),
        "rng_run_base": int(raw_config.get("rng_run_base", 1)),
        "drain_window_seconds": float(raw_config.get("drain_window_seconds", 0.0)),
        "defaults": raw_config["defaults"],
        "profile_overrides": {},
        "variant_overrides": {
            "six_by_six": {
                "fabricEndpointMode": "six_by_six",
            }
        },
        "workload_overrides": {
            "read_only": {
                "truthSendPercent": 0,
                "truthWritePercent": 0,
                "truthReadPercent": 100,
            }
        },
        "tuning_overrides": tuning_overrides,
        "matrix": {
            "truthPressureProfile": [str(raw_config["defaults"].get("truthPressureProfile", "mixed"))],
            "variant": ["six_by_six"],
            "workload": ["read_only"],
            "tuning": tuning_names,
        },
        "outputs": {
            "expanded_matrix_json": "read-stage-diagnose-expanded-matrix.json",
            "summary_csv": "benchmark-summary.csv",
            "comparison_csv": "benchmark-comparison.csv",
            "report_md": "benchmark-report.md",
            "metadata_json": "benchmark-metadata.json",
        },
    }
    config_path = output_dir / "read-stage-diagnose-config.json"
    config_path.write_text(json.dumps(config, indent=2, sort_keys=True), encoding="utf-8")
    return config_path


def stage_mean_map(row: dict[str, Any]) -> dict[str, float]:
    return {
        "responder-generate": float(row.get("read_stage_responder_budget_generate_mean_ns", 0.0)),
        "pending-response-dispatch": float(
            row.get("read_stage_pending_response_queue_dispatch_mean_ns", 0.0)
        ),
        "first-packet-tx": float(row.get("read_stage_response_first_packet_tx_mean_ns", 0.0)),
        "network-visibility": float(row.get("read_stage_network_return_visibility_mean_ns", 0.0)),
        "reassembly": float(row.get("read_stage_reassembly_complete_mean_ns", 0.0)),
        "terminal": float(row.get("read_stage_terminal_mean_ns", 0.0)),
    }


def classify_dominant_stage(row: dict[str, Any]) -> str:
    means = stage_mean_map(row)
    dominant = max(means.items(), key=lambda item: item[1])[0]
    return f"{dominant}-dominant"


def recovery_mean_map(row: dict[str, Any]) -> dict[str, float]:
    return {
        "detect-to-nack": float(row.get("read_recovery_detect_to_nack_mean_ns", 0.0)),
        "nack-to-retransmit": float(row.get("read_recovery_nack_to_retransmit_mean_ns", 0.0)),
        "retransmit-to-visible": float(
            row.get("read_recovery_retransmit_to_first_visible_mean_ns", 0.0)
        ),
        "visible-to-unblocked": float(
            row.get("read_recovery_first_visible_to_unblocked_mean_ns", 0.0)
        ),
    }


def network_visibility_mean_map(row: dict[str, Any]) -> dict[str, float]:
    return {
        "return-queue-serialization": float(
            row.get("read_stage_network_queue_serialization_mean_ns", 0.0)
        ),
        "mixed-reorder-hold": float(row.get("read_stage_network_reorder_hold_mean_ns", 0.0)),
        "requester-visibility": float(row.get("read_stage_requester_visibility_mean_ns", 0.0)),
    }


def classify_dominant_network_visibility_stage(row: dict[str, Any]) -> str:
    means = network_visibility_mean_map(row)
    dominant = max(means.items(), key=lambda item: item[1])[0]
    return f"{dominant}-dominant"


def return_queue_mean_map(row: dict[str, Any]) -> dict[str, float]:
    return {
        "responder-egress-queue": float(
            row.get("read_stage_pending_response_queue_dispatch_mean_ns", 0.0)
        ),
        "tpdc-transport-send-serialization": float(
            row.get("read_stage_tpdc_transport_send_serialization_mean_ns", 0.0)
        ),
        "return-path-fragment-delay": float(
            row.get("read_stage_return_path_data_fragment_delay_mean_ns", 0.0)
        ),
    }


def classify_dominant_return_queue_stage(row: dict[str, Any]) -> str:
    means = return_queue_mean_map(row)
    dominant = max(means.items(), key=lambda item: item[1])[0]
    return f"{dominant}-dominant"


def classify_dominant_recovery_stage(row: dict[str, Any]) -> str:
    if int(row.get("read_recovery_sample_count", 0)) <= 0:
        return "no-recovery-samples"
    means = recovery_mean_map(row)
    dominant = max(means.items(), key=lambda item: item[1])[0]
    return f"{dominant}-dominant"


def classify_dominant_failure_stage(row: dict[str, Any]) -> str:
    value = str(row.get("read_recovery_failure_dominant_stage", "") or "")
    return value if value else "no-failure-recovery-samples"


def add_stage_shares(row: dict[str, Any]) -> None:
    means = stage_mean_map(row)
    total = sum(means.values())
    if total <= 0.0:
        row["read_stage_responder_generate_share_pct"] = 0.0
        row["read_stage_pending_dispatch_share_pct"] = 0.0
        row["read_stage_first_packet_tx_share_pct"] = 0.0
        row["read_stage_network_visibility_share_pct"] = 0.0
        row["read_stage_reassembly_share_pct"] = 0.0
        row["read_stage_terminal_share_pct"] = 0.0
        return
    row["read_stage_responder_generate_share_pct"] = round(means["responder-generate"] * 100.0 / total, 6)
    row["read_stage_pending_dispatch_share_pct"] = round(
        means["pending-response-dispatch"] * 100.0 / total, 6
    )
    row["read_stage_first_packet_tx_share_pct"] = round(
        means["first-packet-tx"] * 100.0 / total, 6
    )
    row["read_stage_network_visibility_share_pct"] = round(
        means["network-visibility"] * 100.0 / total, 6
    )
    row["read_stage_reassembly_share_pct"] = round(means["reassembly"] * 100.0 / total, 6)
    row["read_stage_terminal_share_pct"] = round(means["terminal"] * 100.0 / total, 6)


def render_report(report_path: Path, metadata: dict[str, Any], rows: list[dict[str, Any]]) -> None:
    lines = [
        "# soft_ue_fabric READ stage diagnose report",
        "",
        "## Summary",
        "",
        f"- Generated at: `{metadata['generated_at_utc']}`",
        f"- Git revision: `{metadata['git_revision']}`",
        f"- Config: `{metadata['config_path']}`",
        f"- Output dir: `{metadata['output_dir']}`",
        "- scenario: `soft_ue_fabric + explicit_multipath + six_by_six + all_to_all + read_only + mixed + dynamic + 4x100G`",
        "",
        "## Stage Breakdown",
        "",
        "| case | goodput_gbps | completion_pct | samples | responder_generate_p95_us | pending_dispatch_p95_us | first_packet_tx_p95_us | network_visibility_p95_us | reassembly_p95_us | terminal_p95_us | responder_share_pct | pending_dispatch_share_pct | first_packet_tx_share_pct | network_visibility_share_pct | reassembly_share_pct | terminal_share_pct | late_response_total | dominant |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for row in rows:
        lines.append(
            "| {case} | {goodput:.3f} | {completion:.3f} | {samples} | {generate_p95:.3f} | {pending_dispatch_p95:.3f} | {first_tx_p95:.3f} | {network_visibility_p95:.3f} | {reassembly_p95:.3f} | {terminal_p95:.3f} | {generate_share:.2f} | {pending_dispatch_share:.2f} | {first_tx_share:.2f} | {network_visibility_share:.2f} | {reassembly_share:.2f} | {terminal_share:.2f} | {late:.0f} | {dominant} |".format(
                case=str(row["case_name"]),
                goodput=float(row.get("semantic_goodput_mbps", 0.0)) / 1000.0,
                completion=float(row.get("semantic_completion_rate_pct_final", 0.0)),
                samples=int(row.get("read_stage_sample_count", 0)),
                generate_p95=float(row.get("read_stage_responder_budget_generate_p95_ns", 0.0)) / 1000.0,
                pending_dispatch_p95=float(
                    row.get("read_stage_pending_response_queue_dispatch_p95_ns", 0.0)
                )
                / 1000.0,
                first_tx_p95=float(row.get("read_stage_response_first_packet_tx_p95_ns", 0.0)) / 1000.0,
                network_visibility_p95=float(
                    row.get("read_stage_network_return_visibility_p95_ns", 0.0)
                )
                / 1000.0,
                reassembly_p95=float(row.get("read_stage_reassembly_complete_p95_ns", 0.0)) / 1000.0,
                terminal_p95=float(row.get("read_stage_terminal_p95_ns", 0.0)) / 1000.0,
                generate_share=float(row.get("read_stage_responder_generate_share_pct", 0.0)),
                pending_dispatch_share=float(row.get("read_stage_pending_dispatch_share_pct", 0.0)),
                first_tx_share=float(row.get("read_stage_first_packet_tx_share_pct", 0.0)),
                network_visibility_share=float(row.get("read_stage_network_visibility_share_pct", 0.0)),
                reassembly_share=float(row.get("read_stage_reassembly_share_pct", 0.0)),
                terminal_share=float(row.get("read_stage_terminal_share_pct", 0.0)),
                late=float(row.get("late_response_observed_total", 0.0)),
                dominant=str(row["dominant_stage"]),
            )
        )
    lines.extend(
        [
            "",
            "## Pre-Visibility Admission",
            "",
            "| case | tracked_reads | retry_allocations | terminal_resource_exhaust | first_no_context_total | arrival_reserve_fail_total | target_to_first_no_context_p95_us | target_to_arrival_reserved_p95_us | arrival_reserved_to_first_visible_p95_us | target_hold_to_release_p95_us | arrival_hold_to_release_p95_us | dominant_pre_failure_stage |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
        ]
    )
    for row in rows:
        lines.append(
            "| {case} | {tracked} | {retries} | {resource_exhaust} | {no_context_total} | {reserve_fail_total} | {target_to_no_context:.3f} | {target_to_reserved:.3f} | {reserved_to_visible:.3f} | {target_hold:.3f} | {arrival_hold:.3f} | {dominant} |".format(
                case=str(row["case_name"]),
                tracked=int(row.get("read_pre_admission_sample_count", 0)),
                retries=int(row.get("read_pre_context_allocated_retry_count", 0)),
                resource_exhaust=int(row.get("read_pre_terminal_resource_exhaust_count", 0)),
                no_context_total=int(row.get("read_pre_first_packet_no_context_count_total", 0)),
                reserve_fail_total=int(row.get("read_pre_arrival_reserve_fail_count_total", 0)),
                target_to_no_context=float(
                    row.get("read_pre_target_to_first_no_context_p95_ns", 0.0)
                )
                / 1000.0,
                target_to_reserved=float(
                    row.get("read_pre_target_to_arrival_reserved_p95_ns", 0.0)
                )
                / 1000.0,
                reserved_to_visible=float(
                    row.get("read_pre_arrival_reserved_to_first_response_visible_p95_ns", 0.0)
                )
                / 1000.0,
                target_hold=float(row.get("read_pre_target_hold_to_release_p95_ns", 0.0))
                / 1000.0,
                arrival_hold=float(row.get("read_pre_arrival_hold_to_release_p95_ns", 0.0))
                / 1000.0,
                dominant=str(row.get("read_pre_failure_dominant_stage", "")) or "no-pre-failure",
            )
        )
    lines.extend(
        [
            "",
            "## Network Visibility Breakdown",
            "",
            "| case | queue_serialization_p95_us | reorder_hold_p95_us | requester_visibility_p95_us | queue_serialization_share_pct | reorder_hold_share_pct | requester_visibility_share_pct | dominant_network_visibility_stage |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
        ]
    )
    for row in rows:
        queue_mean = float(row.get("read_stage_network_queue_serialization_mean_ns", 0.0))
        reorder_mean = float(row.get("read_stage_network_reorder_hold_mean_ns", 0.0))
        requester_mean = float(row.get("read_stage_requester_visibility_mean_ns", 0.0))
        network_total = queue_mean + reorder_mean + requester_mean
        if network_total <= 0.0:
            queue_share = 0.0
            reorder_share = 0.0
            requester_share = 0.0
        else:
            queue_share = queue_mean * 100.0 / network_total
            reorder_share = reorder_mean * 100.0 / network_total
            requester_share = requester_mean * 100.0 / network_total
        lines.append(
            "| {case} | {queue_p95:.3f} | {reorder_p95:.3f} | {requester_p95:.3f} | {queue_share:.2f} | {reorder_share:.2f} | {requester_share:.2f} | {dominant} |".format(
                case=str(row["case_name"]),
                queue_p95=float(
                    row.get("read_stage_network_queue_serialization_p95_ns", 0.0)
                )
                / 1000.0,
                reorder_p95=float(row.get("read_stage_network_reorder_hold_p95_ns", 0.0))
                / 1000.0,
                requester_p95=float(
                    row.get("read_stage_requester_visibility_p95_ns", 0.0)
                )
                / 1000.0,
                queue_share=queue_share,
                reorder_share=reorder_share,
                requester_share=requester_share,
                dominant=str(row.get("dominant_network_visibility_stage", "")),
            )
        )
    lines.extend(
        [
            "",
            "## Return Queue Breakdown",
            "",
            "| case | responder_egress_queue_p95_us | tpdc_transport_send_p95_us | return_path_fragment_delay_p95_us | tpdc_send_window_queued_pct | responder_egress_share_pct | tpdc_transport_send_share_pct | return_path_fragment_share_pct | dominant_return_queue_stage |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
        ]
    )
    for row in rows:
        responder_mean = float(
            row.get("read_stage_pending_response_queue_dispatch_mean_ns", 0.0)
        )
        tpdc_mean = float(
            row.get("read_stage_tpdc_transport_send_serialization_mean_ns", 0.0)
        )
        return_path_mean = float(
            row.get("read_stage_return_path_data_fragment_delay_mean_ns", 0.0)
        )
        total = responder_mean + tpdc_mean + return_path_mean
        if total <= 0.0:
            responder_share = 0.0
            tpdc_share = 0.0
            return_path_share = 0.0
        else:
            responder_share = responder_mean * 100.0 / total
            tpdc_share = tpdc_mean * 100.0 / total
            return_path_share = return_path_mean * 100.0 / total
        lines.append(
            "| {case} | {responder_p95:.3f} | {tpdc_p95:.3f} | {return_path_p95:.3f} | {queued_pct:.2f} | {responder_share:.2f} | {tpdc_share:.2f} | {return_path_share:.2f} | {dominant} |".format(
                case=str(row["case_name"]),
                responder_p95=float(
                    row.get("read_stage_pending_response_queue_dispatch_p95_ns", 0.0)
                )
                / 1000.0,
                tpdc_p95=float(
                    row.get("read_stage_tpdc_transport_send_serialization_p95_ns", 0.0)
                )
                / 1000.0,
                return_path_p95=float(
                    row.get("read_stage_return_path_data_fragment_delay_p95_ns", 0.0)
                )
                / 1000.0,
                queued_pct=float(
                    row.get("read_stage_tpdc_send_window_queued_pct", 0.0)
                ),
                responder_share=responder_share,
                tpdc_share=tpdc_share,
                return_path_share=return_path_share,
                dominant=str(row.get("dominant_return_queue_stage", "")),
            )
        )
    lines.extend(
        [
            "",
            "## Recovery Breakdown",
            "",
            "| case | recovery_samples | detect_to_nack_p95_us | nack_to_retransmit_p95_us | retransmit_to_visible_p95_us | visible_to_unblocked_p95_us | detect_to_unblocked_p95_us | recovery_dominant |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
        ]
    )
    for row in rows:
        lines.append(
            "| {case} | {samples} | {detect_to_nack:.3f} | {nack_to_retx:.3f} | {retx_to_visible:.3f} | {visible_to_unblocked:.3f} | {detect_to_unblocked:.3f} | {dominant} |".format(
                case=str(row["case_name"]),
                samples=int(row.get("read_recovery_sample_count", 0)),
                detect_to_nack=float(row.get("read_recovery_detect_to_nack_p95_ns", 0.0)) / 1000.0,
                nack_to_retx=float(row.get("read_recovery_nack_to_retransmit_p95_ns", 0.0)) / 1000.0,
                retx_to_visible=float(
                    row.get("read_recovery_retransmit_to_first_visible_p95_ns", 0.0)
                )
                / 1000.0,
                visible_to_unblocked=float(
                    row.get("read_recovery_first_visible_to_unblocked_p95_ns", 0.0)
                )
                / 1000.0,
                detect_to_unblocked=float(
                    row.get("read_recovery_detect_to_unblocked_p95_ns", 0.0)
                )
                / 1000.0,
                dominant=str(row["dominant_recovery_stage"]),
            )
        )
    lines.extend(
        [
            "",
            "## Failure-Path Recovery Progression",
            "",
            "| case | tracked_recovery | tracked_failures | gap_detect_only | nack_sent_only | nack_observed_only | retransmit_only | first_visible_only | reassembly_before_terminal | dominant_failure_stage |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
        ]
    )
    for row in rows:
        lines.append(
            "| {case} | {tracked} | {failures} | {gap_only} | {nack_only} | {observed_only} | {retransmit_only} | {first_visible_only} | {reassembly_before_terminal} | {dominant} |".format(
                case=str(row["case_name"]),
                tracked=int(row.get("read_recovery_tracked_count", 0)),
                failures=int(row.get("read_recovery_failure_tracked_count", 0)),
                gap_only=int(row.get("read_recovery_failure_gap_detect_only_count", 0)),
                nack_only=int(row.get("read_recovery_failure_nack_sent_only_count", 0)),
                observed_only=int(row.get("read_recovery_failure_nack_observed_only_count", 0)),
                retransmit_only=int(row.get("read_recovery_failure_retransmit_only_count", 0)),
                first_visible_only=int(
                    row.get("read_recovery_failure_first_visible_only_count", 0)
                ),
                reassembly_before_terminal=int(
                    row.get("read_recovery_failure_reassembly_before_terminal_count", 0)
                ),
                dominant=str(row.get("dominant_failure_stage", "")),
            )
        )
    lines.extend(
        [
            "",
            "## Failure-Path Recovery Timing",
            "",
            "| case | detect_to_nack_p95_us | nack_to_sender_observed_p95_us | sender_observed_to_retransmit_p95_us | retransmit_to_terminal_p95_us | detect_to_terminal_p95_us |",
            "| --- | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for row in rows:
        lines.append(
            "| {case} | {detect_to_nack:.3f} | {nack_to_observed:.3f} | {observed_to_retx:.3f} | {retx_to_terminal:.3f} | {detect_to_terminal:.3f} |".format(
                case=str(row["case_name"]),
                detect_to_nack=float(
                    row.get("read_recovery_failure_detect_to_nack_p95_ns", 0.0)
                )
                / 1000.0,
                nack_to_observed=float(
                    row.get("read_recovery_failure_nack_to_observed_at_sender_p95_ns", 0.0)
                )
                / 1000.0,
                observed_to_retx=float(
                    row.get(
                        "read_recovery_failure_observed_at_sender_to_retransmit_p95_ns", 0.0
                    )
                )
                / 1000.0,
                retx_to_terminal=float(
                    row.get("read_recovery_failure_retransmit_to_terminal_p95_ns", 0.0)
                )
                / 1000.0,
                detect_to_terminal=float(
                    row.get("read_recovery_failure_detect_to_terminal_p95_ns", 0.0)
                )
                / 1000.0,
            )
        )
    lines.extend(["", "## Conclusion", ""])
    if rows:
        low = rows[0]
        high = rows[-1]
        lines.append(f"- `{low['case_name']}` dominant stage: `{low['dominant_stage']}`")
        lines.append(f"- `{high['case_name']}` dominant stage: `{high['dominant_stage']}`")
        if low["dominant_stage"] == high["dominant_stage"]:
            lines.append(f"- Dominant stage is stable across both points: `{high['dominant_stage']}`.")
        else:
            lines.append(
                f"- Pressure shifts the dominant stage from `{low['dominant_stage']}` to `{high['dominant_stage']}`."
            )
        lines.append(
            "- Late-response should now be read as a concrete stage issue: compare responder generation p95 `{low_gen:.3f} -> {high_gen:.3f} us`, pending-dispatch p95 `{low_pending:.3f} -> {high_pending:.3f} us`, first-packet-tx p95 `{low_tx:.3f} -> {high_tx:.3f} us`, network-visibility p95 `{low_net:.3f} -> {high_net:.3f} us`, and reassembly p95 `{low_reasm:.3f} -> {high_reasm:.3f} us`.".format(
                low_gen=float(low.get("read_stage_responder_budget_generate_p95_ns", 0.0)) / 1000.0,
                high_gen=float(high.get("read_stage_responder_budget_generate_p95_ns", 0.0)) / 1000.0,
                low_pending=float(low.get("read_stage_pending_response_queue_dispatch_p95_ns", 0.0)) / 1000.0,
                high_pending=float(high.get("read_stage_pending_response_queue_dispatch_p95_ns", 0.0)) / 1000.0,
                low_tx=float(low.get("read_stage_response_first_packet_tx_p95_ns", 0.0)) / 1000.0,
                high_tx=float(high.get("read_stage_response_first_packet_tx_p95_ns", 0.0)) / 1000.0,
                low_net=float(low.get("read_stage_network_return_visibility_p95_ns", 0.0)) / 1000.0,
                high_net=float(high.get("read_stage_network_return_visibility_p95_ns", 0.0)) / 1000.0,
                low_reasm=float(low.get("read_stage_reassembly_complete_p95_ns", 0.0)) / 1000.0,
                high_reasm=float(high.get("read_stage_reassembly_complete_p95_ns", 0.0)) / 1000.0,
            )
        )
        lines.append(
            "- Pre-visibility admission now shows whether failures die before any response data becomes visible. Compare dominant pre-failure stage `{low_stage}` -> `{high_stage}`, resource-exhaust terminals `{low_exhaust} -> {high_exhaust}`, and arrival-reserve-fail totals `{low_fail} -> {high_fail}`.".format(
                low_stage=str(low.get("read_pre_failure_dominant_stage", "")) or "no-pre-failure",
                high_stage=str(high.get("read_pre_failure_dominant_stage", "")) or "no-pre-failure",
                low_exhaust=int(low.get("read_pre_terminal_resource_exhaust_count", 0)),
                high_exhaust=int(high.get("read_pre_terminal_resource_exhaust_count", 0)),
                low_fail=int(low.get("read_pre_arrival_reserve_fail_count_total", 0)),
                high_fail=int(high.get("read_pre_arrival_reserve_fail_count_total", 0)),
            )
        )
        lines.append(
            "- Recovery sub-stages now separate hole detection from sender recovery. Compare detect-to-nack p95 `{low_detect:.3f} -> {high_detect:.3f} us`, nack-to-retransmit p95 `{low_retx:.3f} -> {high_retx:.3f} us`, and retransmit-to-visible p95 `{low_visible:.3f} -> {high_visible:.3f} us`.".format(
                low_detect=float(low.get("read_recovery_detect_to_nack_p95_ns", 0.0)) / 1000.0,
                high_detect=float(high.get("read_recovery_detect_to_nack_p95_ns", 0.0)) / 1000.0,
                low_retx=float(low.get("read_recovery_nack_to_retransmit_p95_ns", 0.0)) / 1000.0,
                high_retx=float(high.get("read_recovery_nack_to_retransmit_p95_ns", 0.0)) / 1000.0,
                low_visible=float(
                    low.get("read_recovery_retransmit_to_first_visible_p95_ns", 0.0)
                )
                / 1000.0,
                high_visible=float(
                    high.get("read_recovery_retransmit_to_first_visible_p95_ns", 0.0)
                )
                / 1000.0,
            )
        )
        lines.append(
            "- Failure-path progression now shows where tracked READ holes die before completion. Compare dominant failure stage `{low_stage}` -> `{high_stage}`, plus retransmit-only failures `{low_retx_only} -> {high_retx_only}` and first-visible-before-terminal failures `{low_visible_only} -> {high_visible_only}`.".format(
                low_stage=str(low.get("dominant_failure_stage", "")),
                high_stage=str(high.get("dominant_failure_stage", "")),
                low_retx_only=int(low.get("read_recovery_failure_retransmit_only_count", 0)),
                high_retx_only=int(high.get("read_recovery_failure_retransmit_only_count", 0)),
                low_visible_only=int(low.get("read_recovery_failure_first_visible_only_count", 0)),
                high_visible_only=int(high.get("read_recovery_failure_first_visible_only_count", 0)),
            )
        )
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run READ stage latency diagnostics for soft_ue_fabric.")
    parser.add_argument(
        "--config",
        type=Path,
        default=repo_root() / "performance" / "config" / "soft-ue-fabric-read-stage-diagnose.json",
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=Path("/tmp/soft-ue-fabric-read-stage-diagnose"),
    )
    parser.add_argument("--force-build", action="store_true", help="Build before running.")
    parser.add_argument("--dry-run", action="store_true", help="Only expand and print planned commands.")
    args = parser.parse_args()

    raw_config = load_raw_config(args.config)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%SZ")
    output_dir = args.output_root / f"{raw_config['name']}_{timestamp}"
    output_dir.mkdir(parents=True, exist_ok=True)

    config_path = build_stage_config(raw_config, output_dir=output_dir)
    result = run_benchmark_matrix(config_path, output_dir / "matrix", force_build=args.force_build, dry_run=args.dry_run)

    rows: list[dict[str, Any]] = []
    for row in result["summary_rows"]:
        enriched = dict(row)
        enriched["case_name"] = str(row["tuning_name"])
        add_stage_shares(enriched)
        enriched["dominant_stage"] = classify_dominant_stage(enriched)
        enriched["dominant_network_visibility_stage"] = classify_dominant_network_visibility_stage(
            enriched
        )
        enriched["dominant_return_queue_stage"] = classify_dominant_return_queue_stage(
            enriched
        )
        enriched["dominant_recovery_stage"] = classify_dominant_recovery_stage(enriched)
        enriched["dominant_failure_stage"] = classify_dominant_failure_stage(enriched)
        rows.append(enriched)
    rows.sort(key=lambda item: str(item["case_name"]))

    summary_path = output_dir / raw_config["outputs"]["summary_csv"]
    comparison_path = output_dir / raw_config["outputs"]["comparison_csv"]
    report_path = output_dir / raw_config["outputs"]["report_md"]
    metadata_path = output_dir / raw_config["outputs"]["metadata_json"]

    write_csv_rows(summary_path, rows)
    write_csv_rows(comparison_path, rows)

    metadata = {
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "git_revision": get_git_revision(repo_root()),
        "config_path": str(args.config),
        "output_dir": str(output_dir),
        "summary_csv_path": str(summary_path),
        "comparison_csv_path": str(comparison_path),
        "run_count": len(rows),
    }
    metadata_path.write_text(json.dumps(metadata, indent=2, sort_keys=True), encoding="utf-8")
    render_report(report_path, metadata, rows)

    print(f"soft_ue_fabric READ stage diagnose finished: {output_dir}")
    print(f"Summary: {summary_path}")
    print(f"Report: {report_path}")
    print(f"Metadata: {metadata_path}")


if __name__ == "__main__":
    main()
