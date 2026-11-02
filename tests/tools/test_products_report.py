# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""What tools/dev/products-report.py says of CTest's files, and when it lets the QQ Player's floor stand."""

import contextlib
import importlib.util
import io
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

SOFTWARE = "llvmpipe (LLVM 21.1.8, 256 bits) 4.5 (Compatibility Profile) Mesa 26.0.8"


def floor_failed(reads="5.2"):
    return (
        "FAIL!  : TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() "
        f"'m.value(QStringLiteral(\"fps\")).toDouble() >= 20' returned FALSE. ({reads})"
    )


def measured(renderer=SOFTWARE):
    return (
        'QINFO  : TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() {"firstFrameMs":6483,"fps":5.2,"lastFrameAgoMs":120,'
        f'"logicErrors":[],"mainMs":13,"problem":"","renderer":"{renderer}","unloaded":[]}}'
    )


def suite(number, name, *lines, totals=None):
    body = [f"{number}/8 Testing: {name}", f"{number}/8 Test: {name}", 'Command: "/x/tests/' + name + '"', "Output:",
            "----------------------------------------------------------", f"********* Start testing of {name} *********"]
    body += lines
    if totals:
        body.append(f"Totals: {totals[0]} passed, {totals[1]} failed, {totals[2]} skipped, 0 blacklisted, 100ms")
    body += ["<end of output>", "Test time =   1.00 sec", "----------------------------------------------------------"]
    return "\n".join(body) + "\n"


def record(passed=(), notrun=(), skipped=(), **failed):
    """CTest's Test.xml, in the shape it writes: suites that passed, could not run, skipped themselves, and failed with (Exit Code, Exit Value)."""
    tests = "".join(f'<Test Status="passed"><Name>{name}</Name><Results/></Test>' for name in passed)
    tests += "".join(
        f'<Test Status="notrun"><Name>{name}</Name><Results><NamedMeasurement type="text/string" name="Completion Status">'
        f"<Value>{why}</Value></NamedMeasurement></Results></Test>"
        for names, why in ((notrun, "Unable to find executable"), (skipped, "SKIP_RETURN_CODE=77")) for name in names)
    tests += "".join(
        f"""<Test Status="failed"><Name>{name}</Name><Path>./tests</Path><Results>
        <NamedMeasurement type="text/string" name="Exit Code"><Value>{code}</Value></NamedMeasurement>
        <NamedMeasurement type="text/string" name="Exit Value"><Value>{value}</Value></NamedMeasurement>
        <NamedMeasurement type="numeric/double" name="Execution Time"><Value>0.5</Value></NamedMeasurement>
        <Measurement><Value>the output, cut short</Value></Measurement></Results></Test>"""
        for name, (code, value) in failed.items())
    return f"""<?xml version="1.0" encoding="UTF-8"?><Site Name="x"><Testing><TestList><Test>./tests/any</Test></TestList>{tests}</Testing></Site>"""


PASSING = suite(1, "tst_world", "PASS   : TstWorld::initTestCase()", "PASS   : TstWorld::oneCase()", totals=(2, 0, 0))
SKIPPING = suite(2, "tst_agent", "PASS   : TstAgent::oneCase()",
                 "SKIP   : TstAgent::theRealProviderIsReachedOverTls() Set QQ_LIVE_ENDPOINT=1 to reach the real provider.",
                 "   Loc: [tst_agent.cpp(827)]", totals=(1, 0, 1))
FLOOR = suite(3, "tst_player", "PASS   : TstPlayer::oneCase()", measured(), floor_failed(), "   Loc: [tst_player.cpp(107)]", totals=(9, 1, 0))
OTHER_FAILURE = "FAIL!  : TstPlayer::itsFirstFrameIsTheGame() 'colours.size() > 200' returned FALSE. (3)"
FAILED_ONE = {"tst_player": ("Failed", "1")}


def player(*lines, renderer=SOFTWARE, totals=(9, 1, 0)):
    return suite(3, "tst_player", measured(renderer), *lines, totals=totals)


