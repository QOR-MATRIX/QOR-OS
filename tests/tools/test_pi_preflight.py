# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Hostile ELF layout and Qt metadata tests, discovered by the existing lint job."""

from pathlib import Path
from contextlib import redirect_stdout, redirect_stderr
import io
import json
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
    def check_version(self, content, impl=None):
        with tempfile.TemporaryDirectory() as directory:
            kit = Path(directory)
            target = kit / "lib/cmake/Qt6Core"
            target.mkdir(parents=True)
            (target / "Qt6CoreConfigVersion.cmake").write_text(content)
            if impl is not None:
                (target / "Qt6CoreConfigVersionImpl.cmake").write_text(impl)
            return pi.qt_version(kit)

    def test_generated_version_pointer_width_decoration(self):
        self.assertEqual(self.check_version('set(PACKAGE_VERSION "6.12.0")\n'
                                           'set(PACKAGE_VERSION "${PACKAGE_VERSION} (${installedBits}bit)")\n'),
                         "6.12.0")

    def test_version_refuses_ambiguous_dynamic_commented_or_malformed_metadata(self):
        literal = 'set(PACKAGE_VERSION "6.12.0")\n'
        for content in (literal * 2, literal + 'set(PACKAGE_VERSION "6.13.0")\n',
                        '#[[\n' + literal + ']]\n', '#[=[\n' + literal + ']=]\n',
                        literal + 'set(\nPACKAGE_VERSION "6.13.0")\n',
                        literal + 'set(PACKAGE_VERSION "${other}")\n',
                        'set(PACKAGE_VERSION "6.12.0)\n',
                        "set(PACKAGE_VERSION '6.12.0')\n",
                        literal + '#' * pi.MAX_READ):
            with self.subTest(content=content[:120]), self.assertRaises(ValueError):
                self.check_version(content)
        with self.assertRaises(ValueError):
            self.check_version(literal, literal)

    def check_spec(self, content):
        with tempfile.TemporaryDirectory() as directory:
            kit = Path(directory)
            target = kit / "lib/cmake/Qt6"
            target.mkdir(parents=True)
            (target / "Qt6Config.cmake").write_text('message(FATAL_ERROR "never run")\n')
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


class QtOnlyTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.kit = Path(self.directory.name)
        self.version = self.kit / "lib/cmake/Qt6Core/Qt6CoreConfigVersion.cmake"
        self.spec = self.kit / "lib/cmake/Qt6/qt.toolchain.cmake"
        self.core = self.kit / "lib/libQt6Core.so.6"
        self.version.parent.mkdir(parents=True)
        self.spec.parent.mkdir(parents=True)
        self.config = self.spec.parent / "Qt6Config.cmake"
        self.config.write_text('message(FATAL_ERROR "must not execute Qt config")\n')
        self.root_version = self.spec.parent / "Qt6ConfigVersion.cmake"
        self.root_version.write_text('set(PACKAGE_VERSION "6.12.0")\n')
        self.version.write_text('set(PACKAGE_VERSION "6.12.0")\n')
        self.spec.write_text('message(FATAL_ERROR "must not execute Qt toolchain")\n'
                             'set(QT_QMAKE_TARGET_MKSPEC linux-g++ CACHE STRING "")\n')
        self.core.write_bytes(elf())

    def run_cli(self):
        output = io.StringIO()
        with redirect_stdout(output), patch.object(pi, "probe", side_effect=AssertionError("full probe ran")), \
                patch.object(pi.subprocess, "run", side_effect=AssertionError("subprocess executed")):
            status = pi.main(["--qt-only", "--qt-target", str(self.kit)])
        return status, json.loads(output.getvalue())

    def test_fixture_pass_is_explicitly_scoped_and_does_not_execute_code(self):
        status, report = self.run_cli()
        self.assertEqual(status, 0)
        self.assertEqual(report["kind"], "read-only-qt-kit-probe")
        self.assertEqual(report["blockers"], [])
        self.assertFalse(report["build_started"])
        self.assertIn("complete Qt modules and plugins", report["not_certified"])
        self.assertIn("Linux GNU ABI", report["not_certified"])
        self.assertEqual(len(report["checks"]), 3)

    def test_only_the_612_version_family_is_accepted(self):
        for version in ("6.11.9", "6.13.0", "7.12.0"):
            with self.subTest(version=version):
                self.version.write_text(f'set(PACKAGE_VERSION "{version}")\n')
                status, report = self.run_cli()
                self.assertEqual(status, 2)
                self.assertEqual(report["blockers"], ["qt_612_metadata"])

    def test_core_must_be_arm64_shared_and_16k_compatible(self):
        foreign = elf()
        struct.pack_into("<H", foreign, 18, 62)
        for data in (elf(kind=2), elf(align=4096), foreign, b"bad ELF"):
            with self.subTest(data=data[:24]):
                self.core.write_bytes(data)
                status, report = self.run_cli()
                self.assertEqual(status, 2)
                self.assertEqual(report["blockers"], ["qt_core_arm64_16k_layout"])

    def test_missing_kit_fails_all_three_checks(self):
        self.version.unlink()
        self.spec.unlink()
        self.core.unlink()
        status, report = self.run_cli()
        self.assertEqual(status, 2)
        self.assertEqual(len(report["blockers"]), 3)

    def test_missing_or_non_regular_root_config_cannot_enable_cmake_fallback(self):
        self.config.unlink()
        for as_directory in (False, True):
            if as_directory:
                self.config.mkdir()
            with self.subTest(as_directory=as_directory):
                status, report = self.run_cli()
                self.assertEqual(status, 2)
                self.assertEqual(report["blockers"], ["qt_linux_desktop_metadata"])

    def test_missing_or_inconsistent_root_version_cannot_enable_cmake_fallback(self):
        for content in (None, 'set(PACKAGE_VERSION "6.11.0")\n', 'set(PACKAGE_VERSION "6.12.1")\n',
                        '# set(PACKAGE_VERSION "6.12.0")\n', 'set(PACKAGE_VERSION "${other}")\n'):
            if content is None:
                self.root_version.unlink()
            else:
                self.root_version.write_text(content)
            with self.subTest(content=content):
                status, report = self.run_cli()
                self.assertEqual(status, 2)
                self.assertEqual(report["blockers"], ["qt_612_metadata"])

    def test_qt_only_refuses_source_or_scratch_arguments(self):
        for extra in (["--source", str(self.kit)], ["--expected-commit", "a" * 40],
                      ["--scratch-root", str(self.kit)], ["--scratch-budget", "budget.json"]):
            with self.subTest(extra=extra), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as caught:
                pi.main(["--qt-only", "--qt-target", str(self.kit), *extra])
            self.assertEqual(caught.exception.code, 2)

    def test_full_probe_still_requires_source_and_commit(self):
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as caught:
            pi.main(["--qt-target", str(self.kit)])
        self.assertEqual(caught.exception.code, 2)


