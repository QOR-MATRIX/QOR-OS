# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Fixed, headless QEMU entry point inside the restricted QOR test container."""
import json
import os
from pathlib import Path
import re
import stat
import subprocess


def regular_fd(path, writable, size=None):
    fd = os.open(path, (os.O_RDWR if writable else os.O_RDONLY) | getattr(os, "O_NOFOLLOW", 0))
    info = os.fstat(fd)
    if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or (size is not None and info.st_size != size):
        os.close(fd)
        raise ValueError("Only the exact single-link regular test artifact is allowed")
    return fd


def arguments(record, live, disk_fd, vars_fd, code_fd, iso_fd=None, *, readonly_fds=()):
    if (not re.fullmatch(r"qor-[0-9a-f]{16}", record["disk"]["serial"])
            or record["disk"]["format"] != "raw" or record["disk"]["backing_file"] is not None):
        raise ValueError("Invalid raw target identity")
    if len(readonly_fds) != 2:
        raise ValueError("Read-only aliases of the held disk/variables descriptors are required")
    argv = ["/usr/bin/qemu-system-x86_64", "-nodefaults", "-no-user-config", "-name", "QOR-disposable-install",
            "-uuid", record["vm_uuid"], "-machine", "q35", "-cpu", "max", "-accel", "tcg,thread=single,tb-size=64",
            "-m", "1024", "-smp", "2", "-nic", "none", "-display", "none", "-no-reboot",
            "-device", "virtio-vga,addr=0x1", "-qmp", "tcp:127.0.0.1:4444,server=on,wait=off",
            "-chardev", "socket,id=serial,host=127.0.0.1,port=4445,server=on,wait=off,logfile=/evidence/serial.log",
            "-device", "isa-serial,chardev=serial",
            "-fw_cfg", "name=opt/qor/allowlist,file=/vm/allowlist.json"]
    for number, fd in enumerate((code_fd, vars_fd, disk_fd), 1):
        argv += ["-add-fd", f"fd={fd},set={number}"]
    # QEMU opens a raw backing file for both read-only probing and read/write
    # access; fdset lookup matches O_ACCMODE exactly. Both aliases still refer
    # to the same already-verified open file, never a reopened artifact path.
    argv += ["-add-fd", f"fd={readonly_fds[0]},set=3", "-add-fd", f"fd={readonly_fds[1]},set=2"]
    argv += ["-drive", "if=pflash,format=raw,unit=0,readonly=on,file=/dev/fdset/1",
             "-drive", "if=pflash,format=raw,unit=1,file=/dev/fdset/2",
             "-drive", "if=none,id=target,format=raw,cache=writeback,file=/dev/fdset/3",
             "-device", f"virtio-blk-pci,drive=target,addr=0x4,disable-legacy=on,serial={record['disk']['serial']}"]
    if live:
        if iso_fd is None:
            raise ValueError("Live mode requires the verified read-only ISO")
        argv += ["-add-fd", f"fd={iso_fd},set=4", "-drive",
                 "if=none,id=media,format=raw,readonly=on,media=cdrom,file=/dev/fdset/4",
                 "-device", "ide-cd,drive=media,bus=ide.0", "-boot", "order=d,strict=on"]
    else:
        argv += ["-boot", "order=c,strict=on"]
    return argv


def main():
    record = json.loads(Path("/vm/allowlist.json").read_text())
    live = os.environ.get("QOR_VM_PHASE") == "live"
    if os.environ.get("QOR_VM_PHASE") not in ("live", "installed"):
        raise ValueError("Explicit VM phase required")
    fds = [regular_fd("/vm/install.raw", True, record["disk"]["id"][2]),
           regular_fd("/vm/nvram.fd", True, record["nvram"]["id"][2]),
           regular_fd("/usr/share/edk2/ovmf/OVMF_CODE.fd", False)]
    if live:
        fds.append(regular_fd("/vm/install.iso", False))
    readonly = []
    for fd in fds[:2]:
        alias = os.open(f"/proc/self/fd/{fd}", os.O_RDONLY)
        before, after = os.fstat(fd), os.fstat(alias)
        if (before.st_dev, before.st_ino, before.st_size) != (after.st_dev, after.st_ino, after.st_size):
            raise ValueError("Descriptor aliases disagree")
        readonly.append(alias)
    argv = arguments(record, live, *fds[:3], fds[3] if live else None, readonly_fds=readonly)
    fds.extend(readonly)
    Path("/evidence/qemu-command.json").write_text(json.dumps(argv, indent=2) + "\n")
    try:
        with Path("/evidence/qemu.log").open("wb") as log:
            return subprocess.call(argv, pass_fds=fds, stdout=log, stderr=subprocess.STDOUT)
    finally:
        for fd in fds:
            os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())
