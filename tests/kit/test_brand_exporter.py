# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Exercise the real renderer and its failure statuses on a private display."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/branding"))
import assets

PROGRAM = sys.argv.pop(1)


class Exporter(unittest.TestCase):
    def run_export(self, output, *, software=False):
        env = dict(os.environ)
        if software:
            env["QT_QUICK_BACKEND"] = "software"
        result = subprocess.run([PROGRAM, "--output", str(output)], env=env,
                                capture_output=True, timeout=25)
        # Never echo captured process output: the status and assertion identify
        # the failing path without exposing unrelated Qt environment diagnostics.
        return result.returncode

    def test_real_mark_has_valid_provenance(self):
        with tempfile.TemporaryDirectory(prefix="qor-mark-") as root:
            output = Path(root) / "new"
            self.assertEqual(self.run_export(output), 0)
            self.assertEqual(assets.validate(REPO, output)["source"], "kit/QorMark.qml")

    def test_nonempty_directory_preserves_work_and_fails(self):
        with tempfile.TemporaryDirectory(prefix="qor-mark-") as root:
            output = Path(root)
            sentinel = output / ".unfinished"
            sentinel.write_bytes(b"preserve me")
            self.assertEqual(self.run_export(output), 2)
            self.assertEqual(sentinel.read_bytes(), b"preserve me")
            self.assertFalse((output / "manifest.json").exists())

    def test_software_shader_failure_is_not_a_successful_asset(self):
        with tempfile.TemporaryDirectory(prefix="qor-mark-") as root:
            output = Path(root) / "new"
            self.assertEqual(self.run_export(output, software=True), 1)
            self.assertFalse((output / "manifest.json").exists())


if __name__ == "__main__":
    unittest.main()
