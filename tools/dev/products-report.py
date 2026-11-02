#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""What the suites of a CTest run said, from CTest's own files. Called by tools/dev/products.sh, which runs `ctest -T Test`.

CTest shows a passing suite's output nowhere else, so a case skipped in a suite that passed would go unseen.

Its log (Testing/Temporary/LastTest_<tag>.log, <tag> being the first line of Testing/TAG) keeps every suite's whole output, and
says only "Test Failed." of a suite that failed, whether it exited with 1 or crashed. Its record (Testing/<tag>/Test.xml) says
how each suite ended, and cuts the output short. So each is read for what it alone has. Measured on CTest 4.2.3:

  a suite that                    the record says                    the log says
  prints its totals, exits 1      failed, Exit Code Failed, Value 1  Test Failed.
  prints its totals, exits 2      failed, Exit Code Failed, Value 2  Test Failed.
  is killed by SIGSEGV            failed, SEGFAULT, 0                Test Failed.
  is killed by SIGABRT            failed, Subprocess aborted, 0      Test Failed.
  has no program to run           notrun, Unable to find executable  Test Passed.
  exits with its SKIP_RETURN_CODE notrun, SKIP_RETURN_CODE=77        Test Passed.

A suite need not be Qt Test: one with no "Start testing" line and no totals is judged by the record alone, and has no counts.

  products-report.py <build folder>                   each suite's counts, and each case failed or skipped, with why
  products-report.py --failed <build folder>          the suites that did not pass, one to a line
  products-report.py --passed-again NAME... <folder>  exit 0 if this run ran each of those suites and it passed
  products-report.py --only-the-floor <build folder>  exit 0 if the QQ Player's frame-rate floor was all that went wrong
"""

import dataclasses
import pathlib
import re
import sys
import xml.etree.ElementTree as ElementTree

SUITE = re.compile(r"^\d+/\d+ Testing: (\S+)$")
QT_TEST = re.compile(r"^\*{9} Start testing of ")
CASE = re.compile(r"^(PASS   |FAIL!  |XPASS  |BFAIL  |SKIP   |QFATAL ): (.*)$")
TOTALS = re.compile(r"^Totals: (\d+) passed, (\d+) failed, (\d+) skipped")
# What drew a suite: drawnin.h's line, or the JSON tst_player prints of what the Player says.
DRAWN_BY = re.compile(r"^QINFO  : \S+ Drawn by (.*); expected: ")
RENDERER = re.compile(r'^QINFO  : TstPlayer::\S+ \{.*"renderer":"([^"]*)"')

# Its check, exactly as Qt Test reports it failing, and nothing else in that suite or any other.
FLOOR_SUITE = "tst_player"
FLOOR = "TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() 'm.value(QStringLiteral(\"fps\")).toDouble() >= 20' returned FALSE."
FLOOR_READ = re.compile(re.escape(FLOOR) + r" \((\d+(?:\.\d+)?)\)\s*$")
# Software is slow, and a Player below this has stopped drawing, which is a fault wherever it happens. Software reads about 6.
# It stops only a Player that hardly draws at all: tst_player's own check, on every machine, is what shows it did not stop.
STILL_DRAWING_FPS = 1.0
# What must have drawn the Player for its floor to be let stand: "in software" is the renderer's own word, not the machine's.
SOFTWARE_RENDERER = "llvmpipe"


@dataclasses.dataclass
class Suite:
    name: str
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    finished: bool = False
    qt_test: bool = False
    renderer: str = ""
    failures: list = dataclasses.field(default_factory=list)
    skips: list = dataclasses.field(default_factory=list)


@dataclasses.dataclass
class Ending:
    """How CTest's record says a suite ended."""

    status: str
    code: str = ""
    value: str = ""
    completion: str = ""

    def skipped_itself(self):
        """The suite ran, and said by its SKIP_RETURN_CODE that it could not do its work: not a failure, and never a pass."""
        return self.status == "notrun" and self.completion.startswith("SKIP_RETURN_CODE=")


def read_suites(log):
    """The suites in the text of CTest's log, in the order they ran."""
    suites = []
    for line in log.splitlines():
        if match := SUITE.match(line):
            suites.append(Suite(match.group(1)))
        elif not suites:
            continue
        elif QT_TEST.match(line):
            suites[-1].qt_test = True
        elif match := TOTALS.match(line):
            suite = suites[-1]
            suite.passed, suite.failed, suite.skipped = (int(group) for group in match.groups())
            suite.finished = suite.qt_test = True
        elif match := CASE.match(line):
            suite = suites[-1]
            if match.group(1) == "SKIP   ":
                suite.skips.append(match.group(2))
            elif match.group(1) == "PASS   ":
                # Only for a suite that stops before its totals, which then say nothing.
                suite.passed += 1
            else:
                suite.failures.append(match.group(2))
        elif match := DRAWN_BY.match(line) or RENDERER.match(line):
            suites[-1].renderer = match.group(1)
    return suites


def read_record(record):
    """How each suite of the run ended, by name, from the text of CTest's record. None if the record cannot be read."""
    try:
        root = ElementTree.fromstring(record)
    except ElementTree.ParseError:
        return None
    endings = {}
    for test in root.iter("Test"):
        if test.get("Status") is None:
            continue
        measured = {item.get("name"): item.findtext("Value", "") for item in test.iter("NamedMeasurement")}
        endings[test.findtext("Name", "")] = Ending(test.get("Status"), measured.get("Exit Code", ""), measured.get("Exit Value", ""),
                                                    measured.get("Completion Status", ""))
    return endings


