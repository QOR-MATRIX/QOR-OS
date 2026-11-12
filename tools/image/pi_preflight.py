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


def qt_version(kit, module="Qt6Core"):
    # Qt 6.12's public version file may include the generated Impl file.
    # Read fixed metadata locations only. Never execute CMake from a kit.
    if module not in ("Qt6", "Qt6Core"):
        raise ValueError("unsupported fixed Qt metadata package")
    directory = kit / "lib/cmake" / module
    if not stat.S_ISREG((directory / f"{module}ConfigVersion.cmake").stat().st_mode):
        raise ValueError("Qt package version entry point is not a regular file")
    versions = []
    for name in (f"{module}ConfigVersion.cmake", f"{module}ConfigVersionImpl.cmake"):
        path = directory / name
        if not path.is_file():
            continue
        data, size = read_prefix(path)
        if size > MAX_READ:
            raise ValueError("Qt metadata too large")
        content = data.decode("utf-8")
        if re.search(r'\[=*\[', content):
            raise ValueError("unsupported bracket syntax in Qt version metadata")
        uncommented = "\n".join(line.split("#", 1)[0] for line in content.splitlines())
        starts = re.findall(r'(?i:\bset)\s*\(\s*PACKAGE_VERSION(?:\s|\))', uncommented)
        assignments = [line.strip() for line in uncommented.splitlines()
                       if re.match(r'(?i:set)\s*\(\s*PACKAGE_VERSION(?:\s|\))', line.strip())]
        if len(starts) != len(assignments):
            raise ValueError("Qt version assignment is not fixed line metadata")
        for assignment in assignments:
            # CMake's generated version file decorates the result on a pointer-
            # width mismatch. Recognise that exact generated suffix, not arbitrary
            # dynamic version expressions; the literal must still occur once.
            if assignment == 'set(PACKAGE_VERSION "${PACKAGE_VERSION} (${installedBits}bit)")':
                continue
            match = re.fullmatch(r'(?i:set)\s*\(\s*PACKAGE_VERSION\s+'
                                 r'(?:"(\d+\.\d+\.\d+)"|(\d+\.\d+\.\d+))\s*\)', assignment)
            if not match:
                raise ValueError("Qt version assignment is not fixed literal metadata")
            versions.append(match.group(1) or match.group(2))
    if len(versions) != 1:
        raise ValueError("Qt version missing or ambiguous in fixed metadata files")
    return versions[0]


def qt_linux_target_spec(kit):
    """Read Qt's literal target mkspec; do not execute its CMake toolchain.

    This distinguishes Linux desktop metadata from Android/Windows ARM64.
    It does not certify the GNU ABI, entitlement or dependency closure.
    """
    # CMake ignores a <Package>_DIR without its config and resumes searching.
    # Require this fixed entry point so selecting the checked kit cannot silently
    # fall back merely because its root package config is absent. Do not run it.
    if not stat.S_ISREG((kit / "lib/cmake/Qt6/Qt6Config.cmake").stat().st_mode):
        raise ValueError("Qt root package config is not a regular file")
    data, size = read_prefix(kit / "lib/cmake/Qt6/qt.toolchain.cmake")
    if size > MAX_READ:
        raise ValueError("Qt toolchain metadata too large")
    content = data.decode("utf-8")
    # This deliberately accepts generated literal metadata, not arbitrary CMake.
    # Bracket strings/comments and split assignments need a separate review.
    if re.search(r'\[=*\[', content):
        raise ValueError("Unsupported bracket syntax in Qt toolchain metadata")
    uncommented = "\n".join(line.split("#", 1)[0] for line in content.splitlines())
    starts = re.findall(r'(?i:\bset)\s*\(\s*QT_QMAKE_TARGET_MKSPEC\b', uncommented)
    assignments = [line.strip() for line in uncommented.splitlines()
                   if re.match(r'(?i:set)\s*\(\s*QT_QMAKE_TARGET_MKSPEC(?:\s|\))', line.strip())]
    if len(starts) != 1 or len(assignments) != 1:
        raise ValueError("Qt target mkspec missing or ambiguous")
    token = r'[A-Za-z0-9_+./-]+'
    match = re.fullmatch(r'(?i:set)\(\s*QT_QMAKE_TARGET_MKSPEC\s+(?:"(' + token +
                         r')"|(' + token + r'))\s+CACHE\s+STRING\s+""\s*\)\s*(?:#.*)?', assignments[0])
    if not match:
        raise ValueError("Qt target mkspec is not fixed literal metadata")
    spec = match.group(1) or match.group(2)
    if spec not in ("linux-g++", "linux-clang", "linux-aarch64-gnu-g++"):
        raise ValueError("Qt target mkspec is not a supported Linux desktop candidate")
    return spec


def qt_kit_checks(kit):
    """Inspect fixed Qt metadata and Core's layout, never load the toolkit.

    This is an early rejection gate, not a complete kit, ABI or dependency audit.
    """
    checks = []

    def add(name, passed, detail):
        checks.append({"check": name, "passed": bool(passed), "detail": detail})

    try:
        version = qt_version(kit)
        root_version = qt_version(kit, "Qt6")
        add("qt_612_metadata", version.startswith("6.12.") and root_version == version,
            {"core": version, "root": root_version})
    except (OSError, ValueError):
        add("qt_612_metadata", False, "Qt version metadata unavailable or invalid")
    try:
        add("qt_linux_desktop_metadata", True, qt_linux_target_spec(kit))
    except (OSError, ValueError):
        add("qt_linux_desktop_metadata", False, "Qt target mkspec missing, ambiguous or not Linux desktop")
    try:
        data, size = read_prefix(kit / "lib/libQt6Core.so.6")
        info = inspect_elf(data, size)
        add("qt_core_arm64_16k_layout", info["elf_type"] == 3, info)
    except (OSError, ValueError, struct.error):
        add("qt_core_arm64_16k_layout", False, "Qt Core is not a qualified ARM64 shared ELF layout")
    return checks


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


