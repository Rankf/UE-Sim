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
        "first-visible": float(row.get("read_stage_first_response_visible_mean_ns", 0.0)),
        "reassembly": float(row.get("read_stage_reassembly_complete_mean_ns", 0.0)),
        "terminal": float(row.get("read_stage_terminal_mean_ns", 0.0)),
    }


def classify_dominant_stage(row: dict[str, Any]) -> str:
    means = stage_mean_map(row)
    dominant = max(means.items(), key=lambda item: item[1])[0]
    return f"{dominant}-dominant"


def add_stage_shares(row: dict[str, Any]) -> None:
    means = stage_mean_map(row)
    total = sum(means.values())
    if total <= 0.0:
        row["read_stage_responder_generate_share_pct"] = 0.0
        row["read_stage_first_visible_share_pct"] = 0.0
        row["read_stage_reassembly_share_pct"] = 0.0
        row["read_stage_terminal_share_pct"] = 0.0
        return
    row["read_stage_responder_generate_share_pct"] = round(means["responder-generate"] * 100.0 / total, 6)
    row["read_stage_first_visible_share_pct"] = round(means["first-visible"] * 100.0 / total, 6)
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
        "| case | goodput_gbps | completion_pct | samples | responder_generate_p95_us | first_visible_p95_us | reassembly_p95_us | terminal_p95_us | responder_share_pct | first_visible_share_pct | reassembly_share_pct | terminal_share_pct | late_response_total | dominant |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for row in rows:
        lines.append(
            "| {case} | {goodput:.3f} | {completion:.3f} | {samples} | {generate_p95:.3f} | {visible_p95:.3f} | {reassembly_p95:.3f} | {terminal_p95:.3f} | {generate_share:.2f} | {visible_share:.2f} | {reassembly_share:.2f} | {terminal_share:.2f} | {late:.0f} | {dominant} |".format(
                case=str(row["case_name"]),
                goodput=float(row.get("semantic_goodput_mbps", 0.0)) / 1000.0,
                completion=float(row.get("semantic_completion_rate_pct_final", 0.0)),
                samples=int(row.get("read_stage_sample_count", 0)),
                generate_p95=float(row.get("read_stage_responder_budget_generate_p95_ns", 0.0)) / 1000.0,
                visible_p95=float(row.get("read_stage_first_response_visible_p95_ns", 0.0)) / 1000.0,
                reassembly_p95=float(row.get("read_stage_reassembly_complete_p95_ns", 0.0)) / 1000.0,
                terminal_p95=float(row.get("read_stage_terminal_p95_ns", 0.0)) / 1000.0,
                generate_share=float(row.get("read_stage_responder_generate_share_pct", 0.0)),
                visible_share=float(row.get("read_stage_first_visible_share_pct", 0.0)),
                reassembly_share=float(row.get("read_stage_reassembly_share_pct", 0.0)),
                terminal_share=float(row.get("read_stage_terminal_share_pct", 0.0)),
                late=float(row.get("late_response_observed_total", 0.0)),
                dominant=str(row["dominant_stage"]),
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
            "- Late-response should now be read as a concrete stage issue: compare responder generation p95 `{low_gen:.3f} -> {high_gen:.3f} us`, first-visible p95 `{low_vis:.3f} -> {high_vis:.3f} us`, and reassembly p95 `{low_reasm:.3f} -> {high_reasm:.3f} us`.".format(
                low_gen=float(low.get("read_stage_responder_budget_generate_p95_ns", 0.0)) / 1000.0,
                high_gen=float(high.get("read_stage_responder_budget_generate_p95_ns", 0.0)) / 1000.0,
                low_vis=float(low.get("read_stage_first_response_visible_p95_ns", 0.0)) / 1000.0,
                high_vis=float(high.get("read_stage_first_response_visible_p95_ns", 0.0)) / 1000.0,
                low_reasm=float(low.get("read_stage_reassembly_complete_p95_ns", 0.0)) / 1000.0,
                high_reasm=float(high.get("read_stage_reassembly_complete_p95_ns", 0.0)) / 1000.0,
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
