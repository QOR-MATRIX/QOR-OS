#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Native Linux QOR job coordination. Windows/WSL keep their original coordinator.

One designated build UID and shared host mount namespace, trusted foreground
commands only. Advisory coordination is not a sandbox against that UID or root.
Never delete the permanent lock. An incomplete marker requires reviewed recovery.
"""

import argparse
import ctypes
import errno
import os
from pathlib import Path, PurePosixPath
import platform
import signal
import stat
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "image"))
import vm_resources

LOCK = "heavy.lock"
ACTIVE = "heavy.active"
MARKER = b"QOR heavy job incomplete; review native workers before recovery.\n"
MAX_SECONDS = 86400


class Refused(Exception):
    """Sanitised operational refusal; never include supplied argv or paths."""


class Busy(Refused):
    pass


def native_identity():
    if (os.name != "posix" or platform.system() != "Linux"
            or any(x in platform.release().lower() for x in ("microsoft", "wsl"))):
        raise Refused("native Linux required; retain the original Windows/WSL coordinator")
    import pwd
    uid = os.geteuid()
    if uid == 0 or os.getuid() != uid:
        raise Refused("use the designated ordinary native build account")
    home = pwd.getpwuid(uid).pw_dir  # HOME/XDG/cwd must not split the lock domain.
    parsed = PurePosixPath(home)
    if (parsed.root != "/" or str(parsed) != home or ".." in parsed.parts
            or len(parsed.parts) > 64 or len(home.encode("utf-8")) > 4096 or "\0" in home):
        raise Refused("unsupported account home")
    return home, uid


def directory_policy(info, uid, *, user=False, private=False):
    if not stat.S_ISDIR(info.st_mode) or info.st_uid not in ((uid,) if user else (0, uid)):
        raise Refused("untrusted coordination directory")
    mode = stat.S_IMODE(info.st_mode)
    # Root-owned sticky ancestors (e.g. /tmp in platform fixtures) protect entries.
    sticky_root = not user and info.st_uid == 0 and bool(mode & stat.S_ISVTX)
    if (mode & 0o022 and not sticky_root) or (private and mode != 0o700):
        raise Refused("unsafe coordination directory permissions")


def file_policy(info, uid, *, empty=False):
    if (not stat.S_ISREG(info.st_mode) or info.st_uid != uid or info.st_nlink != 1
            or stat.S_IMODE(info.st_mode) != 0o600 or (empty and info.st_size != 0)):
        raise Refused("unsafe coordination file")


def same_inode(a, b):
    return (a.st_dev, a.st_ino) == (b.st_dev, b.st_ino)


class Lease:
    """Private primitive; CLI uses only passwd-derived home, never a path override."""

    def __init__(self, home, uid, *, create):
        self.uid = uid
        self.fds = []
        self.chain = []
        self.fd = None
        self.marker = None
        self.folder = Path(home) / ".local/state/qor-os"
        try:
            import fcntl
            self.fcntl = fcntl
            vm_resources.local_disk_directory(home)
            flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC
            parent = os.open("/", flags)
            self.fds.append(parent)
            directory_policy(os.fstat(parent), uid)
            parts = PurePosixPath(home).parts[1:]
            for index, part in enumerate(parts):
                parent = self._directory(parent, part, flags, create=False, user=index == len(parts) - 1)
            directory_policy(os.fstat(parent), uid, user=True)
            for part in (".local", "state", "qor-os"):
                parent = self._directory(parent, part, flags, create=create,
                                         user=True, private=part == "qor-os")
            self.parent = parent
            vm_resources.local_disk_directory(self.folder)
            try:
                before = os.stat(LOCK, dir_fd=parent, follow_symlinks=False)
            except FileNotFoundError:
                if not create:
                    raise Refused("native coordination is not initialised") from None
                self.fd = os.open(LOCK, os.O_RDWR | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW
                                  | os.O_CLOEXEC | os.O_NONBLOCK, 0o600, dir_fd=parent)
                os.fsync(self.fd)
                os.fsync(parent)
            else:
                file_policy(before, uid, empty=True)
                self.fd = os.open(LOCK, (os.O_RDWR if create else os.O_RDONLY)
                                  | os.O_NOFOLLOW | os.O_CLOEXEC | os.O_NONBLOCK, dir_fd=parent)
                if not same_inode(before, os.fstat(self.fd)):
                    raise Refused("coordination file changed")
            self.verify()
            try:
                fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except OSError as error:
                if error.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                    raise Busy("another QOR job holds the native lock") from None
                raise Refused("native OS lock failed") from None
            self.verify()
            try:
                os.stat(ACTIVE, dir_fd=parent, follow_symlinks=False)
            except FileNotFoundError:
                pass
            else:
                raise Refused("incomplete native job; reviewed recovery required")
        except BaseException:
            self.close()
            raise

    def _directory(self, parent, name, flags, *, create, user=False, private=False):
        if create:
            try:
                os.mkdir(name, 0o700, dir_fd=parent)
                os.fsync(parent)
            except FileExistsError:
                pass
        fd = os.open(name, flags, dir_fd=parent)
        self.fds.append(fd)
        directory_policy(os.fstat(fd), self.uid, user=user, private=private)
        self.chain.append((parent, name, fd, user, private))
        return fd

    def verify(self):
        directory_policy(os.fstat(self.fds[0]), self.uid)
        for parent, name, fd, user, private in self.chain:
            observed = os.stat(name, dir_fd=parent, follow_symlinks=False)
            held = os.fstat(fd)
            directory_policy(held, self.uid, user=user, private=private)
            directory_policy(observed, self.uid, user=user, private=private)
            if not same_inode(observed, held):
                raise Refused("coordination directory changed")
        held = os.fstat(self.fd)
        named = os.stat(LOCK, dir_fd=self.parent, follow_symlinks=False)
        file_policy(held, self.uid, empty=True)
        file_policy(named, self.uid, empty=True)
        if not same_inode(held, named):
            raise Refused("coordination file changed")

    def start(self):
        self.verify()
        fd = os.open(ACTIVE, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW
                     | os.O_CLOEXEC, 0o600, dir_fd=self.parent)
        try:
            self.marker = os.fstat(fd)
            file_policy(self.marker, self.uid)
            if os.write(fd, MARKER) != len(MARKER):
                raise Refused("incomplete coordination marker write")
            os.fsync(fd)
            os.fsync(self.parent)
        finally:
            os.close(fd)

    def finish(self):
        self.verify()
        observed = os.stat(ACTIVE, dir_fd=self.parent, follow_symlinks=False)
        file_policy(observed, self.uid)
        if self.marker is None or not same_inode(self.marker, observed) or observed.st_size != len(MARKER):
            raise Refused("coordination marker changed")
        os.unlink(ACTIVE, dir_fd=self.parent)
        os.fsync(self.parent)
        self.marker = None

    def close(self):
        # Never LOCK_UN: it would also unlock surviving inherited descriptions.
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None
        while self.fds:
            os.close(self.fds.pop())

    def __enter__(self):
        return self

    def __exit__(self, *unused):
        self.close()


def subreaper():
    """Dedicated launcher process adopts orphan descendants; no service/privilege."""
    libc = ctypes.CDLL(None, use_errno=True)
    call = libc.prctl
    call.argtypes = [ctypes.c_int, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong]
    call.restype = ctypes.c_int
    if call(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        raise Refused("native child supervision unavailable")


def drain_children(deadline):
    failed = False
    while True:
        try:
            pid, status = os.waitpid(-1, os.WNOHANG)
        except ChildProcessError:
            return True, failed
        if pid:
            failed |= os.waitstatus_to_exitcode(status) != 0
        elif time.monotonic() >= deadline:
            return False, failed
        else:
            time.sleep(0.05)


def stop_group(process):
    # Called only for an unreaped child created with start_new_session=True.
    # Keeping it unreaped prevents this PID/PGID from being recycled meanwhile.
    for sig in (signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(process.pid, sig)
        except ProcessLookupError:
            pass
        try:
            return process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass
    raise Refused("job did not stop; incomplete marker retained")


def run_job(lease, command, seconds):
    """Only from the dedicated CLI, not a process with unrelated children/threads."""
    subreaper()
    stopped = []
    signals = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)
    previous = {sig: signal.getsignal(sig) for sig in signals}
    for sig in signals:
        signal.signal(sig, lambda signum, frame: stopped.append(signum) if not stopped else None)
    try:
        lease.start()  # Must persist before a command can exist.
        if stopped:
            lease.finish()
            return 128 + stopped[0]
        end = time.monotonic() + seconds
        try:
            process = subprocess.Popen(command, shell=False, stdin=subprocess.DEVNULL,
                                       start_new_session=True, pass_fds=(lease.fd,))
        except OSError:
            quiet, _ = drain_children(time.monotonic() + 10)
            if quiet:
                lease.finish()
            raise Refused("job could not start") from None
        while True:
            try:
                code = process.wait(timeout=0.1)
                code = code if code >= 0 else 128 - code
                break
            except subprocess.TimeoutExpired:
                if stopped or time.monotonic() >= end:
                    stop_group(process)
                    code = 128 + stopped[0] if stopped else 124
                    break
        # Escaped/session-changing descendants may survive group termination.
        # Never clear the marker until the kernel says no adopted child remains.
        quiet, failed = drain_children(min(end, time.monotonic() + 10))
        if not quiet:
            raise Refused("descendant still active; incomplete marker retained")
        lease.finish()
        return (128 + stopped[0]) if stopped else (1 if failed and code == 0 else code)
    finally:
        for sig, handler in previous.items():
            signal.signal(sig, handler)


class Parser(argparse.ArgumentParser):
    def error(self, message):
        raise Refused("invalid coordinator arguments")


def native_headroom():
    observed = []

    def reader():
        value = vm_resources.linux_available_memory()
        observed.append(value)
        return value

    try:
        return vm_resources.require_headroom("native QOR job", reader=reader)
    except (ValueError, OSError):
        if observed and type(observed[0]) is int and 0 <= observed[0] < vm_resources.MINIMUM_MEMORY:
            raise Refused(f"RAM available {observed[0]} bytes; required {vm_resources.MINIMUM_MEMORY} bytes "
                          f"(3072 MiB); shortfall {vm_resources.MINIMUM_MEMORY - observed[0]} bytes") from None
        raise Refused("native RAM/cgroup observation unavailable; no job started") from None


def main(argv=None):
    try:
        parser = Parser(description=__doc__)
        parser.add_argument("--check", action="store_true", help="observe an existing lock; create/run nothing")
        parser.add_argument("--timeout-seconds", type=int, default=14400)
        parser.add_argument("command", nargs=argparse.REMAINDER)
        args = parser.parse_args(argv)
        command = args.command[1:] if args.command[:1] == ["--"] else args.command
        if (not 1 <= args.timeout_seconds <= MAX_SECONDS or len(command) > 256
                or sum(len(x.encode("utf-8")) for x in command) > 32768
                or any("\0" in x for x in command) or bool(command) == args.check):
            raise Refused("invalid coordinator arguments")
        home, uid = native_identity()
        with Lease(home, uid, create=not args.check) as lease:
            native_headroom()
            lease.verify()
            if args.check:
                print("native lock available; resource snapshot passed; no reservation or builder acceptance")
                return 0
            return run_job(lease, command, args.timeout_seconds)
    except Busy:
        print("native QOR lock busy; no job started", file=sys.stderr)
        return 75
    except Refused as error:
        print(f"native QOR coordination refused: {error}", file=sys.stderr)
        return 2
    except (OSError, ValueError, ImportError, KeyError):
        print("native QOR coordination refused; no automatic recovery; inspect platform, lock and resource prerequisites", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
