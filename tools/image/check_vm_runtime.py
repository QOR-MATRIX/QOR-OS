# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Validate the complete private runtime in its target image, not on the host."""
import os
from pathlib import Path
import re
import subprocess
import sys


def check_files(paths, runner=subprocess.run):
    failures = []
    for path in paths:
        result = runner(["/usr/bin/ldd", str(path)], capture_output=True, text=True, timeout=30,
                        env={"PATH": "/usr/sbin:/usr/bin", "LANG": "C.UTF-8", "HOME": "/var/roothome",
                             "LD_LIBRARY_PATH": "/usr/lib/qor-os/qt/lib"})
        missing = re.findall(r"^\s*(\S+)\s+=>\s+not found\s*$", result.stdout, re.MULTILINE)
        if result.returncode or missing or "not found" in result.stdout or "not found" in result.stderr:
            failures.append({"file": path.name, "missing": missing, "exit": result.returncode})
    return failures


def main():
    if sys.platform != "linux" or os.environ.get("QOR_VM_RUNTIME_AUDIT") != "1":
        raise ValueError("Runtime audit is an explicit target-image build step")
    runtime = Path("/usr/lib/qor-os/qt")
    programs = [Path("/usr/bin/qor-setup")]
    if Path("/usr/bin/qor-install").is_file():
        programs.append(Path("/usr/bin/qor-install"))
    paths = programs + sorted(path for path in runtime.rglob("*") if path.is_file() and ".so" in path.name)
    if len(paths) < 10:
        raise ValueError("The complete staged runtime is missing")
    for path in paths:
        with path.open("rb") as stream:
            if stream.read(4) != b"\x7fELF":
                raise ValueError("A runtime library or program is not ELF")
    failures = check_files(paths)
    for failure in failures:
        print(f"Unresolved runtime: {failure['file']} (exit {failure['exit']}): {', '.join(failure['missing'])}")
    if failures:
        return 1
    print(f"Target-image runtime dependencies resolved: {len(paths)} ELF programs/libraries/plugins")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.SubprocessError):
        raise SystemExit("Runtime audit failed; no complete dependency proof")
