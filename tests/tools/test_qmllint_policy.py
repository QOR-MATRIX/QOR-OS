# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""The repository's warning limit, exercised by the real Qt linter without running QML or opening a window."""

import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
KIT = pathlib.Path(os.environ.get("QOR_QT_DIR", "C:/Qt/6.12.0/msvc2022_64" if os.name == "nt" else str(pathlib.Path.home() / "Qt/6.12.0/gcc_64")))
LINTER = KIT / "bin" / ("qmllint.exe" if os.name == "nt" else "qmllint")


@unittest.skipUnless(LINTER.is_file(), "needs the existing commercial Qt 6.12 kit")
class QmlLintPolicy(unittest.TestCase):
    def lint(self, source, strict=True):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            if strict:
                shutil.copy2(REPO / ".qmllint.ini", root / ".qmllint.ini")
            else:
                (root / ".qmllint.ini").write_text("[General]\nMaxWarnings=-1\n", encoding="utf-8")
            nested = root / "nested"
            nested.mkdir()
            file = nested / "probe.qml"
            file.write_text(source, encoding="utf-8")
            done = subprocess.run([str(LINTER), "--bare", "-I", str(KIT / "qml"), str(file)],
                                  cwd=nested, capture_output=True, text=True, timeout=30)
            return done.returncode, done.stdout + done.stderr

    def test_a_warning_changes_native_success_to_failure(self):
        source = "import QtQuick\nItem { width: missingWidth }\n"
        code, said = self.lint(source, strict=False)
        self.assertEqual(code, 0, said)
        self.assertIn("Warning:", said)
        code, said = self.lint(source)
        self.assertNotEqual(code, 0, said)
        self.assertIn("Warning:", said)

    def test_clean_qml_still_passes(self):
        code, said = self.lint("import QtQuick\nItem {}\n")
        self.assertEqual(code, 0, said)
        self.assertNotIn("Warning:", said)

    def test_syntax_failure_stays_a_failure(self):
        for strict in (False, True):
            with self.subTest(strict=strict):
                code, said = self.lint("import QtQuick\nItem {", strict=strict)
                self.assertNotEqual(code, 0, said)


if __name__ == "__main__":
    unittest.main()