def not_qt_test(number, name, *lines):
    """As CTest's log keeps a suite that is not Qt Test: what it printed, with no "Start testing" line and no totals."""
    body = [f"{number}/8 Testing: {name}", f"{number}/8 Test: {name}", f'Command: "/x/{name}"', "Output:",
            "----------------------------------------------------------", *lines,
            "<end of output>", "Test time =   1.00 sec", "----------------------------------------------------------"]
    return "\n".join(body) + "\n"


JOURNEY = not_qt_test(9, "first_journey", "== a step of its own", "== another")


class ASuiteThatIsNotQtTest(unittest.TestCase):
    def said(self, *args, **kwargs):
        return report.report(report.read_suites(JOURNEY), report.read_record(record(*args, **kwargs)))

    def test_passing_is_said_as_passing_with_no_counts(self):
        said = self.said(["first_journey"])
        self.assertIn("first_journey: passed, by its exit status", said)
        self.assertNotIn("STOPPED", said)

    def test_skipping_itself_reads_as_not_run_and_never_as_a_pass(self):
        said = self.said(skipped=["first_journey"])
        self.assertIn("first_journey: NOT RUN, by its own word (SKIP_RETURN_CODE=77)", said)
        self.assertNotIn("passed", said)
        self.assertNotIn("STOPPED", said)

    def test_skipping_itself_is_not_passing_it_again(self):
        self.assertFalse(report.passed_again(["first_journey"], report.read_record(record(skipped=["first_journey"]))))

    def test_skipping_itself_is_not_going_wrong(self):
        self.assertEqual(report.not_passed(report.read_record(record(skipped=["first_journey"]))), [])

    def test_failing_is_said_as_failing(self):
        self.assertIn("first_journey: FAILED, exit value 3", self.said(first_journey=("Failed", "3")))

    def test_dying_is_said_as_stopping_with_how(self):
        self.assertIn("first_journey: STOPPED before its end (SEGFAULT)", self.said(first_journey=("SEGFAULT", "0")))

    def test_a_qt_test_suite_with_no_totals_stopped_whatever_its_exit_status(self):
        stopped = suite(4, "tst_games", "PASS   : TestGames::initTestCase()")
        said = report.report(report.read_suites(stopped), report.read_record(record(["tst_games"])))
        self.assertIn("tst_games: STOPPED before its end", said)


class ReadingTheLog(unittest.TestCase):
    def test_a_suite_that_finished_is_counted_by_its_totals(self):
        [only] = report.read_suites(PASSING)
        self.assertEqual((only.name, only.passed, only.failed, only.skipped, only.finished), ("tst_world", 2, 0, 0, True))

    def test_a_case_skipped_in_a_suite_that_passed_is_named_with_why(self):
        said = report.report(report.read_suites(PASSING + SKIPPING))
        self.assertIn("tst_agent: 1 passed, 0 failed, 1 skipped", said)
        self.assertIn("skipped: TstAgent::theRealProviderIsReachedOverTls() Set QQ_LIVE_ENDPOINT=1", said)

    def test_a_case_that_failed_is_named_with_what_it_read(self):
        said = report.report(report.read_suites(FLOOR), report.read_record(record(**FAILED_ONE)))
        self.assertIn("tst_player: 9 passed, 1 failed, 0 skipped", said)
        self.assertIn("failed: " + floor_failed()[len("FAIL!  : "):], said)

    def test_what_drew_a_suite_is_said(self):
        drawn = suite(1, "tst_play", f"QINFO  : TstPlay::oneCase() Drawn by {SOFTWARE}; expected: software", totals=(1, 0, 0))
        self.assertEqual(report.read_suites(drawn)[0].renderer, SOFTWARE)
        self.assertIn(f"tst_play: 1 passed, 0 failed, 0 skipped, drawn by {SOFTWARE}", report.report(report.read_suites(drawn)))
        self.assertEqual(report.read_suites(FLOOR)[0].renderer, SOFTWARE)

    def test_a_suite_stopped_for_its_renderer_says_why(self):
        stopped = suite(1, "tst_play", "QFATAL : TstPlay::oneCase() The renderer is X, and products.sh expected software here")
        self.assertIn("failed: TstPlay::oneCase() The renderer is X", report.report(report.read_suites(stopped)))

    def test_a_suite_that_stopped_says_so_and_what_it_reached(self):
        stopped = suite(4, "tst_games", "PASS   : TestGames::initTestCase()", "PASS   : TestGames::oneCase()")
        said = report.report(report.read_suites(stopped))
        self.assertIn("tst_games: STOPPED before its end", said)
        self.assertIn("2 passed by then", said)

    def test_a_crash_after_the_totals_is_said(self):
        endings = report.read_record(record(tst_player=("SEGFAULT", "0")))
        said = report.report(report.read_suites(FLOOR), endings)
        self.assertIn("tst_player: 9 passed, 1 failed, 0 skipped", said)
        self.assertIn("ended badly: SEGFAULT, exit value 0", said)

    def test_a_normal_failure_is_not_said_to_have_ended_badly(self):
        self.assertNotIn("ended badly", report.report(report.read_suites(FLOOR), report.read_record(record(**FAILED_ONE))))

    def test_a_suite_that_could_not_run_is_said(self):
        said = report.report(report.read_suites(suite(4, "tst_gone")), report.read_record(record(notrun=["tst_gone"])))
        self.assertIn("tst_gone: NOT RUN", said)

    def test_no_record_is_said(self):
        self.assertIn("CTest left no record", report.report(report.read_suites(PASSING), None))

    def test_what_comes_before_the_first_suite_is_not_counted(self):
        self.assertEqual(report.read_suites("Start testing: Nov 01\nPASS   : Nobody::nothing()\n"), [])