class BootstrapProbeTests(unittest.TestCase):
    def run_probe(self, *, machine="aarch64", release="6.18.0+rpt-rpi-2712", page=16384,
                  board="Raspberry Pi 5 Model B Rev 1.0\0", os_text=None, memory=True):
        if os_text is None:
            os_text = 'ID=debian\nVERSION_ID="13"\nVERSION_CODENAME=trixie\n'

        def read(path, limit):
            self.assertIn(path.as_posix(), ("/etc/os-release", "/sys/firmware/devicetree/base/model"))
            self.assertIn(limit, (128, 65536))
            value = os_text if path.name == "os-release" else board
            if isinstance(value, Exception):
                raise value
            return value

        output = io.StringIO()
        with patch.object(pi.platform, "system", return_value="Linux"), \
                patch.object(pi.platform, "machine", return_value=machine), \
                patch.object(pi.platform, "release", return_value=release), \
                patch.object(pi.os, "sysconf", return_value=page, create=True), \
                patch.object(pi, "bootstrap_text", side_effect=read), \
                patch.object(pi, "memory_check", return_value=(memory, "fixture observation")), \
                patch.object(pi.subprocess, "run", side_effect=AssertionError("subprocess executed")), \
                patch.object(pi, "qt_kit_checks", side_effect=AssertionError("Qt inspected")), \
                patch.object(pi, "probe", side_effect=AssertionError("full probe ran")), redirect_stdout(output):
            status = pi.main(["--bootstrap-only"])
        return status, json.loads(output.getvalue())

    def test_fixture_pass_is_only_bootstrap_metadata_not_builder_acceptance(self):
        status, report = self.run_probe()
        self.assertEqual(status, 0)
        self.assertEqual(len(report["checks"]), 5)
        self.assertEqual(report["blockers"], [])
        self.assertFalse(report["builder_qualified"])
        self.assertFalse(report["build_started"])
        self.assertFalse(report["image_built"])
        self.assertIn("namespace privileges", report["not_certified"])
        self.assertIn("Qt entitlement, modules and dependency ABI", report["not_certified"])

    def test_x86_and_wsl_cannot_qualify_as_native(self):
        for args in ({"machine": "x86_64"}, {"release": "6.6.87.2-microsoft-standard-WSL2"}):
            with self.subTest(args=args):
                status, report = self.run_probe(**args)
                self.assertEqual(status, 2)
                self.assertIn("native_arm64_linux", report["blockers"])

    def test_wrong_missing_or_hostile_board_is_not_exported(self):
        for board in ("Raspberry Pi 4 Model B Rev 1.0\0", "Raspberry Pi 5\nunsafe", OSError()):
            with self.subTest(board=board):
                status, report = self.run_probe(board=board)
                self.assertEqual(status, 2)
                self.assertIn("pi5_board_metadata", report["blockers"])
                self.assertNotIn("unsafe", json.dumps(report))

    def test_os_identity_is_exact_and_unrelated_fields_are_not_exported(self):
        for os_text in ('ID=ubuntu\nVERSION_ID="24.04"\nVERSION_CODENAME=noble\n',
                        'ID=debian\nVERSION_ID=12\nVERSION_CODENAME=bookworm\n',
                        'ID=debian\nVERSION_ID=13\n', OSError()):
            with self.subTest(os_text=os_text):
                status, report = self.run_probe(os_text=os_text)
                self.assertEqual(status, 2)
                self.assertIn("adopted_trixie_os", report["blockers"])
        status, report = self.run_probe(os_text="ID=raspbian\nVERSION_ID='13'\nVERSION_CODENAME=trixie\nOTHER=private-value\n")
        self.assertEqual(status, 0)
        self.assertNotIn("private-value", json.dumps(report))

    def test_ambiguous_or_executable_os_identity_is_refused(self):
        for os_text in ('ID=debian\nID=debian\n', 'ID="$(command)"\n',
                        'ID=debian\x1b\n', 'VERSION_ID="13\n', 'ID\n'):
            with self.subTest(os_text=os_text):
                status, report = self.run_probe(os_text=os_text)
                self.assertEqual(status, 2)
                self.assertIn("adopted_trixie_os", report["blockers"])

    def test_4k_unknown_or_boolean_pages_and_low_memory_remain_blockers(self):
        for page in (4096, 65536, None, True):
            with self.subTest(page=page):
                status, report = self.run_probe(page=page)
                self.assertEqual(status, 2)
                self.assertIn("running_kernel_16k_pages", report["blockers"])
        status, report = self.run_probe(memory=False)
        self.assertEqual(status, 2)
        self.assertIn("host_ram_3gib", report["blockers"])

    def test_fixed_metadata_reads_are_bounded_and_ascii(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "model"
            path.write_bytes(b"A" * 128)
            self.assertEqual(len(pi.bootstrap_text(path, 128)), 128)
            for raw in (b"A" * 129, b"\xff"):
                path.write_bytes(raw)
                with self.subTest(raw=raw[:5]), self.assertRaises(ValueError):
                    pi.bootstrap_text(path, 128)

    def test_modes_cannot_mix_and_full_or_qt_mode_still_requires_kit(self):
        for args in (["--bootstrap-only", "--qt-only"], ["--bootstrap-only", "--qt-target", "kit"],
                     ["--bootstrap-only", "--source", "."], ["--bootstrap-only", "--expected-commit", "a" * 40],
                     ["--bootstrap-only", "--scratch-root", "."], ["--bootstrap-only", "--scratch-budget", "budget"],
                     ["--qt-only"], ["--source", ".", "--expected-commit", "a" * 40]):
            with self.subTest(args=args), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as caught:
                pi.main(args)
            self.assertEqual(caught.exception.code, 2)


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
