#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Refuse retired first-party hosts in tracked text without printing their values."""

from __future__ import annotations

import html
from pathlib import Path
import re
import subprocess
import sys
import unicodedata
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[2]
MAX_BYTES = 4 * 1024 * 1024
TEXT_SUFFIXES = {".md", ".rs", ".qml", ".cpp", ".h", ".ts", ".tsx", ".js", ".mjs", ".json",
                 ".toml", ".yml", ".yaml", ".html", ".css", ".sh", ".py", ".txt", ".conf",
                 ".ps1", ".xml", ".svg", ".webmanifest", ".lock"}
TEXT_NAMES = {"Dockerfile", "Containerfile", ".cursorrules"}
RETIRED = re.compile(r"(?<![a-z0-9_-])demiurge\.(?:cloud|guru|io|network)(?![a-z0-9_-])", re.I)


def retired_lines(text: str) -> list[int]:
    # Catch visible HTML entities and percent-encoded links as well as plain copy.
    # Internal reverse-DNS identifiers and cryptographic domain separators are not URLs.
    lines = []
    for number, line in enumerate(text.splitlines(), 1):
        normalized = line
        for _ in range(2):
            normalized = html.unescape(unquote(normalized))
        normalized = unicodedata.normalize("NFKC", normalized)
        if RETIRED.search(normalized):
            lines.append(number)
    return lines


def check(root: Path = ROOT) -> tuple[int, list[tuple[str, int]]]:
    root = root.resolve(strict=True)
    raw = subprocess.check_output(["git", "-C", str(root), "ls-files", "-z"], timeout=30)
    if len(raw) > MAX_BYTES:
        raise ValueError("Oversized tracked-file list")
    checked, findings = 0, []
    for name in raw.decode("utf-8").split("\0"):
        if not name:
            continue
        relative = Path(name)
        if any("secret" in part.lower() or part.lower().startswith(".env") for part in relative.parts):
            continue
        if relative.suffix not in TEXT_SUFFIXES and relative.name not in TEXT_NAMES:
            continue
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Unsafe tracked path")
        path = root
        for part in relative.parts:
            path = path / part
            if path.is_symlink() or path.is_junction():
                raise ValueError("Linked tracked text is not permitted")
        if not path.resolve().is_relative_to(root) or not path.is_file():
            raise ValueError("Missing tracked text")
        if path.stat().st_size > MAX_BYTES:
            raise ValueError("Oversized tracked text")
        with path.open("rb") as stream:
            content = stream.read(MAX_BYTES + 1)
        if len(content) > MAX_BYTES:
            raise ValueError("Oversized tracked text")
        checked += 1
        findings.extend((name, number) for number in retired_lines(content.decode("utf-8-sig")))
    return checked, findings


def main() -> int:
    try:
        checked, findings = check()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError):
        print("Domain policy could not inspect tracked text; review paths and encoding.", file=sys.stderr)
        return 2
    if findings:
        print("Retired ecosystem hosts found; use QORSYNC.DEV and its approved service subdomains.", file=sys.stderr)
        for name, number in findings:
            # Paths are git metadata, never file contents or addresses.
            print(f"  {ascii(name)}:{number}", file=sys.stderr)
        return 1
    print(f"Domain policy: {checked} tracked text files checked; no retired ecosystem hosts.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
