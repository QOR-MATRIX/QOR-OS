# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Validate the existing ShellBenchmark report for Prism's diagnostic gate.

No hardware, rendering, process launch or host configuration is performed here.
Acceptance of the report's numbers is not proof of their provenance or hardware readiness.
"""
import json
import math
from pathlib import Path

MAX_REPORT_BYTES = 1024 * 1024
MAX_OUTPUTS = 64
# The real collector in prism/shell/shellbenchmark.cpp posts forty inputs.
LATENCY_SAMPLES = 40
METRICS = ("averageMs", "p50Ms", "p95Ms", "p99Ms", "maxMs")


class InvalidReport(ValueError):
    """A bounded diagnostic that never includes supplied values."""


def invalid(field):
    raise InvalidReport("Invalid benchmark report: " + field)


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            invalid("duplicate JSON field")
        result[key] = value
    return result


def _constant(_value):
    invalid("non-finite JSON number")


def load_report(path):
    """Read at most one MiB plus the oversize sentinel, including on a growing file."""
    try:
        with Path(path).open("rb") as stream:
            data = stream.read(MAX_REPORT_BYTES + 1)
        if len(data) > MAX_REPORT_BYTES:
            invalid("report exceeds one MiB")
        report = json.loads(data, object_pairs_hook=_unique_object, parse_constant=_constant)
        if not isinstance(report, dict):
            invalid("expected an object")
        # Keep behavior independent of the interpreter's JSON recursion limit.
        nodes = 0
        def visit(value, depth):
            nonlocal nodes
            nodes += 1
            if depth > 16 or nodes > 16384:
                invalid("report structure limit")
            if type(value) is float and not math.isfinite(value):
                invalid("non-finite JSON number")
            children = value.values() if isinstance(value, dict) else value if isinstance(value, list) else ()
            for child in children:
                visit(child, depth + 1)
        visit(report, 0)
        return report
    except InvalidReport:
        raise
    except OSError:
        invalid("report could not be read")
    except (ValueError, UnicodeError, RecursionError):
        invalid("malformed JSON")


def _number(value):
    try:
        return type(value) in (int, float) and math.isfinite(value)
    except OverflowError:
        return False


def _count(value):
    return type(value) is int and 0 <= value <= (1 << 53) - 1


def _distribution(values, samples, field):
    for key in METRICS:
        value = values.get(key)
        if not _number(value) or value < 0:
            invalid(field + " " + key)
    if not values["p50Ms"] <= values["p95Ms"] <= values["p99Ms"] <= values["maxMs"]:
        invalid(field + " percentiles are not ordered")
    if values["averageMs"] > values["maxMs"] and not math.isclose(values["averageMs"], values["maxMs"], rel_tol=1e-12):
        invalid(field + " average exceeds maximum")
    if samples == 0 and any(values[key] != 0 for key in METRICS):
        invalid(field + " empty distribution has nonzero metrics")
    if samples > 0 and (values["averageMs"] <= 0 or values["maxMs"] <= 0):
        invalid(field + " nonempty distribution has no elapsed time")


def assess_report(report, expected_outputs):
    """Reject malformed evidence; return the unchanged timing targets plus completeness.

    Zero/partial observations remain useful diagnostics, but cannot satisfy the gate.
    Latency is measured on output one only, exactly as in the production collector.
    """
    if type(expected_outputs) is not int or not 1 <= expected_outputs <= MAX_OUTPUTS:
        invalid("expected output count")
    if not isinstance(report, dict):
        invalid("expected an object")
    outputs = report.get("outputs")
    if not isinstance(outputs, list) or len(outputs) != expected_outputs:
        invalid("output count differs from the running setup")
    output_passes = []
    for index, output in enumerate(outputs):
        field = f"output {index + 1}"
        if not isinstance(output, dict) or not _count(output.get("frames")):
            invalid(field + " frame count")
        frames = output["frames"]
        refresh = output.get("refreshHz")
        if not _number(refresh) or refresh <= 0 or not math.isfinite(2000.0 / refresh):
            invalid(field + " refresh rate")
        _distribution(output, frames, field)
        missed = output.get("missedFrames")
        percent = output.get("missedPercent")
        if not _count(missed) or missed > frames:
            invalid(field + " missed frame count")
        if not _number(percent) or not 0 <= percent <= 100:
            invalid(field + " missed percentage")
        if not math.isclose(percent, 100.0 * missed / frames if frames else 0.0, rel_tol=1e-9, abs_tol=1e-9):
            invalid(field + " missed percentage disagrees with counts")
        fps = output.get("averageFps")
        expected_fps = 1000.0 / output["averageMs"] if frames else 0.0
        if not _number(fps) or fps < 0 or not math.isclose(fps, expected_fps, rel_tol=1e-9, abs_tol=1e-9):
            invalid(field + " average frame rate disagrees with interval")
        output_passes.append(frames > 0 and percent < 1.0)
    latency = report.get("inputToSwap")
    if not isinstance(latency, dict) or not _count(latency.get("samples")) or not _count(latency.get("posted")):
        invalid("input sample/posted count")
    if not latency["samples"] <= latency["posted"] <= LATENCY_SAMPLES:
        invalid("input sample/posted relationship")
    _distribution(latency, latency["samples"], "input")
    complete = latency["samples"] == latency["posted"] == LATENCY_SAMPLES
    budget = 2000.0 / outputs[0]["refreshHz"]
    latency_ok = complete and latency["p95Ms"] <= budget
    distinct = len({round(output["refreshHz"]) for output in outputs}) > 1
    enough = len(outputs) >= 2
    return {"passed": all(output_passes) and latency_ok and distinct and enough,
            "output_passes": output_passes, "latency_ok": latency_ok, "input_complete": complete,
            "budget_ms": budget, "enough_outputs": enough, "distinct_refresh_rates": distinct}
