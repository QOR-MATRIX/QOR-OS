# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""The exact public-hash classification must leave lookalikes detectable."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("gitleaks"), "The separately pinned gitleaks tool is unavailable")
class BrandingScan(unittest.TestCase):
    def scan(self, relative, manifest):
        with tempfile.TemporaryDirectory(prefix="qor-public-hash-scan-") as root:
            path = Path(root) / relative
            path.parent.mkdir(parents=True)
            path.write_text(json.dumps(manifest, indent=4) + "\n", encoding="utf-8")
            result = subprocess.run(
                ["gitleaks", "dir", root, "--redact=100", "--no-banner", "--log-level", "error",
                 "--exit-code", "1", "--config", str(REPO / "tools/ci/gitleaks.toml")],
                capture_output=True, timeout=30,
            )
            # Captured findings remain in memory; assertions expose only status.
            return result.returncode

    def manifest(self):
        return json.loads((REPO / "kit/branding/manifest.json").read_text(encoding="utf-8"))

    def test_verified_public_hashes_pass(self):
        manifest = self.manifest()
        for name, digest in manifest["inputs"].items():
            self.assertEqual(hashlib.sha256((REPO / name).read_bytes()).hexdigest(), digest)
        self.assertEqual(self.scan("kit/branding/manifest.json", manifest), 0)

    def test_different_value_at_same_path_still_fails(self):
        manifest = self.manifest()
        # Public synthetic control, never an actual credential or private datum.
        manifest["inputs"]["kit/Tokens.qml"] = hashlib.sha256(b"public scanner regression control").hexdigest()
        self.assertEqual(self.scan("kit/branding/manifest.json", manifest), 1)

    def test_same_values_at_another_path_still_fail(self):
        self.assertEqual(self.scan("other/manifest.json", self.manifest()), 1)

    def test_another_field_in_same_manifest_still_fails(self):
        manifest = self.manifest()
        manifest["api_key"] = hashlib.sha256(b"public additional field control").hexdigest()
        self.assertEqual(self.scan("kit/branding/manifest.json", manifest), 1)


@unittest.skipUnless(shutil.which("gitleaks"), "The separately pinned gitleaks tool is unavailable")
class HomepageScan(unittest.TestCase):
    scan = BrandingScan.scan

    def manifest(self):
        return json.loads((REPO / "site/upstream-source.json").read_text(encoding="utf-8"))

    def test_verified_public_hash_passes(self):
        manifest = self.manifest()
        source = subprocess.check_output(["git", "show", "HEAD:site/tokens.css"], cwd=REPO)
        self.assertEqual(hashlib.sha256(source).hexdigest(), manifest["files"]["site/tokens.css"])
        self.assertEqual(self.scan("site/upstream-source.json", manifest), 0)

    def test_different_hash_still_fails(self):
        manifest = self.manifest()
        manifest["files"]["site/tokens.css"] = hashlib.sha256(b"public scanner regression control").hexdigest()
        self.assertEqual(self.scan("site/upstream-source.json", manifest), 1)

    def test_another_path_still_fails(self):
        self.assertEqual(self.scan("other/upstream-source.json", self.manifest()), 1)

    def test_another_field_still_fails(self):
        manifest = self.manifest()
        manifest["api_key"] = hashlib.sha256(b"public additional field control").hexdigest()
        self.assertEqual(self.scan("site/upstream-source.json", manifest), 1)


if __name__ == "__main__":
    unittest.main()
