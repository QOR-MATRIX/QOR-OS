# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""What tools/dev/products-report.py says of CTest's files, and when it lets the QQ Player's floor stand."""

import importlib.util
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

_SCRIPT = pathlib.Path(__file__).resolve().parents[2] / "tools" / "dev" / "products-report.py"
_spec = importlib.util.spec_from_file_location("products_report", _SCRIPT)
report = importlib.util.module_from_spec(_spec)
sys.modules["products_report"] = report
_spec.loader.exec_module(report)


def floor_failed(reads="5.2"):
    return (
        "FAIL!  : TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() "
        f"'m.value(QStringLiteral(\"fps\")).toDouble() >= 20' returned FALSE. ({reads})"
    )


def suite(number, name, *lines, totals=None):
    body = [f"{number}/8 Testing: {name}", f"{number}/8 Test: {name}", 'Command: "/x/tests/' + name + '"', "Output:",
            "----------------------------------------------------------", f"********* Start testing of {name} *********"]
    body += lines
    if totals:
        body.append(f"Totals: {totals[0]} passed, {totals[1]} failed, {totals[2]} skipped, 0 blacklisted, 100ms")
    body += ["<end of output>", "Test time =   1.00 sec", "----------------------------------------------------------"]
    return "\n".join(body) + "\n"


def record(**failed):
    """CTest's Test.xml, in the shape it writes, with each suite named as failed with its (Exit Code, Exit Value)."""
    tests = "".join(
        f"""<Test Status="failed"><Name>{name}</Name><Path>./tests</Path><Results>
        <NamedMeasurement type="text/string" name="Exit Code"><Value>{code}</Value></NamedMeasurement>
        <NamedMeasurement type="text/string" name="Exit Value"><Value>{value}</Value></NamedMeasurement>
        <NamedMeasurement type="numeric/double" name="Execution Time"><Value>0.5</Value></NamedMeasurement>
        <Measurement><Value>the output, cut short</Value></Measurement></Results></Test>"""
        for name, (code, value) in failed.items())
    return f"""<?xml version="1.0" encoding="UTF-8"?><Site Name="x"><Testing><TestList><Test>./tests/any</Test></TestList>
    <Test Status="passed"><Name>tst_world</Name><Results/></Test>{tests}</Testing></Site>"""


PASSING = suite(1, "tst_world", "PASS   : TstWorld::initTestCase()", "PASS   : TstWorld::oneCase()", totals=(2, 0, 0))
SKIPPING = suite(2, "tst_agent", "PASS   : TstAgent::oneCase()",
                 "SKIP   : TstAgent::theRealProviderIsReachedOverTls() Set QQ_LIVE_ENDPOINT=1 to reach the real provider.",
                 "   Loc: [tst_agent.cpp(827)]", totals=(1, 0, 1))
FLOOR = suite(3, "tst_player", "PASS   : TstPlayer::oneCase()", floor_failed(), "   Loc: [tst_player.cpp(107)]", totals=(9, 1, 0))
OTHER_FAILURE = "FAIL!  : TstPlayer::itsFirstFrameIsTheGame() 'colours.size() > 200' returned FALSE. (3)"
FAILED_ONE = {"tst_player": ("Failed", "1")}


class ReadingTheLog(unittest.TestCase):
    def test_a_suite_that_finished_is_counted_by_its_totals(self):
        [only] = report.read_suites(PASSING)
        self.assertEqual((only.name, only.passed, only.failed, only.skipped, only.finished), ("tst_world", 2, 0, 0, True))

    def test_a_case_skipped_in_a_suite_that_passed_is_named_with_why(self):
        said = report.report(report.read_suites(PASSING + SKIPPING))
        self.assertIn("tst_agent: 1 passed, 0 failed, 1 skipped", said)
        self.assertIn("skipped: TstAgent::theRealProviderIsReachedOverTls() Set QQ_LIVE_ENDPOINT=1", said)

    def test_a_case_that_failed_is_named_with_what_it_read(self):
        said = report.report(report.read_suites(FLOOR))
        self.assertIn("tst_player: 9 passed, 1 failed, 0 skipped", said)
        self.assertIn("failed: " + floor_failed()[len("FAIL!  : "):], said)

    def test_a_suite_that_stopped_says_so_and_what_it_reached(self):
        stopped = suite(4, "tst_games", "PASS   : TestGames::initTestCase()", "PASS   : TestGames::oneCase()")
        said = report.report(report.read_suites(stopped))
        self.assertIn("tst_games: STOPPED before its end", said)
        self.assertIn("2 passed by then", said)

    def test_a_crash_after_the_totals_is_said(self):
        endings = report.read_endings(record(tst_player=("SEGFAULT", "0")))
        said = report.report(report.read_suites(FLOOR), endings)
        self.assertIn("tst_player: 9 passed, 1 failed, 0 skipped", said)
        self.assertIn("ended badly: SEGFAULT, exit value 0", said)

    def test_a_normal_failure_is_not_said_to_have_ended_badly(self):
        self.assertNotIn("ended badly", report.report(report.read_suites(FLOOR), report.read_endings(record(**FAILED_ONE))))

    def test_what_comes_before_the_first_suite_is_not_counted(self):
        self.assertEqual(report.read_suites("Start testing: Nov 01\nPASS   : Nobody::nothing()\n"), [])


