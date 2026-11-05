#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Has this commit been built and tested everywhere a release needs? build.yml's last job asks this.

A runner being there is not a build having passed, so this says the two apart: which runner each job was
allowed, and what each required step did. A release needs every required step to run and pass. Outside a
release, unchanged product suites may stand down: exit 0 then accepts the non-release check without
claiming release readiness. It reads what build.yml gives it in its environment, and nothing of the machine.

    release_readiness.py    exit 0 check accepted, 1 a required step missing or unsuccessful
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
    """What became of a required step, and whether this run's policy accepts that state."""
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
    """The report's lines, and whether this run satisfies its applicable requirements."""
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
    accepted = True
    all_ran_and_passed = True
    for item in REQUIRED:
        words, allowed = state_of(item, env)
        lines.append(f"| {item.name} | {words} |")
        accepted = accepted and allowed
        all_ran_and_passed = all_ran_and_passed and env.get(item.outcome) == "success"
    if not accepted:
        conclusion = "**Not ready for a release.** A step not run proves nothing."
    elif not all_ran_and_passed:
        conclusion = "**Non-release checks passed.** **Not ready for a release.** Product suites did not run."
    else:
        conclusion = "**Ready.**"
    lines += ["", conclusion]
    return lines, accepted


def main(env: Mapping[str, str]) -> int:
    lines, accepted = report(env)
    text = "\n".join(lines) + "\n"
    print(text)
    summary = env.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as out:
            out.write(text)
    if not accepted:
        print("::error title=Not ready for a release::A required build or test did not run, or did not pass. See this job's summary.")
    return 0 if accepted else 1


if __name__ == "__main__":
    sys.exit(main(os.environ))
