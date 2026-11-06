# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Synthetic hostile guard fixtures; these are not installation evidence."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("vmguard", Path(__file__).resolve().parents[2] / "tools/image/vmguard.py")
vmguard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vmguard)


class VmGuard(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.workspace = Path(self.temp.name).resolve()
        self.template = self.workspace / "template.fd"
        self.template.write_bytes(bytes(4096))
        self.root = self.workspace / "fresh-vm"
        self.record = vmguard.create(self.root, self.template, 1024**2, "a" * 40, "b" * 64, self.workspace)
        self.manifest = self.root / "allowlist.json"

    def save(self, value):
        self.manifest.write_text(json.dumps(value))

    def test_fresh_files_and_private_variables(self):
        self.assertEqual(vmguard.verify(self.manifest, self.workspace), self.record)
        self.assertEqual((self.root / "nvram.fd").read_bytes(), self.template.read_bytes())
        self.assertNotEqual(vmguard.identity(self.root / "nvram.fd"), vmguard.identity(self.template))
        self.assertEqual(self.record["disk"]["format"], "raw")
        self.assertIsNone(self.record["disk"]["backing_file"])
        # VirtIO's GET_ID field is 20 bytes; longer serials are truncated by
        # QEMU and would never match the guest allowlist.
        self.assertEqual(len(self.record["disk"]["serial"].encode("ascii")), 20)

    def test_existing_directory_never_reused(self):
        before = (self.root / "install.raw").stat()
        with self.assertRaises(FileExistsError):
            vmguard.create(self.root, self.template, 1024**2, "a" * 40, "b" * 64, self.workspace)
        self.assertEqual(before.st_ino, (self.root / "install.raw").stat().st_ino)

    def test_outside_workspace_refused(self):
        with self.assertRaises(ValueError):
            vmguard.create(self.workspace.parent / "outside", self.template, 1024**2, "a" * 40, "b" * 64, self.workspace)

    def test_replaced_disk_refused(self):
        disk = self.root / "install.raw"
        disk.rename(self.root / "preserved.raw")
        disk.write_bytes(bytes(1024**2))
        with self.assertRaises(ValueError):
            vmguard.verify(self.manifest, self.workspace)

    def test_resized_disk_refused(self):
        with (self.root / "install.raw").open("ab") as stream:
            stream.write(b"x")
        with self.assertRaises(ValueError):
            vmguard.verify(self.manifest, self.workspace)

    def test_backing_file_and_renamed_target_refused(self):
        for key, value in (("backing_file", "other.qcow2"), ("format", "qcow2"), ("name", "../../host")):
            record = copy.deepcopy(self.record)
            record["disk"][key] = value
            self.save(record)
            with self.assertRaises(ValueError):
                vmguard.verify(self.manifest, self.workspace)

    def test_hard_link_refused(self):
        os.link(self.root / "install.raw", self.root / "alias.raw")
        with self.assertRaises(ValueError):
            vmguard.verify(self.manifest, self.workspace)

    def test_parent_identity_change_refused(self):
        record = copy.deepcopy(self.record)
        record["ancestry"][0]["id"][1] += 1
        self.save(record)
        with self.assertRaises(ValueError):
            vmguard.verify(self.manifest, self.workspace)

    def test_private_nvram_replacement_refused(self):
        nvram = self.root / "nvram.fd"
        nvram.rename(self.root / "old.fd")
        nvram.write_bytes(bytes(4096))
        with self.assertRaises(ValueError):
            vmguard.verify(self.manifest, self.workspace)

    def observation(self):
        return {"path": "/dev/vda", "serial": self.record["disk"]["serial"], "bytes": 1024**2,
                "driver": "virtio_blk", "sysfs": "/sys/devices/pci0000:00/0000:00:04.0/virtio7/block/vda",
                "major_minor": "252:0", "busy": False, "read_only": False}

    def test_full_guest_identity_required(self):
        disk = self.observation()
        self.assertEqual(vmguard.guest_target(self.record, self.record["vm_uuid"], [disk]), disk)

    def test_guest_name_and_vm_detection_are_insufficient(self):
        for key, value in (("serial", "physical-disk"), ("bytes", 2 * 1024**2), ("driver", "sd"),
                           ("sysfs", "/sys/devices/virtual/block/vda"), ("busy", True), ("read_only", True),
                           ("path", "/dev/sda")):
            disk = self.observation()
            disk[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                vmguard.guest_target(self.record, self.record["vm_uuid"], [disk])

    def test_extra_or_missing_disks_and_wrong_vm_refused(self):
        disk = self.observation()
        for disks in ([], [disk, disk]):
            with self.assertRaises(ValueError):
                vmguard.guest_target(self.record, self.record["vm_uuid"], disks)
        with self.assertRaises(ValueError):
            vmguard.guest_target(self.record, "00000000-0000-0000-0000-000000000000", [disk])


if __name__ == "__main__":
    unittest.main()
