# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Allow only a newly created QOR test disk and private firmware variables.

This module never opens a block device or installs an OS. Guest observations
must be collected again immediately before each destructive backend invocation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import uuid

WORKSPACE = Path(__file__).resolve().parents[2] / "build"


def unlinked(path: Path) -> Path:
    path = path.absolute()
    for item in (path, *path.parents):
        if item.is_symlink() or getattr(item, "is_junction", lambda: False)():
            raise ValueError("Linked paths are not test artifacts")
    if path.resolve() != path:
        raise ValueError("Noncanonical artifact ancestry")
    return path


def identity(path: Path) -> list[int]:
    info = unlinked(path).stat()
    if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or not info.st_ino:
        raise ValueError("A single-link regular file is required")
    return [info.st_dev, info.st_ino, info.st_size]


def ancestry(root: Path) -> list[dict]:
    result = []
    for item in (root, *root.parents):
        info = unlinked(item).stat()
        if not stat.S_ISDIR(info.st_mode):
            raise ValueError("Artifact parent is not a directory")
        result.append({"path": str(item), "id": [info.st_dev, info.st_ino]})
    return result


def confined(root: Path, workspace: Path) -> Path:
    root, workspace = unlinked(root), unlinked(workspace)
    if root == workspace or not root.is_relative_to(workspace):
        raise ValueError("A fresh directory inside the QOR build workspace is required")
    return root


def create(root: Path, firmware_template: Path, size: int, source: str, image_sha256: str,
           workspace: Path = WORKSPACE) -> dict:
    root = confined(root, workspace)
    template = unlinked(firmware_template)
    if not template.is_relative_to(unlinked(workspace)):
        raise ValueError("Firmware template must be an extracted workspace input")
    template_id = identity(template)
    if not 4096 <= template_id[2] <= 16 * 1024 * 1024:
        raise ValueError("Unexpected firmware variable template size")
    if type(size) is not int or not 1024 * 1024 <= size <= 64 * 1024**3:
        raise ValueError("Unexpected virtual disk capacity")
    if not re.fullmatch(r"[0-9a-f]{40}", source) or not re.fullmatch(r"[0-9a-f]{64}", image_sha256):
        raise ValueError("Exact source revision and image checksum are required")
    root.mkdir(mode=0o700, parents=False, exist_ok=False)
    disk, variables = root / "install.raw", root / "nvram.fd"
    with disk.open("xb") as stream:
        stream.truncate(size)
        stream.flush()
        os.fsync(stream.fileno())
    with variables.open("xb") as stream:
        stream.write(template.read_bytes())
        stream.flush()
        os.fsync(stream.fileno())
    for path in (disk, variables):
        path.chmod(0o600)
    vm_id = str(uuid.uuid4())
    record = {"schema": 1, "root": str(root), "vm_uuid": vm_id,
              "source_revision": source, "image_sha256": image_sha256,
              "ancestry": ancestry(root),
              "disk": {"name": disk.name, "id": identity(disk), "format": "raw",
                       "backing_file": None, "serial": "qor-" + uuid.uuid4().hex[:16]},
              "nvram": {"name": variables.name, "id": identity(variables),
                        "template_sha256": hashlib.sha256(template.read_bytes()).hexdigest()}}
    with (root / "allowlist.json").open("x", encoding="utf-8") as stream:
        json.dump(record, stream, indent=2)
        stream.write("\n")
    return verify(root / "allowlist.json", workspace)


