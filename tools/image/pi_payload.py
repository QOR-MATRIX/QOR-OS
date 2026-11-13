#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Audit explicitly listed Q Pi ELF files without executing target code.

This checks static layout, dependency names and GNU version-label agreement,
not symbol binding, full ABI, deployment, entitlement or build acceptance.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat

import pi_elf

MAX_MANIFEST_BYTES = 256 * 1024
MAX_FILES = 512
MAX_TOTAL_BYTES = 2 * 1024**3
MAX_REPORT_BYTES = 8 * 1024**2
ROLES = {"application", "library", "plugin", "helper", "loader"}
LIMITS = ["manifest completeness, including QML imports and dlopen plugins",
          "actual target loader search, per-symbol version binding and relocations",
          "Linux GNU ABI and commercial Qt entitlement",
          "native application execution, graphics and hardware",
          "builder, resources, source and image acceptance"]


def relative_path(value):
    """Portable image-relative names only; no host paths or alternate streams."""
    if (not isinstance(value, str) or not 0 < len(value) <= 1024
            or not re.fullmatch(r"[A-Za-z0-9_./+-]+", value)):
        raise ValueError("invalid manifest path")
    parts = value.split("/")
    if (any(part in ("", ".", "..") or part.endswith(".") for part in parts)
            or any(re.fullmatch(r"(?i:con|prn|aux|nul|com[0-9]|lpt[0-9])(?:\..*)?", part)
                   for part in parts)
            or PurePosixPath(value).is_absolute()):
        raise ValueError("unsafe manifest path")
    return value


def parse_manifest(raw):
    if not isinstance(raw, bytes) or len(raw) > MAX_MANIFEST_BYTES:
        raise ValueError("manifest exceeds input bound")

    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate manifest field")
            result[key] = value
        return result

    try:
        manifest = json.loads(raw.decode("utf-8"), object_pairs_hook=unique)
    except (UnicodeError, RecursionError) as error:
        raise ValueError("invalid manifest encoding or nesting") from error
    if (not isinstance(manifest, dict) or set(manifest) != {"schema", "files"}
            or type(manifest["schema"]) is not int or manifest["schema"] != 1
            or not isinstance(manifest["files"], list)
            or not 1 <= len(manifest["files"]) <= MAX_FILES):
        raise ValueError("unknown or empty manifest schema")
    names = set()
    for entry in manifest["files"]:
        if (not isinstance(entry, dict) or set(entry) != {"path", "sha256", "role"}
                or not isinstance(entry["role"], str) or entry["role"] not in ROLES
                or not isinstance(entry["sha256"], str)
                or not re.fullmatch(r"[0-9a-f]{64}", entry["sha256"])):
            raise ValueError("invalid file manifest entry")
        name = relative_path(entry["path"])
        if name.casefold() in names:
            raise ValueError("duplicate or case-colliding manifest path")
        names.add(name.casefold())
    return manifest


def contained_file(root, name):
    path = (root / name).resolve(strict=True)
    if not path.is_relative_to(root) or not stat.S_ISREG(path.stat().st_mode):
        raise ValueError("input is not a contained regular file")
    return path


