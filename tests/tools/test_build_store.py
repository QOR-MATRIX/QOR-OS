# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Tests for tools/store/build_store.py: the demonstration catalogue, and that the first-party path has not moved.

None of these writes to tests/store/fixtures or tests/store/fixtures-demo, and none reads a signing key.
"""

from __future__ import annotations

import base64
import contextlib
import hashlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("build_store", REPO / "tools" / "store" / "build_store.py")
build_store = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules["build_store"] = build_store
SPEC.loader.exec_module(build_store)


def quietly(function, *arguments):
    with contextlib.redirect_stdout(io.StringIO()):
        return function(*arguments)


class Demo(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.temp.name) / "demo"
        quietly(build_store.demo, cls.out)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def stores(self):
        return [(name, json.loads((self.out / name / "catalog.json").read_bytes())) for name in ("v1", "v2")]

    @staticmethod
    def packaged(catalog):
        return [entry for entry in catalog["items"] if "package" in entry]

    def test_every_catalogue_is_format_2_signed_by_its_own_key_and_holds_no_price(self):
        point = base64.b64decode((self.out / "store.pub").read_bytes().strip())
        for name, catalog in self.stores():
            data = (self.out / name / "catalog.json").read_bytes()
            self.assertTrue(build_store.verify(point, data, (self.out / name / "catalog.json.sig").read_bytes()), name)
            self.assertEqual(catalog["format"], 2)
            self.assertNotIn(b'"price"', data)

    def test_sequences_rise_and_no_catalogue_has_an_audience(self):
        catalogs = dict(self.stores())
        self.assertEqual([catalogs[n]["sequence"] for n in ("v1", "v2")], [1, 2])
        for catalog in catalogs.values():
            self.assertNotIn("audience", catalog)

    def test_every_listing_is_placed_free_and_on_a_platform(self):
        for _, catalog in self.stores():
            for entry in catalog["items"]:
                self.assertTrue(entry["placements"], entry["id"])
                self.assertEqual(entry["entitlement"], {"type": "free"})
                self.assertTrue(entry["platforms"], entry["id"])
                self.assertIs(entry["executable"], False)

    def test_the_game_and_its_project_are_separate_listings_of_one_group(self):
        items = {entry["id"]: entry for entry in dict(self.stores())["v1"]["items"]}
        game = items["org.example.demo.game.ember-climb"]
        project = items["org.example.demo.project.ember-climb"]
        self.assertEqual(game["edition"]["group"], project["edition"]["group"])
        self.assertEqual((game["edition"]["role"], project["edition"]["role"]), ("game", "editable-project"))
        self.assertEqual(game["dependencies"], [])

    def test_each_package_is_stored_listed_exactly_and_mirrors_its_listing_in_the_manifest(self):
        for name, catalog in self.stores():
            for entry in self.packaged(catalog):
                package = (self.out / name / entry["package"]["file"]).read_bytes()
                self.assertEqual(len(package), entry["package"]["size"])
                self.assertEqual(hashlib.sha256(package).hexdigest(), entry["package"]["sha256"])
                with zipfile.ZipFile(io.BytesIO(package)) as archive:
                    infos = archive.infolist()
                    self.assertTrue(all(info.compress_type == zipfile.ZIP_STORED for info in infos))
                    self.assertEqual(sum(info.file_size for info in infos), entry["package"]["unpacked"])
                    manifest = json.loads(archive.read("manifest.json"))
                self.assertEqual(manifest["format"], 2)
                for field in ("id", "kind", "version", "licence", "dependencies"):
                    self.assertEqual(manifest[field], entry[field], field)
                self.assertEqual(manifest["stores"], sorted({p["store"] for p in entry["placements"]}))
                self.assertEqual(manifest.get("edition"), entry.get("edition"))

    def test_the_art_keeps_its_licence_and_notice(self):
        for name, catalog in self.stores():
            for entry in self.packaged(catalog):
                with zipfile.ZipFile(self.out / name / entry["package"]["file"]) as archive:
                    self.assertEqual(archive.read("LICENSE.txt"), build_store.APACHE_LICENCE.read_bytes())
                    self.assertIn(b"Copyright 2026 Demiurge", archive.read("NOTICE.txt"))

    def test_v2_is_a_real_update_and_the_pack_holds_nothing_that_runs(self):
        catalogs = dict(self.stores())
        pack = "org.example.demo.pack.ember-climb-art"
        v1 = next(entry for entry in catalogs["v1"]["items"] if entry["id"] == pack)
        v2 = next(entry for entry in catalogs["v2"]["items"] if entry["id"] == pack)
        self.assertEqual((v1["version"], v2["version"]), ("1.0.0", "1.1.0"))
        with zipfile.ZipFile(self.out / "v2" / v2["package"]["file"]) as archive:
            names = archive.namelist()
        self.assertIn("art/torch.png", names)
        self.assertFalse([n for n in names if n.endswith((".qml", ".js", ".mjs", ".py", ".tmj", ".tsj"))])

    def test_the_game_does_not_carry_the_script_that_made_its_art(self):
        entry = next(entry for entry in dict(self.stores())["v1"]["items"] if entry["kind"] == "game")
        with zipfile.ZipFile(self.out / "v1" / entry["package"]["file"]) as archive:
            names = archive.namelist()
        self.assertIn("logic/rules.qml", names)
        self.assertNotIn("make_art.py", names)

    def test_no_private_key_is_written_and_each_run_makes_a_new_one(self):
        self.assertFalse([p for p in self.out.rglob("*") if p.suffix in (".pem", ".key", ".p12", ".pfx", ".pk8")])
        with tempfile.TemporaryDirectory() as temp:
            second = Path(temp) / "demo"
            quietly(build_store.demo, second)
            self.assertNotEqual((second / "store.pub").read_bytes(), (self.out / "store.pub").read_bytes())

    def test_a_pack_cannot_be_built_with_code_in_it(self):
        listing = {"id": "org.example.demo.pack.bad", "licence": {"spdx": "MIT", "file": "LICENSE.txt"}}
        for path in ("logic/rules.qml", "plugin.dll", "run.sh", "mystery.xyz"):
            with self.assertRaises(SystemExit, msg=path):
                build_store.Format2Item("asset-pack", listing, {"LICENSE.txt": b"x", path: b"x"})


class FirstParty(unittest.TestCase):
    def test_the_first_party_catalogue_still_checks_out_so_nothing_was_re_signed(self):
        self.assertEqual(quietly(build_store.check, build_store.CONTENT, build_store.CATALOG, build_store.STORE_PUB), 0)

    def test_a_theme_keeps_its_format_1_manifest(self):
        listing = {"id": "a.b.c", "version": "1.0.0", "name": "n", "developer": "d"}
        manifest = json.loads(build_store.Item("theme", listing, {"theme.json": b"{}"}).manifest())
        self.assertEqual(set(manifest), {"format", "id", "kind", "version", "name", "developer", "files"})
        self.assertEqual(manifest["format"], 1)

    def test_a_listing_with_a_price_is_not_built(self):
        listing = {"id": "dev.qorsync.theme.x", "name": "X", "version": "1.0.0", "developer": "d", "summary": "s",
                   "description": "d", "released": "2026-11-01", "price": 1}
        with self.assertRaises(SystemExit):
            build_store.validate(build_store.Item("theme", listing, {"theme.json": b"{}"}), {"d": {}})
        listing["price"] = 0
        build_store.validate(build_store.Item("theme", listing, {"theme.json": b"{}"}), {"d": {}})


if __name__ == "__main__":
    unittest.main()
