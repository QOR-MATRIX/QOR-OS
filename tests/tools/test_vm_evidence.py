# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Synthetic evidence negative controls, not an installation/boot result."""
import base64
import copy
import hashlib
import importlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
evidence = importlib.import_module("verify_vm_evidence")


class VmEvidence(unittest.TestCase):
    def setUp(self):
        self.expected = {"source_revision": "a" * 40, "image_sha256": "b" * 64,
                         "vm_uuid": str(uuid.uuid4()), "disk": {"serial": "qor-" + "c" * 16}}
        ids = [str(uuid.uuid4()) for _ in range(3)]
        identity = {"schema": 1, **{key: self.expected[key] for key in
                                    ("source_revision", "image_sha256", "vm_uuid")}}
        raw = b"inert installation log fixture\n"
        self.install = [{**identity, "boot_id": ids[0], "event": "install-log-chunk", "offset": 0,
                         "base64": base64.b64encode(raw).decode()},
                        {**identity, "boot_id": ids[0], "event": "install-result", "exit_code": 0,
                         "log_bytes": len(raw), "log_sha256": hashlib.sha256(raw).hexdigest()}]
        complete = {"state": "complete", "record_sha256": evidence.RECORD_SHA256}
        self.first = [{**identity, "boot_id": ids[1], "event": "before-welcome", "setup": {"state": "pending"}},
                      {**identity, "boot_id": ids[1], "event": "after-welcome", "setup": complete, "process_success": True}]
        self.second = [{**identity, "boot_id": ids[2], "event": "before-welcome", "setup": complete},
                       {**identity, "boot_id": ids[2], "event": "after-welcome", "setup": complete, "process_success": True}]

    def check(self):
        return evidence.validate(self.expected, self.install, self.first, self.second)

    def test_separate_boots_and_checked_state(self):
        self.assertEqual(self.check()["completion_record_sha256"], evidence.RECORD_SHA256)

    def test_failure_or_missing_logs_cannot_pass(self):
        for field, value in (("exit_code", 1), ("exit_code", False), ("log_bytes", 1), ("log_sha256", "d" * 64)):
            install = copy.deepcopy(self.install)
            install[-1][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                evidence.validate(self.expected, install, self.first, self.second)
        for install in ([], self.install[:-1], self.install[1:], self.install + self.install):
            with self.assertRaises(ValueError):
                evidence.validate(self.expected, install, self.first, self.second)

    def test_crashed_welcome_or_missing_persistence_cannot_pass(self):
        self.first[-1]["process_success"] = False
        with self.assertRaises(ValueError):
            self.check()
        self.first[-1]["process_success"] = True
        self.second[0]["setup"] = {"state": "pending"}
        with self.assertRaises(ValueError):
            self.check()

    def test_reusing_a_boot_is_not_a_second_reboot(self):
        for event in self.second:
            event["boot_id"] = self.first[0]["boot_id"]
        with self.assertRaises(ValueError):
            self.check()

    def test_media_left_attached_or_different_source_cannot_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "launch.json").write_text(json.dumps({"phase": "installed", "allowlist": self.expected}))
            command = ["qemu-system-x86_64", "-uuid", self.expected["vm_uuid"]]
            (root / "qemu-command.json").write_text(json.dumps(command))
            log = root / "guest-events.jsonl"
            log.write_text("".join(json.dumps(event) + "\n" for event in self.first))
            self.assertEqual(evidence.observations(root, self.expected, "installed"), self.first)
            (root / "qemu-command.json").write_text(json.dumps(command + ["-drive", "media=cdrom"]))
            with self.assertRaises(ValueError):
                evidence.observations(root, self.expected, "installed")
            (root / "qemu-command.json").write_text(json.dumps(command))
            self.first[0]["source_revision"] = "d" * 40
            log.write_text("".join(json.dumps(event) + "\n" for event in self.first))
            with self.assertRaises(ValueError):
                evidence.observations(root, self.expected, "installed")


if __name__ == "__main__":
    unittest.main()
