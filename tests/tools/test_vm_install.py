# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Negative controls for the VM launcher and fixed installation backend."""
import importlib
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch
import json

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
backend = importlib.import_module("vm_install_backend")
inventory = importlib.import_module("guest_inventory")
container = importlib.import_module("vm_container")
launcher = importlib.import_module("vmrun")
guard = importlib.import_module("vmguard")
builder = importlib.import_module("build_vm_probe")
control = importlib.import_module("vm_control")
runtime_audit = importlib.import_module("check_vm_runtime")
runtime_stage = importlib.import_module("stage_vm_runtime")


class VmInstall(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name).resolve()
        template = self.work / "vars.fd"
        template.write_bytes(bytes(4096))
        self.record = guard.create(self.work / "vm", template, 1024**2, "a" * 40, "b" * 64, self.work)
        self.disk = {"path": "/dev/vda", "serial": self.record["disk"]["serial"]}
        self.receipt = {"schema": 1, "source_revision": "a" * 40, "archive_sha256": "c" * 64,
                        "profile": "private-vm-lab", "encryption": False}

    def test_transaction_has_no_custom_partitioner_or_implicit_disk(self):
        argv = backend.command(self.record, self.disk, self.receipt, self.disk["serial"])
        self.assertEqual(argv[:3], ["/usr/bin/bootc", "install", "to-disk"])
        self.assertEqual(argv[-1], "/dev/vda")
        self.assertIn("--generic-image", argv)
        self.assertIn("docker-archive:/usr/share/qor-os/install-source.tar", argv)
        self.assertNotIn("--via-loopback", argv)

    def test_builder_checks_emitted_errors_even_when_command_succeeds(self):
        for text in ("#9 0.314 dracut[E]: missing input", "dracut-install: ERROR: installing input", "FAILED: missing kernel"):
            self.assertTrue(builder.diagnostics_failed(text))
        self.assertFalse(builder.diagnostics_failed("#9 [live 5/5] RUN grep 'dracut-install: ERROR|FAILED:' log"))
        self.assertFalse(builder.diagnostics_failed("#9 1.5 dracut[I]: Created image\n#9 DONE 1.5s"))

    def test_runtime_loader_zero_exit_with_missing_library_fails(self):
        runner = Mock(return_value=type("Result", (), {"returncode": 0,
                                                       "stdout": "libpulse.so.0 => not found\n", "stderr": ""})())
        failures = runtime_audit.check_files([Path("/fixture/QtMultimedia.so")], runner)
        self.assertEqual(failures[0]["missing"], ["libpulse.so.0"])
        runner.return_value.stdout = "libpulse.so.0 => /lib64/libpulse.so.0 (0x1234)\n"
        self.assertEqual(runtime_audit.check_files([Path("/fixture/QtMultimedia.so")], runner), [])
        runner.return_value.returncode = 1
        self.assertTrue(runtime_audit.check_files([Path("/fixture/QtMultimedia.so")], runner))

    def test_stale_or_changed_source_cannot_receive_runtime_build_proof(self):
        runner = Mock(return_value=type("Result", (), {"returncode": 0, "stdout": "", "stderr": ""})())
        snapshot = Mock(side_effect=[{"revision": "a"}, {"revision": "b"}])
        with self.assertRaises(ValueError):
            runtime_stage.checked_build(["inert-fixture"], snapshot, runner)

    def test_runtime_warning_or_failed_build_never_receives_proof(self):
        result = type("Result", (), {"returncode": 0, "stdout": "Warning: runtime fixture", "stderr": ""})()
        runner = Mock(return_value=result)
        with self.assertRaises(ValueError):
            runtime_stage.checked_build(["inert-fixture"], lambda: {}, runner)
        result.stdout = ""
        result.returncode = 7
        with self.assertRaises(ValueError):
            runtime_stage.checked_build(["inert-fixture"], lambda: {}, runner)

    def test_runtime_receipt_does_not_relabel_an_older_compiler_revision(self):
        runtime = self.work / "runtime"
        runtime.mkdir()
        (runtime / "fixture").write_bytes(b"inert")
        receipt = {"source_head": "a" * 40, "files": {"fixture": builder.digest(runtime / "fixture")}}
        record = builder.verify_runtime_receipt(runtime, receipt, "b" * 64)
        self.assertEqual(record["compiler_source_revision"], "a" * 40)
        self.assertFalse(record["fresh_build_proved"])
        (runtime / "fixture").write_bytes(b"changed")
        with self.assertRaises(ValueError):
            builder.verify_runtime_receipt(runtime, receipt, "b" * 64)

    def test_control_outside_owned_container_cannot_connect(self):
        with patch.dict(os.environ, {}, clear=True), patch.object(control.socket, "create_connection") as connect:
            with self.assertRaises(ValueError):
                control.request({"execute": "quit"})
            connect.assert_not_called()

    def test_control_refuses_wrong_vm_before_sending_mutation(self):
        stream = Mock()
        stream.readline.side_effect = [b'{"QMP":{}}\n', b'{"return":{}}\n',
                                      b'{"return":{"UUID":"00000000-0000-0000-0000-000000000000"}}\n']
        with patch.dict(os.environ, {"QOR_VM_PHASE": "live"}), \
                patch.object(control.Path, "read_text", return_value=json.dumps(self.record)), \
                patch.object(control.socket, "create_connection") as connect:
            connect.return_value.__enter__.return_value.makefile.return_value = stream
            with self.assertRaises(ValueError):
                control.request({"execute": "quit"})
        sent = [json.loads(call.args[0])["execute"] for call in stream.write.call_args_list]
        self.assertEqual(sent, ["qmp_capabilities", "query-uuid"])

    def test_unconfirmed_or_mismatched_payload_never_generates_transaction(self):
        for serial in ("", "qor-" + "0" * 16):
            with self.assertRaises(ValueError):
                backend.command(self.record, self.disk, self.receipt, serial)
        for key, value in (("schema", True), ("source_revision", "d" * 40),
                           ("archive_sha256", "broken"), ("profile", "device"), ("encryption", True)):
            receipt = {**self.receipt, key: value}
            with self.subTest(key=key), self.assertRaises(ValueError):
                backend.command(self.record, self.disk, receipt, self.disk["serial"])

    def test_missing_firmware_metadata_fails_before_device_enumeration(self):
        with patch.object(inventory, "FWCFG", self.work / "missing"), patch.object(inventory, "collect") as collect:
            with self.assertRaises(FileNotFoundError):
                inventory.inspect()
            collect.assert_not_called()

    def test_malformed_contract_refused_without_device_enumeration(self):
        for value in ([], {}, {**self.record, "schema": True}, {**self.record, "disk": {}},
                      {**self.record, "disk": {**self.record["disk"], "serial": "qor-" + "a" * 20}},
                      {**self.record, "ancestry": []}):
            with self.subTest(value=type(value)), self.assertRaises((ValueError, TypeError, KeyError)):
                guard.validate_metadata(value)

    def test_guest_qemu_has_only_regular_fd_drives_and_no_network(self):
        live = container.arguments(self.record, True, 10, 11, 12, 13, readonly_fds=(14, 15))
        installed = container.arguments(self.record, False, 10, 11, 12, readonly_fds=(14, 15))
        self.assertIn("none", live)
        self.assertIn("tcg,thread=single,tb-size=64", live)
        self.assertNotIn("kvm", " ".join(live))
        self.assertIn("virtio-blk-pci,drive=target,addr=0x4,disable-legacy=on,serial=" + self.disk["serial"], live)
        self.assertIn("if=pflash,format=raw,unit=1,file=/dev/fdset/2", live)
        self.assertNotIn("media=cdrom", " ".join(installed))
        self.assertNotIn("/dev/sda", " ".join(live))
        with self.assertRaises(ValueError):
            container.arguments(self.record, True, 10, 11, 12, readonly_fds=(14, 15))
        with self.assertRaises(ValueError):
            container.arguments(self.record, True, 10, 11, 12, 13)

    def test_host_container_never_gets_devices_or_published_network(self):
        argv = launcher.docker_command(self.record, self.work / "image.iso", self.work / "proof",
                                       "sha256:" + "1" * 64, "live", self.work / "entry.py")
        for forbidden in ("--privileged", "--device", "--publish", "-p", "--network=host", "--pid=host"):
            self.assertNotIn(forbidden, argv)
        self.assertIn("none", argv)
        self.assertIn("ALL", argv)
        self.assertEqual(sum("target=/vm/install.raw" in value for value in argv), 1)
        with self.assertRaises(ValueError):
            launcher.docker_command(self.record, self.work / "image.iso", self.work / "proof", "latest", "live", self.work / "entry.py")

    def test_guest_entry_refuses_nonregular_wrong_capacity_and_hardlinks(self):
        path = self.work / "file"
        path.write_bytes(bytes(1024))
        for source, size in ((self.work, None), (path, 2048)):
            with self.assertRaises((ValueError, OSError)):
                container.regular_fd(source, True, size)
        os.link(path, self.work / "alias")
        with self.assertRaises(ValueError):
            container.regular_fd(path, True)

    def test_windows_ancestry_handles_block_disk_replacement(self):
        if os.name != "nt":
            # POSIX guest tests below cover the FD half; this assertion records
            # the explicitly unsupported host instead of claiming a pin pass.
            with self.assertRaises(ValueError):
                with launcher.pinned([]):
                    self.fail("An unsupported host was accepted")
            return
        disk = self.work / "vm/install.raw"
        with launcher.pinned([(disk, False)]):
            with self.assertRaises(OSError):
                disk.rename(self.work / "moved.raw")
        self.assertTrue(disk.exists())


if __name__ == "__main__":
    unittest.main()