def audit(root, manifest):
    # Validate direct callers too, before opening any file from the manifest.
    manifest = parse_manifest(json.dumps(manifest).encode("utf-8"))
    root = Path(root).resolve(strict=True)
    if not root.is_dir():
        raise ValueError("payload root must be an existing directory")
    issues, files, paths = [], [], {}
    total = 0
    report_bytes = 0
    output_exhausted = False

    def add_issue(issue):
        nonlocal report_bytes, output_exhausted
        if output_exhausted:
            return
        report_bytes += len(json.dumps(issue).encode("utf-8"))
        if report_bytes > MAX_REPORT_BYTES:
            output_exhausted = True
            issues.append({"problem": "report exceeds output bound"})
        else:
            issues.append(issue)

    for entry in manifest["files"]:
        name = entry["path"]
        try:
            path = contained_file(root, name)
            size = path.stat().st_size
            if not 0 < size <= pi_elf.MAX_FILE_BYTES:
                raise ValueError("file size exceeds bound")
            total += size
            if total > MAX_TOTAL_BYTES:
                raise ValueError("aggregate input exceeds bound")
            paths[name] = path
        except (OSError, ValueError, RuntimeError):
            add_issue({"path": name, "problem": "file unavailable, outside root or over input bound"})
        if output_exhausted:
            break
    # Never hash part of an oversized or unsafe input set.
    if not issues:
        inspected_bytes = 0
        for entry in manifest["files"]:
            if output_exhausted:
                break
            name = entry["path"]
            try:
                info = pi_elf.inspect_file(paths[name], maximum_bytes=min(
                    pi_elf.MAX_FILE_BYTES, MAX_TOTAL_BYTES - inspected_bytes))
                inspected_bytes += info["file_size"]
                if contained_file(root, name) != paths[name]:
                    raise ValueError("file path changed during audit")
                if info["sha256"] != entry["sha256"]:
                    add_issue({"path": name, "problem": "SHA-256 differs from manifest"})
                    continue
                if entry["role"] in ("library", "plugin", "loader") and info["elf_type"] != 3:
                    add_issue({"path": name, "problem": "shared ELF required for selected role"})
                    continue
                record = {"path": name, "role": entry["role"], **info}
                report_bytes += len(json.dumps(record).encode("utf-8"))
                if report_bytes > MAX_REPORT_BYTES:
                    add_issue({"problem": "metadata exceeds report bound"})
                    break
                files.append(record)
            except (OSError, ValueError, RuntimeError):
                # Do not emit supplied malformed strings or arbitrary OS error paths.
                add_issue({"path": name, "problem": "ELF metadata invalid or file changed during audit"})
            if output_exhausted:
                break

    providers = {}
    for entry in files:
        # These are name candidates only. RPATH, RUNPATH, ld.so.cache and the
        # target's loader determine what would actually load.
        for name in {PurePosixPath(entry["path"]).name, entry["soname"]} - {None}:
            providers.setdefault(name, []).append(entry["path"])
    known_paths = {entry["path"]: entry for entry in files}
    for entry in files:
        if output_exhausted:
            break
        entry["needed_candidates"] = {}
        for name in entry["needed"]:
            candidates = sorted(providers.get(name, [])) if "/" not in name else []
            report_bytes += len(json.dumps({name: candidates}).encode("utf-8"))
            if report_bytes > MAX_REPORT_BYTES:
                add_issue({"problem": "dependency inventory exceeds report bound"})
                break
            entry["needed_candidates"][name] = candidates
            if len(candidates) != 1:
                add_issue({"path": entry["path"], "problem": "needed name lacks exactly one manifest candidate"})
                if output_exhausted:
                    break
        entry["version_label_checks"] = []
        for requirement in entry["version_needs"]:
            if output_exhausted:
                break
            name = requirement["file"]
            candidates = entry["needed_candidates"].get(name, [])
            if len(candidates) != 1:
                # Missing/ambiguous DT_NEEDED candidates already refuse the audit.
                continue
            provider = known_paths[candidates[0]]
            defined = {(item["name"], item["hash"]) for item in provider["version_definitions"]
                       if not item["base"]}
            # Conservative qualification policy: even weak needs must agree.
            # Parents are not definitions; unversioned providers cannot fill gaps.
            missing = [item["name"] for item in requirement["versions"]
                       if (item["name"], item["hash"]) not in defined]
            check = {"needed": name, "candidate": candidates[0],
                     "required_count": len(requirement["versions"]), "missing": missing}
            report_bytes += len(json.dumps(check).encode("utf-8"))
            if report_bytes > MAX_REPORT_BYTES:
                add_issue({"problem": "version label inventory exceeds report bound"})
                break
            entry["version_label_checks"].append(check)
            if missing:
                add_issue({"path": entry["path"],
                           "problem": "required version labels absent from manifest candidate"})
        interpreter = entry["interpreter"]
        if interpreter:
            # The absolute Linux interpreter is matched as an image-relative
            # manifest entry, never opened as an absolute host path.
            candidate = known_paths.get(interpreter[1:]) if interpreter.startswith("/") else None
            if candidate is None or candidate["role"] != "loader":
                add_issue({"path": entry["path"], "problem": "interpreter absent from manifest loader entries"})
    report = {"schema": 1, "kind": "q-pi-static-payload-audit",
              "static_audit_passed": not issues, "files": files, "issues": issues,
              "build_qualified": False, "target_code_executed": False, "not_certified": LIMITS}
    if len(json.dumps(report, indent=2).encode("utf-8")) > MAX_REPORT_BYTES:
        report.update(static_audit_passed=False, files=[], issues=[{"problem": "report exceeds output bound"}])
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True, help="Existing target payload directory")
    parser.add_argument("--manifest", type=Path, required=True, help="Explicit schema-1 file/hash manifest")
    args = parser.parse_args(argv)
    try:
        if not args.manifest.is_file():
            raise ValueError("manifest must be a regular file")
        with args.manifest.open("rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise ValueError("manifest must be a regular file")
            raw = stream.read(MAX_MANIFEST_BYTES + 1)
        report = audit(args.root, parse_manifest(raw))
        report["manifest_sha256"] = hashlib.sha256(raw).hexdigest()
        if len(json.dumps(report, indent=2).encode("utf-8")) > MAX_REPORT_BYTES:
            report.update(static_audit_passed=False, files=[], issues=[{"problem": "report exceeds output bound"}])
    except (OSError, ValueError, RuntimeError):
        report = {"schema": 1, "kind": "q-pi-static-payload-audit", "static_audit_passed": False,
                  "issues": [{"problem": "invalid or unavailable manifest or payload root"}],
                  "build_qualified": False, "target_code_executed": False, "not_certified": LIMITS}
    print(json.dumps(report, indent=2))
    return 0 if report["static_audit_passed"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