class ReadingTheRecord(unittest.TestCase):
    def test_only_failed_suites_are_read_with_how_they_ended(self):
        endings = report.read_endings(record(tst_player=("Failed", "1"), tst_agent=("Subprocess aborted", "0")))
        self.assertEqual(endings, {"tst_player": report.Ending("Failed", "1"), "tst_agent": report.Ending("Subprocess aborted", "0")})

    def test_a_record_that_is_not_one_reads_as_nothing(self):
        self.assertIsNone(report.read_endings("<Site><Testing><Test Status='failed'>"))
        self.assertIsNone(report.read_endings(""))


class TheFloor(unittest.TestCase):
    def stands(self, log, **failed):
        return report.only_the_floor_failed(report.read_suites(log), report.read_endings(record(**failed)))

    def test_it_stands_when_it_is_all_that_failed(self):
        self.assertTrue(self.stands(PASSING + FLOOR, **FAILED_ONE))

    def test_it_stands_at_the_bound_of_a_frame_a_second(self):
        self.assertTrue(self.stands(suite(3, "tst_player", floor_failed("1"), totals=(9, 1, 0)), **FAILED_ONE))

    def test_it_does_not_when_the_player_has_stopped_drawing(self):
        for reads in ("0", "0.5", "0.9"):
            self.assertFalse(self.stands(suite(3, "tst_player", floor_failed(reads), totals=(9, 1, 0)), **FAILED_ONE), reads)

    def test_it_does_not_when_the_check_reads_nothing_it_can_read(self):
        for reads in ("", "nan", "-5", "5 and more"):
            self.assertFalse(self.stands(suite(3, "tst_player", floor_failed(reads), totals=(9, 1, 0)), **FAILED_ONE), reads)

    def test_it_does_not_when_the_player_crashed_after_its_totals(self):
        for ending in (("SEGFAULT", "0"), ("Subprocess aborted", "0"), ("Subprocess killed", "0"), ("Timeout", "0"),
                       ("SEGFAULT", "1")):
            self.assertFalse(self.stands(FLOOR, tst_player=ending), ending)

    def test_it_does_not_when_its_exit_status_is_not_its_one_failure(self):
        for value in ("0", "2", "127", ""):
            self.assertFalse(self.stands(FLOOR, tst_player=("Failed", value)), value)

    def test_it_does_not_when_the_player_crashed_before_its_totals(self):
        self.assertFalse(self.stands(suite(3, "tst_player", floor_failed()), **FAILED_ONE))

    def test_it_does_not_with_another_failure_in_its_suite(self):
        self.assertFalse(self.stands(suite(3, "tst_player", floor_failed(), OTHER_FAILURE, totals=(8, 2, 0)),
                                     tst_player=("Failed", "2")))

    def test_it_does_not_with_another_suite_failed(self):
        self.assertFalse(self.stands(FLOOR + suite(6, "tst_agent", "FAIL!  : TstAgent::a() no", totals=(0, 1, 0)),
                                     tst_player=("Failed", "1"), tst_agent=("Failed", "1")))

    def test_it_does_not_when_ctest_failed_another_suite_that_printed_no_failure(self):
        self.assertFalse(self.stands(PASSING + FLOOR, tst_player=("Failed", "1"), tst_world=("Failed", "3")))

    def test_it_does_not_when_another_suite_stopped(self):
        self.assertFalse(self.stands(FLOOR + suite(7, "tst_play", "PASS   : TstPlay::a()"), **FAILED_ONE))

    def test_it_does_not_for_the_two_dimensional_games_check(self):
        other = "FAIL!  : TstPlayer::aTwoDimensionalGamePlaysToo() 'm.value(QStringLiteral(\"fps\")).toDouble() >= 20' returned FALSE. (5)"
        self.assertFalse(self.stands(suite(3, "tst_player", other, totals=(9, 1, 0)), **FAILED_ONE))

    def test_nothing_failed_is_not_the_floor(self):
        self.assertFalse(self.stands(PASSING))

    def test_an_unreadable_record_is_not_the_floor(self):
        self.assertFalse(report.only_the_floor_failed(report.read_suites(FLOOR), None))


