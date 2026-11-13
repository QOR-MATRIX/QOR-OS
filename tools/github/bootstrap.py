#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Applies the repository's GitHub configuration with the GitHub CLI.

Idempotent: safe to re-run after editing the files it reads.

    python tools/github/bootstrap.py                 # labels, milestones, settings
    python tools/github/bootstrap.py --ruleset       # also apply .github/rulesets/main.json
    python tools/github/bootstrap.py --actions-only  # apply and verify only the Actions policy

Reads:
    .github/labels.json        label names, colours, descriptions
    .github/milestones.json    roadmap phases
    .github/rulesets/main.json branch ruleset for the default branch
    .github/actions-policy.json reviewed Actions permissions and allowlist

Requires `gh` authenticated with admin rights on the repository.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

from check_actions_policy import check_repository

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPO = "QOR-MATRIX/qor-os"


def gh(*args: str, payload: dict | None = None, check: bool = True) -> subprocess.CompletedProcess:
    command = ["gh", *args]
    if payload is not None:
        command += ["--input", "-"]
    return subprocess.run(command, input=json.dumps(payload) if payload is not None else None,
                          capture_output=True, text=True, check=check)


def sync_labels(repo: str) -> None:
    labels = json.loads((REPO_ROOT / ".github" / "labels.json").read_text(encoding="utf-8"))
    for label in labels:
        gh("label", "create", label["name"], "--repo", repo, "--color", label["color"],
           "--description", label["description"], "--force")
    print(f"labels: {len(labels)} synced")


def sync_milestones(repo: str) -> None:
    wanted = json.loads((REPO_ROOT / ".github" / "milestones.json").read_text(encoding="utf-8"))
    existing = json.loads(gh("api", f"repos/{repo}/milestones?state=all&per_page=100").stdout)
    by_title = {m["title"]: m for m in existing}
    for milestone in wanted:
        body = {"title": milestone["title"], "description": milestone["description"],
                "state": milestone.get("state", "open")}
        if milestone["title"] in by_title:
            number = by_title[milestone["title"]]["number"]
            gh("api", "-X", "PATCH", f"repos/{repo}/milestones/{number}", payload=body)
        else:
            gh("api", "-X", "POST", f"repos/{repo}/milestones", payload=body)
    print(f"milestones: {len(wanted)} synced")


def apply_settings(repo: str) -> None:
    settings = {
        "description": "QOR OS: the operating system for players, game makers and creators. "
                       "Qt 6.12, Prism, QQ, ARQADE and the QOR network.",
        "has_issues": True,
        "has_projects": True,
        "has_wiki": False,
        "allow_squash_merge": True,
        "allow_rebase_merge": True,
        # Upstream fixture imports retain git-subtree ancestry (docs/UPSTREAM.md).
        "allow_merge_commit": True,
        "delete_branch_on_merge": True,
        "allow_update_branch": True,
        "squash_merge_commit_title": "PR_TITLE",
        "squash_merge_commit_message": "PR_BODY",
    }
    gh("api", "-X", "PATCH", f"repos/{repo}", payload=settings)
    topics = {"names": ["qt", "qt6", "qml", "qtquick", "operating-system", "wayland-compositor", "design-system",
                        "cpp20", "linux", "desktop-environment", "qt-remote-objects", "gaming", "game-engine",
                        "fedora-atomic"]}
    gh("api", "-X", "PUT", f"repos/{repo}/topics", payload=topics)
    # Dependabot vulnerability alerts and automatic security fixes.
    gh("api", "-X", "PUT", f"repos/{repo}/vulnerability-alerts")
    gh("api", "-X", "PUT", f"repos/{repo}/automated-security-fixes")
    print("settings: applied")


def apply_actions_policy(repo: str) -> None:
    policy, _, _ = check_repository(REPO_ROOT)
    endpoints = (
        ("permissions", "actions/permissions"),
        ("selected_actions", "actions/permissions/selected-actions"),
        ("workflow_permissions", "actions/permissions/workflow"),
    )
    # GitHub accepts a selected-action list only after selected mode is enabled.
    # A partial failure remains restrictive. Never automatically restore 'all'.
    for key, endpoint in endpoints:
        gh("api", "-X", "PUT", f"repos/{repo}/{endpoint}", payload=policy[key])
    for key, endpoint in endpoints:
        actual = json.loads(gh("api", f"repos/{repo}/{endpoint}").stdout)
        for name, value in policy[key].items():
            found = actual.get(name) if isinstance(actual, dict) else None
            if isinstance(value, list) and isinstance(found, list):
                value, found = sorted(value), sorted(found)
            if json.dumps(found, sort_keys=True) != json.dumps(value, sort_keys=True):
                raise ValueError("Actions policy readback did not match; inspect repository settings")
    print("Actions policy: applied and verified by readback")


def apply_ruleset(repo: str) -> bool:
    ruleset = json.loads((REPO_ROOT / ".github" / "rulesets" / "main.json").read_text(encoding="utf-8"))
    existing = gh("api", f"repos/{repo}/rulesets", check=False)
    if existing.returncode == 0:
        for item in json.loads(existing.stdout or "[]"):
            if item.get("name") == ruleset["name"]:
                result = gh("api", "-X", "PUT", f"repos/{repo}/rulesets/{item['id']}", payload=ruleset, check=False)
                break
        else:
            result = gh("api", "-X", "POST", f"repos/{repo}/rulesets", payload=ruleset, check=False)
    else:
        result = existing
    if result.returncode == 0:
        print("ruleset: applied")
        return True
    else:
        print("ruleset: not applied; check repository permissions and plan support", file=sys.stderr)
        return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo", default=DEFAULT_REPO)
    parser.add_argument("--ruleset", action="store_true", help="also apply .github/rulesets/main.json")
    parser.add_argument("--actions-only", action="store_true", help="apply and read back only the reviewed Actions policy")
    args = parser.parse_args()
    if args.actions_only and args.ruleset:
        parser.error("--actions-only cannot be combined with --ruleset")

    try:
        if args.actions_only:
            apply_actions_policy(args.repo)
        else:
            apply_settings(args.repo)
            sync_labels(args.repo)
            sync_milestones(args.repo)
            if args.ruleset and not apply_ruleset(args.repo):
                return 1
    except (OSError, ValueError, TypeError, RecursionError, subprocess.CalledProcessError):
        print("GitHub setup failed; earlier settings may have applied. Inspect and read back before retrying.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
