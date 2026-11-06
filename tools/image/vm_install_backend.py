# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Private VM-only bootc transaction. No partitioning implementation lives here."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import stat
import subprocess
import sys
import threading

from guest_inventory import inspect

SOURCE = Path("/usr/share/qor-os/install-source.tar")
RECEIPT = Path("/usr/share/qor-os/install-source.json")


def emit(kind, **fields):
    print(json.dumps({"type": kind, **fields}), flush=True)


def command(expected: dict, disk: dict, receipt: dict, confirmation: str) -> list[str]:
    if confirmation != disk["serial"] or confirmation != expected["disk"]["serial"]:
        raise ValueError("The displayed target was not explicitly confirmed")
    if (not isinstance(receipt, dict)
            or set(receipt) != {"schema", "source_revision", "archive_sha256", "profile", "encryption"}
            or type(receipt["schema"]) is not int or receipt["schema"] != 1
            or not isinstance(receipt["archive_sha256"], str)
            or not re.fullmatch(r"[0-9a-f]{64}", receipt["archive_sha256"])
            or receipt["profile"] != "private-vm-lab"
            or receipt["source_revision"] != expected["source_revision"] or receipt["encryption"] is not False):
        raise ValueError("Unknown or mismatched offline payload")
    # The private offline image has no published update registry. The stock
    # backend's fetch check is inapplicable; update timers stay disabled, and
    # no update/readiness claim is made. Disk/provenance guards remain required.
    return ["/usr/bin/bootc", "install", "to-disk", "--wipe", "--generic-image",
            "--filesystem", "ext4", "--bootloader", "grub",
            "--source-imgref", "docker-archive:" + SOURCE.as_posix(),
            "--target-imgref", "localhost/qor-os-vm-lab:development", "--skip-fetch-check",
            "--karg", "console=ttyS0,115200", "--karg", "console=tty0", disk["path"]]


def transaction(argv, log, timeout=2400, pass_fds=()):
    """Drain output without unbounded storage; terminate the entire child group.

    A closed/crashed frontend must not leave an installer running without its
    progress UI. The live systemd unit also uses KillMode=control-group.
    """
    if sys.platform != "linux":
        raise ValueError("The transaction runner is guest Linux only")
    previous = signal.getsignal(signal.SIGTERM)

    def interrupted(_number, _frame):
        raise InterruptedError("Installation service terminated")

    signal.signal(signal.SIGTERM, interrupted)
    parent = os.getppid()
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(1, signal.SIGTERM, 0, 0, 0) != 0 or parent <= 1 or os.getppid() != parent:
        raise ValueError("Could not bind transaction lifetime to its frontend")
    first, tail = bytearray(), bytearray()

    def drain(stream):
        while block := stream.read(8192):
            needed = max(0, 128 * 1024 - len(first))
            first.extend(block[:needed])
            tail.extend(block[needed:])
            if len(tail) > 1024 * 1024:
                del tail[:-1024 * 1024]

    process = None
    reader = None
    try:
        env = {"PATH": "/usr/sbin:/usr/bin", "HOME": "/var/roothome", "LANG": "C.UTF-8", "TERM": "dumb"}
        process = subprocess.Popen(argv, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   start_new_session=True, pass_fds=pass_fds)
        reader = threading.Thread(target=drain, args=(process.stdout,), daemon=True)
        reader.start()
        return process.wait(timeout=timeout)
    finally:
        if process is not None:
            # Kill the process group even after the leader exits: a failed
            # backend must not leave a descendant writing to the test disk.
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
        if reader is not None:
            reader.join(timeout=10)
            if reader.is_alive():
                raise RuntimeError("Backend output did not close after process-group cleanup")
            process.stdout.close()
        log.write(first)
        if tail:
            log.write(b"\n[bounded diagnostic tail; intervening output may be omitted]\n")
            log.write(tail)
        log.flush()
        signal.signal(signal.SIGTERM, previous)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("inspect", "install", "reboot"))
    parser.add_argument("--confirm-serial", default="")
    args = parser.parse_args()
    started = False
    try:
        expected, disk = inspect()
        if args.action == "inspect":
            emit("inventory", disk=disk, source_revision=expected["source_revision"],
                 image_sha256=expected["image_sha256"], encryption=False)
            return 0
        if args.action == "reboot":
            if args.confirm_serial != disk["serial"]:
                raise ValueError("Reboot target is not confirmed")
            subprocess.run(["/usr/bin/systemctl", "reboot"], check=True)
            return 0
        with RECEIPT.open("rb") as stream:
            raw_receipt = stream.read(4097)
        if len(raw_receipt) > 4096:
            raise ValueError("Oversized installation receipt")
        receipt = json.loads(raw_receipt)
        argv = command(expected, disk, receipt, args.confirm_serial)
        source_fd = os.open(SOURCE, os.O_RDONLY | os.O_NOFOLLOW)
        with os.fdopen(source_fd, "rb") as stream:
            source_info = os.fstat(stream.fileno())
            if not stat.S_ISREG(source_info.st_mode) or source_info.st_nlink != 1:
                raise ValueError("Offline payload must be a single regular file")
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
            if digest != receipt["archive_sha256"]:
                raise ValueError("Offline installation payload checksum differs")
            # The archive is kept open across execution; bootc cannot follow a
            # replaced pathname after checksum validation.
            argv[argv.index("--source-imgref") + 1] = f"docker-archive:/proc/self/fd/{stream.fileno()}"
            stream.seek(0)
            current_expected, current_disk = inspect()
            if current_expected != expected or current_disk != disk:
                raise ValueError("Target changed after review")
            emit("writing", disk=disk, source_revision=expected["source_revision"],
                 image_sha256=expected["image_sha256"])
            logdir = Path("/run/qor-vm-install")
            logdir.mkdir(mode=0o700, exist_ok=True)
            with (logdir / "bootc.log").open("wb") as log:
                started = True
                code = transaction(argv, log, pass_fds=(stream.fileno(),))
        if code:
            emit("error", message="The bootc transaction failed. Preserve this disk and inspect /run/qor-vm-install/bootc.log.")
            return 1
        emit("complete", message="Installation finished. Remove the media and reboot to verify the installed system.")
        return 0
    except (OSError, ValueError, TypeError, KeyError, RuntimeError, subprocess.SubprocessError):
        message = ("The install transaction was interrupted. This disk may be incomplete; preserve it and inspect diagnostics."
                   if started else "The disposable target or payload could not be verified. No new install transaction was started.")
        emit("error", message=message)
        return 1


if __name__ == "__main__":
    sys.exit(main())