class TheProgram(unittest.TestCase):
    def folder(self, log, xml=None, tag="20261101-1200"):
        made = tempfile.TemporaryDirectory()
        self.addCleanup(made.cleanup)
        testing = pathlib.Path(made.name) / "Testing"
        (testing / "Temporary").mkdir(parents=True)
        (testing / tag).mkdir()
        (testing / "TAG").write_text(f"{tag}\nExperimental\n", encoding="utf-8")
        (testing / "Temporary" / f"LastTest_{tag}.log").write_text(log, encoding="utf-8")
        (testing / tag / "Test.xml").write_text(xml if xml is not None else record(), encoding="utf-8")
        return made.name

    def test_it_answers_by_its_exit_code(self):
        self.assertEqual(report.main(["--only-the-floor", self.folder(FLOOR, record(**FAILED_ONE))]), 0)
        self.assertEqual(report.main(["--only-the-floor", self.folder(FLOOR, record(tst_player=("SEGFAULT", "0")))]), 1)

    def test_no_failure_recorded_is_no_floor(self):
        self.assertEqual(report.main(["--only-the-floor", self.folder(PASSING)]), 1)

    def test_a_broken_record_is_no_floor(self):
        self.assertEqual(report.main(["--only-the-floor", self.folder(FLOOR, "<Site>")]), 1)

    def test_nothing_left_by_ctest_is_said(self):
        with tempfile.TemporaryDirectory() as empty:
            self.assertEqual(report.main([empty]), 1)


@unittest.skipUnless(os.name == "posix" and shutil.which("cmake") and shutil.which("ctest"), "needs CMake and a POSIX signal")
class WhatCTestReallyWrites(unittest.TestCase):
    """A stand-in for tst_player that ends each way there is, run by the real CTest, so that its files cannot drift from the above."""

    STAND_IN = textwrap.dedent(
        """\
        import os, signal, sys
        how, reads = sys.argv[1], sys.argv[2]
        print("FAIL!  : TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() "
              "'m.value(QStringLiteral(\\"fps\\")).toDouble() >= 20' returned FALSE. (" + reads + ")")
        print("Totals: 9 passed, 1 failed, 0 skipped, 0 blacklisted, 100ms", flush=True)
        if how == "exit":
            sys.exit(1)
        if how == "exit-two":
            sys.exit(2)
        os.kill(os.getpid(), getattr(signal, how))
        """
    )

    def run_stand_in(self, how, reads="5.9"):
        made = tempfile.TemporaryDirectory()
        self.addCleanup(made.cleanup)
        source = pathlib.Path(made.name) / "source"
        source.mkdir()
        (source / "stand_in.py").write_text(self.STAND_IN, encoding="utf-8")
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.16)\nproject(standin NONE)\nenable_testing()\n"
            f'add_test(NAME tst_player COMMAND "{sys.executable}" -I "{source}/stand_in.py" {how} {reads})\n',
            encoding="utf-8")
        build = pathlib.Path(made.name) / "build"
        subprocess.run(["cmake", "-S", str(source), "-B", str(build)], check=True, capture_output=True)
        subprocess.run(["ctest", "--test-dir", str(build), "-T", "Test"], capture_output=True)
        return str(build)

    def test_a_normal_failure_stands(self):
        self.assertEqual(report.main(["--only-the-floor", self.run_stand_in("exit")]), 0)

    def test_a_player_that_has_stopped_drawing_does_not(self):
        self.assertEqual(report.main(["--only-the-floor", self.run_stand_in("exit", "0")]), 1)

    def test_an_exit_status_that_is_not_its_one_failure_does_not(self):
        self.assertEqual(report.main(["--only-the-floor", self.run_stand_in("exit-two")]), 1)

    def test_a_crash_after_its_totals_does_not(self):
        for signal in ("SIGSEGV", "SIGABRT", "SIGKILL"):
            self.assertEqual(report.main(["--only-the-floor", self.run_stand_in(signal)]), 1, signal)


if __name__ == "__main__":
    unittest.main()
