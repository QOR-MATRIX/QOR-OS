# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Exercise the real CMake preset's refusal on an unsupported builder."""
from pathlib import Path
import os
import platform
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parents[2] / "products/qq"
NATIVE_GATE = SOURCE.parents[1] / "cmake/QorPiNative.cmake"
SMOKE_SOURCE = SOURCE.parents[1] / "tests/pi"


@unittest.skipUnless(shutil.which("cmake"), "CMake unavailable")
class PiBuildTests(unittest.TestCase):
    def test_smoke_presets_are_accepted_by_cmake(self):
        result = subprocess.run(["cmake", "--list-presets=all"], cwd=SMOKE_SOURCE,
                                capture_output=True, text=True, timeout=30, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('"q-pi-smoke"', result.stdout)
        self.assertIn('"q-pi-smoke-asan"', result.stdout)

    @unittest.skipIf(platform.system() == "Linux" and platform.machine().lower() in ("aarch64", "arm64"),
                     "native positive compilation requires the separately qualified kit and resource guard")
    def test_smoke_wrong_host_refused_even_without_preset(self):
        for preset in ("q-pi-smoke", "q-pi-smoke-asan", None):
            with self.subTest(preset=preset), tempfile.TemporaryDirectory() as directory:
                build = Path(directory) / "build"
                command = ["cmake", "-B", str(build)]
                command += ["--preset", preset] if preset else ["-S", str(SMOKE_SOURCE), "-G", "Ninja"]
                result = subprocess.run(command, cwd=SMOKE_SOURCE, capture_output=True,
                                        text=True, timeout=30, check=False)
                self.assertNotEqual(result.returncode, 0)
                expected = ("disabled preset" if preset and platform.system() != "Linux"
                            else "native ARM64 Linux builder")
                self.assertIn(expected, result.stderr)
                self.assertFalse(list(build.glob("**/CMakeCXXCompiler.cmake")))
                self.assertFalse(list(build.glob("**/compile_commands.json")))

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

    def qt_gate_fixture(self, directory, *, version="6.12.0", spec="linux-g++", machine=183,
                        kind=3, alignment=16384, python_available=True, package_hint=None,
                        hint_source="cache", foreign_hint=True, missing_config=False,
                        root_version="6.12.0"):
        """Run the real include with inert host/ELF fixtures, never a compiler.

        The file command intercept supplies a fictional Linux kernel only. It
        does not make this host native or establish an executable target kit.
        """
        root = Path(directory)
        kit = root / "kit"
        core_metadata = kit / "lib/cmake/Qt6Core"
        toolchain = kit / "lib/cmake/Qt6"
        core_metadata.mkdir(parents=True)
        toolchain.mkdir(parents=True)
        (core_metadata / "Qt6CoreConfigVersion.cmake").write_text(f'set(PACKAGE_VERSION "{version}")\n')
        marker = root / "qt-executed"
        inert_code = f'file(WRITE "{marker.as_posix()}" "Qt executed")\nmessage(FATAL_ERROR "Qt executed")\n'
        (toolchain / "qt.toolchain.cmake").write_text(
            inert_code + f'set(QT_QMAKE_TARGET_MKSPEC {spec} CACHE STRING "")\n')
        (toolchain / "Qt6Config.cmake").write_text(inert_code)
        if root_version is not None:
            (toolchain / "Qt6ConfigVersion.cmake").write_text(f'set(PACKAGE_VERSION "{root_version}")\n')
        if missing_config:
            (toolchain / "Qt6Config.cmake").unlink()
        hint_code = ""
        discovery = ""
        hint_path = None
        if package_hint:
            package = package_hint.removesuffix("_DIR").removesuffix("_ROOT")
            hint_root = root / "foreign" if foreign_hint else kit
            config = hint_root / "lib/cmake" / package
            config.mkdir(parents=True, exist_ok=True)
            (config / f"{package}Config.cmake").write_text(inert_code)
            hint_path = config if package_hint.endswith("_DIR") else hint_root
            if hint_source in ("cache", "normal"):
                suffix = ' CACHE PATH "fixture"' if hint_source == "cache" else ""
                hint_code = f'set({package_hint} "{hint_path.as_posix()}"{suffix})\n'
            if foreign_hint:
                # If the early gate misses the conflicting hint, this would load
                # an inert config with an execution marker. No real kit is used.
                discovery = f'find_package({package} CONFIG REQUIRED)\n'
        data = bytearray(120)
        data[:7] = b"\x7fELF\x02\x01\x01"
        struct.pack_into("<HHI", data, 16, kind, machine, 1)
        struct.pack_into("<Q", data, 32, 64)
        struct.pack_into("<HHH", data, 52, 64, 56, 1)
        struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, 120, 120, alignment)
        (kit / "lib/libQt6Core.so.6").write_bytes(data)
        script = root / "probe.cmake"
        script.write_text('''cmake_minimum_required(VERSION 3.27)
set(CMAKE_HOST_SYSTEM_NAME Linux)
set(CMAKE_HOST_SYSTEM_PROCESSOR aarch64)
set(CMAKE_CROSSCOMPILING FALSE)
set(QOR_HARDENING ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(QQ_PLAYER_ONLY OFF)
macro(file)
    if("${ARGV0}" STREQUAL "READ" AND "${ARGV1}" STREQUAL "/proc/sys/kernel/osrelease")
        set(${ARGV2} "synthetic-native-kernel")
    else()
        _file(${ARGV})
    endif()
endmacro()
''' + hint_code + f'include("{NATIVE_GATE.as_posix()}")\n' + discovery + '''
if(NOT CMAKE_EXE_LINKER_FLAGS_INIT MATCHES "max-page-size=16384"
   OR NOT CMAKE_EXE_LINKER_FLAGS_INIT MATCHES "common-page-size=16384")
    message(FATAL_ERROR "Page size flags missing")
endif()
message(STATUS "Synthetic Qt rejection gate passed")
''')
        env = dict(os.environ, QOR_QT_DIR=kit.as_posix())
        if package_hint and hint_source == "environment":
            env[package_hint] = hint_path.as_posix()
        # Find only this known host interpreter, avoiding Windows Store aliases.
        env["PATH"] = str(Path(sys.executable).parent) if python_available else ""
        result = subprocess.run([shutil.which("cmake"), "-P", str(script)], env=env,
                                capture_output=True, text=True, timeout=30, check=False)
        self.assertFalse(marker.exists(), "Qt CMake code was executed by the read-only gate")
        self.assertFalse(list(root.glob("**/CMakeCXXCompiler.cmake")))
        return result

    def test_native_qt_gate_reads_fixture_without_executing_qt(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.qt_gate_fixture(directory)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Synthetic Qt rejection gate passed", result.stdout)

    def test_native_qt_gate_refuses_unsuitable_kit_before_package_discovery(self):
        for changes in ({"version": "6.13.0"}, {"spec": "android-clang"}, {"machine": 62},
                        {"alignment": 4096}, {"kind": 2}):
            with self.subTest(changes=changes), tempfile.TemporaryDirectory() as directory:
                result = self.qt_gate_fixture(directory, **changes)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Q Pi Qt metadata/Core layout check failed", result.stderr)

    def test_native_qt_gate_requires_an_existing_host_python(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.qt_gate_fixture(directory, python_available=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("existing host Python interpreter", result.stderr)

    def test_conflicting_package_hints_are_refused_before_config_execution(self):
        for hint in ("Qt6_DIR", "Qt6Core_DIR", "Qt6_ROOT", "Qt6Core_ROOT"):
            for source in ("cache", "normal", "environment"):
                with self.subTest(hint=hint, source=source), tempfile.TemporaryDirectory() as directory:
                    result = self.qt_gate_fixture(directory, package_hint=hint, hint_source=source)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("conflicting Qt package paths", result.stderr)

    def test_existing_matching_package_hints_are_preserved(self):
        for hint in ("Qt6_DIR", "Qt6Core_DIR", "Qt6_ROOT", "Qt6Core_ROOT"):
            with self.subTest(hint=hint), tempfile.TemporaryDirectory() as directory:
                result = self.qt_gate_fixture(directory, package_hint=hint, foreign_hint=False)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_missing_root_config_is_refused_before_cmake_can_fall_back(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.qt_gate_fixture(directory, missing_config=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Q Pi Qt metadata/Core layout check failed", result.stderr)

    def test_missing_or_inconsistent_root_version_is_refused_before_discovery(self):
        for version in (None, "6.11.0", "6.12.1"):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as directory:
                result = self.qt_gate_fixture(directory, root_version=version)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Q Pi Qt metadata/Core layout check failed", result.stderr)


if __name__ == "__main__":
    unittest.main()
