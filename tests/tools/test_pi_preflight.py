# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Hostile ELF layout and Qt metadata tests, discovered by the existing lint job."""

from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
import pi_preflight as pi


def elf(*, align=16384, offset=0, address=0, filesz=120, memsz=120, kind=3):
    data = bytearray(120)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHI", data, 16, kind, 183, 1)
    struct.pack_into("<Q", data, 32, 64)
    struct.pack_into("<HHH", data, 52, 64, 56, 1)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, offset, address, 0, filesz, memsz, align)
    return data


class ElfTests(unittest.TestCase):
    def test_shared_object(self):
        self.assertEqual(pi.inspect_elf(elf(), 120)["machine"], "AArch64")

    def test_executable(self):
        self.assertEqual(pi.inspect_elf(elf(kind=2), 120)["elf_type"], 2)

    def test_valid_zero_fill(self):
        self.assertEqual(pi.inspect_elf(elf(memsz=1000), 120)["load_alignments"], [16384])

    def test_file_size_is_not_prefix_size(self):
        self.assertEqual(pi.inspect_elf(elf(filesz=5000, memsz=5000), 6000)["elf_class"], 64)

    def test_4k_declared_alignment_is_not_qualified_for_16k(self):
        data = elf(align=4096)
        with self.assertRaises(ValueError):
            pi.inspect_elf(data, len(data))
        self.assertEqual(pi.inspect_elf(data, len(data), 4096)["selected_page_size"], 4096)

    def test_reject_header_mutations(self):
        # Each field is independent: core files, foreign machines, overlap,
        # extended tables and invalid versions must never become target kits.
        mutations = [(0, "<I", 0), (4, "<B", 1), (5, "<B", 2), (6, "<B", 0),
                     (16, "<H", 4), (18, "<H", 62), (20, "<I", 0),
                     (32, "<Q", 0), (32, "<Q", 1 << 63), (52, "<H", 63),
                     (54, "<H", 64), (54, "<H", 0), (56, "<H", 0), (56, "<H", 65535),
                     (64, "<I", 0)]
        for offset, fmt, value in mutations:
            with self.subTest(offset=offset, value=value):
                data = elf()
                struct.pack_into(fmt, data, offset, value)
                with self.assertRaises(ValueError):
                    pi.inspect_elf(data, len(data))

    def test_reject_truncation(self):
        for size in (0, 7, 63, 64, 119):
            with self.subTest(size=size), self.assertRaises(ValueError):
                pi.inspect_elf(elf()[:size], 120)

    def test_reject_out_of_file(self):
        with self.assertRaises(ValueError):
            pi.inspect_elf(elf(filesz=121, memsz=121), 120)

    def test_reject_offset_past_end(self):
        with self.assertRaises(ValueError):
            pi.inspect_elf(elf(offset=16384, address=16384), 120)

    def test_reject_filesz_greater_than_memsz(self):
        with self.assertRaises(ValueError):
            pi.inspect_elf(elf(memsz=119), 120)

    def test_reject_empty_load(self):
        with self.assertRaises(ValueError):
            pi.inspect_elf(elf(memsz=0, filesz=0), 120)

    def test_reject_overflow(self):
        with self.assertRaises(ValueError):
            pi.inspect_elf(elf(address=(1 << 64) - 16384, memsz=16385), 120)

    def test_reject_bad_alignment(self):
        for align in (0, 1, 4096, 24576):
            with self.subTest(align=align), self.assertRaises(ValueError):
                pi.inspect_elf(elf(align=align), 120)

    def test_reject_only_page_congruent(self):
        # Same 16K page offset, but not congruent at its declared 64K p_align.
        with self.assertRaises(ValueError):
            pi.inspect_elf(elf(align=65536, address=16384), 120)

    def test_reject_invalid_api_limits(self):
        for page in (0, -1, 1, 8192, True):
            with self.subTest(page=page), self.assertRaises(ValueError):
                pi.inspect_elf(elf(), 120, page)
        for size in (119, -1, True, 1 << 64):
            with self.subTest(size=size), self.assertRaises(ValueError):
                pi.inspect_elf(elf(), size)
        with self.assertRaises(ValueError):
            pi.inspect_elf(elf() + bytes(pi.MAX_READ), pi.MAX_READ + 120)

    def test_every_load_is_checked(self):
        data = elf(memsz=200, filesz=176)
        data.extend(elf(align=4096)[64:])
        struct.pack_into("<H", data, 56, 2)
        with self.assertRaises(ValueError):
            pi.inspect_elf(data, len(data))


