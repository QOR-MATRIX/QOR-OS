# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Hostile synthetic OCI fixtures, never an installation or boot pass."""
import copy
import hashlib
import importlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
payload = importlib.import_module("vm_payload")
session = importlib.import_module("vm_session")
control = importlib.import_module("vm_control")


class VmPayload(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        (self.root / "blobs/sha256").mkdir(parents=True)
        self.config = {"os": "linux", "architecture": "amd64", "config": {"Labels": {
            "org.opencontainers.image.revision": "a" * 40, "dev.qorsync.vm-profile": "installed"}}}
        self.manifest = {"schemaVersion": 2, "mediaType": payload.MANIFEST,
                         "config": self.blob(self.config, payload.CONFIG),
                         "layers": [self.blob(b"inert layer fixture", next(iter(payload.LAYERS)))]}
        self.selected = self.blob(self.manifest, payload.MANIFEST)
        (self.root / "oci-layout").write_text('{"imageLayoutVersion":"1.0.0"}')
        self.index = {"schemaVersion": 2, "manifests": [self.selected]}
        (self.root / "index.json").write_text(json.dumps(self.index))
        self.receipt = {"schema": 2, "source_revision": "a" * 40, "manifest_sha256": self.selected["digest"][7:],
                        "profile": "private-vm-lab", "encryption": False}

    def blob(self, value, media):
        raw = value if isinstance(value, bytes) else json.dumps(value).encode()
        digest = hashlib.sha256(raw).hexdigest()
        (self.root / "blobs/sha256" / digest).write_bytes(raw)
        return {"digest": "sha256:" + digest, "size": len(raw), "mediaType": media}

    def check(self):
        return payload.validate(self.root, self.receipt, lambda path: path.open("rb"))

    def test_complete_pinned_image(self):
        self.assertEqual(self.check(), self.receipt)

    def test_corrupt_missing_or_unexpected_blob(self):
        layer = self.root / "blobs/sha256" / self.manifest["layers"][0]["digest"][7:]
        original = layer.read_bytes()
        layer.write_bytes(b"changed")
        with self.assertRaises(ValueError):
            self.check()
        layer.write_bytes(original)
        extra = self.root / "unexpected"
        extra.write_bytes(b"unreferenced fixture")
        with self.assertRaises(ValueError):
            self.check()
        extra.unlink()
        layer.unlink()
        with self.assertRaises(OSError):
            self.check()

    def test_wrong_receipt_revision_encryption_and_manifest(self):
        for field, value in (("schema", True), ("source_revision", "b" * 40), ("manifest_sha256", "b" * 64),
                             ("profile", "release"), ("encryption", True)):
            original = self.receipt
            self.receipt = {**original, field: value}
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.check()
            self.receipt = original

    def test_external_oversized_boolean_and_multiple_descriptors(self):
        for change in ({"urls": ["https://invalid.example/blob"]}, {"size": True},
                       {"size": 1024**3}, {"digest": "../../other"}, {"mediaType": "unknown"}):
            index = copy.deepcopy(self.index)
            index["manifests"][0].update(change)
            (self.root / "index.json").write_text(json.dumps(index))
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.check()
        self.index["manifests"] *= 2
        (self.root / "index.json").write_text(json.dumps(self.index))
        with self.assertRaises(ValueError):
            self.check()

    def test_duplicate_metadata_keys(self):
        (self.root / "oci-layout").write_text('{"imageLayoutVersion":"1.0.0","imageLayoutVersion":"1.0.0"}')
        with self.assertRaises(ValueError):
            self.check()

    def test_unknown_setup_bytes_are_never_evidence(self):
        state = self.root / "completed.json"
        self.assertEqual(session.setup_observation(state), {"state": "pending"})
        state.write_bytes(b"unrecognised private fixture")
        self.assertEqual(session.setup_observation(state), {"state": "invalid"})
        state.write_bytes(session.RECORD)
        observed = session.setup_observation(state)
        self.assertEqual(observed, {"state": "complete",
                                    "record_sha256": hashlib.sha256(session.RECORD).hexdigest()})
        os.link(state, self.root / "alias")
        self.assertEqual(session.setup_observation(state), {"state": "invalid"})

    def test_guest_agent_must_deny_every_other_rpc(self):
        commands = [{"name": name, "enabled": True} for name in control.AGENT_RPCS]
        commands.append({"name": "guest-exec", "enabled": False})
        self.assertEqual(control.agent_policy({"supported_commands": commands})["other_rpcs_disabled"], 1)
        commands[-1]["enabled"] = True
        with self.assertRaises(ValueError):
            control.agent_policy({"supported_commands": commands})
        for result in ({}, {"supported_commands": [{"name": "guest-shutdown", "enabled": "yes"}]}):
            with self.assertRaises(ValueError):
                control.agent_policy(result)

    def test_packaged_agent_uses_the_checked_allowlist_and_no_generic_service(self):
        root = Path(__file__).resolve().parents[2]
        unit = (root / "image/vm/guest-agent.service").read_text()
        start = next(line for line in unit.splitlines() if line.startswith("ExecStart="))
        allow = next(argument for argument in start.split() if argument.startswith("--allow-rpcs="))
        self.assertEqual(set(allow.partition("=")[2].split(",")), control.AGENT_RPCS)
        self.assertIn("--config=/usr/share/qor-os/vm-agent.conf", start)
        self.assertNotIn("--fsfreeze-hook", start)
        recipe = (root / "image/vm/Probe.Containerfile").read_text()
        self.assertIn("getty@tty2.service autovt@tty2.service qemu-guest-agent.service", recipe)
        self.assertIn(f'ATTR{{name}}=="{session.EVENT_PORT}", TAG+="systemd"', recipe)

    def test_port_services_start_from_their_ready_device_event(self):
        root = Path(__file__).resolve().parents[2]
        recipe = (root / "image/vm/Probe.Containerfile").read_text()
        common, live = recipe.split("FROM common AS live", 1)
        self.assertTrue('ENV{SYSTEMD_WANTS}="qor-vm-agent.service"' in common,
                        "The restricted agent must start when its assigned port becomes ready")
        self.assertTrue('ENV{SYSTEMD_WANTS}="qor-vm-first-run.service"' in common,
                        "Installed welcome must start when its evidence port becomes ready")
        for unit in ("qor-vm-agent", "qor-vm-first-run"):
            self.assertFalse(f"ln -s /usr/lib/systemd/system/{unit}.service" in common,
                             "Early target activation races slow virtual-device discovery")
        self.assertFalse("ln -s /usr/lib/systemd/system/qor-vm-installer.service" in live,
                         "The live installer must also wait for its evidence-port event")
        self.assertIn("qor-vm-first-run.service/qor-vm-installer.service", live)
        self.assertIn("ln -s /usr/lib/systemd/system/qor-vm-display-probe.service", live)

    def test_agent_sync_ignores_old_errors_and_requires_exact_nonce(self):
        stream = Mock()
        commands = [{"name": name, "enabled": True} for name in control.AGENT_RPCS]
        stream.readline.side_effect = [
            b'{"error":{"class":"GenericError"}}\n',
            b'\xff{"return":17}\n',
            json.dumps({"return": {"supported_commands": commands}}).encode() + b"\n"]
        with patch.object(control, "request") as qmp, \
                patch.object(control.uuid, "uuid4", return_value=type("Nonce", (), {"int": 17})()), \
                patch.object(control.socket, "create_connection") as connect:
            connect.return_value.__enter__.return_value.makefile.return_value = stream
            self.assertEqual(control.guest_request("guest-info")["enabled"], sorted(control.AGENT_RPCS))
            qmp.assert_called_once_with({"execute": "query-status"})
            connect.assert_called_once_with(("127.0.0.1", 4446), timeout=10)

    def test_wrong_vm_cannot_open_agent_channel(self):
        with patch.object(control, "request", side_effect=ValueError), \
                patch.object(control.socket, "create_connection") as connect:
            with self.assertRaises(ValueError):
                control.guest_request("guest-reboot")
            connect.assert_not_called()

    def test_missing_firmware_prevents_state_or_port_discovery(self):
        with patch.object(session, "expected_target", side_effect=ValueError), \
                patch.object(session, "setup_observation") as state, patch.object(session, "port_node") as port:
            with self.assertRaises(ValueError):
                session.event("before-welcome")
            state.assert_not_called()
            port.assert_not_called()

    def test_unknown_port_or_event_is_refused(self):
        with self.assertRaises(ValueError):
            session.port_node("unapproved")
        with patch.object(session, "expected_target") as target:
            with self.assertRaises(ValueError):
                session.event("read-file")
            target.assert_not_called()


if __name__ == "__main__":
    unittest.main()
