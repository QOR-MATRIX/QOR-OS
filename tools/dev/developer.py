#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Discover reviewed developer entry points without running or connecting to them.

Requires Python 3.12+ only. Run from any directory:
    python tools/dev/developer.py list --json
    python tools/dev/developer.py show qq-mcp
    python tools/dev/developer.py check
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import stat
import sys

ROOT = Path(__file__).resolve().parents[2]
CATALOG = "sdk/developer-tools.json"
MAX_BYTES = 65536
MAX_ENTRIES = 32
AUTHORITY = "Discovery grants no authority to run tools, change projects, access keys or publish."
MESSAGES = {
    "arguments": "Invalid command; use --help.",
    "catalog": "Developer metadata is invalid; review sdk/developer-tools.json.",
    "reference": "A developer reference is missing or unsafe; review sdk/developer-tools.json.",
    "unknown_entry": "Unknown developer entry; use list to find its identifier.",
}


class DiscoveryError(ValueError):
    """Only fixed error codes leave the validator; never echo rejected input."""


def require(condition: bool, code: str = "catalog") -> None:
    if not condition:
        raise DiscoveryError(code)


def plain(value: object, limit: int) -> str:
    require(isinstance(value, str) and 0 < len(value) <= limit)
    require(value == value.strip() and all(32 <= ord(char) <= 126 for char in value))
    return value


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        require(key not in result)
        result[key] = value
    return result


def reject_constant(value: str) -> None:
    raise DiscoveryError("catalog")


