# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Verify a saved data-only Docker image without extracting it or running it."""
import argparse
import io
import json
from pathlib import Path, PurePosixPath
import tarfile

from assemble import payload


def safe_name(name: str) -> str:
    if not isinstance(name, str) or not name:
        raise ValueError("Invalid archive path")
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or "\\" in name:
        raise ValueError("Unsafe archive path")
    return str(path)


def verify(archive: Path, expected=None) -> int:
    expected = payload() if expected is None else expected
    if archive.is_symlink() or not archive.is_file() or archive.stat().st_size > 64 * 1024 * 1024:
        raise ValueError("A bounded regular Docker archive is required")
    with tarfile.open(archive, "r:*") as outer:
        members = []
        total = 0
        for member in outer:
            total += member.size
            if len(members) >= 4096 or member.size > 32 * 1024 * 1024 or total > 64 * 1024 * 1024:
                raise ValueError("Docker archive entries exceed their bounds")
            members.append(member)
        names = [safe_name(member.name) for member in members]
        if len(names) != len(set(names)):
            raise ValueError("Duplicate Docker archive entry")
        by_name = dict(zip(names, members))
        manifest = by_name.get("manifest.json")
        if manifest is None or not manifest.isfile() or manifest.size > 1024 * 1024:
            raise ValueError("Missing or oversized Docker manifest")
        images = json.loads(outer.extractfile(manifest).read())
        if not isinstance(images, list) or len(images) != 1 or not isinstance(images[0], dict):
            raise ValueError("Exactly one data-only image is required")
        layers = images[0].get("Layers")
        if not isinstance(layers, list) or len(layers) != 1 or not isinstance(layers[0], str):
            raise ValueError("The data-only image must have one layer")
        layer = by_name.get(safe_name(layers[0]))
        if layer is None or not layer.isfile() or layer.size > 32 * 1024 * 1024:
            raise ValueError("Missing or oversized data-only layer")
        seen = set()
        all_paths = set()
        with tarfile.open(fileobj=io.BytesIO(outer.extractfile(layer).read()), mode="r:*") as content:
            for member in content:
                name = safe_name(member.name)
                if name in all_paths or len(all_paths) >= len(expected) * 4 + 32:
                    raise ValueError("Duplicate or excessive image layer entries")
                all_paths.add(name)
                if member.isdir():
                    if name in expected:
                        raise ValueError("Directory shadows an image payload file")
                    continue
                if name in seen or name not in expected or not member.isfile() or member.size != len(expected[name]):
                    raise ValueError("Unexpected, linked, duplicate or incomplete image payload entry")
                if content.extractfile(member).read() != expected[name]:
                    raise ValueError("Image branding bytes differ from the reviewed source")
                seen.add(name)
        if seen != set(expected):
            raise ValueError("The saved image is missing branding files")
    return len(seen)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    try:
        print(f"{verify(args.archive)} source-matching image payload files verified; no OS boot/install proof")
    except (OSError, ValueError, tarfile.TarError) as error:
        parser.exit(1, f"Image archive refused: {error}\n")
