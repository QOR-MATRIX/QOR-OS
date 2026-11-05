# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Assemble checked QOR image data; build only against an explicit existing device base."""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/branding"))
from stage_image import confined, payload as boot_payload, _read

spec = importlib.util.spec_from_file_location("image_lumen_tokens", REPO / "tools/tokens/generate_tokens.py")
tokens = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tokens)
BRAND = "usr/share/calamares/branding/qor-os/"
INACTIVE = "usr/share/qor-os/installer/inactive/"


def payload(repo: Path = REPO) -> dict[str, bytes]:
    files = boot_payload(repo)
    source = json.loads(_read(repo, "kit/tokens/lumen.tokens.json"))
    palette = {name: tokens.role_hex(role, "dark", False, source["color"]["accentDefaultHue"])
               for name, role in source["color"]["roles"].items()}
    # Only opaque colors are used by the separate framework's stylesheet.
    for name in ("background", "surface", "surfaceRaised", "surfaceSunken", "textPrimary",
                 "textSecondary", "textDisabled", "accent", "accentText", "onAccent"):
        if re.fullmatch(r"#[0-9a-f]{6}", palette[name]) is None:
            raise ValueError("Installer palette must use opaque Lumen roles")
    for name in ("branding.desc", "stylesheet.qss", "show.qml"):
        template = _read(repo, "image/installer/" + name + ".in").decode("utf-8")
        for key, value in palette.items():
            template = template.replace("@" + key + "@", value)
        for key, value in {"radius_m": source["radius"]["m"], "radius_xs": source["radius"]["xs"],
                           "spacing_xl": source["spacing"]["xl"], "title_size": source["type"]["title2"]["size"]}.items():
            template = template.replace("@" + key + "@", str(value))
        if re.search(r"@[A-Za-z_]+@", template):
            raise ValueError("An installer branding field is unresolved")
        files[BRAND + name] = template.encode("utf-8")
    for name in ("qor-mark.png", "qor-mark.png.license", "manifest.json"):
        files[BRAND + name] = _read(repo, "kit/branding/" + name)
    files[BRAND + "ATTRIBUTION.md"] = _read(repo, "image/installer/ATTRIBUTION.md")
    files[BRAND + "branding.desc.license"] = _read(repo, "image/installer/branding.desc.in.license")
    for name in ("settings.conf.in", "partition.conf", "finished.conf", "iso.yaml"):
        files[INACTIVE + name] = _read(repo, "image/installer/" + name)
    files["usr/share/qor-os/first-run/Welcome.qml"] = _read(repo, "setup/Welcome.qml")
    files["usr/lib/os-release"] = _read(repo, "image/os-release")
    files["usr/share/icons/hicolor/1024x1024/apps/qor-os.png"] = _read(repo, "kit/branding/qor-mark.png")
    files["usr/share/icons/hicolor/1024x1024/apps/qor-os.png.license"] = _read(repo, "kit/branding/qor-mark.png.license")
    files["usr/libexec/qor-os/check-image.py"] = _read(repo, "tools/image/check_image.py")
    files["usr/share/qor-os/installer/capabilities.json"] = json.dumps({
        "schema": 1, "branding": "qor-os", "frameworkCandidate": "Calamares",
        "partitioningBackendIntegrated": False, "bootcDeploymentAdapter": None,
        "installedSystemBootProved": False, "storageModesProved": [],
        "intendedFirmware": "UEFI", "legacyBIOSProved": False,
    }, indent=2).encode() + b"\n"
    return files


def verify(output: Path, repo: Path = REPO) -> int:
    output = confined(repo, output)
    expected = payload(repo)
    seen = set()
    if not output.is_dir():
        raise ValueError("There is no assembled image payload")
    for path in output.rglob("*"):
        if path.is_symlink() or path.resolve() != path:
            raise ValueError("Linked image entries are refused")
        if path.is_dir():
            continue
        name = path.relative_to(output).as_posix()
        if name not in expected or not path.is_file() or path.stat().st_size != len(expected[name]):
            raise ValueError("An unexpected or incomplete image entry was found")
        if path.read_bytes() != expected[name]:
            raise ValueError("An image payload entry differs")
        seen.add(name)
    if seen != set(expected):
        raise ValueError("The image payload is incomplete")
    return len(seen)


def stage(output: Path, repo: Path = REPO) -> int:
    output = confined(repo, output)
    files = payload(repo)
    if output.exists():
        return verify(output, repo)
    output.mkdir(parents=True, exist_ok=False)
    for name, contents in files.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as stream:
            if stream.write(contents) != len(contents):
                raise OSError("Incomplete image payload write")
    return verify(output, repo)


def existing_image(reference: str) -> str:
    if re.fullmatch(r"[a-z0-9./:_-]+@sha256:[0-9a-f]{64}", reference) is None:
        raise ValueError("An explicit existing image digest is required; tags are refused")
    result = subprocess.run(["docker", "image", "inspect", "--format", "{{.Id}}", reference],
                            capture_output=True, text=True, timeout=15)
    if result.returncode or re.fullmatch(r"sha256:[0-9a-f]{64}", result.stdout.strip()) is None:
        raise ValueError("The approved image is unavailable locally; no pull was attempted")
    return reference


def build_device(output: Path, base: str | None, repo: Path = REPO) -> int:
    verify(output, repo)
    if not base:
        raise ValueError("An approved existing Fedora device image digest is required")
    reference = existing_image(base)
    command = ["docker", "build", "--network=none", "--pull=false", "--file", str(repo / "image/Containerfile"),
               "--build-arg", "QOR_BASE_IMAGE=" + reference,
               "--tag", "qor-os/device:development", str(output)]
    return subprocess.run(command, timeout=600).returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("stage", "verify", "device-build"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--base")
    args = parser.parse_args()
    try:
        count = stage(args.output) if args.command == "stage" else verify(args.output)
        if args.command == "device-build":
            return build_device(args.output, args.base)
        print(json.dumps({"files": count, "scope": "image filesystem payload; no OS installation or boot proof"}))
        return 0
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print("Image assembly refused: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
