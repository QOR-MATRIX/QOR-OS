# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Native coordinator contracts; real flock cases explicitly require Linux.

Portable mocks do not prove Linux, ARM64, Pi or resource qualification.
Every real lock used here belongs to an ordinary temporary fixture directory.
"""

from contextlib import redirect_stderr, redirect_stdout
import errno
import importlib.util
import io
import os
from pathlib import Path
import signal
import stat
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import MagicMock, Mock, patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("qor_native_heavy", ROOT / "tools/dev/heavy.py")
heavy = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(heavy)
LINUX = sys.platform.startswith("linux")


def info(mode=stat.S_IFREG | 0o600, uid=1000, links=1, size=0, inode=12):
    return SimpleNamespace(st_mode=mode, st_uid=uid, st_nlink=links, st_size=size, st_dev=1, st_ino=inode)


class PolicyTests(unittest.TestCase):
    def test_file_requires_owner_single_link_regular_private_and_empty(self):
        heavy.file_policy(info(), 1000, empty=True)
        for item in (info(uid=2), info(links=2), info(size=1), info(mode=stat.S_IFLNK | 0o600),
                     info(mode=stat.S_IFIFO | 0o600), info(mode=stat.S_IFREG | 0o640),
                     info(mode=stat.S_IFCHR | 0o600), info(mode=stat.S_IFREG | 0o1600)):
            with self.subTest(item=item), self.assertRaises(heavy.Refused):
                heavy.file_policy(item, 1000, empty=True)

    def test_directory_ownership_permissions_and_sticky_ancestor(self):
        heavy.directory_policy(info(stat.S_IFDIR | 0o755, uid=0), 1000)
        heavy.directory_policy(info(stat.S_IFDIR | 0o1777, uid=0), 1000)
        heavy.directory_policy(info(stat.S_IFDIR | 0o700), 1000, user=True, private=True)
        for item, private in ((info(stat.S_IFDIR | 0o775), False), (info(uid=1000), False),
                              (info(stat.S_IFDIR | 0o755, uid=2), False),
                              (info(stat.S_IFDIR | 0o755), True),
                              (info(stat.S_IFDIR | 0o1777, uid=0), True)):
            with self.subTest(item=item), self.assertRaises(heavy.Refused):
                heavy.directory_policy(item, 1000, user=True, private=private)

    def test_nonlinux_and_wsl_fail_before_account_or_lock(self):
        for system, release, name in (("Windows", "normal", "nt"), ("Darwin", "normal", "posix"),
                                       ("Linux", "microsoft-standard", "posix"),
                                       ("Linux", "WSL2", "posix")):
            with self.subTest(system=system), patch.object(heavy, "os", SimpleNamespace(name=name)), \
                    patch.object(heavy.platform, "system", return_value=system), \
                    patch.object(heavy.platform, "release", return_value=release), self.assertRaises(heavy.Refused):
                heavy.native_identity()

    def test_passwd_home_not_environment_or_cwd(self):
        account = Mock(return_value=SimpleNamespace(pw_dir="/home/qor"))
        fake = SimpleNamespace(name="posix", geteuid=lambda: 1000, getuid=lambda: 1000)
        with patch.object(heavy, "os", fake), patch.object(heavy.platform, "system", return_value="Linux"), \
                patch.object(heavy.platform, "release", return_value="native"), \
                patch.dict(sys.modules, {"pwd": SimpleNamespace(getpwuid=account)}), \
                patch.dict(os.environ, {"HOME": "/wrong", "XDG_STATE_HOME": "/other"}):
            self.assertEqual(heavy.native_identity(), ("/home/qor", 1000))
            account.assert_called_once_with(1000)
            for bad in ("relative", "//home/qor", "/home/../wrong", "/home//qor", "/home/qor/", "/home/\0x"):
                account.return_value.pw_dir = bad
                with self.assertRaises(heavy.Refused):
                    heavy.native_identity()
            fake.geteuid = lambda: 0
            with self.assertRaises(heavy.Refused):
                heavy.native_identity()

    def test_argument_errors_do_not_echo_input(self):
        for argv in (["--private-value=do-not-print"], ["--timeout-seconds", "do-not-print"],
                     ["--check", "do-not-print"], [], ["--timeout-seconds", "0", "--", "job"],
                     ["--timeout-seconds", "86401", "--", "job"], ["--", "x" * 32769]):
            output = io.StringIO()
            with redirect_stderr(output), patch.object(heavy, "Lease") as lease:
                self.assertEqual(heavy.main(argv), 2)
            lease.assert_not_called()
            self.assertNotIn("do-not-print", output.getvalue())
            self.assertLess(len(output.getvalue()), 256)

    def test_missing_home_or_permission_error_is_sanitised(self):
        for error in (OSError("private-path"), KeyError("private-account"), ValueError("private-value")):
            output = io.StringIO()
            with patch.object(heavy, "native_identity", side_effect=error), redirect_stderr(output):
                self.assertEqual(heavy.main(["--check"]), 2)
            self.assertNotIn("private-", output.getvalue())


class LeaseMethodTests(unittest.TestCase):
    def lease(self):
        obj = heavy.Lease.__new__(heavy.Lease)
        obj.uid, obj.fd, obj.parent = 1000, 10, 8
        obj.fds, obj.chain, obj.marker = [7, 8], [], info(size=len(heavy.MARKER), inode=22)
        return obj

    def test_close_never_unlocks_unlinks_or_rewrites(self):
        obj = self.lease()
        fake = SimpleNamespace(close=Mock(), unlink=Mock())
        obj.fcntl = SimpleNamespace(flock=Mock())
        with patch.object(heavy, "os", fake):
            obj.close()
            obj.close()
        self.assertEqual([c.args[0] for c in fake.close.call_args_list], [10, 8, 7])
        fake.unlink.assert_not_called()
        obj.fcntl.flock.assert_not_called()

    def test_named_lock_replacement_and_hardlinks_refused(self):
        obj = self.lease()
        def held(fd):
            return info(stat.S_IFDIR | 0o700) if fd == 7 else info()
        with patch.object(heavy, "os", SimpleNamespace(fstat=Mock(side_effect=held), stat=Mock(return_value=info(inode=99)))):
            with self.assertRaises(heavy.Refused):
                obj.verify()
        with patch.object(heavy, "os", SimpleNamespace(fstat=Mock(side_effect=held), stat=Mock(return_value=info(links=2)))):
            with self.assertRaises(heavy.Refused):
                obj.verify()

    def test_ancestor_replacement_refused(self):
        obj = self.lease()
        obj.chain = [(7, "qor-os", 8, True, True)]
        with patch.object(heavy, "os", SimpleNamespace(fstat=Mock(return_value=info(stat.S_IFDIR | 0o700)),
                  stat=Mock(return_value=info(stat.S_IFDIR | 0o700, inode=99)))):
            with self.assertRaises(heavy.Refused):
                obj.verify()

    def test_same_inode_directory_permission_or_owner_change_refused(self):
        for changed in (info(stat.S_IFDIR | 0o775), info(stat.S_IFDIR | 0o700, uid=2),
                        info(stat.S_IFDIR | 0o755)):
            obj = self.lease()
            obj.chain = [(7, "qor-os", 8, True, True)]
            with patch.object(heavy, "os", SimpleNamespace(fstat=Mock(return_value=info(stat.S_IFDIR | 0o700)),
                      stat=Mock(return_value=changed), open=Mock())) as fake, self.assertRaises(heavy.Refused):
                obj.start()
            fake.open.assert_not_called()

    def test_marker_persisted_before_return(self):
        obj = self.lease()
        obj.verify = Mock()
        fake = Mock(O_WRONLY=1, O_CREAT=64, O_EXCL=128, O_NOFOLLOW=256, O_CLOEXEC=512)
        fake.open.return_value = 20
        fake.fstat.return_value = info()
        fake.write.return_value = len(heavy.MARKER)
        with patch.object(heavy, "os", fake):
            obj.start()
        self.assertEqual([c.args[0] for c in fake.fsync.call_args_list], [20, 8])
        self.assertEqual(fake.write.call_args.args, (20, heavy.MARKER))
        fake.close.assert_called_once_with(20)

    def test_failed_marker_write_or_flush_never_finishes(self):
        for short in (True, False):
            obj = self.lease()
            obj.verify = Mock()
            fake = Mock(O_WRONLY=1, O_CREAT=64, O_EXCL=128, O_NOFOLLOW=256, O_CLOEXEC=512)
            fake.open.return_value = 20
            fake.fstat.return_value = info()
            fake.write.return_value = len(heavy.MARKER) - int(short)
            if not short:
                fake.fsync.side_effect = OSError("disk error")
            with patch.object(heavy, "os", fake), self.assertRaises((heavy.Refused, OSError)):
                obj.start()
            fake.unlink.assert_not_called()
            fake.close.assert_called_once_with(20)

    def test_finish_checks_identity_before_removal_and_syncs_directory(self):
        obj = self.lease()
        obj.verify = Mock()
        fake = SimpleNamespace(stat=Mock(return_value=obj.marker), unlink=Mock(), fsync=Mock())
        with patch.object(heavy, "os", fake):
            obj.finish()
        fake.unlink.assert_called_once_with(heavy.ACTIVE, dir_fd=8)
        fake.fsync.assert_called_once_with(8)
        obj.marker = info(inode=99)
        fake.unlink.reset_mock()
        with patch.object(heavy, "os", fake), self.assertRaises(heavy.Refused):
            obj.finish()
        fake.unlink.assert_not_called()


class AdmissionTests(unittest.TestCase):
    def test_check_uses_existing_lease_and_existing_native_ram_reader(self):
        lease = MagicMock()
        factory = Mock(return_value=lease)
        with patch.object(heavy, "native_identity", return_value=("/home/qor", 1000)), \
                patch.object(heavy, "Lease", factory), patch.object(heavy, "native_headroom") as ram, \
                patch.object(heavy, "run_job") as job, redirect_stdout(io.StringIO()):
            self.assertEqual(heavy.main(["--check"]), 0)
        factory.assert_called_once_with("/home/qor", 1000, create=False)
        ram.assert_called_once_with()
        job.assert_not_called()
        lease.__exit__.assert_called_once()

    def test_ram_failure_after_lock_starts_no_child(self):
        lease = unittest.mock.MagicMock()
        with patch.object(heavy, "native_identity", return_value=("/home/qor", 1000)), \
                patch.object(heavy, "Lease", return_value=lease), \
                patch.object(heavy.vm_resources, "require_headroom", side_effect=ValueError("guard")), \
                patch.object(heavy, "run_job") as job, redirect_stderr(io.StringIO()):
            self.assertEqual(heavy.main(["--", "job"]), 2)
        job.assert_not_called()
        lease.__exit__.assert_called_once()

    def test_busy_never_retries_or_calls_ram_or_job(self):
        with patch.object(heavy, "native_identity", return_value=("/home/qor", 1000)), \
                patch.object(heavy, "Lease", side_effect=heavy.Busy("busy")) as lease, \
                patch.object(heavy.vm_resources, "require_headroom") as ram, \
                patch.object(heavy, "run_job") as job, redirect_stderr(io.StringIO()):
            self.assertEqual(heavy.main(["--", "job"]), 75)
        lease.assert_called_once()
        ram.assert_not_called()
        job.assert_not_called()

    def test_real_resource_floor_is_unchanged(self):
        for available in (0, 3072 * 1024**2 - 1):
            with self.assertRaises(ValueError):
                heavy.vm_resources.require_headroom("fixture", reader=lambda: available)
        self.assertEqual(heavy.vm_resources.require_headroom("fixture", reader=lambda: 3072 * 1024**2)
                         ["minimum_bytes"], 3072 * 1024**2)
        with self.assertRaises(ValueError):
            heavy.vm_resources.require_headroom("fixture", minimum=3072 * 1024**2 - 1)

    def test_exact_resource_shortfall_without_raw_error_or_second_observation(self):
        output = io.StringIO()
        with patch.object(heavy, "native_identity", return_value=("/home/qor", 1000)), \
                patch.object(heavy, "Lease", return_value=MagicMock()), \
                patch.object(heavy.vm_resources, "linux_available_memory", return_value=3071 * 1024**2) as read, \
                patch.object(heavy, "run_job") as job, redirect_stderr(output):
            self.assertEqual(heavy.main(["--", "job"]), 2)
        read.assert_called_once_with()
        job.assert_not_called()
        self.assertIn("shortfall 1048576 bytes", output.getvalue())
        self.assertIn("3072 MiB", output.getvalue())
        with patch.object(heavy.vm_resources, "linux_available_memory", side_effect=OSError("private-value")):
            with self.assertRaisesRegex(heavy.Refused, "observation unavailable") as error:
                heavy.native_headroom()
        self.assertNotIn("private-value", str(error.exception))


class SupervisionTests(unittest.TestCase):
    def run_mock(self, code=0, drain=(True, False), error=None, waits=None):
        lease = Mock(fd=10)
        child = Mock(pid=42)
        child.wait.side_effect = waits
        child.wait.return_value = code
        signals = SimpleNamespace(SIGINT=2, SIGTERM=15, SIGHUP=1, SIGKILL=9,
                                  getsignal=Mock(return_value=0), signal=Mock())
        spawn = Mock(return_value=child, side_effect=error)
        with patch.object(heavy, "subreaper"), patch.object(heavy, "signal", signals), \
                patch.object(heavy.subprocess, "Popen", spawn), patch.object(heavy, "drain_children", return_value=drain):
            try:
                result = heavy.run_job(lease, ["job", "literal ; $(x)", "space value"], 30)
            except heavy.Refused:
                result = "refused"
        return result, lease, spawn, signals

    def test_literal_argv_no_shell_and_inherited_descriptor(self):
        result, lease, spawn, signals = self.run_mock()
        self.assertEqual(result, 0)
        spawn.assert_called_once_with(["job", "literal ; $(x)", "space value"], shell=False,
                                     stdin=subprocess.DEVNULL, start_new_session=True, pass_fds=(10,))
        lease.start.assert_called_once()
        lease.finish.assert_called_once()
        self.assertEqual(signals.signal.call_count, 6)

    def test_failure_and_signal_exit_codes_not_masked(self):
        for code, expected in ((23, 23), (-9, 137), (-15, 143)):
            self.assertEqual(self.run_mock(code=code)[0], expected)

    def test_adopted_child_failure_is_not_success(self):
        self.assertEqual(self.run_mock(drain=(True, True))[0], 1)

    def test_surviving_descendant_keeps_incomplete_marker(self):
        result, lease, _, _ = self.run_mock(drain=(False, False))
        self.assertEqual(result, "refused")
        lease.finish.assert_not_called()

    def test_spawn_failure_cleanup_requires_no_children(self):
        for quiet in (True, False):
            result, lease, _, _ = self.run_mock(drain=(quiet, False), error=OSError("private command"))
            self.assertEqual(result, "refused")
            self.assertEqual(lease.finish.call_count, int(quiet))

    def test_subreaper_failure_prevents_marker_and_spawn(self):
        lease = Mock()
        with patch.object(heavy, "subreaper", side_effect=heavy.Refused("unsupported")), \
                patch.object(heavy.subprocess, "Popen") as spawn, self.assertRaises(heavy.Refused):
            heavy.run_job(lease, ["job"], 30)
        lease.start.assert_not_called()
        spawn.assert_not_called()

    def test_stop_only_owned_unreaped_group_and_no_late_group_kill(self):
        child = Mock(pid=42)
        child.wait.side_effect = [subprocess.TimeoutExpired("fixture", 5), -9]
        fake_os = SimpleNamespace(killpg=Mock())
        with patch.object(heavy, "os", fake_os), patch.object(heavy, "signal", SimpleNamespace(SIGTERM=15, SIGKILL=9)):
            self.assertEqual(heavy.stop_group(child), -9)
        self.assertEqual([c.args for c in fake_os.killpg.call_args_list], [(42, 15), (42, 9)])
        child.wait.side_effect = None
        child.wait.return_value = 0
        fake_os.killpg.reset_mock()
        with patch.object(heavy, "os", fake_os), patch.object(heavy, "signal", SimpleNamespace(SIGTERM=15, SIGKILL=9)):
            heavy.stop_group(child)
        self.assertEqual(fake_os.killpg.call_count, 1)

    def test_drain_never_signals_old_process_group(self):
        fake = SimpleNamespace(waitpid=Mock(side_effect=[(8, 0), ChildProcessError()]), WNOHANG=1,
                               waitstatus_to_exitcode=lambda x: x, killpg=Mock())
        with patch.object(heavy, "os", fake):
            self.assertEqual(heavy.drain_children(0), (True, False))
        fake.killpg.assert_not_called()
        fake.waitpid.side_effect = [(0, 0)]
        with patch.object(heavy, "os", fake):
            self.assertEqual(heavy.drain_children(0), (False, False))

    def test_deadline_stops_owned_group_and_clears_only_after_drain(self):
        with patch.object(heavy.time, "monotonic", side_effect=[0, 31, 31]), \
                patch.object(heavy, "stop_group") as stop:
            result, lease, _, _ = self.run_mock(waits=[subprocess.TimeoutExpired("fixture", 0.1)])
        self.assertEqual(result, 124)
        stop.assert_called_once()
        lease.finish.assert_called_once()

    def test_repeated_interrupts_preserve_first_signal(self):
        lease = Mock(fd=10)
        handlers = {}
        child = Mock(pid=42)
        def wait(timeout):
            handlers[15](15, None)
            handlers[2](2, None)
            raise subprocess.TimeoutExpired("fixture", timeout)
        child.wait.side_effect = wait
        signals = SimpleNamespace(SIGINT=2, SIGTERM=15, SIGHUP=1,
                                  getsignal=lambda x: 0, signal=lambda sig, cb: handlers.update({sig: cb}))
        with patch.object(heavy, "subreaper"), patch.object(heavy, "signal", signals), \
                patch.object(heavy.subprocess, "Popen", return_value=child), \
                patch.object(heavy, "stop_group") as stop, \
                patch.object(heavy, "drain_children", return_value=(True, False)):
            self.assertEqual(heavy.run_job(lease, ["job"], 30), 143)
        stop.assert_called_once_with(child)
        lease.finish.assert_called_once()
        self.assertEqual(set(handlers.values()), {0})


@unittest.skipUnless(LINUX, "requires real Linux flock/dir_fd/process supervision; mocks are separate")
class LinuxLockTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="qor-heavy-fixture-")
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        self.uid = os.geteuid()
        self.local = patch.object(heavy.vm_resources, "local_disk_directory", side_effect=lambda p: Path(p))
        self.local.start()
        self.addCleanup(self.local.stop)

    def lease(self, create=True):
        return heavy.Lease(str(self.home), self.uid, create=create)

    def test_busy_and_stale_holder_note_do_not_split_lock(self):
        with self.lease() as first:
            (first.folder / "heavy.holder").write_text("stale synthetic fixture")
            with self.assertRaises(heavy.Busy):
                self.lease()
        with self.lease(create=False):
            pass

    def test_readonly_check_never_creates_state(self):
        with self.assertRaises((heavy.Refused, FileNotFoundError)):
            self.lease(create=False)
        self.assertEqual(list(self.home.iterdir()), [])

    def test_incomplete_marker_blocks_after_parent_closes(self):
        with self.lease() as first:
            first.start()
        with self.assertRaises(heavy.Refused):
            self.lease()

    def test_lock_symlink_hardlink_and_ancestor_symlink_refused(self):
        with self.lease() as first:
            folder = first.folder
        lock = folder / heavy.LOCK
        lock.unlink()  # Fixture only; production never removes this inode.
        other = self.home / "other"
        other.touch(mode=0o600)
        lock.symlink_to(other)
        with self.assertRaises(heavy.Refused):
            self.lease()
        lock.unlink()
        os.link(other, lock)
        with self.assertRaises(heavy.Refused):
            self.lease()
        lock.unlink()
        moved = self.home / "moved"
        folder.rename(moved)
        folder.symlink_to(moved, target_is_directory=True)
        with self.assertRaises(OSError):
            self.lease()

    def test_replaced_lock_refused_without_touching_replacement(self):
        with self.lease() as first:
            lock = first.folder / heavy.LOCK
            lock.unlink()
            lock.touch(mode=0o600)
            with self.assertRaises(heavy.Refused):
                first.verify()

    def test_failed_flush_leaves_incomplete_marker(self):
        with self.lease() as first:
            with patch.object(heavy.os, "fsync", side_effect=OSError("fixture")), self.assertRaises(OSError):
                first.start()
        with self.assertRaises(heavy.Refused):
            self.lease()

    def test_inherited_lock_survives_parent_close(self):
        first = self.lease()
        child = subprocess.Popen([sys.executable, "-c", "import sys; sys.stdin.buffer.read(1)"],
                                 stdin=subprocess.PIPE, pass_fds=(first.fd,))
        try:
            first.close()
            with self.assertRaises(heavy.Busy):
                self.lease()
        finally:
            child.communicate(b"x", timeout=10)
            first.close()
        with self.lease():
            pass

    def launcher(self, code, seconds=5, deadline_file=""):
        # Dedicated temporary fixture process: real flock/prctl/waitpid/signals,
        # no production lock, RAM admission or local-filesystem qualification.
        script = """
