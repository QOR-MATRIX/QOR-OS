# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Windows build-asset tests, never boot or animation evidence."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / "tools/branding/resize_menu_mark.ps1"


@unittest.skipUnless(sys.platform == "win32", "The menu derivative uses the existing Windows build host")
class MenuMark(unittest.TestCase):
    def run_script(self, path):
        return subprocess.run(["pwsh", "-NoProfile", "-File", str(SCRIPT), "-Output", str(path)],
                              capture_output=True, text=True, timeout=30)

    def test_bounded_official_derivative_and_existing_output_preserved(self):
        with tempfile.TemporaryDirectory(dir=REPO / "build") as directory:
            output = Path(directory) / "menu-mark.png"
            result = self.run_script(output)
            self.assertEqual(result.returncode, 0, result.stderr)
            data = output.read_bytes()
            self.assertEqual(struct.unpack_from(">II", data, 16), (112, 112))
            record = json.loads(output.with_suffix(".png.json").read_text())
            self.assertEqual(record["sha256"], hashlib.sha256(data).hexdigest())
            self.assertEqual(record["source_sha256"], hashlib.sha256((REPO / record["source"]).read_bytes()).hexdigest())
            self.assertEqual(output.with_suffix(".png.license").read_bytes(),
                             (REPO / "kit/branding/qor-mark.png.license").read_bytes())
            self.assertNotEqual(self.run_script(output).returncode, 0)
            self.assertEqual(data, output.read_bytes())

    def test_output_outside_qor_build_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "must-not-exist.png"
            self.assertNotEqual(self.run_script(output).returncode, 0)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    if sys.platform != "win32":
        raise SystemExit("Windows menu-asset suite was not run on this host")
    unittest.main()
