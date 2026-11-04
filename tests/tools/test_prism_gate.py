# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Run the real gate launcher on Linux with inert programs and a stubbed hardware probe.

No compositor, client, display, DRM device or hardware command is reached. These test evidence and exit handling, not readiness.
"""

from __future__ import annotations

import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOL = REPO / "tools" / "dev" / "prism-gate.sh"


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("bash"), "needs Bash on Linux; no hardware is used")
class PrismGate(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.out = self.root / "result"
        self.build = self.root / "build"
        self.commands = self.root / "commands"
        self.commands.mkdir()
        self.marker = self.root / "launched"
        self.device = self.root / "inert-drm-placeholder"
        self.device.write_text("A file, never a device.\n")
        self.report = self.root / "fixture.json"
        self.script = self.root / "gate.sh"
        source = TOOL.read_text()
        before_probe = 'if [ "$action" = probe ]; then\n'
        self.assertEqual(source.count(before_probe), 1)
        # Override the discovery function before its first call; no real /sys, lspci or DRM probe can be used.
        source = source.replace(before_probe, 'probe() { best=fixture; best_count=2; total=2; panel=""; }\n\n' + before_probe, 1)
        self.script.write_text(source)
        self.program(
            self.build / "prism/compositor/prism",
            "import os,pathlib,sys,time\n"
            "pathlib.Path(os.environ['GATE_TEST_MARKER']).write_text('launched')\n"
            "args=sys.argv[1:]\n"
            "out=pathlib.Path(args[args.index('--benchmark-out')+1])\n"
            "if os.environ['GATE_TEST_WRITE_REPORT']=='1':\n"
            " out.write_bytes(pathlib.Path(os.environ['GATE_TEST_REPORT']).read_bytes())\n"
            "print('Output fixture-1',flush=True)\n"
            "print('Output fixture-2',flush=True)\n"
            "print('Prism is running',flush=True)\n"
            "time.sleep(1.3)\n"
            "raise SystemExit(int(os.environ['GATE_TEST_STATUS']))\n",
        )
        self.program(self.build / "tests/prism-test-client", "import time\ntime.sleep(60)\n")
        self.program(self.commands / "lspci", "print('Fixture VGA controller')\n")
        self.program(self.commands / "id", "import sys\nprint('fixture input' if sys.argv[1:] == ['-nG'] else '1000')\n")
        # The summary uses platform metadata; its values are inert too.
        (self.commands / "sitecustomize.py").write_text("import platform\nplatform.node=lambda:'fixture'\nplatform.release=lambda:'fixture'\n")

    def program(self, path, body):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f"#!{sys.executable}\n" + body)
        path.chmod(0o700)

    def run_gate(self, *, status=0, write_report=True, rates=(60, 120), trial=False):
        report = {
            "graphicsApi": "fixture",
            "outputs": [
                {"screen": f"fixture-{i}", "size": "100x100", "refreshHz": rate, "frames": 120, "p95Ms": 8, "missedPercent": 0.1}
                for i, rate in enumerate(rates)
            ],
            "inputToSwap": {"samples": 20, "p50Ms": 5, "p95Ms": 10},
        }
        self.report.write_text(json.dumps(report))
        environment = {
            **os.environ,
            "PATH": f"{self.commands}:{os.environ['PATH']}",
            "PYTHONPATH": str(self.commands),
            "XDG_RUNTIME_DIR": str(self.root),
            "GATE_TEST_MARKER": str(self.marker),
            "GATE_TEST_REPORT": str(self.report),
            "GATE_TEST_WRITE_REPORT": "1" if write_report else "0",
            "GATE_TEST_STATUS": str(status),
        }
        environment.pop("WAYLAND_DISPLAY", None)
        environment.pop("DISPLAY", None)
        args = ["bash", str(self.script), "--build-dir", str(self.build), "--out", str(self.out), "--seconds", "1"]
        if trial:
            environment["DISPLAY"] = "inert-fixture"
            args += ["--try"]
        else:
            args += ["--device", str(self.device)]
        process = subprocess.Popen(args, env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, start_new_session=True)
        try:
            stdout, stderr = process.communicate(timeout=20)
        finally:
            # Only the private session created above: no external process or real device is part of a test.
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            if process.poll() is None:
                process.wait(timeout=5)
        return subprocess.CompletedProcess(args, process.returncode, stdout, stderr)

    def test_a_fresh_success_with_different_refresh_rates_passes(self):
        result = self.run_gate()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.out / "summary.md").is_file())

    def test_existing_output_is_preserved_and_never_reused(self):
        self.out.mkdir()
        old_report = b'{"old":true}'
        (self.out / "report.json").write_bytes(old_report)
        result = self.run_gate(write_report=False, trial=True)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(self.marker.exists())
        self.assertEqual((self.out / "report.json").read_bytes(), old_report)
        self.assertFalse((self.out / "summary.md").exists())

    def test_a_failed_producer_cannot_pass_with_a_report(self):
        for status in (7, 124):
            with self.subTest(status=status):
                self.out = self.root / f"failed-{status}"
                result = self.run_gate(status=status)
                self.assertEqual(result.returncode, status, result.stdout + result.stderr)
                self.assertFalse((self.out / "summary.md").exists())

    def test_a_failed_trial_cannot_pass_with_a_report(self):
        result = self.run_gate(status=7, trial=True)
        self.assertEqual(result.returncode, 7, result.stdout + result.stderr)

    def test_missing_reports_fail_for_gate_and_trial(self):
        for trial in (False, True):
            with self.subTest(trial=trial):
                self.out = self.root / f"missing-{trial}"
                result = self.run_gate(write_report=False, trial=trial)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_equal_refresh_rates_do_not_pass_the_gate(self):
        result = self.run_gate(rates=(60, 60))
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_a_successful_equal_refresh_trial_is_labelled_as_a_trial(self):
        result = self.run_gate(rates=(60, 60), trial=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Trial inside a desktop session (not gate data)", (self.out / "summary.md").read_text())


if __name__ == "__main__":
    unittest.main()
