# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Read-only image resource checks; never free memory, reserve space or manage services."""
import ctypes
import json
import os
from pathlib import Path
import platform
import shutil

GIB = 1024**3
MINIMUM_MEMORY = 3 * GIB
MAX_BUDGET_BYTES = 65536
SCRATCH_COMPONENTS = ("source", "qt_sysroot", "package_cache", "expanded_root",
                      "raw_image", "compressed_output", "reserve")


def available_memory():
    if os.name != "nt":
        raise ValueError("This private VM host procedure supports Windows only")
    from ctypes import wintypes

    class Status(ctypes.Structure):
        _fields_ = [("length", wintypes.DWORD), ("load", wintypes.DWORD),
                    ("total_physical", ctypes.c_uint64), ("available_physical", ctypes.c_uint64),
                    ("total_pagefile", ctypes.c_uint64), ("available_pagefile", ctypes.c_uint64),
                    ("total_virtual", ctypes.c_uint64), ("available_virtual", ctypes.c_uint64),
                    ("available_extended", ctypes.c_uint64)]

    status = Status()
    status.length = ctypes.sizeof(status)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GlobalMemoryStatusEx.argtypes = [ctypes.POINTER(Status)]
    kernel.GlobalMemoryStatusEx.restype = wintypes.BOOL
    if not kernel.GlobalMemoryStatusEx(ctypes.byref(status)):
        raise OSError("Could not read host memory availability")
    if not status.total_physical or status.available_physical > status.total_physical:
        raise ValueError("Invalid host memory observation")
    return status.available_physical


def require_headroom(phase, minimum=MINIMUM_MEMORY, reader=None):
    if type(minimum) is not int or not MINIMUM_MEMORY <= minimum < 1 << 64:
        raise ValueError("Memory requirement must be an integer of at least 3 GiB")
    available = (reader or available_memory)()
    if type(available) is not int or available < 0:
        raise ValueError("Unknown memory availability; no heavy job started")
    if available < minimum:
        raise ValueError(f"{phase} not started: {available // 1024**2} MiB RAM available; "
                         f"{minimum // 1024**2} MiB required. Free memory on the host before retrying.")
    return {"available_bytes": available, "minimum_bytes": minimum}


def scratch_requirement(budget):
    """Sum a measured peak budget, including reserve; provide no invented default."""
    if (not isinstance(budget, dict) or set(budget) != {"schema", "components"}
            or type(budget["schema"]) is not int or budget["schema"] != 1
            or not isinstance(budget["components"], dict)
            or set(budget["components"]) != set(SCRATCH_COMPONENTS)):
        raise ValueError("Unknown scratch budget schema or missing components")
    sizes = list(budget["components"].values())
    if any(type(value) is not int or not 0 < value < 1 << 64 for value in sizes):
        raise ValueError("Every scratch component requires a positive integer byte count")
    required = sum(sizes)
    if required >= 1 << 64:
        raise ValueError("Scratch budget exceeds the supported byte range")
    return required


def parse_scratch_budget(raw):
    if not isinstance(raw, bytes) or len(raw) > MAX_BUDGET_BYTES:
        raise ValueError("Oversized or invalid scratch budget input")

    def unique(pairs):
        result = {}
        for name, value in pairs:
            if name in result:
                raise ValueError("Duplicate scratch budget field")
            result[name] = value
        return result

    try:
        budget = json.loads(raw, object_pairs_hook=unique)
    except RecursionError as error:
        raise ValueError("Scratch budget nesting exceeds the supported depth") from error
    scratch_requirement(budget)
    return budget


def _check_linux_mount(directory, raw, device_number):
    if len(raw) > 1024 * 1024:
        raise ValueError("Mount observation exceeds its bound")
    mounts = []
    for line in raw.decode("utf-8").splitlines():
        fields = line.split()
        try:
            separator = fields.index("-")
            if separator < 6 or len(fields) < separator + 4:
                raise ValueError("Incomplete mount observation")
            mount = fields[4]
            for escaped, decoded in ((r"\040", " "), (r"\011", "\t"), (r"\012", "\n"), (r"\134", "\\")):
                mount = mount.replace(escaped, decoded)
            if directory.is_relative_to(Path(mount)):
                mounts.append((len(Path(mount).parts), fields[2], fields[separator + 1], fields[separator + 2]))
        except (IndexError, ValueError) as error:
            raise ValueError("Invalid mount observation") from error
    if not mounts:
        raise ValueError("Scratch mount could not be identified")
    depth = max(item[0] for item in mounts)
    visible = [item for item in mounts if item[0] == depth and item[1] == device_number]
    if not visible or any(item[2] not in ("ext4", "xfs", "btrfs", "f2fs")
                          or not item[3].startswith("/dev/") for item in visible):
        raise ValueError("Scratch capacity needs a supported local disk filesystem")


def local_disk_directory(path):
    """Reject virtual/remote capacity that cannot size this host's disk budget."""
    directory = Path(path).resolve(strict=True)
    if not directory.is_dir():
        raise ValueError("Scratch location must be an existing directory")
    if "microsoft" in platform.release().lower() or "wsl" in platform.release().lower():
        raise ValueError("WSL capacity does not establish Windows backing-volume headroom")
    if os.name == "nt":
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.GetVolumePathNameW.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint32]
        kernel.GetVolumePathNameW.restype = ctypes.c_int
        kernel.GetDriveTypeW.argtypes = [ctypes.c_wchar_p]
        kernel.GetDriveTypeW.restype = ctypes.c_uint
        volume = ctypes.create_unicode_buffer(32768)
        if not kernel.GetVolumePathNameW(str(directory), volume, len(volume)):
            raise OSError("Scratch volume could not be identified")
        if kernel.GetDriveTypeW(volume.value) != 3:  # DRIVE_FIXED; no network or removable media.
            raise ValueError("Scratch directory is not on a fixed local volume")
    elif platform.system() == "Linux":
        # The kernel's mount table identifies the most specific mount. Do not
        # mistake overlay/tmpfs/network mounts for a sized native build disk.
        with Path("/proc/self/mountinfo").open("rb") as stream:
            raw = stream.read(1024 * 1024 + 1)
        device = directory.stat().st_dev
        _check_linux_mount(directory, raw, f"{os.major(device)}:{os.minor(device)}")
    else:
        raise ValueError("Scratch capacity is not qualified on this host platform")
    return directory


def require_scratch_headroom(path, budget, reader=None):
    required = scratch_requirement(budget)
    directory = local_disk_directory(path)
    usage = (reader or shutil.disk_usage)(directory)
    if (any(type(value) is not int or not 0 <= value < 1 << 64
            for value in (usage.total, usage.used, usage.free))
            or not usage.total or usage.free > usage.total or usage.used > usage.total):
        raise ValueError("Invalid scratch capacity observation")
    if usage.free < required:
        raise ValueError("Scratch volume has insufficient space for the supplied budget")
    return {"scratch_root": str(directory), "required_bytes": required,
            "available_bytes": usage.free, "components": dict(budget["components"]),
            "scope": "local disk capacity snapshot; budget sizing and builder qualification remain separate"}


if __name__ == "__main__":
    import json
    try:
        print(json.dumps(require_headroom("QOR VM lab")))
    except (OSError, ValueError) as error:
        raise SystemExit(str(error))