def validate_metadata(record: dict) -> None:
    """Strict shared contract, without consulting any host/guest disk paths."""
    if not isinstance(record, dict) or set(record) != {
            "schema", "root", "vm_uuid", "source_revision", "image_sha256", "ancestry", "disk", "nvram"}:
        raise ValueError("Unknown allowlist contract")
    if (type(record["schema"]) is not int or record["schema"] != 1
            or not isinstance(record["root"], str) or len(record["root"]) > 4096):
        raise ValueError("Invalid allowlist schema or root")
    if str(uuid.UUID(record["vm_uuid"])) != record["vm_uuid"]:
        raise ValueError("Invalid VM identity")
    if (not re.fullmatch(r"[0-9a-f]{40}", record["source_revision"])
            or not re.fullmatch(r"[0-9a-f]{64}", record["image_sha256"])):
        raise ValueError("Missing source/image provenance")
    if not isinstance(record["ancestry"], list) or not 1 <= len(record["ancestry"]) <= 128:
        raise ValueError("Invalid test workspace ancestry")
    for parent in record["ancestry"]:
        if (not isinstance(parent, dict) or set(parent) != {"path", "id"}
                or not isinstance(parent["path"], str) or len(parent["path"]) > 4096
                or not isinstance(parent["id"], list) or len(parent["id"]) != 2
                or any(type(value) is not int or value < 0 for value in parent["id"])):
            raise ValueError("Invalid parent identity")
    disk, variables = record["disk"], record["nvram"]
    if (not isinstance(disk, dict) or set(disk) != {"name", "id", "format", "backing_file", "serial"}
            or disk["name"] != "install.raw" or disk["format"] != "raw" or disk["backing_file"] is not None
            or not re.fullmatch(r"qor-[0-9a-f]{16}", disk["serial"])):
        raise ValueError("Only the newly created raw disk is allowed")
    if (not isinstance(variables, dict) or set(variables) != {"name", "id", "template_sha256"} or variables["name"] != "nvram.fd"
            or not re.fullmatch(r"[0-9a-f]{64}", variables["template_sha256"])):
        raise ValueError("Private firmware variables are required")
    for entry in (disk, variables):
        if (not isinstance(entry["id"], list) or len(entry["id"]) != 3
                or any(type(value) is not int or value < 0 for value in entry["id"])
                or not entry["id"][1]):
            raise ValueError("Invalid regular-file identity")
    if (not 1024**2 <= disk["id"][2] <= 64 * 1024**3
            or not 4096 <= variables["id"][2] <= 16 * 1024**2):
        raise ValueError("Invalid disk or private firmware capacity")


def verify(manifest: Path, workspace: Path = WORKSPACE) -> dict:
    root = confined(manifest.parent, workspace)
    if manifest.name != "allowlist.json" or identity(manifest)[2] > 16384:
        raise ValueError("Invalid allowlist file")
    record = json.loads(manifest.read_text(encoding="utf-8"))
    validate_metadata(record)
    if record["root"] != str(root) or record["ancestry"] != ancestry(root):
        raise ValueError("Test workspace ancestry changed")
    for entry in (record["disk"], record["nvram"]):
        if identity(root / entry["name"]) != entry["id"]:
            raise ValueError("An allowlisted artifact was replaced or resized")
    return record


def guest_target(expected: dict, vm_uuid: str, disks: list[dict]) -> dict:
    """Validate fresh guest inventory, not a device name or a VM detector.

    The fixed PCI slot and virtio driver must match the sealed launcher. All
    writable guest disks must match the one-item allowlist; read-only optical
    installation media is excluded by the inventory collector, not by this test.
    """
    if vm_uuid.lower() != expected["vm_uuid"] or len(disks) != 1:
        raise ValueError("VM identity or writable-disk inventory does not match")
    disk = disks[0]
    if set(disk) != {"path", "serial", "bytes", "driver", "sysfs", "major_minor", "busy", "read_only"}:
        raise ValueError("Incomplete guest disk observation")
    if (not re.fullmatch(r"/dev/[a-z][a-z0-9]*", disk["path"])
            or disk["serial"] != expected["disk"]["serial"]
            or type(disk["bytes"]) is not int or disk["bytes"] != expected["disk"]["id"][2]
            or disk["driver"] != "virtio_blk"
            or not re.fullmatch(r"/sys/devices/pci0000:00/0000:00:04\.0/virtio[0-9]+/block/[a-z][a-z0-9]*", disk["sysfs"])
            or not re.fullmatch(r"[0-9]+:[0-9]+", disk["major_minor"])
            or disk["busy"] is not False or disk["read_only"] is not False):
        raise ValueError("Unexpected disk identity, ancestry, capacity or active use")
    if Path(disk["sysfs"]).name != Path(disk["path"]).name:
        raise ValueError("Device node and sysfs ancestry disagree")
    return disk


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    new = sub.add_parser("create")
    new.add_argument("--root", type=Path, required=True)
    new.add_argument("--vars-template", type=Path, required=True)
    new.add_argument("--size-gib", type=int, choices=range(8, 65), default=16)
    new.add_argument("--source", required=True)
    new.add_argument("--image-sha256", required=True)
    check = sub.add_parser("verify")
    check.add_argument("manifest", type=Path)
    args = parser.parse_args()
    try:
        record = (create(args.root, args.vars_template, args.size_gib * 1024**3, args.source, args.image_sha256)
                  if args.action == "create" else verify(args.manifest))
        print(json.dumps(record, indent=2))
        return 0
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(1, f"Disposable VM guard refused: {error}\n")


if __name__ == "__main__":
    main()
