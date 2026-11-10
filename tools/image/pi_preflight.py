#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Read-only Q Pi preparation probe. Never builds, installs, mounts or flashes."""

import argparse
import json
import os
from pathlib import Path
import platform
import re
import shutil
import stat
import struct
import subprocess
import vm_resources

MAX_READ = 1024 * 1024
UINT64_LIMIT = 1 << 64


def inspect_elf(data, file_size, page_size=16384):
    """Bounded ELF64/AArch64 load-layout check, not Linux ABI certification.

    file_size is the whole regular file's size, not merely the prefix read.
    Extended program tables and empty load segments are unsupported here.
    """
    if type(page_size) is not int or page_size not in (4096, 16384):
        raise ValueError("unsupported page size")
    if type(file_size) is not int or not len(data) <= file_size < UINT64_LIMIT:
        raise ValueError("invalid file size")
    if len(data) > MAX_READ or len(data) < 64 or data[:7] != b"\x7fELF\x02\x01\x01":
        raise ValueError("expected bounded ELF64 little-endian version 1")
    kind, machine, version = struct.unpack_from("<HHI", data, 16)
    if machine != 183 or kind not in (2, 3) or version != 1:
        raise ValueError("expected executable/shared AArch64 ELF version 1")
    phoff = struct.unpack_from("<Q", data, 32)[0]
    ehsize, phentsize, phnum = struct.unpack_from("<HHH", data, 52)
    if ehsize != 64 or phentsize != 56 or not 1 <= phnum <= 1024 or phoff < 64:
        raise ValueError("invalid or unsupported program table")
    if phoff + phentsize * phnum > len(data):
        raise ValueError("program table outside bounded prefix")
    alignments = []
    for index in range(phnum):
        row = struct.unpack_from("<IIQQQQQQ", data, phoff + index * phentsize)
        ptype, _, offset, vaddr, _, filesz, memsz, align = row
        if ptype != 1:
            continue
        if not memsz or filesz > memsz or offset + filesz > file_size:
            raise ValueError("load segment outside file or invalid size")
        if vaddr + memsz >= UINT64_LIMIT:
            raise ValueError("load address overflow")
        # Conservative Q Pi qualification policy, not a universal ELF validity
        # rule: some smaller declared alignments may still load on a given OS.
        if align < page_size or align & (align - 1):
            raise ValueError("load alignment incompatible with selected page size")
        if offset % align != vaddr % align:
            raise ValueError("load offset/address incongruent at segment alignment")
        alignments.append(align)
    if not alignments:
        raise ValueError("no load segments")
    return {"machine": "AArch64", "elf_class": 64, "elf_type": kind,
            "load_alignments": alignments, "selected_page_size": page_size}


def read_prefix(path):
    """Read a bounded prefix of an existing regular file; execute nothing."""
    if not stat.S_ISREG(path.stat().st_mode):
        raise ValueError("expected regular input file")
    with path.open("rb") as handle:
        info = os.fstat(handle.fileno())
        if not stat.S_ISREG(info.st_mode):
            raise ValueError("expected regular input file")
        return handle.read(MAX_READ), info.st_size


def qt_version(kit):
    # Qt 6.12's public version file may include the generated Impl file.
    # Read fixed metadata locations only. Never execute CMake from a kit.
    directory = kit / "lib/cmake/Qt6Core"
    for name in ("Qt6CoreConfigVersion.cmake", "Qt6CoreConfigVersionImpl.cmake"):
        path = directory / name
        if not path.is_file():
            continue
        data, size = read_prefix(path)
        if size > MAX_READ:
            raise ValueError("Qt metadata too large")
        match = re.search(r'^\s*set\(PACKAGE_VERSION\s+["\']?(\d+\.\d+\.\d+)["\']?\s*\)',
                          data.decode("utf-8"), re.MULTILINE | re.IGNORECASE)
        if match:
            return match.group(1)
    raise ValueError("Qt version not identified in fixed metadata files")


def git_read(source, args):
    result = subprocess.run(["git", "--no-optional-locks", "-C", str(source), *args],
                            capture_output=True, text=True, timeout=60, check=False)
    if result.returncode:
        raise ValueError("read-only Git query failed")
    return result.stdout.strip()


def scratch_check(root, budget_path):
    if root is None or budget_path is None:
        return False, {"required_bytes": None,
                       "note": "supply a measured component budget and an existing local scratch directory"}
    try:
        raw, size = read_prefix(budget_path)
        if size > vm_resources.MAX_BUDGET_BYTES:
            raise ValueError("Scratch budget exceeds its input bound")
        budget = vm_resources.parse_scratch_budget(raw)
        return True, vm_resources.require_scratch_headroom(root, budget)
    except (OSError, ValueError):
        return False, {"note": "budget invalid, local backing storage unqualified, or capacity insufficient"}