class ReadingTheRecord(unittest.TestCase):
    def test_every_suite_is_read_with_how_it_ended(self):
        endings = report.read_record(record(passed=["tst_world"], notrun=["tst_gone"], tst_player=("Failed", "1"), tst_agent=("Subprocess aborted", "0")))
        self.assertEqual(endings, {"tst_world": report.Ending("passed"), "tst_gone": report.Ending("notrun", completion="Unable to find executable"),
                                   "tst_player": report.Ending("failed", "Failed", "1"),
                                   "tst_agent": report.Ending("failed", "Subprocess aborted", "0")})

    def test_a_suite_that_could_not_run_has_not_passed(self):
        self.assertEqual(report.not_passed(report.read_record(record(passed=["tst_world"], notrun=["tst_gone"], tst_player=("Failed", "1")))),
                         ["tst_gone", "tst_player"])

    def test_a_record_that_is_not_one_reads_as_nothing(self):
        self.assertIsNone(report.read_record("<Site><Testing><Test Status='failed'>"))
        self.assertIsNone(report.read_record(""))


class TheFloor(unittest.TestCase):
    def stands(self, log, passed=(), notrun=(), skipped=(), **failed):
        return report.only_the_floor_failed(report.read_suites(log), report.read_record(record(passed, notrun, skipped, **failed)))

    def test_a_suite_that_is_not_qt_test_and_passed_does_not_stop_it(self):
        self.assertTrue(self.stands(FLOOR + JOURNEY, ["first_journey"], **FAILED_ONE))

    def test_a_suite_that_skipped_itself_does_not_stop_it_and_is_said(self):
        self.assertTrue(self.stands(FLOOR + JOURNEY, skipped=["first_journey"], **FAILED_ONE))

    def test_it_does_not_when_a_suite_that_is_not_qt_test_failed(self):
        self.assertFalse(self.stands(FLOOR + JOURNEY, tst_player=("Failed", "1"), first_journey=("Failed", "1")))

    def test_it_does_not_when_a_suite_that_is_not_qt_test_died(self):
        self.assertFalse(self.stands(FLOOR + JOURNEY, tst_player=("Failed", "1"), first_journey=("SEGFAULT", "0")))

    def test_it_does_not_when_a_suite_that_is_not_qt_test_could_not_start(self):
        self.assertFalse(self.stands(FLOOR + JOURNEY, notrun=["first_journey"], **FAILED_ONE))

    def test_it_does_not_when_a_suite_that_passed_says_it_failed_a_case(self):
        self.assertFalse(self.stands(FLOOR + not_qt_test(9, "first_journey", "FAIL!  : Journey::aStep() no"), ["first_journey"], **FAILED_ONE))

    def test_it_does_not_when_a_qt_test_suite_ended_with_no_totals_and_said_it_passed(self):
        self.assertFalse(self.stands(FLOOR + suite(7, "tst_play", "PASS   : TstPlay::a()"), ["tst_play"], **FAILED_ONE))

    def test_it_stands_when_it_is_all_that_failed(self):
        self.assertTrue(self.stands(PASSING + FLOOR, ["tst_world"], **FAILED_ONE))

    def test_it_stands_at_the_bound_of_a_frame_a_second(self):
        self.assertTrue(self.stands(player(floor_failed("1")), **FAILED_ONE))

    def test_it_does_not_when_the_player_has_stopped_drawing(self):
        for reads in ("0", "0.5", "0.9"):
            self.assertFalse(self.stands(player(floor_failed(reads)), **FAILED_ONE), reads)

    def test_it_does_not_when_the_check_reads_nothing_it_can_read(self):
        for reads in ("", "nan", "-5", "5 and more"):
            self.assertFalse(self.stands(player(floor_failed(reads)), **FAILED_ONE), reads)

    def test_it_does_not_when_the_player_was_drawn_by_anything_else(self):
        # No render node, so the script expected software, and a renderer that is not it: the floor means something there.
        self.assertFalse(self.stands(player(floor_failed(), renderer="NVIDIA GeForce RTX 4070"), **FAILED_ONE))

    def test_it_does_not_when_nothing_says_what_drew_the_player(self):
        self.assertFalse(self.stands(suite(3, "tst_player", floor_failed(), totals=(9, 1, 0)), **FAILED_ONE))

    def test_it_does_not_when_the_player_stopped_drawing_part_way(self):
        # A Player that drew four frames and stopped reads 1.3: tst_player's own check fails first, and that is no floor.
        stopped = "FAIL!  : TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() 'still drawing' returned FALSE. (its last frame was 2400 ms before the end)"
        self.assertFalse(self.stands(player(stopped), **FAILED_ONE))

    def test_it_does_not_when_the_player_crashed_after_its_totals(self):
        for ending in (("SEGFAULT", "0"), ("Subprocess aborted", "0"), ("Subprocess killed", "0"), ("Timeout", "0"),
                       ("SEGFAULT", "1")):
            self.assertFalse(self.stands(FLOOR, tst_player=ending), ending)

    def test_it_does_not_when_its_exit_status_is_not_its_one_failure(self):
        for value in ("0", "2", "127", ""):
            self.assertFalse(self.stands(FLOOR, tst_player=("Failed", value)), value)

    def test_it_does_not_when_the_player_crashed_before_its_totals(self):
        self.assertFalse(self.stands(suite(3, "tst_player", measured(), floor_failed()), **FAILED_ONE))

    def test_it_does_not_with_another_failure_in_its_suite(self):
        self.assertFalse(self.stands(player(floor_failed(), OTHER_FAILURE, totals=(8, 2, 0)), tst_player=("Failed", "2")))

    def test_it_does_not_with_another_suite_failed(self):
        self.assertFalse(self.stands(FLOOR + suite(6, "tst_agent", "FAIL!  : TstAgent::a() no", totals=(0, 1, 0)),
                                     tst_player=("Failed", "1"), tst_agent=("Failed", "1")))

    def test_it_does_not_when_ctest_failed_another_suite_that_printed_no_failure(self):
        # An exit value of 0 is CTest's own verdict: a FAIL_REGULAR_EXPRESSION that matched, or a WILL_FAIL that did not fail.
        for value in ("3", "0"):
            self.assertFalse(self.stands(PASSING + FLOOR, tst_player=("Failed", "1"), tst_world=("Failed", value)), value)

    def test_it_does_not_when_another_suite_counts_a_failure_it_never_names(self):
        self.assertFalse(self.stands(FLOOR + suite(6, "tst_agent", totals=(0, 1, 0)), tst_player=("Failed", "1"), tst_agent=("Failed", "1")))

    def test_it_does_not_when_a_suite_could_not_run(self):
        self.assertFalse(self.stands(FLOOR + suite(5, "tst_gone"), notrun=["tst_gone"], **FAILED_ONE))

    def test_it_does_not_when_another_suite_stopped(self):
        self.assertFalse(self.stands(FLOOR + suite(7, "tst_play", "PASS   : TstPlay::a()"), ["tst_play"], **FAILED_ONE))

    def test_it_does_not_when_a_suite_passed_with_a_failure_in_its_log(self):
        self.assertFalse(self.stands(FLOOR + suite(7, "tst_play", totals=(1, 1, 0)), ["tst_play"], **FAILED_ONE))

    def test_it_does_not_for_the_two_dimensional_games_check(self):
        other = "FAIL!  : TstPlayer::aTwoDimensionalGamePlaysToo() 'm.value(QStringLiteral(\"fps\")).toDouble() >= 20' returned FALSE. (5)"
        self.assertFalse(self.stands(player(other), **FAILED_ONE))

    def test_nothing_failed_is_not_the_floor(self):
        self.assertFalse(self.stands(PASSING, ["tst_world"]))

    def test_an_unreadable_record_is_not_the_floor(self):
        self.assertFalse(report.only_the_floor_failed(report.read_suites(FLOOR), None))

    def test_a_partial_log_beside_an_old_record_is_not_the_floor(self):
        # A CTest killed part way leaves its log, and the record of the run before: the two must be of one run.
        self.assertFalse(self.stands(FLOOR, ["tst_world"], **FAILED_ONE))
        self.assertFalse(self.stands(PASSING + FLOOR, **FAILED_ONE))