class MetadataTests(unittest.TestCase):
    def check_spec(self, content):
        with tempfile.TemporaryDirectory() as directory:
            kit = Path(directory)
            target = kit / "lib/cmake/Qt6"
            target.mkdir(parents=True)
            (target / "qt.toolchain.cmake").write_text(content)
            return pi.qt_linux_target_spec(kit)

    def test_linux_target_spec_is_read_without_executing_the_toolchain(self):
        for spec in ("linux-g++", "linux-clang", "linux-aarch64-gnu-g++"):
            with self.subTest(spec=spec):
                self.assertEqual(self.check_spec('message(FATAL_ERROR "never run")\n'
                                                f'set(QT_QMAKE_TARGET_MKSPEC "{spec}" CACHE STRING "")\n'), spec)

    def test_arm64_does_not_make_a_mobile_or_windows_kit_a_linux_desktop_kit(self):
        for spec in ("android-clang", "win32-msvc", "ohos-clang", "linux-unknown"):
            with self.subTest(spec=spec), self.assertRaises(ValueError):
                self.check_spec(f'set(QT_QMAKE_TARGET_MKSPEC {spec} CACHE STRING "")\n')

    def test_target_spec_refuses_missing_commented_dynamic_or_ambiguous_assignments(self):
        literal = 'set(QT_QMAKE_TARGET_MKSPEC linux-g++ CACHE STRING "")\n'
        for content in ("", "# " + literal, literal * 2,
                        "#[[\n" + literal + "]]\n", "#[=[\n" + literal + "]=]\n",
                        literal + 'set(\nQT_QMAKE_TARGET_MKSPEC ${other})\n',
                        literal + 'set(QT_QMAKE_TARGET_MKSPEC ${other})\n',
                        'set(QT_QMAKE_TARGET_MKSPEC ${platform} CACHE STRING "")\n',
                        'set(QT_QMAKE_TARGET_MKSPEC "linux-g++ CACHE STRING "")\n'):
            with self.subTest(content=content), self.assertRaises(ValueError):
                self.check_spec(content)

    def test_target_spec_metadata_is_bounded(self):
        with self.assertRaises(ValueError):
            self.check_spec('set(QT_QMAKE_TARGET_MKSPEC linux-g++ CACHE STRING "")\n' +
                            "#" * pi.MAX_READ)

    def test_wrapper_impl(self):
        with tempfile.TemporaryDirectory() as directory:
            kit = Path(directory)
            cmake = kit / "lib/cmake/Qt6Core"
            cmake.mkdir(parents=True)
            (cmake / "Qt6CoreConfigVersion.cmake").write_text('include(arbitrary-code.cmake)\n')
            (cmake / "Qt6CoreConfigVersionImpl.cmake").write_text('set(PACKAGE_VERSION "6.12.0")\n')
            self.assertEqual(pi.qt_version(kit), "6.12.0")

    def test_no_version_execution_or_comment_match(self):
        with tempfile.TemporaryDirectory() as directory:
            kit = Path(directory)
            cmake = kit / "lib/cmake/Qt6Core"
            cmake.mkdir(parents=True)
            (cmake / "Qt6CoreConfigVersion.cmake").write_text('# set(PACKAGE_VERSION "6.12.0")\n')
            with self.assertRaises(ValueError):
                pi.qt_version(kit)

    def test_read_regular_files_only(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(ValueError):
            pi.read_prefix(Path(directory))


class MemoryProbeTests(unittest.TestCase):
    def test_native_linux_headroom_can_be_measured(self):
        with patch.object(pi.platform, "system", return_value="Linux"), \
                patch.object(pi.vm_resources, "linux_available_memory", return_value=4 * 1024**3) as native, \
                patch.object(pi.vm_resources, "available_memory", side_effect=AssertionError("Windows reader used")):
            passed, detail = pi.memory_check()
        self.assertTrue(passed)
        self.assertEqual(detail["minimum_bytes"], 3 * 1024**3)
        native.assert_called_once_with()

    def test_native_linux_cgroup_shortfall_remains_a_blocker(self):
        with patch.object(pi.platform, "system", return_value="Linux"), \
                patch.object(pi.vm_resources, "linux_available_memory", return_value=2 * 1024**3):
            self.assertFalse(pi.memory_check()[0])

    def test_windows_keeps_its_existing_host_guard(self):
        with patch.object(pi.platform, "system", return_value="Windows"), \
                patch.object(pi.vm_resources, "available_memory", return_value=4 * 1024**3), \
                patch.object(pi.vm_resources, "linux_available_memory") as native:
            self.assertTrue(pi.memory_check()[0])
        native.assert_not_called()

    def test_wsl_or_unmeasured_linux_does_not_claim_host_headroom(self):
        with patch.object(pi.platform, "system", return_value="Linux"), \
                patch.object(pi.vm_resources, "linux_available_memory", side_effect=ValueError):
            self.assertFalse(pi.memory_check()[0])


class ScratchProbeTests(unittest.TestCase):
    def test_unsized_scratch_remains_a_blocker(self):
        passed, detail = pi.scratch_check(None, None)
        self.assertFalse(passed)
        self.assertIsNone(detail["required_bytes"])

    def test_invalid_or_oversized_budget_never_measures_capacity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "budget.json"
            for content in (b'{"schema":1,"schema":1}', b"x" * (65536 + 1), b"[]"):
                path.write_bytes(content)
                with self.subTest(size=len(content)), patch.object(pi.vm_resources, "require_scratch_headroom") as capacity:
                    passed, _ = pi.scratch_check(Path(directory), path)
                    self.assertFalse(passed)
                    capacity.assert_not_called()

    def test_valid_budget_reaches_the_selected_capacity_check(self):
        import json
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "budget.json"
            budget = {"schema": 1, "components": {name: 100 for name in pi.vm_resources.SCRATCH_COMPONENTS}}
            path.write_text(json.dumps(budget))
            with patch.object(pi.vm_resources, "require_scratch_headroom", return_value={"required_bytes": 700}) as capacity:
                passed, detail = pi.scratch_check(Path(directory), path)
            self.assertTrue(passed)
            self.assertEqual(detail["required_bytes"], 700)
            capacity.assert_called_once_with(Path(directory), budget)


if __name__ == "__main__":
    unittest.main()