def bootstrap_text(path, limit):
    """Bound a fixed OS/kernel metadata read, including zero-size sysfs files."""
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    if len(raw) > limit:
        raise ValueError("bootstrap metadata exceeds its bound")
    return raw.decode("ascii")


def bootstrap_os():
    """Read only public OS identity fields; never source os-release as code."""
    fields = {}
    for line in bootstrap_text(Path("/etc/os-release"), 65536).splitlines():
        key, separator, value = line.partition("=")
        if key not in ("ID", "VERSION_ID", "VERSION_CODENAME"):
            continue
        if not separator or key in fields:
            raise ValueError("ambiguous OS identity")
        if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
            value = value[1:-1]
        if not re.fullmatch(r"[a-z0-9._-]{1,64}", value):
            raise ValueError("unsupported OS identity")
        fields[key] = value
    return fields


def bootstrap_probe():
    """Observe this Pi before Qt/source provisioning; never qualify a builder."""
    checks = []

    def add(name, passed, detail):
        checks.append({"check": name, "passed": bool(passed), "detail": detail})

    native = (platform.system() == "Linux" and platform.machine().lower() in ("aarch64", "arm64")
              and struct.calcsize("P") == 8
              and not any(word in platform.release().lower() for word in ("microsoft", "wsl")))
    add("native_arm64_linux", native, "requires native Linux, ARM64 and 64-bit userspace; excludes WSL")
    try:
        model = bootstrap_text(Path("/sys/firmware/devicetree/base/model"), 128).removesuffix("\0")
        if not re.fullmatch(r"Raspberry Pi 5 Model B Rev [0-9]+\.[0-9]+", model):
            raise ValueError("unexpected board")
        add("pi5_board_metadata", True, model)
    except (OSError, ValueError):
        add("pi5_board_metadata", False, "Pi 5 model metadata unavailable or unsupported")
    try:
        osr = bootstrap_os()
        add("adopted_trixie_os", osr.get("ID") in ("debian", "raspbian")
            and osr.get("VERSION_ID") == "13" and osr.get("VERSION_CODENAME") == "trixie", osr)
    except (OSError, ValueError):
        add("adopted_trixie_os", False, "public OS identity unavailable or invalid")
    try:
        size = os.sysconf("SC_PAGE_SIZE") if platform.system() == "Linux" else None
        add("running_kernel_16k_pages", type(size) is int and size == 16384, {"page_size": size})
    except (AttributeError, OSError, ValueError):
        add("running_kernel_16k_pages", False, "kernel page size unavailable")
    passed, detail = memory_check()
    add("host_ram_3gib", passed, detail)
    return {"name": "Q Pi", "kind": "read-only-bootstrap-probe", "checks": checks,
            "blockers": [item["check"] for item in checks if not item["passed"]],
            "builder_qualified": False, "build_started": False, "image_built": False,
            "not_certified": ["physical board authenticity or installed RAM capacity", "desktop and login policy",
                              "power and cooling", "storage capacity and measured scratch budget",
                              "heavy-job coordination", "namespace privileges", "package/tool versions",
                              "Qt entitlement, modules and dependency ABI", "source provenance",
                              "QOR applications", "image or hardware acceptance"]}


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
    checks.extend(qt_kit_checks(kit))
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
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--qt-only", action="store_true",
                      help="Read fixed Qt metadata and Core ELF only; no host, Git or resource checks")
    mode.add_argument("--bootstrap-only", action="store_true",
                      help="Read Pi 5/Trixie/kernel/RAM metadata without Qt, source, packages or subprocesses")
    parser.add_argument("--source", type=Path)
    parser.add_argument("--qt-target", type=Path)
    parser.add_argument("--expected-commit")
    parser.add_argument("--scratch-root", type=Path, help="Existing fixed local build directory; never created by this probe")
    parser.add_argument("--scratch-budget", type=Path, help="Measured schema-1 component budget JSON; no default estimate")
    args = parser.parse_args(argv)
    if args.bootstrap_only:
        if any(value is not None for value in
               (args.source, args.expected_commit, args.scratch_root, args.scratch_budget, args.qt_target)):
            parser.error("bootstrap-only does not accept source, commit, scratch or Qt arguments")
        report = bootstrap_probe()
        print(json.dumps(report, indent=2))
        return 2 if report["blockers"] else 0
    if args.qt_target is None:
        parser.error("qt-target is required unless bootstrap-only is selected")
    if args.qt_only:
        if any(value is not None for value in
               (args.source, args.expected_commit, args.scratch_root, args.scratch_budget)):
            parser.error("qt-only does not accept source, commit or scratch arguments")
        checks = qt_kit_checks(args.qt_target)
        report = {"name": "Q Pi", "kind": "read-only-qt-kit-probe", "checks": checks,
                  "blockers": [item["check"] for item in checks if not item["passed"]],
                  "build_started": False, "image_built": False,
                  "not_certified": ["Qt binary version", "complete Qt modules and plugins", "Linux GNU ABI",
                                    "commercial Qt entitlement", "all ELF dependencies", "native builder",
                                    "resources", "QOR applications", "physical Pi boot"]}
        print(json.dumps(report, indent=2))
        return 2 if report["blockers"] else 0
    if args.source is None or args.expected_commit is None:
        parser.error("source and expected-commit are required unless qt-only is selected")
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
