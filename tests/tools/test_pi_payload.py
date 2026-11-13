# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Explicit target inventory and containment tests; no target program is run."""

import contextlib
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
import pi_payload as payload
from test_pi_elf import versioned_fixture


def elf(needed=(), soname=None, interpreter=None, machine=183, align=16384):
    """Small ELF fixture with genuine dynamic entries and one mapped load."""
    data = bytearray(1024)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHI", data, 16, 3, machine, 1)
    struct.pack_into("<Q", data, 32, 64)
    struct.pack_into("<HHH", data, 52, 64, 56, 3 if interpreter else 2)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, len(data), len(data), align)
    strings = bytearray(b"\0")
    entries = []
    for tag, value in [(1, value) for value in needed] + ([(14, soname)] if soname else []):
        entries.append((tag, len(strings)))
        strings.extend(value.encode() + b"\0")
    entries.extend([(5, 512), (10, len(strings)), (0, 0)])
    struct.pack_into("<IIQQQQQQ", data, 120, 2, 6, 256, 256, 0,
                     16 * len(entries), 16 * len(entries), 8)
    for index, (tag, value) in enumerate(entries):
        struct.pack_into("<qQ", data, 256 + 16 * index, tag, value)
    data[512:512 + len(strings)] = strings
    if interpreter:
        raw = interpreter.encode() + b"\0"
        struct.pack_into("<IIQQQQQQ", data, 176, 3, 4, 768, 768, 0, len(raw), len(raw), 1)
        data[768:768 + len(raw)] = raw
    return bytes(data)


class PayloadTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.manifest = {"schema": 1, "files": []}

    def add(self, name, content=None, role="library"):
        content = elf() if content is None else content
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        self.manifest["files"].append({"path": name, "role": role,
                                       "sha256": hashlib.sha256(content).hexdigest()})
        return path

    def check(self):
        return payload.audit(self.root, self.manifest)

    def test_complete_explicit_names_do_not_certify_build(self):
        self.add("bin/app", elf(["libfixture.so.1"], interpreter="/lib/ld-linux-aarch64.so.1"), "application")
        self.add("lib/libfixture.so.1", elf(soname="libfixture.so.1"))
        self.add("lib/ld-linux-aarch64.so.1", elf(), "loader")
        report = self.check()
        self.assertTrue(report["static_audit_passed"], report["issues"])
        self.assertFalse(report["build_qualified"])
        self.assertFalse(report["target_code_executed"])
        self.assertEqual(report["files"][0]["needed_candidates"],
                         {"libfixture.so.1": ["lib/libfixture.so.1"]})

    def test_never_searches_unlisted_dependency(self):
        self.add("bin/app", elf(["libmissing.so.1"]), "application")
        (self.root / "libmissing.so.1").write_bytes(elf())
        report = self.check()
        self.assertFalse(report["static_audit_passed"])
        self.assertEqual(report["files"][0]["needed_candidates"], {"libmissing.so.1": []})

    def test_transitive_missing_dependency_fails(self):
        self.add("bin/app", elf(["liba.so"]), "application")
        self.add("lib/liba.so", elf(["libb.so"], soname="liba.so"))
        self.assertFalse(self.check()["static_audit_passed"])

    def test_exact_version_labels_agree_without_abi_or_runtime_acceptance(self):
        self.add("bin/app", versioned_fixture(required={"libc.so.6":
                 ("GLIBC_2.17", "GLIBC_2.34", "GLIBC_ABI_DT_RELR")}), "application")
        self.add("lib/libc.so.6", versioned_fixture(base_name="libc.so.6",
                 defined=(("GLIBC_2.17", ()), ("GLIBC_2.34", ("GLIBC_2.17",)), ("GLIBC_ABI_DT_RELR", ()))))
        report = self.check()
        self.assertTrue(report["static_audit_passed"], report["issues"])
        self.assertEqual(report["files"][0]["version_label_checks"],
                         [{"needed": "libc.so.6", "candidate": "lib/libc.so.6",
                           "required_count": 3, "missing": []}])
        self.assertFalse(report["build_qualified"])
        self.assertFalse(report["target_code_executed"])

    def test_higher_looking_label_or_parent_does_not_supply_exact_definition(self):
        for parents in ((), ("GLIBC_2.34",)):
            self.manifest["files"] = []
            self.add("bin/app", versioned_fixture(required={"libc.so.6": ("GLIBC_2.34",)}), "application")
            self.add("lib/libc.so.6", versioned_fixture(defined=(("GLIBC_2.99", parents),)))
            report = self.check()
            self.assertFalse(report["static_audit_passed"])
            self.assertEqual(report["files"][0]["version_label_checks"][0]["missing"], ["GLIBC_2.34"])

    def test_label_from_other_dependency_cannot_satisfy_requirement(self):
        self.add("bin/app", versioned_fixture(required={"libc.so.6": ("GLIBC_2.34",),
                                                       "libother.so": ("OTHER_1",)}), "application")
        self.add("lib/libc.so.6", versioned_fixture(defined=(("GLIBC_2.17", ()),)))
        self.add("lib/libother.so", versioned_fixture(defined=(("GLIBC_2.34", ()), ("OTHER_1", ()))))
        report = self.check()
        self.assertFalse(report["static_audit_passed"])
        self.assertEqual(report["files"][0]["version_label_checks"][0]["missing"], ["GLIBC_2.34"])

    def test_unversioned_or_base_only_provider_refuses_even_weak_needs(self):
        for content in (elf(), versioned_fixture(base_name="GLIBC_2.34")):
            for weak in (False, True):
                with self.subTest(weak=weak, size=len(content)):
                    self.manifest["files"] = []
                    self.add("bin/app", versioned_fixture(required={"libc.so.6": ("GLIBC_2.34",)},
                                                         weak_needs=weak), "application")
                    self.add("lib/libc.so.6", content)
                    self.assertFalse(self.check()["static_audit_passed"])

    def test_ambiguous_provider_never_gets_version_agreement(self):
        self.add("bin/app", versioned_fixture(required={"libc.so.6": ("GLIBC_2.34",)}), "application")
        for directory in ("one", "two"):
            self.add(directory + "/libc.so.6", versioned_fixture(defined=(("GLIBC_2.34", ()),)))
        report = self.check()
        self.assertFalse(report["static_audit_passed"])
        self.assertEqual(report["files"][0]["version_label_checks"], [])

    def test_ambiguous_dependency_fails(self):
        self.add("bin/app", elf(["libsame.so"]), "application")
        self.add("one/libsame.so")
        self.add("two/libsame.so")
        self.assertFalse(self.check()["static_audit_passed"])

    def test_missing_interpreter_is_not_opened_on_host(self):
        self.add("bin/app", elf(interpreter="/lib/ld-linux-aarch64.so.1"), "application")
        report = self.check()
        self.assertFalse(report["static_audit_passed"])
        self.assertIn("interpreter absent", report["issues"][0]["problem"])

    def test_interpreter_must_have_loader_role(self):
        self.add("bin/app", elf(interpreter="/lib/ld-linux-aarch64.so.1"), "application")
        self.add("lib/ld-linux-aarch64.so.1")
        self.assertFalse(self.check()["static_audit_passed"])

    def test_wrong_architecture_or_alignment_in_plugin_fails(self):
        for content in (elf(machine=62), elf(align=4096)):
            with self.subTest(content=content[18:20]):
                self.manifest["files"] = []
                self.add("plugins/platforms/plugin.so", content, "plugin")
                self.assertFalse(self.check()["static_audit_passed"])

    def test_changed_hash_and_malformed_elf_fail(self):
        path = self.add("lib/library.so")
        path.write_bytes(elf(soname="changed.so"))
        self.assertIn("SHA-256 differs", self.check()["issues"][0]["problem"])
        self.manifest["files"] = []
        self.add("lib/library.so", b"not an ELF")
        self.assertFalse(self.check()["static_audit_passed"])

    def test_unsafe_manifest_paths(self):
        for name in ("../outside", "/absolute", "C:/host", "a\\b", "a/./b", "a//b",
                     "a/../b", "file:stream", "NUL", "nul.so", "a/COM1", "a/b.", "a\nsecret"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                payload.relative_path(name)

    def test_manifest_schema_refusals(self):
        self.add("lib/a.so")
        valid = json.dumps(self.manifest)
        bad = [b"{}", b'{"schema":1,"schema":1,"files":[]}', b"\xff",
               valid.replace('"schema": 1', '"schema": true').encode(),
               valid.replace('"role": "library"', '"role": "unknown"').encode(),
               valid.replace('"files":', '"extra": 1, "files":').encode(),
               b"[" * 2000]
        for raw in bad:
            with self.subTest(raw=raw[:50]), self.assertRaises(ValueError):
                payload.parse_manifest(raw)
        with self.assertRaises(ValueError):
            payload.parse_manifest(b" " * (payload.MAX_MANIFEST_BYTES + 1))

    def test_duplicate_case_colliding_paths(self):
        self.add("lib/a.so")
        self.manifest["files"].append({**self.manifest["files"][0], "path": "LIB/A.SO"})
        with self.assertRaises(ValueError):
            self.check()

    def test_empty_manifest_and_excessive_file_count(self):
        with self.assertRaises(ValueError):
            self.check()
        self.add("lib/a.so")
        with patch.object(payload, "MAX_FILES", 0), self.assertRaises(ValueError):
            self.check()

    def test_missing_or_directory_input_fails_before_inspection(self):
        self.add("lib/a.so").unlink()
        with patch.object(payload.pi_elf, "inspect_file") as inspect:
            self.assertFalse(self.check()["static_audit_passed"])
            inspect.assert_not_called()
        (self.root / "lib/a.so").mkdir()
        self.assertFalse(self.check()["static_audit_passed"])

    def test_aggregate_budget_checked_before_any_hashing(self):
        self.add("lib/a.so")
        self.add("lib/b.so")
        with patch.object(payload, "MAX_TOTAL_BYTES", 1024), patch.object(payload.pi_elf, "inspect_file") as inspect:
            self.assertFalse(self.check()["static_audit_passed"])
            inspect.assert_not_called()

    def test_report_expansion_is_bounded(self):
        for index in range(8):
            self.add(f"lib/library{index}.so")
        with patch.object(payload, "MAX_REPORT_BYTES", 1024):
            report = self.check()
        self.assertFalse(report["static_audit_passed"])
        self.assertLessEqual(len(json.dumps(report, indent=2).encode()), 1024)

    def test_missing_dependency_issues_are_bounded_while_collected(self):
        path = self.add("lib/" + "a" * 180 + ".so")
        info = payload.pi_elf.inspect_file(path)
        info["needed"] = [f"libmissing{index}.so" for index in range(200)]
        with patch.object(payload.pi_elf, "inspect_file", return_value=info), \
                patch.object(payload, "MAX_REPORT_BYTES", 8192):
            report = self.check()
        self.assertFalse(report["static_audit_passed"])
        self.assertLess(len(report["issues"]), 200)
        self.assertLessEqual(len(json.dumps(report, indent=2).encode()), 8192)

    def test_outside_symlink_refused(self):
        with tempfile.TemporaryDirectory() as other:
            target = Path(other) / "outside.so"
            target.write_bytes(elf())
            path = self.add("lib/a.so")
            path.unlink()
            try:
                path.symlink_to(target)
            except OSError:
                self.skipTest("symlink privilege unavailable")
            with patch.object(payload.pi_elf, "inspect_file") as inspect:
                self.assertFalse(self.check()["static_audit_passed"])
                inspect.assert_not_called()

    def test_cli_json_and_exit_codes(self):
        self.add("lib/a.so")
        manifest = self.root / "manifest.json"
        manifest.write_text(json.dumps(self.manifest))
        for valid in (True, False):
            if not valid:
                manifest.write_text("{}")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                code = payload.main(["--root", str(self.root), "--manifest", str(manifest)])
            report = json.loads(output.getvalue())
            self.assertEqual(code, 0 if valid else 2)
            self.assertEqual(report["static_audit_passed"], valid)
            self.assertFalse(report["build_qualified"])

    def test_cli_budget_includes_manifest_fingerprint(self):
        self.add("lib/a.so")
        manifest = self.root / "manifest.json"
        manifest.write_text(json.dumps(self.manifest))
        before_digest_size = len(json.dumps(self.check(), indent=2).encode())
        output = io.StringIO()
        with patch.object(payload, "MAX_REPORT_BYTES", before_digest_size), contextlib.redirect_stdout(output):
            code = payload.main(["--root", str(self.root), "--manifest", str(manifest)])
        self.assertEqual(code, 2)
        self.assertLessEqual(len(output.getvalue().rstrip().encode()), before_digest_size)


if __name__ == "__main__":
    unittest.main()
