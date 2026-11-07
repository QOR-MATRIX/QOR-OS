# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""A bounded, complete offline OCI image; no extraction or network fallback."""
import hashlib
import json
import os
import re

MANIFEST = "application/vnd.oci.image.manifest.v1+json"
CONFIG = "application/vnd.oci.image.config.v1+json"
LAYERS = frozenset(("application/vnd.oci.image.layer.v1.tar",
                    "application/vnd.oci.image.layer.v1.tar+gzip",
                    "application/vnd.oci.image.layer.v1.tar+zstd"))


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate payload JSON key")
        result[key] = value
    return result


def validate(root, receipt, opener):
    if (root.is_symlink() or not root.is_dir() or not isinstance(receipt, dict)
            or set(receipt) != {"schema", "source_revision", "manifest_sha256", "profile", "encryption"}
            or type(receipt["schema"]) is not int or receipt["schema"] != 2
            or receipt["profile"] != "private-vm-lab" or receipt["encryption"] is not False
            or not re.fullmatch(r"[0-9a-f]{40}", receipt["source_revision"])
            or not re.fullmatch(r"[0-9a-f]{64}", receipt["manifest_sha256"])):
        raise ValueError("Unknown offline image receipt")
    allowed = {"oci-layout", "index.json"}
    total = 0

    def document(name, bound):
        with opener(root / name) as stream:
            raw = stream.read(bound + 1)
        if len(raw) > bound:
            raise ValueError("Oversized OCI metadata")
        value = json.loads(raw, object_pairs_hook=unique_object)
        if not isinstance(value, dict):
            raise ValueError("Unknown OCI metadata")
        return value

    def descriptor(value, media, bound, metadata=False):
        nonlocal total
        if (not isinstance(value, dict) or not {"mediaType", "digest", "size"} <= set(value)
                or set(value) - {"mediaType", "digest", "size", "annotations"}
                or value["mediaType"] not in media or not isinstance(value["digest"], str)
                or not re.fullmatch(r"sha256:[0-9a-f]{64}", value["digest"])
                or type(value["size"]) is not int or not 1 <= value["size"] <= bound):
            raise ValueError("Unsupported or external OCI descriptor")
        name = "blobs/sha256/" + value["digest"][7:]
        allowed.add(name)
        total += value["size"]
        if total > 16 * 1024**3:
            raise ValueError("Oversized offline image")
        with opener(root / name) as stream:
            info = os.fstat(stream.fileno())
            if info.st_size != value["size"] or hashlib.file_digest(stream, "sha256").hexdigest() != value["digest"][7:]:
                raise ValueError("Offline blob size or checksum differs")
        return document(name, bound) if metadata else None

    if document("oci-layout", 4096) != {"imageLayoutVersion": "1.0.0"}:
        raise ValueError("Unknown OCI layout")
    index = document("index.json", 16384)
    if (index.get("schemaVersion") != 2 or type(index["schemaVersion"]) is not int
            or not isinstance(index.get("manifests"), list) or len(index["manifests"]) != 1):
        raise ValueError("Exactly one offline installation image is required")
    selected = index["manifests"][0]
    manifest = descriptor(selected, {MANIFEST}, 128 * 1024, True)
    if selected["digest"] != "sha256:" + receipt["manifest_sha256"]:
        raise ValueError("Wrong installation manifest")
    if manifest.get("schemaVersion") != 2 or type(manifest["schemaVersion"]) is not int:
        raise ValueError("Unknown installation manifest")
    config = descriptor(manifest.get("config"), {CONFIG}, 2 * 1024**2, True)
    settings = config.get("config")
    labels = settings.get("Labels") if isinstance(settings, dict) else None
    if not isinstance(labels, dict):
        raise ValueError("Missing image source labels")
    if (config.get("os") != "linux" or config.get("architecture") != "amd64"
            or labels.get("org.opencontainers.image.revision") != receipt["source_revision"]
            or labels.get("dev.qorsync.vm-profile") != "installed"):
        raise ValueError("Unknown architecture, profile or source revision")
    layers = manifest.get("layers")
    if not isinstance(layers, list) or not 1 <= len(layers) <= 128:
        raise ValueError("Missing or excessive installation layers")
    for layer in layers:
        descriptor(layer, LAYERS, 8 * 1024**3)
    directories = {"blobs", "blobs/sha256"}
    observed = set()
    for path in root.rglob("*"):
        name = path.relative_to(root).as_posix()
        if path.is_symlink() or name not in allowed | directories:
            raise ValueError("Unexpected or linked OCI ancestry")
        if not path.is_dir():
            observed.add(name)
    if observed != allowed:
        raise ValueError("Missing or unexpected offline payload files")
    return receipt
