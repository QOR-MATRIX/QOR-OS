# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Linux-only inert subprocess tests; no bootc, VM or block device is opened."""
import importlib
import gc
import io
import os
import signal
from pathlib import Path
from types import SimpleNamespace
import subprocess
import sys
import tempfile
import time
import unittest
import warnings
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
backend = importlib.import_module("vm_install_backend")
guard = importlib.import_module("vmguard")
container = importlib.import_module("vm_container")
session = importlib.import_module("vm_session")


@unittest.skipUnless(sys.platform == "linux", "POSIX transaction lifetime checks require Linux")
class Transaction(unittest.TestCase):
    def setUp(self):
        self.warning_context = warnings.catch_warnings(record=True)
        self.observed = self.warning_context.__enter__()
        warnings.simplefilter("always", ResourceWarning)

    def tearDown(self):
        try:
            gc.collect()
            self.assertFalse([item for item in self.observed if issubclass(item.category, ResourceWarning)])
        finally:
            self.warning_context.__exit__(None, None, None)

    def test_exit_failure_is_not_success(self):
        log = io.BytesIO()
        code = backend.transaction([sys.executable, "-c", "print('fixture failure');raise SystemExit(7)"], log, timeout=10)
        self.assertEqual(code, 7)
        self.assertIn(b"fixture failure", log.getvalue())

    def test_refused_supervision_preserves_the_signal_handler(self):
        previous = signal.getsignal(signal.SIGTERM)
        with patch.object(backend.os, "getppid", return_value=1):
            with self.assertRaises(ValueError):
                backend.transaction([sys.executable, "-c", "raise SystemExit(99)"], io.BytesIO())
        self.assertIs(signal.getsignal(signal.SIGTERM), previous)

    def test_symlink_target_and_ancestry_cannot_be_opened(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory).resolve()
            template = workspace / "vars.fd"
            template.write_bytes(bytes(4096))
            root = workspace / "vm"
            guard.create(root, template, 1024**2, "a" * 40, "b" * 64, workspace)
            (root / "install.raw").rename(root / "preserved.raw")
            (root / "install.raw").symlink_to(root / "preserved.raw")
            with self.assertRaises(ValueError):
                guard.verify(root / "allowlist.json", workspace)
            with self.assertRaises(OSError):
                container.regular_fd(root / "install.raw", True)
            alias = workspace / "alias"
            alias.symlink_to(root, target_is_directory=True)
            with self.assertRaises(ValueError):
                guard.verify(alias / "allowlist.json", workspace)

    def test_payload_rejects_writable_inode_even_after_media_check(self):
        with tempfile.TemporaryDirectory() as directory:
            media = Path(directory).resolve()
            payload = media / "payload.tar"
            payload.write_bytes(b"inert payload fixture")
            with patch.object(backend, "MEDIA", media), \
                    patch.object(backend.os, "fstatvfs", return_value=SimpleNamespace(f_flag=0)):
                with self.assertRaises(ValueError):
                    backend.regular_source(payload)
            # The positive is a synthetic read-only-filesystem control, not
            # proof that this temporary fixture is an ISO or installable OS.
            with patch.object(backend, "MEDIA", media), \
                    patch.object(backend.os, "fstatvfs", return_value=SimpleNamespace(f_flag=os.ST_RDONLY)):
                with backend.regular_source(payload) as stream:
                    self.assertEqual(stream.read(), b"inert payload fixture")
                alias = media / "linked.tar"
                alias.symlink_to(payload)
                with self.assertRaises(OSError):
                    backend.regular_source(alias)
                os.link(payload, media / "hardlink.tar")
                with self.assertRaises(ValueError):
                    backend.regular_source(payload)

    def test_payload_refuses_linked_parent_or_outside_media(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory).resolve()
            media = workspace / "iso"
            media.mkdir()
            external = workspace / "other"
            external.mkdir()
            payload = external / "payload.tar"
            payload.write_bytes(b"inert fixture")
            (media / "qor").symlink_to(external, target_is_directory=True)
            with patch.object(backend, "MEDIA", media):
                for path in (payload, media / "qor/payload.tar"):
                    with self.subTest(path=path), self.assertRaises(ValueError):
                        backend.regular_source(path)

    def test_public_state_refuses_linked_ancestry_and_fifo(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            folder = root / "state"
            folder.mkdir()
            marker = folder / "completed.json"
            marker.write_bytes(session.RECORD)
            self.assertEqual(session.setup_observation(marker)["state"], "complete")
            (root / "alias").symlink_to(folder, target_is_directory=True)
            self.assertEqual(session.setup_observation(root / "alias/completed.json"), {"state": "invalid"})
            fifo = root / "fifo"
            os.mkfifo(fifo)
            self.assertEqual(session.setup_observation(fifo), {"state": "invalid"})
            self.assertEqual(marker.read_bytes(), session.RECORD)

    def test_output_is_drained_and_bounded(self):
        log = io.BytesIO()
        code = backend.transaction([sys.executable, "-c", "import sys;sys.stdout.write('a'*3000000+'TAIL')"], log, timeout=10)
        self.assertEqual(code, 0)
        self.assertLess(len(log.getvalue()), 1200000)
        self.assertTrue(log.getvalue().endswith(b"TAIL"))

    def test_timeout_stops_descendant_before_it_can_write(self):
        with tempfile.TemporaryDirectory() as directory:
            unexpected = Path(directory) / "must-not-be-created"
            child = "import time;from pathlib import Path;time.sleep(2);Path(%r).write_text('unexpected')" % str(unexpected)
            parent = "import subprocess,sys,time;subprocess.Popen([sys.executable,'-c',%r]);time.sleep(10)" % child
            with self.assertRaises(subprocess.TimeoutExpired):
                backend.transaction([sys.executable, "-c", parent], io.BytesIO(), timeout=0.2)
            time.sleep(2.2)
            self.assertFalse(unexpected.exists())


if __name__ == "__main__":
    if sys.platform != "linux":
        raise SystemExit("Linux transaction suite was not run on this host")
    unittest.main()
