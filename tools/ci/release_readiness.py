#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Has this commit been built and tested everywhere a release needs? build.yml's last job asks this.

A runner being there is not a build having passed, so this says the two apart: which runner each job was
allowed, and what each required step did. It exits 0 only when every required step ran and passed. A step
that was skipped, for any reason, counts against it, as a failure does. It reads what build.yml gives it in
its environment, and nothing of the machine it runs on.

    release_readiness.py    exit 0 ready, 1 not ready
"""

from __future__ import annotations

import dataclasses
import os
import sys
from collections.abc import Mapping


@dataclasses.dataclass(frozen=True)
class Required:
    name: str
    outcome: str
    # The step that says whether the products changed, if this one may stand down when they did not.
    changed: str = ""


REQUIRED = (
    Required("Windows: build, mitigations, qmllint, tests", "WINDOWS_JOB"),
    Required("Windows: suites that draw", "WINDOWS_GPU"),
    Required("Windows: the products", "WINDOWS_PRODUCTS", "WINDOWS_PRODUCTS_CHANGED"),
    Required("Linux: build, mitigations, qmllint, tests, sanitizers", "LINUX_JOB"),
    Required("Linux: suites that draw", "LINUX_GPU"),
    Required("Linux: Prism's protocol check", "LINUX_PRISM"),
    Required("Linux: the products", "LINUX_PRODUCTS", "LINUX_PRODUCTS_CHANGED"),
    # A run drawn in software proves nothing of how the products draw and keep time on a graphics card, so a release also
    # needs them run there, on Linux, strictly. No job does that yet: build.yml tells this nothing, and it reads "not run".
    # It never stands down for unchanged products.
    Required("Linux: the products on a graphics card, unrelaxed", "LINUX_PRODUCTS_GPU"),
)


def state_of(item: Required, env: Mapping[str, str]) -> tuple[str, bool]:
    """What became of a required step, in words, and whether it counts as passed."""
    outcome = env.get(item.outcome, "")
    if outcome == "success":
        return "passed", True
    # Unchanged products may stand down, and only that: their step was skipped, the job said it changed nothing, and this is
    # said not to be a release. Nothing here checks that they passed on an earlier commit. A release always runs them, and
    # build.yml's own step says it does; this does not rely on that. Any other word, or none, counts against it.
    if item.changed and outcome == "skipped" and env.get(item.changed) == "false" and env.get("IS_RELEASE") == "false":
        return "not run: unchanged by this commit", True
    if outcome == "failure":
        return "FAILED", False
    if outcome == "cancelled":
        return "not finished: cancelled", False
    return "not run", False


def report(env: Mapping[str, str]) -> tuple[list[str], bool]:
    """The report's lines, and whether this commit is ready."""
    lines = [
        "### Release readiness",
        "",
        "A runner being allowed is not a build having passed.",
        "",
        "| Platform | Runner allowed |",
        "|----------|----------------|",
        f"| Windows | {env.get('WINDOWS_RUNNER') or 'not known'} |",
        f"| Linux | {env.get('LINUX_RUNNER') or 'not known'} |",
        "",
        "| Required | What became of it |",
        "|----------|-------------------|",
    ]
    ready = True
    for item in REQUIRED:
        words, passed = state_of(item, env)
        lines.append(f"| {item.name} | {words} |")
        ready = ready and passed
    lines += ["", "**Ready.**" if ready else "**Not ready for a release.** A step not run proves nothing."]
    return lines, ready


def main(env: Mapping[str, str]) -> int:
    lines, ready = report(env)
    text = "\n".join(lines) + "\n"
    print(text)
    summary = env.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as out:
            out.write(text)
    if not ready:
        print("::error title=Not ready for a release::A required build or test did not run, or did not pass. See this job's summary.")
    return 0 if ready else 1


if __name__ == "__main__":
    sys.exit(main(os.environ))
