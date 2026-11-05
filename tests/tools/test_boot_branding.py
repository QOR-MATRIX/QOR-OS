# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Real asset/payload checks with temporary, synthetic checkout copies."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/branding"))
import assets
import stage_image


class Branding(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name).resolve() / "repo"
        for name in (*assets.INPUTS, "kit/branding/manifest.json", "kit/branding/qor-mark.png",
                     "kit/branding/qor-mark.png.license", "image/branding/qor-os.script.in",
                     "image/branding/qor-os.plymouth", "image/branding/plymouthd.conf"):
            path = self.repo / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO / name, path)
        self.manifest = self.repo / "kit/branding/manifest.json"
        self.output = self.repo / "build/payload"

    def change(self, mutate):
        data = json.loads(self.manifest.read_text())
        mutate(data)
        self.manifest.write_text(json.dumps(data))

    def assert_refused(self):
        with self.assertRaises((ValueError, OSError)):
            assets.validate(self.repo)

    def test_approved_source_and_complete_payload(self):
        self.assertEqual(assets.validate(self.repo)["source"], "kit/QorMark.qml")
        self.assertEqual(stage_image.stage(self.repo, self.output), 7)
        self.assertEqual(stage_image.verify(self.repo, self.output), 7)
        self.assertEqual(stage_image.stage(self.repo, self.output), 7)
        self.assertIn(b"add_dracutmodules+=\" plymouth \"", (self.output / stage_image.DRACUT).read_bytes())
        self.assertEqual((self.output / (stage_image.THEME + "qor-mark.png")).read_bytes(),
                         (self.repo / "kit/branding/qor-mark.png").read_bytes())

    def test_stale_logo_source_refused(self):
        with (self.repo / "kit/QorMark.qml").open("a") as stream:
            stream.write("\n// source changed\n")
        self.assert_refused()

    def test_stale_render_recipe_refused(self):
        with (self.repo / "tools/branding/Mark.qml").open("a") as stream:
            stream.write("\n// render recipe changed\n")
        self.assert_refused()

    def test_duplicate_and_oversized_manifest_refused(self):
        original = self.manifest.read_text()
        self.manifest.write_text('{"schema":1,' + original[1:])
        self.assert_refused()
        self.manifest.write_text(original + " " * 8192)
        self.assert_refused()

    def test_hostile_manifest_fields_refused(self):
        original = self.manifest.read_bytes()
        changes = [lambda d: d.update(schema=True), lambda d: d.update(source="../outside.qml"),
                   lambda d: d.update(inputs={}), lambda d: d.update(background="#ffffff"),
                   lambda d: d["logo"].update(file="../../outside.png"),
                   lambda d: d["logo"].update(width=True)]
        for change in changes:
            with self.subTest(change=changes.index(change)):
                self.manifest.write_bytes(original)
                self.change(change)
                self.assert_refused()

    def test_hash_mismatch_refused(self):
        (self.repo / "kit/branding/qor-mark.png").write_bytes(b"bad")
        self.assert_refused()

    def test_png_corruption_refused_even_with_matching_hash(self):
        path = self.repo / "kit/branding/qor-mark.png"
        original = path.read_bytes()
        for data in (original[:-12], original + b"extra", original[:40] + b"bad" + original[43:]):
            with self.subTest(size=len(data)):
                path.write_bytes(data)
                self.change(lambda d: d["logo"].update(sha256=hashlib.sha256(data).hexdigest()))
                self.assert_refused()

    def test_compressed_pixel_bomb_refused(self):
        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
        data = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1024, 1024, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(b"\0" * (1024 * 4097 + 1000))) + chunk(b"IEND", b""))
        (self.repo / "kit/branding/qor-mark.png").write_bytes(data)
        self.change(lambda d: d["logo"].update(sha256=hashlib.sha256(data).hexdigest()))
        self.assert_refused()

    def test_missing_asset_refused_before_creating_output(self):
        (self.repo / "kit/branding/qor-mark.png").unlink()
        with self.assertRaises(ValueError):
            stage_image.stage(self.repo, self.output)
        self.assertFalse(self.output.exists())

    def test_host_root_parent_and_traversal_refused(self):
        for target in (Path(self.temp.name), self.repo / "build", self.repo / "build/../outside"):
            with self.subTest(target=target):
                with self.assertRaises(ValueError):
                    stage_image.stage(self.repo, target)

    def test_modified_payload_preserved_and_rejected(self):
        stage_image.stage(self.repo, self.output)
        target = self.output / stage_image.CONFIG
        target.write_bytes(b"preserve this unfinished work")
        with self.assertRaises(ValueError):
            stage_image.stage(self.repo, self.output)
        self.assertEqual(target.read_bytes(), b"preserve this unfinished work")

    def test_missing_payload_file_refused(self):
        stage_image.stage(self.repo, self.output)
        (self.output / (stage_image.THEME + "qor-os.script")).unlink()
        with self.assertRaises(ValueError):
            stage_image.verify(self.repo, self.output)

    def test_extra_payload_file_refused(self):
        stage_image.stage(self.repo, self.output)
        (self.output / "unreviewed").write_bytes(b"unintended content")
        with self.assertRaises(ValueError):
            stage_image.verify(self.repo, self.output)

    def test_linked_theme_source_refused(self):
        target = self.repo / "image/branding/qor-os.script.in"
        saved = target.read_bytes()
        target.unlink()
        other = self.repo / "other-script"
        other.write_bytes(saved)
        try:
            target.symlink_to(other)
        except OSError:
            self.skipTest("Creating symbolic links requires a host privilege")
        with self.assertRaises(ValueError):
            stage_image.stage(self.repo, self.output)
        self.assertFalse(self.output.exists())

    def test_linked_staging_root_refused(self):
        self.output.parent.mkdir()
        outside = self.repo / "outside"
        outside.mkdir()
        try:
            self.output.symlink_to(outside, target_is_directory=True)
        except OSError:
            self.skipTest("Creating symbolic links requires a host privilege")
        with self.assertRaises(ValueError):
            stage_image.stage(self.repo, self.output)
        self.assertEqual(list(outside.iterdir()), [])

    def test_cli_failure_propagates(self):
        result = subprocess.run([sys.executable, str(REPO / "tools/branding/stage_image.py"),
                                 "--output", str(REPO / "image")], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"refused", result.stderr)


if __name__ == "__main__":
    unittest.main()