import importlib.util, os, pathlib, sys, time, types
spec = importlib.util.spec_from_file_location('fixture_heavy', sys.argv[1])
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
h.vm_resources.local_disk_directory = lambda p: pathlib.Path(p)
if sys.argv[5]:
    # A controlled deadline starts only after the test proves the orphan lives.
    # Real process/lock syscalls remain active; this case does not test wall time.
    base = time.monotonic()
    trigger = pathlib.Path(sys.argv[5])
    h.time = types.SimpleNamespace(monotonic=lambda: base + (100 if trigger.exists() else 0), sleep=time.sleep)
try:
    with h.Lease(sys.argv[2], os.geteuid(), create=True) as lease:
        code = sys.argv[3].replace('FIXTURE_LOCK_FD', str(lease.fd))
        result = h.run_job(lease, [sys.executable, '-c', code], float(sys.argv[4]))
except h.Refused:
    result = 2
sys.exit(result)
"""
        process = subprocess.Popen([sys.executable, "-B", "-c", script, str(ROOT / "tools/dev/heavy.py"),
                                    str(self.home), code, str(seconds), str(deadline_file)], stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        def cleanup():
            if process.poll() is None:
                process.kill()  # Only the fixture process created immediately above.
            process.communicate(timeout=10)  # All fixture descendants have bounded lifetimes.
        self.addCleanup(cleanup)
        return process

    def ready(self, process):
        import select
        self.assertTrue(select.select([process.stdout], [], [], 5)[0], "fixture ready timeout")
        self.assertEqual(process.stdout.readline().strip(), "ready")

    def test_subreaper_waits_for_fd_closing_orphan(self):
        release = self.home / "release-orphan"
        orphan = ("import pathlib, time; print('ready', flush=True); end=time.monotonic()+5\n"
                  f"while not pathlib.Path({str(release)!r}).exists() and time.monotonic()<end: time.sleep(0.02)\n")
        process = self.launcher(f"import subprocess, sys; subprocess.Popen([sys.executable, '-c', {orphan!r}], close_fds=True)")
        self.ready(process)
        with self.assertRaises(subprocess.TimeoutExpired):
            process.wait(timeout=0.2)
        with self.assertRaises(heavy.Busy):
            self.lease()
        release.touch()
        output, errors = process.communicate(timeout=10)
        self.assertEqual((process.returncode, errors), (0, ""))
        self.assertEqual(output, "")
        with self.lease():
            pass  # Marker removed only after the orphan was adopted and reaped.

    def test_sigterm_stops_fixture_group_and_releases_completed_marker(self):
        process = self.launcher("import time; print('ready', flush=True); time.sleep(5)")
        self.ready(process)
        process.send_signal(signal.SIGTERM)
        _, errors = process.communicate(timeout=10)
        self.assertEqual((process.returncode, errors), (143, ""))
        with self.lease():
            pass

    def test_sigkill_leaves_marker_after_fd_closing_child_exits(self):
        process = self.launcher("import os, time; os.close(FIXTURE_LOCK_FD); "
                                "print('ready', flush=True); time.sleep(0.4)")
        self.ready(process)
        process.kill()
        process.communicate(timeout=10)  # Pipe EOF also waits for the bounded child to exit.
        self.assertEqual(process.returncode, -signal.SIGKILL)
        with self.assertRaisesRegex(heavy.Refused, "incomplete native job"):
            self.lease()

    def test_deadline_keeps_marker_for_escaped_fd_closing_descendant(self):
        release, trigger = self.home / "release-escaped", self.home / "trigger-deadline"
        orphan = ("import pathlib, time; print('ready', flush=True); end=time.monotonic()+5\n"
                  f"while not pathlib.Path({str(release)!r}).exists() and time.monotonic()<end: time.sleep(0.02)\n")
        process = self.launcher(f"import subprocess, sys, time; subprocess.Popen([sys.executable, '-c', {orphan!r}], "
                                "start_new_session=True, close_fds=True); time.sleep(5)", seconds=30, deadline_file=trigger)
        self.ready(process)
        trigger.touch()
        self.assertEqual(process.wait(timeout=5), 2)
        with self.assertRaisesRegex(heavy.Refused, "incomplete native job"):
            self.lease()
        release.touch()
        _, errors = process.communicate(timeout=10)
        self.assertEqual(errors, "")


if __name__ == "__main__":
    unittest.main()
