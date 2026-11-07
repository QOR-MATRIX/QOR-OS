# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Run only an allowlisted disposable QOR VM; no host devices or networking."""
import argparse
from contextlib import contextmanager
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

from vmguard import WORKSPACE, ancestry, confined, identity, unlinked, verify
from vm_resources import require_headroom


@contextmanager
def pinned(paths):
    """Deny rename/delete (and manifest writes) for the entire VM lifetime.

    Windows is the supported host for this launcher. The guest QEMU also keeps
    regular-file descriptors open rather than reopening a mutable path.
    """
    if os.name != "nt":
        raise ValueError("This sealed launcher currently supports the Windows workspace only")
    from ctypes import wintypes
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handles = []
    try:
        for path, immutable in paths:
            handle = kernel.CreateFileW(str(path), 0x80000000, 1 if immutable else 3, None, 3, 0x02000000, None)
            if handle == ctypes.c_void_p(-1).value:
                raise OSError(ctypes.get_last_error(), "Could not pin the test artifact ancestry")
            handles.append(handle)
        yield
    finally:
        for handle in reversed(handles):
            kernel.CloseHandle(handle)


def docker_command(record, iso, evidence, tools_id, phase, launcher):
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", tools_id) or phase not in ("live", "installed"):
        raise ValueError("Pinned tooling and explicit phase are required")
    root = Path(record["root"])
    argv = ["docker", "run", "--rm", "--pull", "never", "--name", "qor-vm-" + record["vm_uuid"],
            "--label", "dev.qorsync.disposable-vm=" + record["vm_uuid"],
            "--network", "none", "--cap-drop", "ALL", "--security-opt", "no-new-privileges",
            "--read-only", "--pids-limit", "64", "--cpus", "2", "--memory", "1408m",
            "--tmpfs", "/tmp:rw,nosuid,nodev,size=32m", "--env", "QOR_VM_PHASE=" + phase]
    mounts = [(root / "install.raw", "/vm/install.raw", False), (root / "nvram.fd", "/vm/nvram.fd", False),
              (root / "allowlist.json", "/vm/allowlist.json", True), (launcher, "/vm/entry.py", True),
              (evidence, "/evidence", False)]
    if phase == "live":
        mounts.append((iso, "/vm/install.iso", True))
    for source, target, readonly in mounts:
        if any(char in str(source) for char in (",", "\n", "\r")):
            raise ValueError("Invalid bind path")
        argv += ["--mount", f"type=bind,source={source},target={target}" + (",readonly" if readonly else "")]
    return argv + [tools_id, "/usr/bin/python3", "/vm/entry.py"]


def checked_iso(iso, checksum, previous=None):
    observed = identity(iso)
    if previous is not None and observed != previous:
        raise ValueError("The installation ISO was replaced while acquiring its handles")
    with iso.open("rb") as stream:
        if hashlib.file_digest(stream, "sha256").hexdigest() != checksum:
            raise ValueError("ISO differs from the allowlisted image")
    return observed


def run(manifest, iso, evidence, tools_id, phase):
    record = verify(manifest)
    resources = require_headroom("Disposable VM")
    iso = unlinked(iso)
    if not iso.is_relative_to(WORKSPACE) or identity(iso)[2] < 64 * 1024**2:
        raise ValueError("The ISO must be a regular QOR workspace artifact")
    iso_identity = checked_iso(iso, record["image_sha256"])
    evidence = confined(evidence, WORKSPACE)
    evidence.mkdir(exist_ok=False)
    launcher = Path(__file__).with_name("vm_container.py")
    # Pin parents from root to volume, the mutable disk/vars, and the immutable
    # allowlist/ISO/entrypoint. Verify again once every handle has been acquired.
    paths = [(Path(item["path"]), False) for item in ancestry(Path(record["root"]))]
    paths += [(Path(item["path"]), False) for parent in (iso.parent, launcher.parent) for item in ancestry(parent)]
    paths += [(Path(record["root"]) / name, name == "allowlist.json")
              for name in ("install.raw", "nvram.fd", "allowlist.json")]
    paths += [(iso, True), (launcher, True)]
    with pinned(paths):
        if verify(manifest) != record:
            raise ValueError("The allowlist changed while acquiring artifact handles")
        checked_iso(iso, record["image_sha256"], iso_identity)
        resources = require_headroom("Disposable VM")
        argv = docker_command(record, iso, evidence, tools_id, phase, launcher)
        sources = {}
        for name in ("vmguard.py", "vmrun.py", "vm_container.py", "vm_control.py", "vm_resources.py"):
            source = Path(__file__).with_name(name)
            sources[name] = hashlib.sha256(source.read_bytes()).hexdigest()
        (evidence / "launch.json").write_text(json.dumps({"phase": phase, "allowlist": record,
                                                        "tools_image": tools_id, "launcher_sources": sources,
                                                        "host_memory_preflight": resources,
                                                        "command": argv}, indent=2) + "\n")
        with (evidence / "container.log").open("wb") as log:
            return subprocess.call(argv, stdout=log, stderr=subprocess.STDOUT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--iso", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--tools-id", required=True)
    parser.add_argument("--phase", choices=("live", "installed"), required=True)
    args = parser.parse_args()
    try:
        return run(args.manifest, args.iso, args.evidence, args.tools_id, args.phase)
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(1, "Disposable VM launch refused: " + str(error) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
