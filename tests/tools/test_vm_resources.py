# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Memory rejection must precede VM creation or an expensive ISO hash."""
import importlib
from pathlib import Path
import sys
import unittest
from collections import namedtuple
import tempfile
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
resources = importlib.import_module("vm_resources")
launcher = importlib.import_module("vmrun")
builder = importlib.import_module("build_vm_probe")


class VmResources(unittest.TestCase):
    def test_callers_cannot_lower_or_disable_the_memory_floor(self):
        reader = Mock(return_value=4 * resources.GIB)
        for minimum in (0, -1, True, None, 3 * resources.GIB - 1, float(3 * resources.GIB)):
            with self.subTest(minimum=minimum), self.assertRaises(ValueError):
                resources.require_headroom("fixture", minimum=minimum, reader=reader)
        reader.assert_not_called()

    def test_a_higher_memory_floor_is_enforced(self):
        with self.assertRaises(ValueError):
            resources.require_headroom("fixture", minimum=5 * resources.GIB, reader=lambda: 4 * resources.GIB)

    def test_exact_memory_floor_remains_accepted(self):
        self.assertEqual(resources.require_headroom("fixture", reader=lambda: 3 * resources.GIB)["available_bytes"],
                         3 * resources.GIB)

    def test_budget_and_unknown_availability(self):
        self.assertEqual(resources.require_headroom("fixture", reader=lambda: 4 * resources.GIB)["minimum_bytes"],
                         3 * resources.GIB)
        for available in (327 * 1024**2, 3 * resources.GIB - 1, -1, True, None):
            with self.subTest(available=available), self.assertRaises(ValueError):
                resources.require_headroom("fixture", reader=lambda: available)

    def test_low_memory_prevents_vm_hash_output_and_launch(self):
        with patch.object(launcher, "verify", return_value={}), \
                patch.object(launcher, "require_headroom", side_effect=ValueError), \
                patch.object(launcher, "checked_iso") as image, \
                patch.object(launcher, "docker_command") as docker:
            with self.assertRaises(ValueError):
                launcher.run(Path("fixture/allowlist.json"), Path("fixture/media.iso"),
                             Path("fixture/evidence"), "sha256:" + "a" * 64, "live")
            image.assert_not_called()
            docker.assert_not_called()

    def test_low_memory_prevents_image_build_and_log_creation(self):
        log = Mock()
        with patch.object(builder, "require_headroom", side_effect=ValueError), \
                patch.object(builder.subprocess, "run") as run:
            with self.assertRaises(ValueError):
                builder.checked_image_build(["inert-build-fixture"], log)
            run.assert_not_called()
            log.open.assert_not_called()


