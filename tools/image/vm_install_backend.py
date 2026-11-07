# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Private VM-only bootc transaction. No partitioning implementation lives here."""
import argparse
from contextlib import contextmanager
import ctypes
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
from vm_payload import unique_object, validate as validate_payload
from vm_session import installation_log

MEDIA = Path("/run/initramfs/live")
SOURCE = MEDIA / "qor/install-source"
RECEIPT = MEDIA / "qor/install-source.json"


def read_only_media():
    """Accept only the sealed launcher's immutable ISO, never a live overlay."""
    for path in (MEDIA, *MEDIA.parents):
        if path.is_symlink() or not path.is_dir():
            raise ValueError("Linked or missing installation media ancestry")
    with Path("/proc/self/mountinfo").open() as stream:
        raw = stream.read(1024 * 1024 + 1)
    if len(raw) > 1024 * 1024:
        raise ValueError("Oversized mount inventory")
    matches = []
    for line in raw.splitlines():
        fields, separator, filesystem = line.partition(" - ")
        fields, filesystem = fields.split(), filesystem.split()
        if separator and len(fields) >= 6 and fields[4] == str(MEDIA):
            matches.append((fields[5].split(","), filesystem))
    if (len(matches) != 1 or "ro" not in matches[0][0]
            or not matches[0][1] or matches[0][1][0] != "iso9660"
            or not os.statvfs(MEDIA).f_flag & os.ST_RDONLY):
        raise ValueError("The offline source must be on the read-only installation ISO")


def regular_source(path):
    if not path.is_relative_to(MEDIA):
        raise ValueError("Source outside installation media")
    for parent in path.parents:
        if parent == MEDIA:
            break
        if parent.is_symlink() or not parent.is_dir():
            raise ValueError("Linked or missing payload ancestry")
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        info = os.fstat(descriptor)
        if (not stat.S_ISREG(info.st_mode) or info.st_nlink != 1
                or info.st_dev != MEDIA.stat().st_dev or not os.fstatvfs(descriptor).f_flag & os.ST_RDONLY):
            raise ValueError("Offline payload must be a single regular file on the read-only ISO")
        return os.fdopen(descriptor, "rb")
    except BaseException:
        os.close(descriptor)
        raise


@contextmanager
def offline_source():
    read_only_media()
    with regular_source(RECEIPT) as stream:
        raw = stream.read(4097)
    if len(raw) > 4096:
        raise ValueError("Oversized installation receipt")
    receipt = json.loads(raw, object_pairs_hook=unique_object)
    validate_payload(SOURCE, receipt, regular_source)
    yield receipt


def emit(kind, **fields):
    print(json.dumps({"type": kind, **fields}), flush=True)


def command(expected: dict, disk: dict, receipt: dict, confirmation: str) -> list[str]:
    if confirmation != disk["serial"] or confirmation != expected["disk"]["serial"]:
        raise ValueError("The displayed target was not explicitly confirmed")
    if (not isinstance(receipt, dict)
            or set(receipt) != {"schema", "source_revision", "manifest_sha256", "profile", "encryption"}
            or type(receipt["schema"]) is not int or receipt["schema"] != 2
            or not isinstance(receipt["manifest_sha256"], str)
            or not re.fullmatch(r"[0-9a-f]{64}", receipt["manifest_sha256"])
            or receipt["profile"] != "private-vm-lab"
            or receipt["source_revision"] != expected["source_revision"] or receipt["encryption"] is not False):
        raise ValueError("Unknown or mismatched offline payload")
    # The private offline image has no published update registry. The stock
    # backend's fetch check is inapplicable; update timers stay disabled, and
    # no update/readiness claim is made. Disk/provenance guards remain required.
    return ["/usr/bin/bootc", "install", "to-disk", "--wipe", "--generic-image",
            "--filesystem", "ext4", "--bootloader", "grub",
            "--source-imgref", "oci:" + SOURCE.as_posix(),
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

    parent = os.getppid()
    if parent <= 1:
        raise ValueError("Could not bind transaction lifetime to its frontend")
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(1, signal.SIGTERM, 0, 0, 0) != 0 or os.getppid() != parent:
        raise ValueError("Could not bind transaction lifetime to its frontend")
    signal.signal(signal.SIGTERM, interrupted)
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
        with offline_source() as receipt:
            argv = command(expected, disk, receipt, args.confirm_serial)
            # Every content-addressed blob was checked on the immutable ISO.
            # OCI avoids unpacking a multi-gigabyte archive into live RAM.
            current_expected, current_disk = inspect()
            if current_expected != expected or current_disk != disk:
                raise ValueError("Target changed after review")
            emit("writing", disk=disk, source_revision=expected["source_revision"],
                 image_sha256=expected["image_sha256"])
            logdir = Path("/run/qor-vm-install")
            logdir.mkdir(mode=0o700, exist_ok=True)
            with (logdir / "bootc.log").open("wb") as log:
                started = True
                code = transaction(argv, log)
            installation_log(code)
        if code:
            emit("error", message="The bootc transaction failed. Preserve this disk and inspect /run/qor-vm-install/bootc.log.")
            return 1
        emit("complete", message="Installation finished. Remove the media and reboot to verify the installed system.")
        return 0
    except (OSError, ValueError, TypeError, KeyError, RuntimeError, subprocess.SubprocessError):
        message = ("The install transaction or its evidence did not complete. This disk may be incomplete; preserve it and inspect diagnostics."
                   if started else "The disposable target or payload could not be verified. No new install transaction was started.")
        emit("error", message=message)
        return 1


if __name__ == "__main__":
    sys.exit(main())
