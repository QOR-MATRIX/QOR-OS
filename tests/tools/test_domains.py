# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""The canonical domain check finds old links without exposing their contents."""

import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("domain_policy", ROOT / "tools/ci/check_domains.py")
policy = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(policy)


class DomainPolicyTest(unittest.TestCase):
    def test_repository_has_no_retired_domains(self):
        checked, findings = policy.check(ROOT)
        self.assertGreater(checked, 0)
        self.assertEqual(findings, [])

    def test_retired_subdomains_case_encodings_and_mail_are_found(self):
        for suffix in ("cloud", "guru", "io", "network"):
            host = "demiurge" + "." + suffix
            for text in (host, "https://rpc." + host.upper(), "contact@" + host,
                         host.replace(".", "&#46;"), host.replace(".", "%2E"),
                         host.replace(".", "%252e"), host.replace(".", "\uff0e")):
                with self.subTest(suffix=suffix):
                    self.assertEqual(policy.retired_lines("safe\n" + text), [2])

    def test_canonical_hosts_source_links_local_fixtures_and_identifiers_remain_valid(self):
        values = ["https://qorsync.dev", "wss://rpc.qorsync.dev", "https://id.qorsync.dev/api/v1",
                  "https://github.com/QOR-MATRIX/qor-os", "http://localhost:8080", "https://example.invalid",
                  "cloud.demiurge.qor", "demiurge:qor-id:challenge:v1:", "demiurge.initialize()"]
        self.assertEqual(policy.retired_lines("\n".join(values)), [])

    def test_long_subdomain_prefix_does_not_need_recursive_matching(self):
        prefix = "subdomain." * 10000
        self.assertEqual(policy.retired_lines(prefix + "qorsync.dev"), [])
        self.assertEqual(policy.retired_lines(prefix + "demiurge" + ".cloud"), [1])

    def test_diagnostics_never_print_matched_content(self):
        output = io.StringIO()
        with patch.object(policy, "check", return_value=(1, [("docs/page.md", 7)])), contextlib.redirect_stderr(output):
            self.assertEqual(policy.main(), 1)
        self.assertIn("'docs/page.md':7", output.getvalue())
        with patch.object(policy, "check", side_effect=ValueError("PRIVATE_TEST_VALUE")), contextlib.redirect_stderr(output):
            self.assertEqual(policy.main(), 2)
        self.assertNotIn("PRIVATE_TEST_VALUE", output.getvalue())

    def test_real_temporary_repository_and_restricted_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "--quiet", str(root)], check=True, capture_output=True)
            (root/"README.md").write_text("https://qorsync.dev\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(root), "add", "README.md"], check=True, capture_output=True)
            original = Path.open

            def read_allowed(path, *args, **kwargs):
                self.assertFalse(path.name.startswith(".env"))
                return original(path, *args, **kwargs)

            with patch.object(policy.subprocess, "check_output", return_value=b"README.md\0.env.fixture\0secret.md\0"), \
                    patch.object(Path, "open", read_allowed):
                self.assertEqual(policy.check(root), (1, []))
            (root/"README.md").write_text("https://" + "demiurge" + ".cloud\n", encoding="utf-8")
            self.assertEqual(policy.check(root), (1, [("README.md", 1)]))

    def test_symlink_and_junction_refused_before_opening(self):
        for name in ("is_symlink", "is_junction"):
            with self.subTest(name=name), patch.object(policy.subprocess, "check_output", return_value=b"README.md\0"), \
                    patch.object(Path, name, return_value=True), patch.object(Path, "open") as opened:
                with self.assertRaises(ValueError):
                    policy.check(ROOT)
            opened.assert_not_called()

    def test_oversized_or_malformed_text_is_not_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root/"README.md"
            for content in (b"x" * (policy.MAX_BYTES+1), b"\xff"):
                path.write_bytes(content)
                with patch.object(policy.subprocess, "check_output", return_value=b"README.md\0"):
                    with self.assertRaises(ValueError):
                        policy.check(root)


if __name__ == "__main__":
    unittest.main()
