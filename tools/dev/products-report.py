#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""What the suites of a CTest run said, from CTest's own files. Called by tools/dev/products.sh, which runs `ctest -T Test`.

CTest shows a passing suite's output nowhere else, so a case skipped in a suite that passed would go unseen.

Its log (Testing/Temporary/LastTest_<tag>.log, <tag> being the first line of Testing/TAG) keeps every suite's whole output, and
says only "Test Failed." of a suite that failed, whether it exited with 1 or crashed. Its record (Testing/<tag>/Test.xml) says
how each failed suite ended, and cuts the output short. So each is read for what it alone has. Measured on CTest 4.2.3: a suite
that prints its totals and then exits with 1, with 2, is killed by SIGSEGV or by SIGABRT is recorded with the Exit Code and Exit
Value `Failed` 1, `Failed` 2, `SEGFAULT` 0 and `Subprocess aborted` 0, all four as "Test Failed." in the log.

  products-report.py <build folder>                   each suite's counts, and each case failed or skipped, with why
  products-report.py --only-the-floor <build folder>  exit 0 if the QQ Player's frame-rate floor was all that went wrong
"""

import dataclasses
import pathlib
import re
import sys
import xml.etree.ElementTree as ElementTree

SUITE = re.compile(r"^\d+/\d+ Testing: (\S+)$")
CASE = re.compile(r"^(PASS   |FAIL!  |XPASS  |BFAIL  |SKIP   ): (.*)$")
TOTALS = re.compile(r"^Totals: (\d+) passed, (\d+) failed, (\d+) skipped")

# Its check, exactly as Qt Test reports it failing, and nothing else in that suite or any other.
FLOOR_SUITE = "tst_player"
FLOOR = "TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() 'm.value(QStringLiteral(\"fps\")).toDouble() >= 20' returned FALSE."
FLOOR_READ = re.compile(re.escape(FLOOR) + r" \((\d+(?:\.\d+)?)\)\s*$")
# Software is slow, and a Player below this has stopped drawing, which is a fault wherever it happens. Software reads 5 to 6.
STILL_DRAWING_FPS = 1.0


@dataclasses.dataclass
class Suite:
    name: str
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    finished: bool = False
    failures: list = dataclasses.field(default_factory=list)
    skips: list = dataclasses.field(default_factory=list)


@dataclasses.dataclass
class Ending:
    """How CTest's record says a failed suite ended."""

    code: str
    value: str


def read_suites(log):
    """The suites in the text of CTest's log, in the order they ran."""
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


def read_endings(record):
    """How each failed suite ended, by name, from the text of CTest's record. None if the record cannot be read."""
    try:
        root = ElementTree.fromstring(record)
    except ElementTree.ParseError:
        return None
    endings = {}
    for test in root.iter("Test"):
        if test.get("Status") != "failed":
            continue
        measured = {item.get("name"): item.findtext("Value", "") for item in test.iter("NamedMeasurement")}
        endings[test.findtext("Name", "")] = Ending(measured.get("Exit Code", ""), measured.get("Exit Value", ""))
    return endings


def ended_as_it_said(suite, endings):
    """Whether a suite that reached its totals then exited normally, with its failures as its status, as Qt Test does."""
    ending = endings.get(suite.name)
    return suite.finished and (ending is None or (ending.code == "Failed" and ending.value == str(suite.failed)))


def report(suites, endings=None):
    """Each suite's counts, and each case failed or skipped, by name. Qt Test counts initTestCase and cleanupTestCase."""
    endings = endings or {}
    lines = []
    for suite in suites:
        if suite.finished:
            lines.append(f"   {suite.name}: {suite.passed} passed, {suite.failed} failed, {suite.skipped} skipped")
        else:
            lines.append(f"   {suite.name}: STOPPED before its end (a crash or a timeout), {suite.passed} passed by then")
        if suite.name in endings and not ended_as_it_said(suite, endings):
            ending = endings[suite.name]
            lines.append(f"     ended badly: {ending.code}, exit value {ending.value}")
        lines += [f"     failed: {case}" for case in suite.failures]
        lines += [f"     skipped: {case}" for case in suite.skips]
    return "\n".join(lines)


def only_the_floor_failed(suites, endings):
    """Whether what went wrong was that check alone: tst_player the only suite failed, ended normally, with a Player drawing."""
    if endings is None or list(endings) != [FLOOR_SUITE] or not all(suite.finished for suite in suites):
        return False
    failures = [case for suite in suites for case in suite.failures]
    if len(failures) != 1:
        return False
    read = FLOOR_READ.match(failures[0])
    if not read or float(read.group(1)) < STILL_DRAWING_FPS:
        return False
    return ended_as_it_said(next(suite for suite in suites if suite.name == FLOOR_SUITE), endings)


def main(arguments):
    floor = "--only-the-floor" in arguments
    folders = [argument for argument in arguments if argument != "--only-the-floor"]
    if len(folders) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    testing = pathlib.Path(folders[0]) / "Testing"
    try:
        tag = (testing / "TAG").read_text(encoding="utf-8").splitlines()[0].strip()
        suites = read_suites((testing / "Temporary" / f"LastTest_{tag}.log").read_text(encoding="utf-8", errors="replace"))
        endings = read_endings((testing / tag / "Test.xml").read_text(encoding="utf-8", errors="replace"))
    except (OSError, IndexError) as error:
        print(f"CTest left no record: {error}", file=sys.stderr)
        return 1
    if floor:
        return 0 if only_the_floor_failed(suites, endings) else 1
    print(report(suites, endings))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
