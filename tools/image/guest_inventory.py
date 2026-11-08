# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Guest-only observations. Missing sealed firmware metadata fails before disk discovery."""
import json
import os
from pathlib import Path
import re
import stat

from vmguard import guest_target, validate_metadata

FWCFG = Path("/sys/firmware/qemu_fw_cfg/by_name/opt/qor/allowlist/raw")


def expected_target() -> dict:
    with FWCFG.open("rb") as stream:
        data = stream.read(16385)
    if len(data) > 16384:
        raise ValueError("Oversized firmware allowlist")
    expected = json.loads(data)
    # Validate before even enumerating devices. This program must not collect
    # host disk information when invoked outside its sealed disposable VM.
    validate_metadata(expected)
    actual = Path("/sys/class/dmi/id/product_uuid").read_text().strip().lower()
    if actual != expected["vm_uuid"]:
        raise ValueError("Firmware allowlist belongs to another VM")
    if not Path("/sys/firmware/efi").is_dir():
        raise ValueError("This VM proof supports UEFI only")
    return expected


def text(path: Path) -> str:
    with path.open(encoding="utf-8") as stream:
        value = stream.read(1025)
    if len(value) > 1024:
        raise ValueError("Oversized kernel disk attribute")
    return value.strip()


def unused_loop(entry: Path, major_minor: str, sectors: int, busy: bool) -> bool:
    """An unbound kernel loop node has no backing storage to write."""
    if (not re.fullmatch(r"loop[0-9]+", entry.name)
            or entry.resolve() != Path("/sys/devices/virtual/block") / entry.name
            or not re.fullmatch(r"7:[0-9]+", major_minor) or sectors != 0 or busy):
        return False
    try:
        (entry / "loop").stat()
    except FileNotFoundError:
        return True
    return False


def collect(expected: dict) -> dict:
    mounts = {line.split()[2] for line in Path("/proc/self/mountinfo").read_text().splitlines()}
    swaps = {line.split()[0] for line in Path("/proc/swaps").read_text().splitlines()[1:]}
    disks = []
    for entry in Path("/sys/class/block").iterdir():
        if (entry / "partition").exists() or text(entry / "ro") == "1":
            continue
        name = entry.name
        node = Path("/dev") / name
        info = node.stat()
        if not stat.S_ISBLK(info.st_mode):
            raise ValueError("Kernel disk name is not a block device")
        major_minor = text(entry / "dev")
        if major_minor != f"{os.major(info.st_rdev)}:{os.minor(info.st_rdev)}":
            raise ValueError("Device node and kernel identity disagree")
        descendants = [entry, *(part for part in entry.resolve().iterdir() if (part / "partition").exists())]
        busy = any(text(part / "dev") in mounts or str(Path("/dev") / part.name) in swaps
                   or any((part / "holders").iterdir()) for part in descendants)
        sectors = int(text(entry / "size"))
        if unused_loop(entry, major_minor, sectors, busy):
            continue
        serials = [text(path) for path in (entry / "serial", entry / "device/serial") if path.exists()]
        serial = serials[0] if serials and len(set(serials)) == 1 else ""
        driver_link = entry / "device/driver"
        disks.append({"path": str(node), "serial": serial, "bytes": sectors * 512,
                      "driver": driver_link.resolve().name if driver_link.exists() else "",
                      "sysfs": str(entry.resolve()), "major_minor": major_minor,
                      "busy": busy, "read_only": False})
    return guest_target(expected, expected["vm_uuid"], disks)


def inspect() -> tuple[dict, dict]:
    expected = expected_target()
    return expected, collect(expected)


if __name__ == "__main__":
    try:
        expected, disk = inspect()
        print(json.dumps({"type": "inventory", "disk": disk, "source_revision": expected["source_revision"],
                          "image_sha256": expected["image_sha256"], "encryption": False}))
    except (OSError, ValueError, TypeError, KeyError):
        print(json.dumps({"type": "error", "message": "No verified disposable VM target. Installation is disabled."}))
        raise SystemExit(1)
