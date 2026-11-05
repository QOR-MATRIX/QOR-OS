# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Hostile saved-image fixtures are read in memory, never extracted."""
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/image"))
import verify_archive


class InstallArchive(unittest.TestCase):
    def archive(self, entries):
        def add(tar, name, data, kind=None):
            member = tarfile.TarInfo(name)
            member.size = len(data)
            if kind is not None:
                member.type = kind
                member.linkname = "outside"
            tar.addfile(member, io.BytesIO(data))
        layer = io.BytesIO()
        with tarfile.open(fileobj=layer, mode="w") as tar:
            for name, data, kind in entries:
                add(tar, name, data, kind)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name) / "image.tar"
        with tarfile.open(path, "w") as tar:
            add(tar, "manifest.json", json.dumps([{"Layers": ["layer.tar"]}]).encode())
            add(tar, "layer.tar", layer.getvalue())
        return path

    def test_exact_saved_payload_passes(self):
        path = self.archive([("usr/share/mark", b"original", None)])
        self.assertEqual(verify_archive.verify(path, {"usr/share/mark": b"original"}), 1)

    def test_missing_or_modified_file_fails(self):
        for rows in ([], [("usr/share/mark", b"modified", None)]):
            with self.assertRaises(ValueError):
                verify_archive.verify(self.archive(rows), {"usr/share/mark": b"original"})

    def test_traversal_links_and_duplicates_fail(self):
        cases = [[("../../outside", b"original", None)], [("/outside", b"original", None)],
                 [("usr/share/mark", b"", tarfile.SYMTYPE)],
                 [("usr/share/mark", b"original", None), ("usr/share/mark", b"", tarfile.DIRTYPE)],
                 [("usr/share/mark", b"original", None), ("usr/share/mark", b"original", None)]]
        for rows in cases:
            with self.assertRaises(ValueError):
                verify_archive.verify(self.archive(rows), {"usr/share/mark": b"original"})


if __name__ == "__main__":
    unittest.main()
