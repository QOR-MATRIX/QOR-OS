# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Exercise the exact public-artifact exception with the real pinned scanner."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
LOCK = Path("image/pi/kernel-wait-bo/builder-lock.json")
SCANNER = None


class PublicArchiveBounds(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        result = subprocess.run([SCANNER, "version"], capture_output=True, text=True, timeout=10, check=True)
        if result.stdout.strip() != "8.30.1":
            raise RuntimeError("Use the Security workflow's pinned gitleaks version")
        cls.manifest = json.loads((ROOT / LOCK).read_text(encoding="utf-8"))
        records = [p for p in cls.manifest["packages"] if p["Package"] == "pass" + "wd"]
        if len(records) != 1:
            raise RuntimeError("Expected one public archive record")
        cls.archive = records[0]["archive"]

    def scan(self, path, content):
        with tempfile.TemporaryDirectory(prefix="qor-public-archive-") as temp:
            base = Path(temp)
            source = base / "input"
            target = source / path
            target.parent.mkdir(parents=True)
            target.write_text(content, encoding="utf-8")
            report = base / "findings.json"
            result = subprocess.run(
                [SCANNER, "dir", str(source), "--redact", "--no-banner", "--log-level", "error",
                 "--exit-code", "1", "--config", str(ROOT / "tools/ci/gitleaks.toml"),
                 "--report-format", "json", "--report-path", str(report)],
                capture_output=True, text=True, timeout=30,
            )
            self.assertIn(result.returncode, (0, 1), "Scanner execution failed")
            self.assertTrue(report.is_file(), "Scanner did not write its report")
            findings = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(result.returncode, int(bool(findings)))
            return [f["RuleID"] for f in findings]

    def test_complete_verified_manifest_is_accepted(self):
        self.assertEqual(self.scan(LOCK, json.dumps(self.manifest, indent=2)), [])

    def test_exact_value_is_not_allowed_in_another_path(self):
        self.assertIn("generic-api-key", self.scan(Path("other.json"), json.dumps({"archive": self.archive}, indent=2)))

    def test_changed_archive_is_not_allowed(self):
        changed = self.archive.replace("4.17.4-2", "4.17.4-3")
        self.assertNotEqual(changed, self.archive)
        self.assertIn("generic-api-key", self.scan(LOCK, json.dumps({"archive": changed}, indent=2)))

    def test_exact_value_is_not_allowed_in_another_field(self):
        self.assertIn("generic-api-key", self.scan(LOCK, json.dumps({"other": self.archive}, indent=2)))

    def test_extra_field_on_the_line_is_not_exempt(self):
        self.assertIn("generic-api-key", self.scan(LOCK, json.dumps({"archive": self.archive, "other": "value"})))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--gitleaks", required=True)
    args, rest = parser.parse_known_args()
    SCANNER = args.gitleaks
    unittest.main(argv=[sys.argv[0], *rest])
