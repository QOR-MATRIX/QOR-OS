# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Public, non-enrolled fixtures only: no real credential, server or network."""

import base64
from contextlib import redirect_stdout, redirect_stderr
import io
import json
from pathlib import Path
import struct
import stat
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
import pi_headless as headless

INSTANCE = "11111111-1111-4111-8111-111111111111"
IMAGE = "a" * 64
# Public-format bytes, with no private counterpart generated or enrolled.
KEY = "ssh-ed25519 " + base64.b64encode(struct.pack(">I", 11) + b"ssh-ed25519"
                                      + struct.pack(">I", 32) + bytes(range(1, 33))).decode()


def receipt():
    return headless.make_receipt(INSTANCE, "qpi-fixture", IMAGE, KEY)


class PublicReceiptTests(unittest.TestCase):
    def test_canonical_receipt_and_comments_never_exported(self):
        value = headless.make_receipt(INSTANCE, "qpi-fixture", IMAGE, KEY + " private-comment\n")
        self.assertEqual(value, receipt())
        self.assertNotIn("private-comment", json.dumps(value))
        self.assertEqual(len(value["host_fingerprint"]), 50)
        self.assertEqual(headless.validate_receipt(value), value)

    def test_wrong_schema_duplicate_unknown_or_sensitive_fields_refused(self):
        for change in ({"schema": True}, {"schema": 2}, {"kind": "other"}, {"password": "do-not-log"},
                       {"host_fingerprint": "SHA256:wrong"}, {"host_key": KEY + " comment"}):
            with self.subTest(change=list(change)), self.assertRaises(ValueError):
                headless.validate_receipt({**receipt(), **change})
        with self.assertRaises(ValueError):
            headless.parse_json(b'{"schema":1,"schema":1}')

    def test_identity_hostname_and_digest_injection_refused(self):
        for args in (("bad", "qpi-fixture", IMAGE), ("1" * 36, "qpi-fixture", IMAGE),
                     (INSTANCE.upper().replace("1111", "AAAA", 1), "qpi-fixture", IMAGE),
                     (INSTANCE, "qpi-x\nProxyCommand cmd", IMAGE), (INSTANCE, "qpi-*", IMAGE),
                     (INSTANCE, "qpi-x.local", IMAGE), (INSTANCE, "qpi-", IMAGE),
                     (INSTANCE, "qpi-fixture", "A" * 64), (INSTANCE, "qpi-fixture", "a" * 65)):
            with self.subTest(args=args), self.assertRaises(ValueError):
                headless.make_receipt(*args, KEY)

    def test_public_blob_is_bounded_and_rejects_other_keys_or_multiline_content(self):
        for key in ("ssh-rsa abc", "ssh-ed25519 !!!", KEY + "\nsecond-key", KEY + "\0", KEY + "\tcomment",
                    "openssh-private-material", "x" * 1025, None,
                    "ssh-ed25519 " + base64.b64encode(bytes(51)).decode(), KEY[:-4]):
            with self.subTest(kind=type(key).__name__), self.assertRaises(ValueError):
                headless.public_key(key)

    def test_independent_write_identity_and_existing_pin_must_match(self):
        value = receipt()
        self.assertEqual(headless.verify_receipt(value, instance_id=INSTANCE, image_sha256=IMAGE,
                                                fingerprint=value["host_fingerprint"]), value)
        for params in ({"instance_id": "22222222-2222-4222-8222-222222222222", "image_sha256": IMAGE},
                       {"instance_id": INSTANCE, "image_sha256": "b" * 64},
                       {"instance_id": INSTANCE, "image_sha256": IMAGE, "fingerprint": "changed"}):
            with self.subTest(params=params), self.assertRaises(ValueError):
                headless.verify_receipt(value, **params)

    def test_bad_json_depth_encoding_and_size_fail(self):
        for raw in (b"\xff", b"[" * 2000 + b"]" * 2000, b" " * (headless.MAX_INPUT + 1), b"{"):
            with self.subTest(size=len(raw)), self.assertRaises(ValueError):
                headless.parse_json(raw)

    def test_publish_and_retry_preserve_same_identity_and_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(headless.publish_receipt(directory, receipt()), "created")
            path = Path(directory) / headless.RECEIPT_NAME
            original = path.read_bytes()
            self.assertEqual(headless.publish_receipt(directory, receipt()), "unchanged")
            self.assertEqual(path.read_bytes(), original)
            changed = headless.make_receipt(INSTANCE, "qpi-other", IMAGE, KEY)
            with self.assertRaises(ValueError):
                headless.publish_receipt(directory, changed)
            self.assertEqual(path.read_bytes(), original)

    def test_interrupted_partial_receipt_never_replaced_on_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / headless.RECEIPT_NAME
            path.write_bytes(b'{"schema":')
            with self.assertRaises(ValueError):
                headless.publish_receipt(directory, receipt())
            self.assertEqual(path.read_bytes(), b'{"schema":')

    def test_flush_failure_preserves_file_and_does_not_announce_completion(self):
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(headless.os, "fsync", side_effect=OSError("fixture interruption")):
            with self.assertRaises(OSError):
                headless.publish_receipt(directory, receipt())
            self.assertTrue((Path(directory) / headless.RECEIPT_NAME).is_file())

    def test_nonregular_and_oversized_files_are_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            with self.assertRaises(ValueError):
                headless.read_public(path)
            file = path / "oversized.json"
            file.write_bytes(b"x" * (headless.MAX_INPUT + 1))
            with self.assertRaises(ValueError):
                headless.read_public(file)
            with self.assertRaises(ValueError):
                headless.publish_receipt(file, receipt())

    def test_windows_reparse_directory_and_public_file_are_refused(self):
        with patch.object(Path, "lstat", return_value=SimpleNamespace(st_mode=stat.S_IFDIR, st_file_attributes=0x400)):
            with self.assertRaises(ValueError):
                headless.publish_receipt("not-opened", receipt())
        with patch.object(Path, "lstat", return_value=SimpleNamespace(st_mode=stat.S_IFREG, st_size=1, st_file_attributes=0x400)):
            with self.assertRaises(ValueError):
                headless.read_public("not-opened")

    def test_cli_never_echoes_unexpected_private_data(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input.json"
            path.write_text(json.dumps({"password": "do-not-log"}))
            output = io.StringIO()
            with redirect_stdout(output):
                code = headless.main(["--receipt", str(path), "--instance-id", INSTANCE, "--image-sha256", IMAGE])
            self.assertEqual(code, 2)
            self.assertNotIn("do-not-log", output.getvalue())
            self.assertFalse(json.loads(output.getvalue())["network_used"])

    def test_valid_cli_receipt_does_not_claim_boot_or_authenticated_access(self):
        with tempfile.TemporaryDirectory() as directory:
            headless.publish_receipt(directory, receipt())
            output = io.StringIO()
            with redirect_stdout(output):
                code = headless.main(["--receipt", str(Path(directory) / headless.RECEIPT_NAME),
                                      "--instance-id", INSTANCE, "--image-sha256", IMAGE])
            self.assertEqual(code, 0)
            result = json.loads(output.getvalue())
            self.assertFalse(result["authenticated_access_verified"])
            self.assertIn("native first-boot completion", result["not_proved"])


class ConnectionPlanTests(unittest.TestCase):
    def plan(self, **changes):
        params = {"receipt": receipt(), "address": "192.168.50.20", "user": "qpiowner",
                  "identity_file": "C:/Q Pi Access/client", "known_hosts_file": "C:/Q Pi Access/known_hosts"}
        params.update(changes)
        return headless.connection_plan(**params)

    def test_plan_is_fixed_argv_with_strict_host_pin_and_no_execution(self):
        result = self.plan()
        argv = result["argv"]
        self.assertEqual(argv[:3], [headless.SSH, "-F", "none"])
        self.assertEqual(argv[-1], headless.PROBE)
        for option in ("StrictHostKeyChecking=yes", "IdentityAgent=none", "ProxyCommand=none", "ProxyJump=none",
                       "ForwardAgent=no", "ClearAllForwardings=yes", "ControlMaster=no", "ControlPath=none",
                       "PasswordAuthentication=no", "BatchMode=yes", "HostKeyAlgorithms=ssh-ed25519",
                       "GlobalKnownHostsFile=NUL", "UpdateHostKeys=no", "VerifyHostKeyDNS=no"):
            self.assertIn(option, argv)
        self.assertIn('UserKnownHostsFile="C:/Q Pi Access/known_hosts"', argv)
        self.assertEqual(result["known_hosts_line"], "qpi-" + INSTANCE + " " + KEY + "\n")
        self.assertEqual(result["required_wall_timeout_seconds"], 30)
        self.assertFalse(result["network_used"])
        self.assertFalse(result["authenticated_access_verified"])

    def test_only_one_explicit_private_ipv4_endpoint_is_accepted(self):
        for address in ("127.0.0.1", "169.254.1.1", "224.0.0.1", "8.8.8.8", "192.0.2.2", "::1",
                        "qpi-fixture.local", "192.168.1.1/24", "-oProxyCommand=bad", "192.168.1.1\n", None):
            with self.subTest(address=address), self.assertRaises(ValueError):
                self.plan(address=address)

    def test_root_default_account_and_user_injection_are_refused(self):
        for user in ("root", "pi", "User", "a@host", "-lroot", "a\nProxyCommand", "a;cmd", "a" * 32, None):
            with self.subTest(user=user), self.assertRaises(ValueError):
                self.plan(user=user)

    def test_paths_cannot_expand_tokens_add_trust_files_or_inject_options(self):
        for path in ("C:/keys/%h", "C:/keys/${VAR}", 'C:/keys/"bad"', "C:/keys/known\nextra", "C:/keys/../other",
                     "C:/keys/one:two", "C:/keys/NUL", "C:/keys/CON.txt", "C:/keys/file.", "C:/keys//file",
                     "//server/share/key", "relative/key", "C:\\keys\\key", "C:/keys/a;cmd", None):
            for field in ("identity_file", "known_hosts_file"):
                with self.subTest(path=path, field=field), self.assertRaises(ValueError):
                    self.plan(**{field: path})
        with self.assertRaises(ValueError):
            self.plan(identity_file="C:/Q Pi Access/known_hosts")

    def test_plan_cli_requires_all_fields_and_existing_fingerprint(self):
        for extras in (["--address", "192.168.1.2"], ["--address", "192.168.1.2", "--user", "owner",
                       "--identity-file", "C:/keys/client", "--known-hosts-file", "C:/keys/hosts"]):
            with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as caught:
                headless.main(["--receipt", "not-read.json", "--instance-id", INSTANCE, "--image-sha256", IMAGE, *extras])
            self.assertEqual(caught.exception.code, 2)

    def test_remote_probe_fails_fast_and_valid_output_stays_scoped(self):
        self.assertEqual(headless.PROBE, "uname -m && getconf PAGE_SIZE && id -u")
        result = headless.verify_probe_output(0, b"aarch64\n16384\n1000\n")
        self.assertEqual(result["uid"], 1000)
        self.assertFalse(result["native_build_qualified"])

    def test_failed_wrong_root_or_polluted_remote_probe_is_refused(self):
        for code, raw in ((1, b"aarch64\n16384\n1000\n"), (False, b"aarch64\n16384\n1000\n"),
                          (0, b"aarch64\n16384\n0\n"), (0, b"x86_64\n16384\n1000\n"),
                          (0, b"aarch64\n4096\n1000\n"), (0, b"banner\naarch64\n16384\n1000\n"),
                          (0, b"aarch64\n16384\n4294967296\n"), (0, b"aarch64\n16384\n01\n"),
                          (0, b"x" * 1025), (0, b"\xff")):
            with self.subTest(code=code, size=len(raw)), self.assertRaises(ValueError):
                headless.verify_probe_output(code, raw)


if __name__ == "__main__":
    unittest.main()