def local_parts(reference: str) -> list[str]:
    # No absolute paths, backslashes, device names, traversal, URLs or encoded paths.
    path, _, fragment = reference.partition("#")
    require(bool(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._/-]*", path)), "reference")
    require(not fragment or bool(re.fullmatch(r"[a-z0-9_-]+", fragment)), "reference")
    parts = path.split("/")
    require(all(part and part not in (".", "..") and not part.endswith((".", " ")) for part in parts), "reference")
    require(all(not part.startswith(".") and "secret" not in part.lower() for part in parts), "reference")
    require(parts[0] in {"AGENTS.md", "CONTRIBUTING.md", "SECURITY.md", "apps", "audio", "core", "docs",
                         "image", "input", "kit", "platform", "prism", "products", "sdk", "services",
                         "sim", "store", "tests", "tools"}, "reference")
    require(all(part.split(".")[0].upper() not in {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3",
                "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4",
                "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"} for part in parts), "reference")
    return parts


def local_target(root: Path, reference: str) -> Path:
    target = root
    for part in local_parts(reference):
        target = target / part
        require(not target.is_symlink() and not target.is_junction(), "reference")
        # lstat refuses missing components and never opens a referenced document.
        mode = target.lstat().st_mode
        require(stat.S_ISREG(mode) or stat.S_ISDIR(mode), "reference")
    require(target.resolve().is_relative_to(root), "reference")
    return target


def upstream_reference(reference: str) -> None:
    match = re.fullmatch(
        r"https://github\.com/QOR-MATRIX/demiurge-chain/(blob|tree)/[0-9a-f]{40}/([A-Za-z0-9._/-]+)", reference)
    require(match is not None, "reference")
    path = match[2]
    require(all(part not in ("", ".", "..") for part in path.split("/")), "reference")
    require(any(path == prefix or path.startswith(prefix + "/") for prefix in
                ("tools/qor-launcher", "chain/pallets/drc369", "chain/pallets/drc369-royalties")), "reference")


def string_list(value: object, limit: int, item_limit: int) -> list[str]:
    require(isinstance(value, list) and 0 < len(value) <= limit)
    values = [plain(item, item_limit) for item in value]
    require(len(set(values)) == len(values))
    return values


def load_catalog(root: Path = ROOT) -> tuple[list[dict], dict]:
    """Read only the fixed catalog; stat local links and check pinned URL syntax.

    This validates a reviewed checkout, not concurrent hostile filesystem changes
    or upstream content. It never reads linked source files or follows URLs.
    """
    try:
        root = root.resolve(strict=True)
        path = local_target(root, CATALOG)
        require(stat.S_ISREG(path.lstat().st_mode))
        require(path.stat().st_size <= MAX_BYTES)
        with path.open("rb") as stream:
            raw = stream.read(MAX_BYTES + 1)
        require(len(raw) <= MAX_BYTES)
        catalog = json.loads(raw.decode("utf-8"), object_pairs_hook=unique_object, parse_constant=reject_constant)
        require(isinstance(catalog, dict) and set(catalog) == {"schema_version", "entries"})
        require(type(catalog["schema_version"]) is int and catalog["schema_version"] == 1)
        entries = catalog["entries"]
        require(isinstance(entries, list) and 0 < len(entries) <= MAX_ENTRIES)
        ids = set()
        local = set()
        upstream = set()
        for entry in entries:
            require(isinstance(entry, dict) and set(entry) == {
                "id", "title", "summary", "repository", "docs", "sources", "prerequisites", "boundary"})
            identifier = plain(entry["id"], 48)
            require(bool(re.fullmatch(r"[a-z][a-z0-9-]*", identifier)) and identifier not in ids)
            ids.add(identifier)
            plain(entry["title"], 80)
            plain(entry["summary"], 320)
            plain(entry["boundary"], 640)
            require(entry["repository"] in ("QOR-MATRIX/qor-os", "QOR-MATRIX/demiurge-chain"))
            string_list(entry["prerequisites"], 8, 320)
            for reference in string_list(entry["docs"], 8, 320):
                require(reference.partition("#")[0].endswith(".md"), "reference")
                require(stat.S_ISREG(local_target(root, reference).lstat().st_mode), "reference")
                local.add(reference)
            for reference in string_list(entry["sources"], 12, 320):
                if entry["repository"] == "QOR-MATRIX/qor-os":
                    local_target(root, reference)
                    local.add(reference)
                else:
                    upstream_reference(reference)
                    upstream.add(reference)
        return sorted(entries, key=lambda entry: entry["id"]), {
            "local_references_checked": len(local), "upstream_references_checked": len(upstream),
            "upstream_content_verified": False, "scope": "Local path existence and pinned upstream URL syntax only.",
        }
    except DiscoveryError:
        raise
    except (OSError, ValueError, TypeError, RecursionError, RuntimeError):
        raise DiscoveryError("catalog") from None


class Parser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise DiscoveryError("arguments")


def parser() -> argparse.ArgumentParser:
    result = Parser(description=__doc__, allow_abbrev=False)
    result.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="machine-readable output")
    commands = result.add_subparsers(dest="command")
    for name, help_text in (("list", "list reviewed entry points (default)"),
                            ("show", "show one entry, prerequisites and action boundaries"),
                            ("check", "validate metadata and references offline")):
        command = commands.add_parser(name, help=help_text, allow_abbrev=False)
        command.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="machine-readable output")
        if name == "show":
            command.add_argument("id", help="identifier from list")
    return result


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    as_json = "--json" in args
    try:
        options = parser().parse_args(args)
        entries, validation = load_catalog()
        command = options.command or "list"
        if command == "show":
            entries = [entry for entry in entries if entry["id"] == options.id]
            require(bool(entries), "unknown_entry")
        result = {"schema_version": 1, "ok": True, "mode": "read-only", "authority": AUTHORITY,
                  "validation": validation}
        if command == "check":
            result["entries_checked"] = len(entries)
        else:
            result["entries"] = entries
        if as_json:
            print(json.dumps(result, indent=2, sort_keys=True, ensure_ascii=True))
        else:
            print("QOR OS developer entry points (offline)\n" + AUTHORITY)
            if command == "check":
                print(f"Checked {len(entries)} entries. {validation['scope']}")
            for entry in entries if command != "check" else []:
                print(f"\n{entry['id']} - {entry['title']}\n  {entry['summary']}")
                if command == "show":
                    print("  Repository: " + entry["repository"])
                    for field, label in (("docs", "Guide"), ("sources", "Source"), ("prerequisites", "Requires")):
                        for value in entry[field]:
                            print(f"  {label}: {value}")
                    print("  Boundary: " + entry["boundary"])
            if command == "list":
                print("\nUse show <id> for prerequisites, references and action boundaries.")
        return 0
    except DiscoveryError as error:
        code = error.args[0]
        if as_json:
            print(json.dumps({"schema_version": 1, "ok": False, "error": {"code": code, "message": MESSAGES[code]}}))
        else:
            print(MESSAGES[code], file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