class PassedAgain(unittest.TestCase):
    def again(self, names, xml):
        return report.passed_again(names, report.read_record(xml))

    def test_each_suite_it_was_to_run_passed(self):
        self.assertTrue(self.again(["tst_flaky"], record(["tst_flaky"])))

    def test_running_nothing_is_not_passing(self):
        self.assertFalse(self.again(["tst_flaky"], record()))
        self.assertFalse(self.again([], record(["tst_flaky"])))
        self.assertFalse(report.passed_again(["tst_flaky"], None))

    def test_running_something_else_is_not_passing(self):
        self.assertFalse(self.again(["tst_flaky", "tst_other"], record(["tst_flaky"])))

    def test_failing_again_is_not_passing(self):
        self.assertFalse(self.again(["tst_flaky"], record(tst_flaky=("Failed", "1"))))
        self.assertFalse(self.again(["tst_flaky"], record(notrun=["tst_flaky"])))


class TheProgram(unittest.TestCase):
    def folder(self, log, xml=None, tag="20261101-1200"):
        made = tempfile.TemporaryDirectory()
        self.addCleanup(made.cleanup)
        testing = pathlib.Path(made.name) / "Testing"
        (testing / "Temporary").mkdir(parents=True)
        (testing / tag).mkdir()
        (testing / "TAG").write_text(f"{tag}\nExperimental\n", encoding="utf-8")
        (testing / "Temporary" / f"LastTest_{tag}.log").write_text(log, encoding="utf-8")
        if xml is not None:
            (testing / tag / "Test.xml").write_text(xml, encoding="utf-8")
        return made.name

    def test_it_answers_by_its_exit_code(self):
        self.assertEqual(report.main(["--only-the-floor", self.folder(FLOOR, record(**FAILED_ONE))]), 0)
        self.assertEqual(report.main(["--only-the-floor", self.folder(FLOOR, record(tst_player=("SEGFAULT", "0")))]), 1)

    def test_no_failure_recorded_is_no_floor(self):
        self.assertEqual(report.main(["--only-the-floor", self.folder(PASSING, record(["tst_world"]))]), 1)

    def test_a_broken_record_is_no_floor(self):
        self.assertEqual(report.main(["--only-the-floor", self.folder(FLOOR, "<Site>")]), 1)

    def test_a_missing_record_is_no_floor_and_is_said(self):
        folder = self.folder(FLOOR)
        self.assertEqual(report.main(["--only-the-floor", folder]), 1)
        self.assertEqual(report.main([folder]), 0)

    def test_a_retry_is_read_by_the_program(self):
        self.assertEqual(report.main(["--passed-again", "tst_flaky", self.folder(PASSING, record(["tst_flaky"]))]), 0)
        self.assertEqual(report.main(["--passed-again", "tst_flaky", self.folder(PASSING, record())]), 1)

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
        print('QINFO  : TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() {"fps":' + reads + ',"renderer":"llvmpipe (LLVM 21)"}')
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

    def run_stand_in(self, how, reads="5.9", more=""):
        made = tempfile.TemporaryDirectory()
        self.addCleanup(made.cleanup)
        source = pathlib.Path(made.name) / "source"
        source.mkdir()
        (source / "stand_in.py").write_text(self.STAND_IN, encoding="utf-8")
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.16)\nproject(standin NONE)\nenable_testing()\n"
            f'add_test(NAME tst_player COMMAND "{sys.executable}" -I "{source}/stand_in.py" {how} {reads})\n{more}',
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

    def test_a_suite_that_could_not_run_does_not(self):
        build = self.run_stand_in("exit", more="add_test(NAME tst_gone COMMAND /no/such/program)\n")
        self.assertEqual(report.main(["--only-the-floor", build]), 1)

    def another_suite(self, ends, extra=""):
        """A build with the floor failed, and beside it a suite that is not Qt Test, which prints no totals and ends with `ends`."""
        return self.run_stand_in("exit", more=(
            f'add_test(NAME first_journey COMMAND "{sys.executable}" -I -c '
            f'"print(\'== a step of its own\', flush=True); import os, signal, sys; {ends}")\n{extra}'))

    def said(self, build):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            report.main([build])
        return out.getvalue()

    def test_another_suite_that_passed_lets_it_stand_and_reads_as_passed(self):
        build = self.another_suite("sys.exit(0)")
        self.assertEqual(report.main(["--only-the-floor", build]), 0)
        self.assertIn("first_journey: passed, by its exit status", self.said(build))

    def test_another_suite_that_skipped_itself_lets_it_stand_and_reads_as_not_run(self):
        build = self.another_suite("sys.exit(77)", "set_tests_properties(first_journey PROPERTIES SKIP_RETURN_CODE 77)\n")
        self.assertEqual(report.main(["--only-the-floor", build]), 0)
        said = self.said(build)
        self.assertIn("first_journey: NOT RUN, by its own word (SKIP_RETURN_CODE=77)", said)
        self.assertNotIn("first_journey: passed", said)

    def test_another_suite_that_failed_does_not(self):
        build = self.another_suite("sys.exit(3)")
        self.assertEqual(report.main(["--only-the-floor", build]), 1)
        self.assertIn("first_journey: FAILED, exit value 3", self.said(build))

    def test_77_with_no_skip_code_is_a_failure(self):
        self.assertEqual(report.main(["--only-the-floor", self.another_suite("sys.exit(77)")]), 1)

    def test_another_suite_that_died_does_not(self):
        build = self.another_suite("os.kill(os.getpid(), signal.SIGSEGV)")
        self.assertEqual(report.main(["--only-the-floor", build]), 1)
        self.assertIn("first_journey: STOPPED before its end (SEGFAULT)", self.said(build))

    def test_a_suite_ctest_failed_though_it_counted_no_failure_does_not_and_is_said(self):
        # It prints clean totals and exits 0, and CTest fails it on what it printed: "0 failed" must not be all the report says.
        build = self.run_stand_in("exit", more=(
            f'add_test(NAME tst_world COMMAND "{sys.executable}" -I -c "print(\'Totals: 1 passed, 0 failed, 0 skipped, 0 blacklisted, 1ms\')")\n'
            'set_tests_properties(tst_world PROPERTIES FAIL_REGULAR_EXPRESSION "Totals")\n'))
        self.assertEqual(report.main(["--only-the-floor", build]), 1)
        self.assertIn("tst_world: 1 passed, 0 failed, 0 skipped\n     ended badly: Failed, exit value 0", self.said(build))


if __name__ == "__main__":
    unittest.main()
