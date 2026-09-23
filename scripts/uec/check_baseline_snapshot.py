#!/usr/bin/env python3
"""Check frozen UEC baseline metadata and existing result files without simulation."""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = REPO_ROOT / "requirements" / "uec-baseline-800g.json"


def nested_value(document: dict[str, Any], dotted_path: str) -> Any:
    value: Any = document
    for component in dotted_path.split("."):
        if not isinstance(value, dict) or component not in value:
            raise KeyError(dotted_path)
        value = value[component]
    return value


def values_match(actual: Any, expected: Any) -> bool:
    if isinstance(expected, float):
        return isinstance(actual, (int, float)) and math.isclose(
            float(actual), expected, rel_tol=1e-12, abs_tol=1e-6
        )
    return actual == expected


def current_revision() -> str | None:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate the frozen UEC baseline using existing files only."
    )
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument(
        "--strict-revision",
        action="store_true",
        help="fail instead of warning when HEAD differs from the frozen source revision",
    )
    args = parser.parse_args()

    manifest_path = args.manifest.resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    failures: list[str] = []
    warnings: list[str] = []

    revision = current_revision()
    frozen_revision = manifest["source_revision"]
    if revision is None:
        warnings.append("unable to read the current Git revision")
    elif revision != frozen_revision:
        message = f"HEAD {revision} differs from frozen revision {frozen_revision}"
        (failures if args.strict_revision else warnings).append(message)

    coverage = manifest["coverage"]
    requirements_path = REPO_ROOT / coverage["requirements_file"]
    if not requirements_path.is_file():
        failures.append(f"missing requirements file: {requirements_path}")
    else:
        requirements_text = requirements_path.read_text(encoding="utf-8")
        for requirement_id in coverage["mandatory_ids"]:
            matching_lines = [
                line
                for line in requirements_text.splitlines()
                if f"id: {requirement_id}," in line
            ]
            if len(matching_lines) != 1:
                failures.append(
                    f"{requirement_id}: expected one requirement entry, found {len(matching_lines)}"
                )
            elif f"status: {coverage['required_status']}" not in matching_lines[0]:
                failures.append(
                    f"{requirement_id}: status is not {coverage['required_status']}"
                )

    checked_experiments = 0
    for experiment in manifest["experiments"]:
        experiment_id = experiment["id"]
        for reference_name in ("script", "documentation"):
            reference = REPO_ROOT / experiment[reference_name]
            if not reference.is_file():
                failures.append(f"{experiment_id}: missing {reference_name} {reference}")

        summary_path = REPO_ROOT / experiment["summary"]
        if not summary_path.is_file():
            failures.append(f"{experiment_id}: missing ignored result {summary_path}")
            continue

        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        for field, expected in experiment["assertions"].items():
            try:
                actual = nested_value(summary, field)
            except KeyError:
                failures.append(f"{experiment_id}: missing summary field {field}")
                continue
            if not values_match(actual, expected):
                failures.append(
                    f"{experiment_id}: {field} is {actual!r}, expected {expected!r}"
                )
        checked_experiments += 1

    print(f"Baseline: {manifest['baseline_id']}")
    print(f"Mandatory AI Base rows checked: {len(coverage['mandatory_ids'])}")
    print(f"Existing experiment summaries checked: {checked_experiments}")
    print("Simulation executed: no")
    for warning in warnings:
        print(f"WARNING: {warning}", file=sys.stderr)
    for failure in failures:
        print(f"ERROR: {failure}", file=sys.stderr)

    if failures:
        print(f"Snapshot check failed with {len(failures)} error(s).", file=sys.stderr)
        return 1
    print("Snapshot check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
