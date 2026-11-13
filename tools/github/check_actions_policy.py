#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Check the reviewed Actions policy and workflow boundaries offline; never execute YAML."""

from __future__ import annotations

import json
from pathlib import Path
import re
import sys

import yaml

ROOT = Path(__file__).resolve().parents[2]
MAX_BYTES = 1024 * 1024
ACTION = re.compile(r"([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)@([0-9a-f]{40})")
PATTERN = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+@\*")


def unique_mapping(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate configuration key")
        result[key] = value
    return result


class WorkflowLoader(yaml.SafeLoader):
    pass


def mapping(loader, node):
    return unique_mapping((loader.construct_object(key), loader.construct_object(value))
                          for key, value in node.value)


WorkflowLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, mapping)


def read_text(path):
    if path.is_symlink():
        raise ValueError("configuration symlinks are not supported")
    with path.open("rb") as stream:
        raw = stream.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise ValueError("configuration exceeds size limit")
    return raw.decode("utf-8")


def load_policy(root=ROOT):
    policy = json.loads(read_text(root / ".github/actions-policy.json"), object_pairs_hook=unique_mapping)
    if not isinstance(policy, dict) or set(policy) != {"permissions", "selected_actions", "workflow_permissions"}:
        raise ValueError("invalid Actions policy sections")
    # Exact JSON comparisons reject strings and integers masquerading as booleans.
    expected = {"enabled": True, "allowed_actions": "selected", "sha_pinning_required": True}
    if json.dumps(policy["permissions"], sort_keys=True) != json.dumps(expected, sort_keys=True):
        raise ValueError("Actions must require selected actions and full commit pins")
    expected = {"default_workflow_permissions": "read", "can_approve_pull_request_reviews": False}
    if json.dumps(policy["workflow_permissions"], sort_keys=True) != json.dumps(expected, sort_keys=True):
        raise ValueError("workflow tokens must be read-only without review approval")
    selected = policy["selected_actions"]
    if (not isinstance(selected, dict)
            or set(selected) != {"github_owned_allowed", "verified_allowed", "patterns_allowed"}
            or selected["github_owned_allowed"] is not False or selected["verified_allowed"] is not False):
        raise ValueError("broad publisher allowances are forbidden")
    patterns = selected["patterns_allowed"]
    if (not isinstance(patterns, list) or not patterns
            or any(not isinstance(item, str) or not PATTERN.fullmatch(item) for item in patterns)
            or len(set(patterns)) != len(patterns)):
        raise ValueError("allow only explicit action repositories without duplicates")
    return policy


def check_workflow(text, policy):
    """Raise a value-free diagnostic for unsupported or unsafe workflow input."""
    if len(text.encode("utf-8")) > MAX_BYTES:
        raise ValueError("workflow exceeds size limit")
    try:
        if any(isinstance(event, yaml.AliasEvent) for event in yaml.parse(text)):
            raise ValueError("workflow aliases are not supported by this policy check")
        data = yaml.load(text, Loader=WorkflowLoader)
    except (yaml.YAMLError, TypeError, RecursionError) as error:
        raise ValueError("invalid workflow YAML") from error
    if not isinstance(data, dict) or data.get("permissions") != {"contents": "read"}:
        raise ValueError("workflow must declare only contents: read")
    # PyYAML's YAML 1.1 loader reads the unquoted GitHub key 'on' as True.
    events = data.get("on", data.get(True))
    if isinstance(events, str):
        events = [events]
    if not isinstance(events, (list, dict)) or not events:
        raise ValueError("workflow must declare its events")
    if any(event in events for event in ("pull_request_target", "workflow_run")):
        raise ValueError("privileged follow-up events need a separate security review")
    jobs = data.get("jobs")
    if not isinstance(jobs, dict) or not jobs:
        raise ValueError("workflow must contain jobs")
    count = 0
    for job in jobs.values():
        if not isinstance(job, dict):
            raise ValueError("invalid workflow job")
        permissions = job.get("permissions", {})
        if not isinstance(permissions, dict) or any(value not in ("read", "none") for value in permissions.values()):
            raise ValueError("job token permissions must not grant write access")
        steps = job.get("steps", [])
        if not isinstance(steps, list) or any(not isinstance(step, dict) for step in steps):
            raise ValueError("invalid workflow steps")
        for item in [job, *steps]:
            if "uses" not in item:
                continue
            ref = item["uses"]
            match = ACTION.fullmatch(ref) if isinstance(ref, str) else None
            if not match or match[1] + "@*" not in policy["selected_actions"]["patterns_allowed"]:
                raise ValueError("action must be allowlisted and pinned to a full commit")
            if match[1] == "actions/checkout":
                options = item.get("with", {})
                value = options.get("persist-credentials") if isinstance(options, dict) else None
                if value is not False and value != "false":
                    raise ValueError("checkout must disable persisted credentials")
            count += 1
    return count


def check_repository(root=ROOT):
    if (root / ".github").is_symlink() or (root / ".github/workflows").is_symlink():
        raise ValueError("configuration directory symlinks are not supported")
    policy = load_policy(root)
    workflows = sorted((root / ".github/workflows").glob("*.y*ml"))
    if not workflows:
        raise ValueError("no workflows found")
    count = sum(check_workflow(read_text(path), policy) for path in workflows)
    return policy, len(workflows), count


def main():
    try:
        _, workflows, actions = check_repository()
    except (OSError, ValueError, TypeError, RecursionError):
        # Parser exceptions can quote complete source lines, including private values.
        print("Actions policy: failed; inspect policy and workflow boundaries locally", file=sys.stderr)
        return 1
    print(f"Actions policy: {workflows} workflows, {actions} pinned uses checked (offline)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