def memory_check():
    if platform.system() == "Windows":
        try:
            return True, vm_resources.require_headroom("Q Pi image build")
        except (OSError, ValueError) as error:
            return False, str(error)
    try:
        ram = vm_resources.require_headroom("Q Pi native Linux build", reader=vm_resources.linux_available_memory)
        return True, {**ram, "scope": "native Linux and visible cgroup v2 snapshot"}
    except (OSError, ValueError):
        return False, "native Linux memory unavailable or below 3 GiB; WSL requires the Windows host guard"


def probe(source, kit, expected, scratch_root=None, scratch_budget=None):
    checks = []

    def add(name, passed, detail):
        checks.append({"check": name, "passed": bool(passed), "detail": detail})

    machine = platform.machine().lower()
    add("native_arm64_linux", platform.system() == "Linux" and machine in ("aarch64", "arm64"),
        {"system": platform.system(), "machine": machine})
    osr = {}
    os_release = Path("/etc/os-release")
    if os_release.is_file():
        for line in os_release.read_text(encoding="utf-8").splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                osr[key] = value.strip('"')
    add("supported_native_generator_host",
        osr.get("ID") in ("debian", "raspbian") and osr.get("VERSION_CODENAME") in ("bookworm", "trixie"),
        {key: osr.get(key) for key in ("ID", "VERSION_ID", "VERSION_CODENAME")})
    tools = ("git", "python3", "cmake", "ninja", "pkg-config", "gcc", "g++", "xz",
             "podman", "mmdebstrap", "bdebstrap", "genimage")
    missing = [tool for tool in tools if not shutil.which(tool)]
    add("builder_tools_on_path", not missing,
        {"missing": missing, "qualification": "a pinned generator private sysroot needs separate verification"})
    add("source_layout", all((source / item).is_file() for item in
                             ("AGENTS.md", "CMakeLists.txt", "CMakePresets.json", "OWNER.md")),
        "expected QOR files")
    try:
        actual = git_read(source, ["rev-parse", "HEAD"])
        dirty = git_read(source, ["status", "--porcelain", "--untracked-files=normal"])
        add("source_commit", actual == expected, {"actual": actual, "expected": expected})
        add("clean_source", not dirty, {"dirty_entry_count": len(dirty.splitlines())})
    except (OSError, ValueError, subprocess.TimeoutExpired):
        add("source_identity", False, "read-only Git query failed or timed out; no names/content exported")
    try:
        version = qt_version(kit)
        add("qt_612_metadata", version.startswith("6.12."), version)
    except (OSError, ValueError):
        add("qt_612_metadata", False, "Qt version metadata unavailable or invalid")
    try:
        data, size = read_prefix(kit / "lib/libQt6Core.so.6")
        info = inspect_elf(data, size)
        add("qt_core_arm64_16k_layout", info["elf_type"] == 3, info)
    except (OSError, ValueError, struct.error):
        add("qt_core_arm64_16k_layout", False, "Qt Core is not a qualified ARM64 shared ELF layout")
    memory_passed, memory_detail = memory_check()
    add("host_ram_3gib", memory_passed, memory_detail)
    scratch_passed, scratch_detail = scratch_check(scratch_root, scratch_budget)
    add("physical_scratch_budget", scratch_passed, scratch_detail)
    add("qualified_image_inputs", False,
        "no verified local base artifact, pinned qualified builder or complete ARM64 Qt dependency closure")
    return {"name": "Q Pi", "kind": "read-only-preparation-probe", "checks": checks,
            "blockers": [item["check"] for item in checks if not item["passed"]],
            "build_started": False, "image_built": False,
            "not_certified": ["Linux GNU ABI", "commercial Qt entitlement", "all ELF dependencies",
                              "namespace privileges", "login and first boot", "boot image", "Pi graphics",
                              "physical Pi boot", "QOR applications", "full QOR device session"]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--qt-target", type=Path, required=True)
    parser.add_argument("--expected-commit", required=True)
    parser.add_argument("--scratch-root", type=Path, help="Existing fixed local build directory; never created by this probe")
    parser.add_argument("--scratch-budget", type=Path, help="Measured schema-1 component budget JSON; no default estimate")
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[0-9a-f]{40}", args.expected_commit):
        parser.error("expected-commit must be a full lowercase Git SHA-1")
    if (args.scratch_root is None) != (args.scratch_budget is None):
        parser.error("scratch-root and scratch-budget must be supplied together")
    try:
        report = probe(args.source, args.qt_target, args.expected_commit, args.scratch_root, args.scratch_budget)
    except (OSError, ValueError):
        print(json.dumps({"kind": "read-only-preparation-probe", "error": "input observation failed",
                          "build_started": False, "image_built": False}))
        return 2
    print(json.dumps(report, indent=2))
    return 2 if report["blockers"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
