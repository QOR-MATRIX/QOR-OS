# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Run the actual workflow's change detector with a stand-in Git; no remote or product build is touched."""

import json
import os
import pathlib
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

from tests.tools.test_build_workflow import job


class ProductChanges(unittest.TestCase):
    def detect(self, changes="products/qq/runtime/ScreenText.qml\n", failure="", event="push", ref="refs/heads/test"):
        windows = sys.platform == "win32"
        lines = job("windows" if windows else "linux")
        start = lines.index("      - name: Did the carried products change?")
        start = lines.index("        run: |", start) + 1
        end = next(i for i in range(start, len(lines)) if lines[i].strip() and not lines[i].startswith("          "))
        script = "\n".join(line[10:] for line in lines[start:end])
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            fixture = root / "git.py"
            fixture.write_text(
                "import json, os, pathlib, sys\n"
                "with open(os.environ['QOR_GIT_CALLS'], 'a') as calls:\n"
                "    calls.write(json.dumps(sys.argv[1:]) + '\\n')\n"
                "if sys.argv[1] == os.environ['QOR_GIT_FAIL']:\n"
                "    sys.exit(23)\n"
                "if sys.argv[1] == 'diff':\n"
                "    sys.stdout.write(pathlib.Path(os.environ['QOR_GIT_DIFF']).read_text())\n", encoding="utf-8")
            if windows:
                (root / "git.cmd").write_text(f'@echo off\n"{sys.executable}" "{fixture}" %*\n', encoding="utf-8")
                # Match GitHub's pwsh wrapper: native status is checked again at the end, too.
                script = '$ErrorActionPreference = "Stop"\n' + script + '\nif (Test-Path variable:LASTEXITCODE) { exit $LASTEXITCODE }'
                command = [shutil.which("pwsh"), "-NoProfile", "-Command", script]
            else:
                fake = root / "git"
                fake.write_text(f'#!/bin/sh\nexec {shlex.quote(sys.executable)} {shlex.quote(str(fixture))} "$@"\n', encoding="utf-8")
                fake.chmod(0o700)
                command = [shutil.which("bash"), "-euo", "pipefail", "-c", script]
            diff, calls, output, summary = (root / name for name in ("diff", "calls", "output", "summary"))
            diff.write_text(changes, encoding="utf-8")
            env = dict(os.environ, PATH=str(root) + os.pathsep + os.environ.get("PATH", ""),
                       QOR_GIT_FAIL=failure, QOR_GIT_DIFF=str(diff), QOR_GIT_CALLS=str(calls),
                       EVENT=event, BEFORE="1234abcd", GITHUB_REF=ref, GITHUB_OUTPUT=str(output), GITHUB_STEP_SUMMARY=str(summary))
            done = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=30)
            return (done.returncode, output.read_text(encoding="utf-8-sig") if output.exists() else "",
                    summary.read_text(encoding="utf-8-sig") if summary.exists() else "",
                    [json.loads(line) for line in calls.read_text().splitlines()] if calls.exists() else [], done.stdout + done.stderr)

    def test_git_failures_stop_without_claiming_unchanged(self):
        for failure in ("fetch", "diff"):
            with self.subTest(failure=failure):
                code, output, summary, calls, said = self.detect(failure=failure)
                self.assertNotEqual(code, 0, said)
                self.assertEqual(output, "")
                self.assertEqual(summary, "")
                self.assertEqual([call[0] for call in calls], ["fetch"] if failure == "fetch" else ["fetch", "diff"])

    def test_a_large_diff_still_requests_the_products(self):
        code, output, summary, _, said = self.detect(changes="products/qq/runtime/ScreenText.qml\n" + "docs/unchanged-check.txt\n" * 100000)
        self.assertEqual(code, 0, said)
        self.assertEqual(output, "run=true\n")
        self.assertIn("requested", summary)
        self.assertNotIn("built and tested", summary)

    def test_unchanged_products_are_only_a_request_decision(self):
        code, output, summary, _, said = self.detect(changes="docs/README.md\n")
        self.assertEqual(code, 0, said)
        self.assertEqual(output, "run=false\n")
        self.assertIn("requested", summary)
        self.assertNotIn("built and tested", summary)

    def test_manual_and_tag_runs_do_not_need_a_diff(self):
        for event, ref in (("workflow_dispatch", "refs/heads/test"), ("push", "refs/tags/vtest")):
            with self.subTest(event=event, ref=ref):
                code, output, _, calls, said = self.detect(event=event, ref=ref)
                self.assertEqual(code, 0, said)
                self.assertEqual(output, "run=true\n")
                self.assertEqual(calls, [])


if __name__ == "__main__":
    unittest.main()
