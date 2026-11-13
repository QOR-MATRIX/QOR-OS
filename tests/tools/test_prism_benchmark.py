# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Portable checks of the real report evaluator; no compositor or hardware access."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = Path(__file__).resolve().parents[2]
TOOL = REPO / "tools/dev/prism-gate.sh"
SPEC = importlib.util.spec_from_file_location("prism_benchmark", TOOL.with_name("prism_benchmark.py"))
benchmark = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(benchmark)


def report_fixture():
    return {
        "graphicsApi": "fixture",
        "outputs": [
            {"screen": f"fixture-{index}", "size": "100x100", "refreshHz": rate,
             "frames": 1000, "averageMs": 8, "averageFps": 125, "p50Ms": 7,
             "p95Ms": 8, "p99Ms": 10, "maxMs": 30, "missedFrames": 1, "missedPercent": 0.1}
            for index, rate in enumerate((60, 120))
        ],
        "inputToSwap": {"samples": 40, "posted": 40, "averageMs": 7,
                        "p50Ms": 5, "p95Ms": 10, "p99Ms": 11, "maxMs": 12},
    }


class EmbeddedReportEvaluator(unittest.TestCase):
    def evaluate(self, report, *, trial=False):
        # Execute the production here-document unchanged, not a copied evaluator.
        source = TOOL.read_text(encoding="utf-8")
        marker = "<< 'PY' || status=$?\n"
        self.assertEqual(source.count(marker), 1)
        code = source.split(marker, 1)[1].split("\nPY\n", 1)[0]
        def command(argv, **kwargs):
            self.assertEqual(argv, ["lspci"])
            return subprocess.CompletedProcess(argv, 0, "Fixture VGA controller\n", "")
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out / "report.json").write_text(json.dumps(report), encoding="utf-8")
            with (mock.patch.object(sys, "argv", ["-", str(out), str(TOOL.parent)]),
                  mock.patch.object(sys, "path", list(sys.path)),
                  mock.patch.dict(os.environ, {"GATE_TRIAL": str(int(trial)), "GATE_OUTPUTS": "2", "GATE_DEVICE": "fixture"}),
                  mock.patch("subprocess.run", side_effect=command),
                  mock.patch("platform.node", return_value="fixture"),
                  mock.patch("platform.release", return_value="fixture"),
                  contextlib.redirect_stdout(io.StringIO())):
                with self.assertRaises(SystemExit) as caught:
                    exec(compile(code, str(TOOL), "exec"), {"__name__": "__main__"})
            summary = out / "summary.md"
            return caught.exception.code, summary.read_text(encoding="utf-8") if summary.exists() else ""

    def test_one_recorded_input_cannot_pass_a_forty_input_run(self):
        report = report_fixture()
        report["inputToSwap"]["samples"] = 1
        status, _ = self.evaluate(report)
        self.assertNotEqual(status, 0)

    def test_samples_cannot_exceed_posted_events(self):
        report = report_fixture()
        report["inputToSwap"]["posted"] = 1
        status, summary = self.evaluate(report)
        self.assertNotEqual(status, 0)
        self.assertEqual(summary, "")

    def test_missed_percentage_must_match_the_measured_count(self):
        report = report_fixture()
        report["outputs"][0]["missedFrames"] = 50
        status, summary = self.evaluate(report)
        self.assertNotEqual(status, 0)
        self.assertEqual(summary, "")

    def test_complete_valid_measurement_passes(self):
        status, summary = self.evaluate(report_fixture())
        self.assertEqual(status, 0)
        self.assertIn("40/40 samples recorded/posted", summary)
        self.assertIn("every criterion passes", summary)

    def test_partial_trial_is_diagnostic_and_explicitly_fails_the_criteria(self):
        report = report_fixture()
        report["inputToSwap"]["samples"] = 1
        status, summary = self.evaluate(report, trial=True)
        self.assertEqual(status, 0)
        self.assertIn("not gate data", summary)
        self.assertIn("1/40 samples recorded/posted", summary)
        self.assertIn("a criterion fails", summary)

    def test_malformed_trial_cannot_pass(self):
        report = report_fixture()
        report["inputToSwap"]["p99Ms"] = -1
        status, summary = self.evaluate(report, trial=True)
        self.assertNotEqual(status, 0)
        self.assertEqual(summary, "")


