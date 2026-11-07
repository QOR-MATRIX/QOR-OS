# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Private host-built VM runtime closure, never a Qt kit or distribution grant."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from vmguard import WORKSPACE, confined, unlinked

REPO = Path(__file__).resolve().parents[2]


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def source_snapshot():
    if subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=REPO):
        raise ValueError("Commit the reviewed runtime source before staging; no dirty build is accepted")
    command = ["git", "ls-files", "-z", "--cached", "--",
               "CMakeLists.txt", "CMakePresets.json", "cmake", "kit", "setup", "platform/common",
               "audio", "input", "services", "store", "apps", "prism", "tools/branding"]
    names = subprocess.check_output(command, cwd=REPO).decode().split("\0")
    files = {}
    for name in names:
        if not name:
            continue
        path = unlinked(REPO / name)
        if path.is_file():
            files[name] = sha256(path)
    return {"revision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip(),
            "dirty": False,
            "inputs": files}


def checked_build(command, snapshot=source_snapshot, runner=subprocess.run):
    before = snapshot()
    env = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL="1", CARGO_BUILD_JOBS="1", LP_NUM_THREADS="1")
    result = runner(command, cwd=REPO, env=env, capture_output=True, text=True)
    diagnostics = result.stdout + result.stderr
    if result.returncode or re.search(r"\bwarning\b|FAILED:|\berror:", diagnostics, re.IGNORECASE):
        raise ValueError("The fresh runtime build or its warning gate failed; runtime not accepted")
    if snapshot() != before:
        raise ValueError("Source inputs changed during the runtime build; runtime not accepted")
    return before


def stage(qt, build, output, installer):
    if sys.platform != "linux":
        raise ValueError("Use the existing Linux commercial build host; no kit is installed in the guest")
    qt, build = unlinked(qt), unlinked(build)
    cache = (build / "CMakeCache.txt").read_text()
    home = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$", cache, re.MULTILINE)
    kit = re.search(r"^Qt6_DIR:PATH=(.+)$", cache, re.MULTILINE)
    if (not home or not os.path.samefile(home[1], REPO) or not kit
            or not os.path.samefile(kit[1], qt / "lib/cmake/Qt6")
            or "QOR_WARNINGS_AS_ERRORS:BOOL=ON" not in cache or "QOR_BUILD_SIMULATOR:BOOL=OFF" not in cache
            or (installer and "QOR_BUILD_VM_INSTALLER:BOOL=ON" not in cache)):
        raise ValueError("An explicitly configured QOR device build is required")
    output = confined(output, WORKSPACE)
    if output.exists():
        raise ValueError("Preserving existing runtime output; use a fresh directory")
    targets = ["qor-setup", "qor_setup_qmllint"]
    if installer:
        targets.append("qor-install")
    build_proof = checked_build(["cmake", "--build", str(build), "--target", *targets, "--parallel", "1"])
    output.mkdir(exist_ok=False)
    files, libraries = {}, {}

    def copy(source, target):
        destination = output / target
        resolved = source.resolve(strict=True)
        if not resolved.is_file() or resolved.stat().st_size > 500 * 1024**2:
            raise ValueError("Unexpected runtime input")
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            if sha256(destination) != sha256(resolved):
                raise ValueError("Conflicting runtime destination")
            return
        shutil.copyfile(resolved, destination)
        files[destination.relative_to(output).as_posix()] = sha256(destination)

    def dependencies(path):
        result = subprocess.check_output(["ldd", str(path)], text=True)
        if "not found" in result:
            raise ValueError("A host dependency is missing; runtime not accepted")
        for target in re.findall(r"=> (/\S+)", result):
            original = Path(target)
            if original.resolve().is_relative_to(qt / "lib"):
                previous = libraries.setdefault(original.name, original)
                if previous.resolve() != original.resolve():
                    raise ValueError("Ambiguous Qt library identity")

    capabilities = {}
    programs = [("qor-setup", {"schema": 1, "development": False, "firstRun": True})]
    if installer:
        programs.append(("qor-install", {"schema": 1, "vmOnly": True, "encryption": False}))
    for name, expected in programs:
        program = build / "setup" / name
        capabilities[name] = json.loads(subprocess.check_output([str(program), "--build-capabilities"]))
        if capabilities[name] != expected:
            raise ValueError("Development or unknown application capabilities")
        copy(program, "usr/bin/" + name)
        (output / "usr/bin" / name).chmod(0o755)
        dependencies(program)
    imports = json.loads(subprocess.check_output([str(qt / "libexec/qmlimportscanner"), "-rootPath", str(REPO / "setup"),
                                                "-importPath", str(qt / "qml"), "-importPath", str(build / "qml")]))
    roots = {Path(module["path"]) for module in imports if module.get("path")
             and Path(module["path"]).resolve().is_relative_to(qt / "qml")}

    def module_files(root):
        for path in sorted(root.iterdir()):
            if path.is_file():
                if path.suffix not in (".a", ".debug", ".prl", ".cmake"):
                    yield path
            elif path.is_dir() and not (path / "qmldir").exists():
                yield from module_files(path)

    for root in sorted(roots):
        for source in module_files(root):
            if not source.resolve().is_relative_to(qt):
                raise ValueError("Qt module escapes the existing kit")
            copy(source, "usr/lib/qor-os/qt/qml/" + source.relative_to(qt / "qml").as_posix())
            if ".so" in source.name:
                dependencies(source)
    for relative in ("platforms/libqlinuxfb.so", "imageformats/libqsvg.so"):
        source = qt / "plugins" / relative
        copy(source, "usr/lib/qor-os/qt/plugins/" + relative)
        dependencies(source)
    for name, source in sorted(libraries.items()):
        copy(source, "usr/lib/qor-os/qt/lib/" + name)
    if source_snapshot() != build_proof:
        raise ValueError("Sources changed while staging the runtime; no receipt accepted")
    revision = build_proof["revision"]
    record = {"schema": 1, "profile": "private-vm-runtime-only", "source_revision": revision,
              "source_dirty": build_proof["dirty"], "fresh_build_inputs": build_proof["inputs"],
              "capabilities": capabilities, "files": files, "qml_imports": imports,
              "notice": "Original Qt runtime notices are retained. No Qt kit, credentials or public distribution approval is included."}
    (output.parent / (output.name + "-record.json")).write_text(json.dumps(record, indent=2) + "\n")
    return {"files": len(files), "source_revision": revision, "source_dirty": record["source_dirty"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--installer", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(stage(args.qt, args.build, args.output, args.installer)))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError):
        parser.exit(1, "VM runtime staging refused; no accepted runtime record was produced\n")


if __name__ == "__main__":
    main()
