#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""What the suites of a CTest run said, from CTest's own log of it. Called by tools/dev/products.sh.

CTest shows a passing suite's output nowhere else, so a case skipped in a suite that passed would go unseen.

  products-report.py <build folder>                   each suite's counts, and each case failed or skipped, with why
  products-report.py --only-the-floor <build folder>  exit 0 if the QQ Player's frame-rate floor was all that failed
"""

import dataclasses
import pathlib
import re
import sys

SUITE = re.compile(r"^\d+/\d+ Testing: (\S+)$")
CASE = re.compile(r"^(PASS   |FAIL!  |XPASS  |BFAIL  |SKIP   ): (.*)$")
TOTALS = re.compile(r"^Totals: (\d+) passed, (\d+) failed, (\d+) skipped")

# Its check, exactly as Qt Test reports it failing, and nothing else in that suite or any other.
FLOOR_SUITE = "tst_player"
FLOOR = "TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() 'm.value(QStringLiteral(\"fps\")).toDouble() >= 20' returned FALSE."


@dataclasses.dataclass
class Suite:
    name: str
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    finished: bool = False
    failures: list = dataclasses.field(default_factory=list)
    skips: list = dataclasses.field(default_factory=list)


def read_suites(log):
    """The suites in the text of CTest's LastTest.log, in the order they ran."""
    suites = []
    for line in log.splitlines():
        if match := SUITE.match(line):
            suites.append(Suite(match.group(1)))
        elif not suites:
            continue
        elif match := TOTALS.match(line):
            suite = suites[-1]
            suite.passed, suite.failed, suite.skipped = (int(group) for group in match.groups())
            suite.finished = True
        elif match := CASE.match(line):
            suite = suites[-1]
            if match.group(1) == "SKIP   ":
                suite.skips.append(match.group(2))
            elif match.group(1) == "PASS   ":
                # Only for a suite that stops before its totals, which then say nothing.
                suite.passed += 1
            else:
                suite.failures.append(match.group(2))
    return suites


def report(suites):
    """Each suite's counts, and each case failed or skipped, by name. Qt Test counts initTestCase and cleanupTestCase."""
    lines = []
    for suite in suites:
        if suite.finished:
            lines.append(f"   {suite.name}: {suite.passed} passed, {suite.failed} failed, {suite.skipped} skipped")
        else:
            lines.append(f"   {suite.name}: STOPPED before its end (a crash or a timeout), {suite.passed} passed by then")
        lines += [f"     failed: {case}" for case in suite.failures]
        lines += [f"     skipped: {case}" for case in suite.skips]
    return "\n".join(lines)


def only_the_floor_failed(suites, failed):
    """Whether `failed`, the suites CTest says failed, is tst_player alone, having reached its end, with that one check."""
    if failed != [FLOOR_SUITE] or not all(suite.finished for suite in suites):
        return False
    failures = [case for suite in suites for case in suite.failures]
    return len(failures) == 1 and failures[0].startswith(FLOOR)


def read_failed(text):
    """The suites in CTest's LastTestsFailed.log, whose lines are `number:name`."""
    return [line.split(":", 1)[1] for line in text.splitlines() if ":" in line]


def main(arguments):
    floor = "--only-the-floor" in arguments
    folders = [argument for argument in arguments if argument != "--only-the-floor"]
    if len(folders) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    temporary = pathlib.Path(folders[0]) / "Testing" / "Temporary"
    try:
        suites = read_suites((temporary / "LastTest.log").read_text(encoding="utf-8", errors="replace"))
    except OSError as error:
        print(f"CTest left no log: {error}", file=sys.stderr)
        return 1
    if not floor:
        print(report(suites))
        return 0
    try:
        failed = read_failed((temporary / "LastTestsFailed.log").read_text(encoding="utf-8", errors="replace"))
    except OSError:
        return 1
    return 0 if only_the_floor_failed(suites, failed) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
