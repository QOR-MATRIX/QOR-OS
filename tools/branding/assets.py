# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Validate the reviewed rendering of the existing QorMark, without loading images."""
from __future__ import annotations

import hashlib
import json
import re
import struct
import zlib
from pathlib import Path

INPUTS = (
    "kit/QorMark.qml", "kit/Surface.qml", "kit/Tokens.qml", "kit/shaders/surface.frag",
    "kit/src/oklchmath.cpp", "kit/tokens/lumen.tokens.json",
    "tools/branding/Mark.qml",
)


def _pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError("Duplicate branding field")
        result[key] = value
    return result


def _png(image: bytes) -> None:
    if image[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Invalid branding PNG")
    offset = 8
    compressed = bytearray()
    first = True
    ended = False
    while offset + 12 <= len(image):
        size = struct.unpack_from(">I", image, offset)[0]
        kind = image[offset + 4:offset + 8]
        end = offset + size + 12
        if end > len(image):
            raise ValueError("Truncated branding PNG")
        chunk = image[offset + 8:end - 4]
        if zlib.crc32(kind + chunk) != struct.unpack_from(">I", image, end - 4)[0]:
            raise ValueError("Branding PNG checksum differs")
        if first:
            if kind != b"IHDR" or chunk != struct.pack(">IIBBBBB", 1024, 1024, 8, 6, 0, 0, 0):
                raise ValueError("Branding must be a square, non-interlaced RGBA image")
            first = False
        elif kind == b"IHDR":
            raise ValueError("Duplicate branding PNG header")
        if kind == b"IDAT":
            compressed.extend(chunk)
        if kind == b"IEND":
            ended = size == 0 and end == len(image)
            break
        offset = end
    if not ended or not compressed:
        raise ValueError("Branding PNG is incomplete")
    # Refuse compressed-image bombs before a GUI or early-boot decoder sees it.
    expected = 1024 * (1024 * 4 + 1)
    decoder = zlib.decompressobj()
    try:
        pixels = decoder.decompress(compressed, expected + 1)
    except zlib.error as error:
        raise ValueError("Invalid branding PNG pixels") from error
    if (len(pixels) != expected or not decoder.eof or decoder.unused_data
            or any(pixels[row * 4097] > 4 for row in range(1024))):
        raise ValueError("Branding PNG pixels exceed their bounds or are incomplete")


def validate(repo: Path, assets: Path | None = None) -> dict:
    repo = repo.resolve(strict=True)
    assets = assets or repo / "kit/branding"
    manifest_path = assets / "manifest.json"
    logo_path = assets / "qor-mark.png"
    for path in (manifest_path, logo_path):
        if path.is_symlink() or not path.is_file():
            raise ValueError("A branding file is missing or linked")
    if manifest_path.stat().st_size > 8192 or logo_path.stat().st_size > 1024 * 1024:
        raise ValueError("Branding files exceed their bounds")
    data = json.loads(manifest_path.read_text(encoding="utf-8"), object_pairs_hook=_pairs)
    if not isinstance(data, dict) or type(data.get("schema")) is not int or data["schema"] != 1:
        raise ValueError("Unsupported branding manifest")
    if data.get("source") != "kit/QorMark.qml" or not isinstance(data.get("inputs"), dict):
        raise ValueError("Branding must name the existing QorMark")
    if set(data["inputs"]) != set(INPUTS):
        raise ValueError("Branding source set differs")
    for name in INPUTS:
        source = repo / name
        if source.is_symlink() or not source.resolve(strict=True).is_relative_to(repo):
            raise ValueError("A branding source is outside the checkout")
        if hashlib.sha256(source.read_bytes()).hexdigest() != data["inputs"][name]:
            raise ValueError("Branding is stale; regenerate the existing mark")
    logo = data.get("logo")
    if not isinstance(logo, dict) or logo.get("file") != "qor-mark.png":
        raise ValueError("Unexpected branding image")
    if any(type(logo.get(key)) is not int or logo[key] != 1024 for key in ("width", "height")):
        raise ValueError("Branding must retain its square source aspect")
    image = logo_path.read_bytes()
    if hashlib.sha256(image).hexdigest() != logo.get("sha256"):
        raise ValueError("Branding image hash differs")
    _png(image)
    background = data.get("background")
    if not isinstance(background, str) or re.fullmatch(r"#[0-9a-f]{6}", background) is None:
        raise ValueError("Invalid boot background")
    if max(int(background[index:index + 2], 16) for index in (1, 3, 5)) > 48:
        raise ValueError("Boot background must retain the dark Lumen role")
    return data
