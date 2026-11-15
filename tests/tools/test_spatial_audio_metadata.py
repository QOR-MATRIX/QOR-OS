# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Check the narrowly scoped Qt metadata correction without an installed Qt kit."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

HELPER = Path(__file__).resolve().parents[2] / "cmake/QorSpatialAudioMetadata.cmake"


@unittest.skipUnless(shutil.which("cmake"), "CMake is required")
class SpatialAudioMetadata(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "sdk/qml/QtQuick3D/SpatialAudio"
        self.source.mkdir(parents=True)
        self.module = "module QtQuick3D.SpatialAudio\ntypeinfo plugins.qmltypes\n"
        self.types = 'Module { Component { prototype: "QQuick3DNode" } }\n'
        (self.source / "qmldir").write_text(self.module, encoding="utf-8")
        (self.source / "plugins.qmltypes").write_text(self.types, encoding="utf-8")

    def configure(self, version="6.12.0"):
        script = self.root / "check.cmake"
        script.write_text(
            f'set(QT6_INSTALL_PREFIX "{(self.root / "sdk").as_posix()}")\n'
            'set(QT6_INSTALL_QML "qml")\n'
            f'set(Qt6_VERSION "{version}")\n'
            f'set(CMAKE_CURRENT_BINARY_DIR "{(self.root / "build").as_posix()}")\n'
            f'include("{HELPER.as_posix()}")\n'
            'qor_spatial_audio_metadata_import_path(path)\n'
            f'file(WRITE "{(self.root / "path.txt").as_posix()}" "${{path}}")\n',
            encoding="utf-8",
        )
        return subprocess.run(
            ["cmake", "-P", str(script)], capture_output=True, text=True, timeout=20
        )

    def test_known_omission_changes_only_the_copy_and_is_repeatable(self):
        before = {p.name: p.read_bytes() for p in self.source.iterdir()}
        self.assertEqual(self.configure().returncode, 0)
        destination = Path((self.root / "path.txt").read_text()) / "QtQuick3D/SpatialAudio"
        self.assertEqual((destination / "qmldir").read_text(), self.module + "\ndepends QtQuick3D\n")
        self.assertEqual((destination / "plugins.qmltypes").read_bytes(), before["plugins.qmltypes"])
        timestamps = {p.name: p.stat().st_mtime_ns for p in destination.iterdir()}
        self.assertEqual(self.configure().returncode, 0)
        self.assertEqual({p.name: p.stat().st_mtime_ns for p in destination.iterdir()}, timestamps)
        self.assertEqual({p.name: p.read_bytes() for p in self.source.iterdir()}, before)

    def test_fixed_sdk_uses_the_original_module_without_an_override(self):
        (self.source / "qmldir").write_text(self.module + "depends QtQuick3D 6.13\n")
        self.assertEqual(self.configure("6.13.0").returncode, 0)
        self.assertEqual((self.root / "path.txt").read_text(), "")
        self.assertFalse((self.root / "build/qor-qml-tooling").exists())

    def test_unreviewed_version_with_missing_dependency_is_refused(self):
        self.assertNotEqual(self.configure("6.13.0").returncode, 0)
        self.assertFalse((self.root / "build/qor-qml-tooling").exists())

    def test_unexpected_module_is_refused(self):
        (self.source / "qmldir").write_text("module Unexpected\n")
        self.assertNotEqual(self.configure().returncode, 0)

    def test_unexpected_base_type_is_refused(self):
        (self.source / "plugins.qmltypes").write_text('Module { Component { prototype: "QObject" } }')
        self.assertNotEqual(self.configure().returncode, 0)

    def test_incomplete_sdk_is_refused(self):
        (self.source / "plugins.qmltypes").unlink()
        self.assertNotEqual(self.configure().returncode, 0)


if __name__ == "__main__":
    unittest.main()
