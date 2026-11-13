# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Offline discovery: useful on a fresh clone; metadata cannot run or escape it."""

import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("developer", ROOT / "tools/dev/developer.py")
developer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(developer)


class DeveloperTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        (self.root / "sdk").mkdir()
        (self.root / "docs").mkdir()
        (self.root / "docs/guide.md").write_text("# Guide\n", encoding="utf-8")
        (self.root / "sdk/example.cpp").write_text("PRIVATE_FIXTURE_CONTENT", encoding="utf-8")
        self.data = {"schema_version": 1, "entries": [{
            "id": "example", "title": "Example", "summary": "A reviewed integration entry.",
            "repository": "QOR-MATRIX/qor-os", "docs": ["docs/guide.md#guide"],
            "sources": ["sdk/example.cpp"], "prerequisites": ["A reviewed checkout."],
            "boundary": "Discovery does not execute the example.",
        }]}
        self.write()

    def write(self, data=None):
        (self.root / developer.CATALOG).write_text(json.dumps(self.data if data is None else data), encoding="utf-8")

    def invoke(self, args, root=None):
        original = developer.load_catalog
        output, errors = io.StringIO(), io.StringIO()
        with patch.object(developer, "load_catalog", side_effect=lambda: original(root or self.root)), \
                contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            code = developer.main(args)
        return code, output.getvalue(), errors.getvalue()

    def test_real_catalog_covers_integration_routes(self):
        entries, validation = developer.load_catalog(ROOT)
        by_id = {entry["id"]: entry for entry in entries}
        self.assertTrue({"apps", "lumen", "services", "prism", "q-pi", "qq", "qq-mcp", "arqade",
                         "media", "store", "qor-core", "launcher", "drc369", "launcher-agents"} <= set(by_id))
        self.assertEqual(by_id["drc369"]["repository"], "QOR-MATRIX/demiurge-chain")
        core = (ROOT / "core/CMakeLists.txt").read_text(encoding="utf-8")
        pin = re.search(r'set\(QOR_CORE_COMMIT "([0-9a-f]{40})"', core)[1]
        self.assertTrue(all("/" + pin + "/" in source for source in by_id["qor-core"]["sources"]))
        self.assertIn("not a sandbox", by_id["qq-mcp"]["boundary"])
        self.assertFalse(validation["upstream_content_verified"])

    def test_list_show_and_check_are_structured_and_do_not_run_tools(self):
        with patch.object(subprocess, "run", side_effect=AssertionError("Unexpected execution")), \
                patch.object(socket, "socket", side_effect=AssertionError("Unexpected connection")):
            for args in (["list", "--json"], ["--json", "show", "example"], ["check", "--json"], ["--json"]):
                with self.subTest(args=args):
                    code, output, errors = self.invoke(args)
                    result = json.loads(output)
                    self.assertEqual(code, 0)
                    self.assertEqual(errors, "")
                    self.assertTrue(result["ok"])
                    self.assertEqual(result["mode"], "read-only")
                    self.assertEqual(result["authority"], developer.AUTHORITY)
                    self.assertNotIn("PRIVATE_FIXTURE_CONTENT", output)
                    if "check" not in args:
                        self.assertEqual(result["entries"][0]["boundary"], self.data["entries"][0]["boundary"])

    def test_human_output_has_guides_prerequisites_and_boundary_without_ansi(self):
        code, output, errors = self.invoke(["show", "example"])
        self.assertEqual(code, 0)
        for word in ("Guide:", "Source:", "Requires:", "Boundary:", developer.AUTHORITY):
            self.assertIn(word, output)
        self.assertNotIn("\x1b", output)
        self.assertEqual(errors, "")

    def test_output_is_deterministic_and_sorted(self):
        second = copy.deepcopy(self.data["entries"][0])
        second["id"] = "aaa"
        self.data["entries"].append(second)
        self.write()
        first = self.invoke(["list", "--json"])
        self.assertEqual(first, self.invoke(["list", "--json"]))
        self.assertEqual([entry["id"] for entry in json.loads(first[1])["entries"]], ["aaa", "example"])

    def test_cli_from_another_directory_needs_only_python(self):
        result = subprocess.run([sys.executable, "-I", "-B", str(ROOT / "tools/dev/developer.py"), "check", "--json"],
                                cwd=self.root, capture_output=True, encoding="utf-8", timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)["ok"])

    def test_invalid_cli_never_echoes_arguments(self):
        for args in (["show", "PRIVATE_TEST_VALUE", "--json"], ["--token=PRIVATE_TEST_VALUE", "--json"],
                     ["show", "--json"], ["check", "PRIVATE_TEST_VALUE", "--json"], ["--js"]):
            with self.subTest(args=args):
                code, output, errors = self.invoke(args)
                self.assertEqual(code, 2)
                self.assertNotIn("PRIVATE_TEST_VALUE", output + errors)
                if "--json" in args:
                    self.assertFalse(json.loads(output)["ok"])
                    self.assertEqual(errors, "")

    def test_invalid_json_is_bounded_and_diagnostics_do_not_echo_values(self):
        payloads = [b'{"PRIVATE_TEST_VALUE":', b'\xff', b'{}' + b' ' * developer.MAX_BYTES,
                    b'[' * 2000 + b']' * 2000, b'{"schema_version":1,"schema_version":1,"entries":[]}',
                    b'{"schema_version":NaN,"entries":[]}']
        for raw in payloads:
            with self.subTest(size=len(raw)):
                (self.root / developer.CATALOG).write_bytes(raw)
                code, output, errors = self.invoke(["list", "--json"])
                self.assertEqual(code, 2)
                self.assertFalse(json.loads(output)["ok"])
                self.assertNotIn("PRIVATE_TEST_VALUE", output + errors)

    def test_unknown_fields_types_and_duplicate_entries_are_rejected(self):
        invalid = []
        for key, value in (("schema_version", True), ("schema_version", 2), ("entries", {}), ("entries", []),
                           ("entries", self.data["entries"] * 2), ("entries", self.data["entries"] * 33),
                           ("execute", "PRIVATE_TEST_VALUE")):
            data = copy.deepcopy(self.data)
            data[key] = value
            invalid.append(data)
        for field, value in (("id", "../escape"), ("title", "\x1b[2J"), ("summary", "\u202ePRIVATE_TEST_VALUE"),
                             ("boundary", "\nPRIVATE_TEST_VALUE"), ("summary", "x" * 321), ("title", 0),
                             ("repository", "other/project"), ("prerequisites", "not a list"),
                             ("prerequisites", ["duplicate", "duplicate"]), ("prerequisites", []),
                             ("prerequisites", [None]), ("docs", []), ("sources", []),
                             ("run", "PRIVATE_TEST_VALUE")):
            data = copy.deepcopy(self.data)
            data["entries"][0][field] = value
            invalid.append(data)
        for data in invalid:
            with self.subTest(data=data):
                self.write(data)
                code, output, errors = self.invoke(["list", "--json"])
                self.assertEqual(code, 2)
                self.assertNotIn("PRIVATE_TEST_VALUE", output + errors)

    def test_hostile_local_references_are_rejected(self):
        for value in ("../outside", "/etc/passwd", "C:/Windows", "//server/share", "sdk/../../outside",
                      "sdk\\example.cpp", "sdk/%2e%2e/file", "sdk/.env", "sdk/provider-secret.txt",
                      "sdk/CON", "sdk/aux.txt", "sdk/example.cpp:stream", "sdk/example.cpp.",
                      "build/private.txt", "https://example.invalid/file", "sdk//example.cpp",
                      "sdk/missing", "sdk/example.cpp#bad#fragment"):
            with self.subTest(value=value):
                self.data["entries"][0]["sources"] = [value]
                self.write()
                with self.assertRaises(developer.DiscoveryError):
                    developer.load_catalog(self.root)

    def test_upstream_urls_require_exact_host_pins_and_active_source_roots(self):
        base = "https://github.com/QOR-MATRIX/demiurge-chain/blob/" + "a" * 40
        valid = base + "/tools/qor-launcher/core/src/agents.rs"
        self.data["entries"][0]["repository"] = "QOR-MATRIX/demiurge-chain"
        self.data["entries"][0]["sources"] = [valid]
        self.write()
        self.assertEqual(developer.load_catalog(self.root)[1]["upstream_references_checked"], 1)
        for value in (valid.replace("github.com/", "github.com.evil.invalid/"),
                      valid.replace("github.com/", "user@github.com/"), valid.replace("https:", "http:"),
                      valid.replace("a" * 40, "main"), valid + "?access=PRIVATE_TEST_VALUE", valid + "#fragment",
                      base + "/sdk/src/drc369.ts", base + "/tools/qor-launcher/../../sdk/file", "docs/guide.md"):
            with self.subTest(value=value):
                self.data["entries"][0]["sources"] = [value]
                self.write()
                code, output, errors = self.invoke(["check", "--json"])
                self.assertEqual(code, 2)
                self.assertNotIn("PRIVATE_TEST_VALUE", output + errors)

    def test_links_and_junctions_are_refused_before_catalog_open(self):
        for name in ("is_symlink", "is_junction"):
            with self.subTest(name=name), patch.object(Path, name, return_value=True), patch.object(Path, "open") as opened:
                with self.assertRaises(developer.DiscoveryError):
                    developer.load_catalog(self.root)
            opened.assert_not_called()

    def test_real_symlink_cannot_redirect_references_outside_checkout(self):
        with tempfile.TemporaryDirectory() as outside:
            link = self.root / "sdk/linked"
            try:
                link.symlink_to(outside, target_is_directory=True)
            except OSError:
                self.skipTest("Host does not permit creating symlinks")
            self.data["entries"][0]["sources"] = ["sdk/linked"]
            self.write()
            with self.assertRaises(developer.DiscoveryError):
                developer.load_catalog(self.root)

    def test_referenced_documents_and_sources_are_never_opened(self):
        original = Path.open
        opened = []

        def catalog_only(path, *args, **kwargs):
            self.assertEqual(path, self.root / developer.CATALOG)
            opened.append(path)
            return original(path, *args, **kwargs)

        with patch.object(Path, "open", catalog_only):
            developer.load_catalog(self.root)
        self.assertEqual(len(opened), 1)

    def test_missing_directory_or_oversized_catalog_fails_before_read(self):
        path = self.root / developer.CATALOG
        path.unlink()
        with self.assertRaises(developer.DiscoveryError):
            developer.load_catalog(self.root)
        path.mkdir()
        with self.assertRaises(developer.DiscoveryError):
            developer.load_catalog(self.root)
        path.rmdir()
        path.write_bytes(b" " * (developer.MAX_BYTES + 1))
        with patch.object(Path, "open") as opened:
            with self.assertRaises(developer.DiscoveryError):
                developer.load_catalog(self.root)
        opened.assert_not_called()


if __name__ == "__main__":
    unittest.main()