def not_passed(endings):
    """The suites that went wrong, in the record's order: failed, or could not be started. CTest's own list of failures is the same:
    a suite that skipped itself is neither on it nor a pass."""
    return [name for name, ending in endings.items() if ending.status != "passed" and not ending.skipped_itself()]


def ended_as_it_said(suite, endings):
    """Whether a suite ended as what it printed says it should. Qt Test's must reach its totals and exit normally, with its failures
    as its status. Any other suite has only the record's word, and one that skipped itself has done nothing wrong."""
    ending = endings.get(suite.name)
    if ending is None:
        return False
    if ending.skipped_itself():
        return True
    if ending.status == "passed":
        return suite.failed == 0 and (suite.finished or not suite.qt_test)
    return suite.failed > 0 and suite.finished and ending.status == "failed" and ending.code == "Failed" and ending.value == str(suite.failed)


def report(suites, endings=None):
    """Each suite's counts, what drew it, and each case failed or skipped, by name. Qt Test counts initTestCase and cleanupTestCase."""
    lines = []
    for suite in suites:
        drawn = f", drawn by {suite.renderer}" if suite.renderer else ""
        ending = endings.get(suite.name) if endings is not None else None
        if ending is not None and ending.skipped_itself():
            lines.append(f"   {suite.name}: NOT RUN, by its own word ({ending.completion}): this says nothing of it")
        elif ending is not None and ending.status == "notrun":
            lines.append(f"   {suite.name}: NOT RUN (CTest could not start it)")
        elif suite.finished:
            lines.append(f"   {suite.name}: {suite.passed} passed, {suite.failed} failed, {suite.skipped} skipped{drawn}")
        elif ending is not None and not suite.qt_test and ending.status == "passed":
            lines.append(f"   {suite.name}: passed, by its exit status (not a Qt Test suite, so no counts)")
        elif ending is not None and not suite.qt_test and ending.code == "Failed":
            lines.append(f"   {suite.name}: FAILED, exit value {ending.value} (not a Qt Test suite, so no counts)")
        else:
            why = ending.code if ending is not None and ending.code else "a crash or a timeout"
            lines.append(f"   {suite.name}: STOPPED before its end ({why}), {suite.passed} passed by then{drawn}")
        if ending is not None and suite.finished and not ended_as_it_said(suite, endings):
            ending = endings[suite.name]
            lines.append(f"     ended badly: {ending.code or ending.status}, exit value {ending.value or 'none'}")
        lines += [f"     failed: {case}" for case in suite.failures]
        lines += [f"     skipped: {case}" for case in suite.skips]
    if endings is None:
        lines.append("   CTest left no record of how these ended: it may have been stopped, and nothing here is proven")
    return "\n".join(lines)


def only_the_floor_failed(suites, endings):
    """Whether what went wrong was that check alone: tst_player the only suite that did not pass, ended normally, with a
    Player drawn in software and drawing. The log and the record must be of one run, or a partial log could lean on an old record."""
    if endings is None or not_passed(endings) != [FLOOR_SUITE] or set(endings) != {suite.name for suite in suites}:
        return False
    if not all(ended_as_it_said(suite, endings) for suite in suites):
        return False
    failures = [case for suite in suites for case in suite.failures]
    if len(failures) != 1:
        return False
    read = FLOOR_READ.match(failures[0])
    if not read or float(read.group(1)) < STILL_DRAWING_FPS:
        return False
    return SOFTWARE_RENDERER in next(suite for suite in suites if suite.name == FLOOR_SUITE).renderer


def passed_again(names, endings):
    """Whether this run ran each of `names` and it passed: a retry that ran nothing has not passed anything."""
    return bool(names) and endings is not None and all(endings.get(name, Ending("")).status == "passed" for name in names)


def main(arguments):
    arguments = list(arguments)
    mode = next((flag for flag in ("--only-the-floor", "--failed", "--passed-again") if flag in arguments), None)
    if mode:
        arguments.remove(mode)
    if not arguments or (mode != "--passed-again" and len(arguments) != 1):
        print(__doc__, file=sys.stderr)
        return 2
    testing = pathlib.Path(arguments[-1]) / "Testing"
    try:
        tag = (testing / "TAG").read_text(encoding="utf-8").splitlines()[0].strip()
        suites = read_suites((testing / "Temporary" / f"LastTest_{tag}.log").read_text(encoding="utf-8", errors="replace"))
    except (OSError, IndexError) as error:
        print(f"CTest left no log: {error}", file=sys.stderr)
        return 1
    try:
        endings = read_record((testing / tag / "Test.xml").read_text(encoding="utf-8", errors="replace"))
    except OSError:
        endings = None
    if mode == "--only-the-floor":
        return 0 if only_the_floor_failed(suites, endings) else 1
    if mode == "--passed-again":
        return 0 if passed_again(arguments[:-1], endings) else 1
    if mode == "--failed":
        print("\n".join(not_passed(endings) if endings is not None else [suite.name for suite in suites]))
        return 0
    print(report(suites, endings))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
