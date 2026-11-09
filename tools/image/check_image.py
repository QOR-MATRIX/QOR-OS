# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Read-only structural checks inside an assembled device container, never an installer."""
from pathlib import Path
import json
import subprocess
import sys

REQUIRED = (
    "usr/bin/prism", "usr/bin/qor-setup", "usr/bin/bootc",
    "usr/lib/systemd/system/qor-session.service",
    "usr/lib/os-release",
)


def check(root: Path, probe=None) -> list[str]:
    errors = []
    for name in REQUIRED:
        path = root / name
        if not path.is_file():
            errors.append("Missing device component: " + name)
    if (root / "usr/lib/qor-os/live-session").exists():
        errors.append("Live installation backend has not been integrated or proved")
    if (root / "etc/calamares/settings.conf").exists():
        errors.append("An active installer configuration is not supported by this checkpoint")
    identity = root / "usr/lib/os-release"
    if identity.is_file() and (identity.stat().st_size > 4096 or b"ID=qor-os\n" not in identity.read_bytes()):
        errors.append("The device identity is not the QOR OS identity")
    if not errors and probe is None:
        errors.append("Device application capability probes are required")
    elif not errors:
        capabilities = probe(root / "usr/bin/qor-setup")
        if (not isinstance(capabilities, dict) or set(capabilities) != {"schema", "development", "firstRun"}
                or type(capabilities.get("schema")) is not int or capabilities["schema"] != 1
                or capabilities.get("development") is not False or capabilities.get("firstRun") is not True):
            errors.append("The first-run application is a development build or has an unknown contract")
        capabilities = probe(root / "usr/bin/prism")
        if (not isinstance(capabilities, dict) or set(capabilities) != {"schema", "development", "authentication"}
                or type(capabilities.get("schema")) is not int or capabilities["schema"] != 1
                or capabilities.get("development") is not False or capabilities.get("authentication") is not True):
            errors.append("Prism is simulated, lacks authentication, or has an unknown build contract")
    return errors


def main() -> int:
    def probe(program):
        result = subprocess.run([str(program), "--build-capabilities"], capture_output=True, timeout=10)
        if result.returncode:
            raise ValueError("The first-run build check failed")
        return json.loads(result.stdout)
    try:
        errors = check(Path("/"), probe)
    except (OSError, ValueError, subprocess.TimeoutExpired):
        errors = ["The device application contract could not be checked"]
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        return 1
    print("Structural device checks pass; boot, install, recovery and hardware readiness remain unproven")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
