# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Exercise the real CMake preset's refusal on an unsupported builder."""
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parents[2] / "products/qq"


@unittest.skipUnless(shutil.which("cmake"), "CMake unavailable")
class PiBuildTests(unittest.TestCase):
    def test_presets_are_accepted_by_cmake(self):
        result = subprocess.run(["cmake", "--list-presets=all"], cwd=SOURCE,
                                capture_output=True, text=True, timeout=30, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('"q-pi-release"', result.stdout)
        self.assertIn('"q-pi-asan"', result.stdout)

    @unittest.skipIf(platform.system() == "Linux" and platform.machine().lower() in ("aarch64", "arm64"),
                     "native ARM64 positive compilation requires the separately qualified kit and resource guard")
    def test_wrong_host_refused_before_compiler_or_qt_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "build"
            result = subprocess.run(["cmake", "--preset", "q-pi-release", "-B", str(build)],
                                    cwd=SOURCE, capture_output=True, text=True, timeout=30, check=False)
            self.assertNotEqual(result.returncode, 0)
            if platform.system() == "Linux":
                self.assertIn("native ARM64 Linux builder", result.stderr)
            else:
                self.assertIn("disabled preset", result.stderr)
            self.assertFalse(list(build.glob("**/CMakeCXXCompiler.cmake")))
            self.assertFalse(list(build.glob("**/compile_commands.json")))


if __name__ == "__main__":
    unittest.main()
