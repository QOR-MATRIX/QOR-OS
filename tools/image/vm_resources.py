# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Read-only Windows RAM preflight for the private VM lab; never free host memory."""
import ctypes
import os

GIB = 1024**3


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


def require_headroom(phase, minimum=3 * GIB, reader=None):
    available = (reader or available_memory)()
    if type(available) is not int or available < 0:
        raise ValueError("Unknown memory availability; no heavy job started")
    if available < minimum:
        raise ValueError(f"{phase} not started: {available // 1024**2} MiB RAM available; "
                         f"{minimum // 1024**2} MiB required. Free memory on the host before retrying.")
    return {"available_bytes": available, "minimum_bytes": minimum}


if __name__ == "__main__":
    import json
    try:
        print(json.dumps(require_headroom("QOR VM lab")))
    except (OSError, ValueError) as error:
        raise SystemExit(str(error))
