# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Stage/verify an image payload. Never install on the host or rebuild its initrd."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from assets import validate

THEME = "usr/share/plymouth/themes/qor-os/"
CONFIG = "etc/plymouth/plymouthd.conf"
DRACUT = "usr/lib/dracut/dracut.conf.d/50-qor-branding.conf"


def _read(repo: Path, name: str) -> bytes:
    path = repo / name
    if path.is_symlink() or path.resolve(strict=True) != path or not path.is_file():
        raise ValueError("Linked or non-file branding sources are refused")
    if path.stat().st_size > 1024 * 1024:
        raise ValueError("An image branding source exceeds its bound")
    return path.read_bytes()


def payload(repo: Path) -> dict[str, bytes]:
    repo = repo.resolve(strict=True)
    manifest = validate(repo)
    files = {THEME + name: _read(repo, "image/branding/" + name)
             for name in ("qor-os.plymouth",)}
    script = _read(repo, "image/branding/qor-os.script.in").decode("utf-8")
    for channel, index in (("RED", 1), ("GREEN", 3), ("BLUE", 5)):
        script = script.replace("@BACKGROUND_" + channel + "@",
                                f"{int(manifest['background'][index:index + 2], 16) / 255:.9f}")
    if "@BACKGROUND_" in script:
        raise ValueError("The boot background template is incomplete")
    files[THEME + "qor-os.script"] = script.encode("utf-8")
    files.update({THEME + name: _read(repo, "kit/branding/" + name)
                  for name in ("qor-mark.png", "manifest.json", "qor-mark.png.license")})
    files[CONFIG] = _read(repo, "image/branding/plymouthd.conf")
    paths = " ".join("/" + name for name in files)
    files[DRACUT] = ("# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary\n"
                     'add_dracutmodules+=" plymouth "\n'
                     f'install_items+=" {paths} "\n').encode()
    return files


def confined(repo: Path, output: Path) -> Path:
    # Inspect every existing ancestor before resolve: also rejects Windows
    # junctions and links which happen to point back inside build/.
    build = repo.absolute() / "build"
    output = output.absolute()
    if ".." in output.parts or not output.is_relative_to(build) or output == build:
        raise ValueError("The payload must be a child of this checkout's build directory")
    for path in (build, *output.relative_to(build).parents):
        candidate = path if path.is_absolute() else build / path
        if candidate.exists() and candidate.resolve() != candidate:
            raise ValueError("Linked staging paths are refused")
    for candidate in (output, *output.parents):
        if candidate.is_symlink() or (candidate.exists() and candidate.resolve() != candidate):
            raise ValueError("Linked staging paths are refused")
        if candidate == build:
            break
    return output


def verify(repo: Path, output: Path) -> int:
    output = confined(repo, output)
    expected = payload(repo)
    if not output.is_dir():
        raise ValueError("There is no staged payload")
    actual = set()
    for path in output.rglob("*"):
        if path.is_symlink() or path.resolve() != path:
            raise ValueError("Linked payload entries are refused")
        if path.is_file():
            name = path.relative_to(output).as_posix()
            actual.add(name)
            if name not in expected or path.read_bytes() != expected[name]:
                raise ValueError("A staged payload file differs")
        elif not path.is_dir():
            raise ValueError("Special payload entries are refused")
    if actual != set(expected):
        raise ValueError("The image payload is incomplete")
    return len(expected)


def stage(repo: Path, output: Path) -> int:
    output = confined(repo, output)
    expected = payload(repo)
    if output.exists():
        # Incremental CMake invocation verifies; never silently overwrites.
        return verify(repo, output)
    output.mkdir(parents=True, exist_ok=False)
    for name, data in expected.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as stream:
            if stream.write(data) != len(data):
                raise OSError("Incomplete payload write")
    return verify(repo, output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    try:
        count = (verify if args.verify else stage)(repo, args.output)
    except (OSError, ValueError) as error:
        print(f"Image branding refused: {error}", file=sys.stderr)
        return 1
    print(json.dumps({"files": count, "scope": "filesystem payload only; no boot or initramfs proof"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