class ReportValidation(unittest.TestCase):
    def assess(self, report=None, count=2):
        return benchmark.assess_report(report_fixture() if report is None else report, count)

    def test_complete_report_satisfies_existing_targets(self):
        verdict = self.assess()
        self.assertTrue(verdict["passed"])
        self.assertEqual(verdict["budget_ms"], 2000 / 60)

    def test_input_contract_matches_real_collector(self):
        source = (REPO / "prism/shell/shellbenchmark.cpp").read_text(encoding="utf-8")
        self.assertIn(f"constexpr int kLatencySamples = {benchmark.LATENCY_SAMPLES};", source)

    def test_missing_and_invalid_output_counts_fail(self):
        for value in ([], None, {}, [None], [None, None]):
            with self.subTest(outputs=value):
                report = report_fixture()
                report["outputs"] = value
                with self.assertRaises(benchmark.InvalidReport):
                    self.assess(report)
        for value in (0, -1, True, 2.0, 65, "2"):
            with self.subTest(expected=value), self.assertRaises(benchmark.InvalidReport):
                self.assess(count=value)

    def test_report_root_must_be_an_object(self):
        for value in ([], "text", 7, True):
            with self.subTest(value=value), self.assertRaises(benchmark.InvalidReport):
                self.assess(value)

    def test_nonfinite_negative_and_wrong_numeric_types_fail(self):
        for where, fields in (("outputs", ("frames", "refreshHz", "averageMs", "averageFps", "p50Ms", "p95Ms", "p99Ms", "maxMs", "missedFrames", "missedPercent")),
                              ("inputToSwap", ("samples", "posted", *benchmark.METRICS))):
            for field in fields:
                for value in (-1, True, "1", None, float("inf"), float("nan"), 10 ** 400):
                    with self.subTest(where=where, field=field, value_type=type(value).__name__):
                        report = report_fixture()
                        target = report[where][0] if where == "outputs" else report[where]
                        target[field] = value
                        with self.assertRaises(benchmark.InvalidReport):
                            self.assess(report)

    def test_required_distribution_fields_cannot_be_omitted(self):
        for where in ("outputs", "inputToSwap"):
            for field in benchmark.METRICS:
                with self.subTest(where=where, field=field):
                    report = report_fixture()
                    target = report[where][0] if where == "outputs" else report[where]
                    del target[field]
                    with self.assertRaises(benchmark.InvalidReport):
                        self.assess(report)

    def test_unordered_percentiles_and_impossible_averages_fail(self):
        for where in ("outputs", "inputToSwap"):
            for change in ({"p50Ms": 12}, {"p95Ms": 13}, {"p99Ms": 99}, {"maxMs": 1}, {"averageMs": 100}):
                with self.subTest(where=where, change=change):
                    report = report_fixture()
                    target = report[where][0] if where == "outputs" else report[where]
                    target.update(change)
                    with self.assertRaises(benchmark.InvalidReport):
                        self.assess(report)

    def test_missed_frame_counts_and_rates_must_agree(self):
        for change in ({"missedFrames": 1001}, {"missedFrames": 50}, {"missedPercent": 0}, {"averageFps": 1000}):
            with self.subTest(change=change):
                report = report_fixture()
                report["outputs"][0].update(change)
                with self.assertRaises(benchmark.InvalidReport):
                    self.assess(report)

    def test_exact_one_percent_missed_fails_and_point_nine_passes(self):
        for missed, expected in ((9, True), (10, False)):
            with self.subTest(missed=missed):
                report = report_fixture()
                report["outputs"][0].update(missedFrames=missed, missedPercent=missed / 10)
                self.assertEqual(self.assess(report)["passed"], expected)

    def test_exact_two_period_latency_passes_and_over_budget_fails(self):
        for delta, expected in ((0, True), (0.01, False)):
            with self.subTest(delta=delta):
                report = report_fixture()
                value = 2000 / 60 + delta
                report["inputToSwap"].update(p95Ms=value, p99Ms=value, maxMs=value)
                self.assertEqual(self.assess(report)["passed"], expected)

    def test_a_faster_input_tail_does_not_hide_an_incomplete_run(self):
        for samples, posted in ((1, 40), (39, 40), (1, 1)):
            with self.subTest(samples=samples, posted=posted):
                report = report_fixture()
                report["inputToSwap"].update(samples=samples, posted=posted)
                self.assertFalse(self.assess(report)["passed"])

    def test_impossible_input_counts_fail(self):
        for change in ({"posted": 39}, {"posted": 41}, {"samples": 41}, {"posted": 40.0}, {"samples": 40.0}):
            with self.subTest(change=change):
                report = report_fixture()
                report["inputToSwap"].update(change)
                with self.assertRaises(benchmark.InvalidReport):
                    self.assess(report)

    def test_missing_posted_count_requires_fresh_complete_evidence(self):
        report = report_fixture()
        del report["inputToSwap"]["posted"]
        with self.assertRaises(benchmark.InvalidReport):
            self.assess(report)

    def test_zero_samples_are_valid_diagnostic_data_but_do_not_pass(self):
        report = report_fixture()
        for output in report["outputs"]:
            output.update({key: 0 for key in benchmark.METRICS})
            output.update(frames=0, missedFrames=0, missedPercent=0, averageFps=0)
        report["inputToSwap"].update({key: 0 for key in benchmark.METRICS})
        report["inputToSwap"].update(samples=0, posted=0)
        self.assertFalse(self.assess(report)["passed"])
        report["inputToSwap"]["maxMs"] = 1
        with self.assertRaises(benchmark.InvalidReport):
            self.assess(report)

    def test_nonempty_distribution_cannot_claim_zero_elapsed_time(self):
        report = report_fixture()
        report["inputToSwap"].update({key: 0 for key in benchmark.METRICS})
        with self.assertRaises(benchmark.InvalidReport):
            self.assess(report)

    def test_equal_refresh_and_one_output_remain_diagnostics(self):
        report = report_fixture()
        report["outputs"][1]["refreshHz"] = 60
        self.assertFalse(self.assess(report)["passed"])
        report["outputs"].pop()
        self.assertFalse(self.assess(report, 1)["passed"])

    def test_extreme_positive_refresh_does_not_overflow_budget(self):
        report = report_fixture()
        report["outputs"][0]["refreshHz"] = 5e-324
        with self.assertRaises(benchmark.InvalidReport):
            self.assess(report)

    def test_load_rejects_duplicate_fields_constants_and_malformed_json(self):
        for data in (b'{"outputs":[],"outputs":[]}', b'{"x":{"a":1,"a":2}}', b'{"x":NaN}',
                     b'{"x":Infinity}', b'{"x":-Infinity}', b'{"x":1e999}', b'{"x":-1e999}',
                     b'private-untrusted-value', b'\xff',
                     b'[' * 2000 + b']' * 2000,
                     b'{"x":' * 30 + b'0' + b'}' * 30,
                     b'{"x":[' + b'0,' * 16384 + b'0]}'):
            with self.subTest(data_length=len(data)), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "report.json"
                path.write_bytes(data)
                with self.assertRaises(benchmark.InvalidReport) as caught:
                    benchmark.load_report(path)
                self.assertNotIn("private-untrusted-value", str(caught.exception))
                self.assertNotIn(str(path), str(caught.exception))

    def test_load_is_bounded_and_closes_the_file(self):
        data = json.dumps(report_fixture()).encode()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            path.write_bytes(data + b' ' * (benchmark.MAX_REPORT_BYTES - len(data)))
            self.assertTrue(self.assess(benchmark.load_report(path))["passed"])
            with path.open("ab") as stream:
                stream.write(b' ')
            with self.assertRaises(benchmark.InvalidReport):
                benchmark.load_report(path)
            path.unlink()  # Also checks that the bounded loader released its file handle on Windows.

    def test_missing_file_has_a_bounded_diagnostic(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(benchmark.InvalidReport) as caught:
                benchmark.load_report(Path(directory) / "missing.json")
            self.assertNotIn(directory, str(caught.exception))


if __name__ == "__main__":
    unittest.main()