class NativeLinuxMemory(unittest.TestCase):
    def setUp(self):
        self.data = {
            "/proc/meminfo": "MemTotal: 8388608 kB\nMemAvailable: 6291456 kB\n",
            "/proc/self/cgroup": "0::/builder.slice/job.scope\n",
            "/proc/self/mountinfo": "1 0 0:1 / /sys/fs/cgroup rw - cgroup2 cgroup rw\n",
            "/sys/fs/cgroup/builder.slice/memory.max": "max\n",
            "/sys/fs/cgroup/builder.slice/memory.current": "0\n",
            "/sys/fs/cgroup/builder.slice/job.scope/memory.max": "max\n",
            "/sys/fs/cgroup/builder.slice/job.scope/memory.current": "0\n",
        }
        for name, value in (("system", "Linux"), ("release", "fixture-native-kernel")):
            patcher = patch.object(resources.platform, name, return_value=value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def read(self, path):
        try:
            return self.data[str(path)]
        except KeyError:
            raise FileNotFoundError("fixture missing") from None

    def available(self):
        return resources.linux_available_memory(reader=self.read)

    def test_native_host_available_memory_without_swap(self):
        self.assertEqual(self.available(), 6 * resources.GIB)
        self.assertEqual(resources.require_headroom("fixture", reader=self.available)["minimum_bytes"],
                         3 * resources.GIB)

    def test_leaf_and_ancestor_limits_both_constrain_headroom(self):
        self.data["/sys/fs/cgroup/builder.slice/job.scope/memory.max"] = str(5 * resources.GIB)
        self.data["/sys/fs/cgroup/builder.slice/job.scope/memory.current"] = str(resources.GIB)
        self.assertEqual(self.available(), 4 * resources.GIB)
        self.data["/sys/fs/cgroup/builder.slice/memory.max"] = str(4 * resources.GIB)
        self.data["/sys/fs/cgroup/builder.slice/memory.current"] = str(2 * resources.GIB)
        self.assertEqual(self.available(), 2 * resources.GIB)
        with self.assertRaises(ValueError):
            resources.require_headroom("fixture", reader=self.available)

    def test_a_large_cgroup_limit_cannot_inflate_host_memory(self):
        self.data["/sys/fs/cgroup/builder.slice/job.scope/memory.max"] = str(16 * resources.GIB)
        self.assertEqual(self.available(), 6 * resources.GIB)

    def test_exhausted_or_over_limit_group_has_zero_headroom(self):
        self.data["/sys/fs/cgroup/builder.slice/job.scope/memory.max"] = "100"
        for current in (100, 101):
            self.data["/sys/fs/cgroup/builder.slice/job.scope/memory.current"] = str(current)
            self.assertEqual(self.available(), 0)

    def test_missing_available_memory_has_no_memfree_or_swap_fallback(self):
        self.data["/proc/meminfo"] = "MemTotal: 8388608 kB\nMemFree: 6291456 kB\nSwapFree: 9999999 kB\n"
        with self.assertRaises(ValueError):
            self.available()

    def test_malformed_duplicate_or_impossible_meminfo_is_rejected(self):
        for text in ("MemTotal: 1 kB\nMemAvailable: 2 kB\n", "MemTotal: 0 kB\nMemAvailable: 0 kB\n",
                     "MemTotal: 8192 kB\nMemAvailable: 1.5 kB\n",
                     "MemTotal: 8192 kB\nMemAvailable: 1 kB\nMemAvailable: 2 kB\n",
                     "MemTotal: 18446744073709551615 kB\nMemAvailable: 1 kB\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.data["/proc/meminfo"] = text
                self.available()

    def test_unknown_counter_values_fail_closed(self):
        for key in ("memory.max", "memory.current"):
            for value in ("", "-1", "1.5", "0x100", "true", "18446744073709551616"):
                self.data["/sys/fs/cgroup/builder.slice/job.scope/" + key] = value
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    self.available()
            self.data["/sys/fs/cgroup/builder.slice/job.scope/" + key] = "0"

    def test_missing_ancestor_counter_remains_unmeasured(self):
        del self.data["/sys/fs/cgroup/builder.slice/memory.max"]
        with self.assertRaises(OSError):
            self.available()

    def test_unknown_or_unsafe_cgroup_membership_is_rejected(self):
        for group in ("1:memory:/job\n", "0::/job\n0::/other\n", "0::/../outside\n",
                      "0::/job/./child\n", "0::relative\n", "0::/job//child\n", "0::/job\x00x\n",
                      "0:://\n", "0:://job\n"):
            self.data["/proc/self/cgroup"] = group
            with self.subTest(group=group), self.assertRaises(ValueError):
                self.available()

    def test_root_group_needs_no_nonexistent_root_limit_file(self):
        self.data["/proc/self/cgroup"] = "0::/\n"
        self.assertEqual(self.available(), 6 * resources.GIB)

    def test_hidden_or_unknown_mount_root_is_rejected(self):
        for text in ("", "malformed", "1 0 0:1 /hidden /sys/fs/cgroup rw - cgroup2 cgroup rw\n",
                     "1 0 0:1 / /sys/fs/cgroup rw - tmpfs tmpfs rw\n"):
            self.data["/proc/self/mountinfo"] = text
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.available()

    def test_wsl_does_not_read_linux_counters_as_windows_headroom(self):
        reader = Mock()
        with patch.object(resources.platform, "release", return_value="fixture-microsoft-WSL2"):
            with self.assertRaises(ValueError):
                resources.linux_available_memory(reader=reader)
        reader.assert_not_called()

    def test_non_linux_host_is_rejected_without_observation(self):
        reader = Mock()
        with patch.object(resources.platform, "system", return_value="Windows"):
            with self.assertRaises(ValueError):
                resources.linux_available_memory(reader=reader)
        reader.assert_not_called()

    def test_oversized_observation_is_rejected(self):
        self.data["/proc/meminfo"] = "x" * 65537
        with self.assertRaises(ValueError):
            self.available()

    def test_migration_during_observation_is_rejected(self):
        original = self.read
        reads = 0

        def migrating(path):
            nonlocal reads
            if path == "/proc/self/cgroup":
                reads += 1
                if reads > 1:
                    return "0::/different.scope\n"
            return original(path)

        with self.assertRaises(ValueError):
            resources.linux_available_memory(reader=migrating)


class ScratchResources(unittest.TestCase):
    def budget(self):
        # Small, obvious byte-count fixtures; never a real builder estimate.
        return {"schema": 1, "components": {name: 100 for name in resources.SCRATCH_COMPONENTS}}

    def test_every_component_and_reserve_count_towards_the_requirement(self):
        self.assertEqual(resources.scratch_requirement(self.budget()), 700)

    def test_missing_extra_or_ambiguous_components_are_rejected(self):
        for name in resources.SCRATCH_COMPONENTS:
            value = self.budget()
            del value["components"][name]
            with self.subTest(missing=name), self.assertRaises(ValueError):
                resources.scratch_requirement(value)
        for value in (None, [], {"schema": True, "components": self.budget()["components"]},
                      {**self.budget(), "qualified": True}):
            with self.subTest(value=value), self.assertRaises(ValueError):
                resources.scratch_requirement(value)
        value = self.budget()
        value["components"]["unmeasured"] = 1
        with self.assertRaises(ValueError):
            resources.scratch_requirement(value)

    def test_precision_unknown_values_and_overflow_are_rejected(self):
        for size in (0, -1, True, None, 100.5, "100", 1 << 64):
            value = self.budget()
            value["components"]["raw_image"] = size
            with self.subTest(size=size), self.assertRaises(ValueError):
                resources.scratch_requirement(value)
        value = self.budget()
        value["components"]["raw_image"] = (1 << 64) - 1
        with self.assertRaises(ValueError):
            resources.scratch_requirement(value)

    def test_duplicate_budget_fields_are_rejected(self):
        with self.assertRaises(ValueError):
            resources.parse_scratch_budget(b'{"schema": 1, "schema": 1, "components": {}}')
        with self.assertRaises(ValueError):
            resources.parse_scratch_budget(b'{"schema": 1, "components": {"source": 1, "source": 2}}')

    def test_deeply_nested_or_oversized_budget_fails_without_a_traceback(self):
        for raw in (b"[" * 2000 + b"0" + b"]" * 2000, b" " * 65537):
            with self.subTest(size=len(raw)), self.assertRaises(ValueError):
                resources.parse_scratch_budget(raw)

    def test_capacity_is_measured_at_the_selected_scratch_directory(self):
        usage = namedtuple("Usage", "total used free")
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(resources, "local_disk_directory", return_value=Path(directory)):
            reader = Mock(return_value=usage(1000, 300, 700))
            result = resources.require_scratch_headroom(Path(directory), self.budget(), reader=reader)
            self.assertEqual(result["required_bytes"], 700)
            self.assertEqual(result["available_bytes"], 700)
            reader.assert_called_once_with(Path(directory))
            reader.return_value = usage(1000, 301, 699)
            with self.assertRaises(ValueError):
                resources.require_scratch_headroom(Path(directory), self.budget(), reader=reader)

    def test_unknown_backing_storage_never_reaches_a_capacity_reader(self):
        reader = Mock()
        with patch.object(resources, "local_disk_directory", side_effect=ValueError):
            with self.assertRaises(ValueError):
                resources.require_scratch_headroom(Path("fixture"), self.budget(), reader=reader)
        reader.assert_not_called()

    def test_nonexistent_or_non_directory_scratch_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            with self.assertRaises((OSError, ValueError)):
                resources.local_disk_directory(path / "missing")
            file = path / "regular-file"
            file.write_bytes(b"fixture")
            with self.assertRaises(ValueError):
                resources.local_disk_directory(file)

    def test_wsl_virtual_capacity_is_not_physical_host_capacity(self):
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(resources.platform, "release", return_value="fixture-microsoft-WSL2"):
            with self.assertRaises(ValueError):
                resources.local_disk_directory(Path(directory))

    def test_invalid_capacity_observations_fail_closed(self):
        usage = namedtuple("Usage", "total used free")
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(resources, "local_disk_directory", return_value=Path(directory)):
            for observed in (usage(1000, 0, True), usage(1000, 0, -1), usage(1000, 0, 1001),
                             usage(0, 0, 0), usage(1000, 1001, 0)):
                with self.subTest(observed=observed), self.assertRaises(ValueError):
                    resources.require_scratch_headroom(Path(directory), self.budget(), reader=lambda _: observed)


@unittest.skipUnless(sys.platform == "linux", "Linux mount layout")
class LinuxScratchMounts(unittest.TestCase):
    def test_local_disk_and_escaped_mount_name(self):
        raw = b"1 0 8:1 / / rw - ext4 /dev/fixture rw\n2 1 8:2 / /scratch\\040space rw - xfs /dev/fixture2 rw\n"
        resources._check_linux_mount(Path("/scratch space/work"), raw, "8:2")

    def test_nested_virtual_or_remote_mount_hides_local_disk(self):
        base = b"1 0 8:1 / / rw - ext4 /dev/fixture rw\n"
        for filesystem, source in (("tmpfs", "tmpfs"), ("overlay", "overlay"), ("nfs", "host:/share")):
            row = f"2 1 0:99 / /scratch rw - {filesystem} {source} rw\n".encode()
            with self.subTest(filesystem=filesystem), self.assertRaises(ValueError):
                resources._check_linux_mount(Path("/scratch/work"), base + row, "0:99")

    def test_a_hidden_disk_mount_cannot_qualify_a_virtual_overmount(self):
        raw = (b"1 0 8:1 / /scratch rw - ext4 /dev/fixture rw\n"
               b"2 1 0:99 / /scratch rw - tmpfs tmpfs rw\n")
        with self.assertRaises(ValueError):
            resources._check_linux_mount(Path("/scratch/work"), raw, "0:99")

    def test_missing_malformed_or_mismatched_mount_is_unqualified(self):
        for raw in (b"", b"malformed", b"1 0 8:1 / / rw - ext4\n",
                    b"1 0 8:1 / / rw - ext4 /dev/fixture rw\n", b"x" * (1024 * 1024 + 1)):
            with self.subTest(size=len(raw)), self.assertRaises(ValueError):
                resources._check_linux_mount(Path("/scratch/work"), raw, "0:99")


if __name__ == "__main__":
    unittest.main()
