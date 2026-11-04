# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Exercise the bundle packager with inert files, fake dependency output and no real Qt kit or package database."""

from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOL = REPO / "tools" / "dev" / "prism-gate-bundle.sh"


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("bash"), "needs Bash on Linux; the kit is a fixture")
class PrismGateBundle(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.qt = self.root / "fixture-qt"
        self.commands = self.root / "commands"
        self.commands.mkdir()
        self.scratch = self.root / "scratch"
        self.scratch.mkdir()
        self.out = self.root / "bundle.tar.gz"
        self.manifest = self.root / "fixture.manifest"
        self.manifest.write_text("fixture-base 1.0\n")
        self.script = self.root / "prism-gate-bundle.sh"
        self.script.write_bytes(TOOL.read_bytes())
        (self.root / "prism-gate.sh").write_text("#!/usr/bin/env bash\n# inert launcher fixture\n")
        self.prism = self.file(self.build / "prism/compositor/prism", b"inert Prism fixture\n", executable=True)
        self.file(self.build / "tests/prism-test-client", b"inert client fixture\n", executable=True)
        self.file(self.qt / "lib/libQt6Core.so.6", b"inert Qt library fixture\n")
        for name in ("qeglfs", "qwayland", "qminimal", "qoffscreen"):
            self.file(self.qt / f"plugins/platforms/lib{name}.so", b"inert plugin\n")
        for name in (
            "egldeviceintegrations", "wayland-graphics-integration-server", "wayland-graphics-integration-client",
            "wayland-shell-integration", "wayland-decoration-client", "imageformats", "platforminputcontexts",
        ):
            self.file(self.qt / f"plugins/{name}/fixture.so", b"inert plugin\n")
        for module in ("QtCore", "QtQml", "QtQuick", "QtWayland", "QtGraphs", "QtMultimedia", "QtQuick3D"):
            self.file(self.qt / f"qml/{module}/qmldir", b"# inert module\n")
        self.program(
            "find",
            "import os,pathlib,sys\n"
            "root=pathlib.Path(sys.argv[1]).resolve()\n"
            "assert root.is_relative_to(pathlib.Path(os.environ['GATE_TEST_ROOT']).resolve())\n"
            "if os.environ['GATE_TEST_FAIL']=='find':\n"
            " print(root/'prism/compositor/prism')\n"
            " raise SystemExit(11)\n"
            f"os.execv({shutil.which('find')!r},['find',*sys.argv[1:]])\n",
        )
        self.program(
            "ldd",
            "import os,sys\n"
            "if os.environ['GATE_TEST_PARTIAL']=='1':\n"
            " print('libQt6Core.so.6 => '+os.environ['GATE_TEST_QT']+'/lib/libQt6Core.so.6 (0x0)')\n"
            "if os.environ['GATE_TEST_FAIL']=='ldd':\n"
            " print('fixture ldd failure',file=sys.stderr)\n"
            " raise SystemExit(7)\n"
            "if os.environ['GATE_TEST_FAIL']=='missing':\n"
            " print('libFixtureMissing.so => not found')\n",
        )
        for name, status in (("awk", 8), ("sort", 9)):
            self.program(
                name,
                "import os,sys\n"
                f"if os.environ['GATE_TEST_FAIL']=={name!r}:\n"
                " print('libQt6Core.so.6 '+os.environ['GATE_TEST_QT']+'/lib/libQt6Core.so.6')\n"
                f" raise SystemExit({status})\n"
                f"os.execv({shutil.which(name)!r},[{name!r},*sys.argv[1:]])\n",
            )
        self.program("git", "print('fixture-checkout')\n")
        self.program("sed", "print('Fixture Linux')\n")
        self.program("dpkg", "raise SystemExit('No package database is reachable by these tests')\n")

    def file(self, path, content, executable=False):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        if executable:
            path.chmod(0o700)
        return path

    def program(self, name, body):
        self.file(self.commands / name, (f"#!{sys.executable}\n" + body).encode(), executable=True)

    def run_bundle(self, fail="", partial=True):
        return subprocess.run(
            ["bash", str(self.script), "--build-dir", str(self.build), "--qt", str(self.qt), "--manifest", str(self.manifest), "--out", str(self.out)],
            capture_output=True, text=True, timeout=20,
            env={**os.environ, "PATH": f"{self.commands}:{os.environ['PATH']}", "TMPDIR": str(self.scratch),
                 "GATE_TEST_ROOT": str(self.root), "GATE_TEST_QT": str(self.qt), "GATE_TEST_FAIL": fail,
                 "GATE_TEST_PARTIAL": "1" if partial else "0"},
        )

    def test_a_complete_inventory_packs_the_fixture(self):
        result = self.run_bundle()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        with tarfile.open(self.out) as archive:
            self.assertEqual(archive.extractfile("qor-gate/prism/compositor/prism").read(), self.prism.read_bytes())
            self.assertEqual(archive.extractfile("qor-gate/qt/lib/libQt6Core.so.6").read(), b"inert Qt library fixture\n")

    def test_a_failed_file_inventory_cannot_pass_with_partial_output(self):
        result = self.run_bundle(fail="find")
        self.assertEqual(result.returncode, 11, result.stdout + result.stderr)
        self.assertFalse(self.out.exists())

    def test_a_failed_dependency_inventory_preserves_an_old_archive(self):
        for partial in (False, True):
            with self.subTest(partial=partial):
                self.out.write_bytes(b"previous evidence")
                result = self.run_bundle(fail="ldd", partial=partial)
                self.assertEqual(result.returncode, 7, result.stdout + result.stderr)
                self.assertEqual(self.out.read_bytes(), b"previous evidence")

    def test_failed_dependency_filters_cannot_pass_with_partial_output(self):
        for name, status in (("awk", 8), ("sort", 9)):
            with self.subTest(command=name):
                self.out = self.root / f"{name}.tar.gz"
                result = self.run_bundle(fail=name)
                self.assertEqual(result.returncode, status, result.stdout + result.stderr)
                self.assertFalse(self.out.exists())

    def test_missing_dependencies_still_fail(self):
        result = self.run_bundle(fail="missing")
        self.assertEqual(result.returncode, 4, result.stdout + result.stderr)
        self.assertFalse(self.out.exists())

    def test_checkout_metadata_does_not_claim_the_executables_build_revision(self):
        result = self.run_bundle()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        with tarfile.open(self.out) as archive:
            metadata = archive.extractfile("qor-gate/BUNDLE.txt").read().decode()
        self.assertIn("Launcher checkout HEAD: fixture-checkout", metadata)
        self.assertIn("Executable build revision: not established by this packager", metadata)
        self.assertIn(hashlib.sha256(self.prism.read_bytes()).hexdigest(), metadata)


if __name__ == "__main__":
    unittest.main()
