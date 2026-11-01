# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""What tools/dev/products-report.py says of a CTest log, and when it lets the QQ Player's floor stand."""

import importlib.util
import pathlib
import sys
import tempfile
import unittest

_SCRIPT = pathlib.Path(__file__).resolve().parents[2] / "tools" / "dev" / "products-report.py"
_spec = importlib.util.spec_from_file_location("products_report", _SCRIPT)
report = importlib.util.module_from_spec(_spec)
sys.modules["products_report"] = report
_spec.loader.exec_module(report)

FLOOR_FAILED = (
    "FAIL!  : TstPlayer::thePlaygroundPlaysAndItsStartIsMeasured() "
    "'m.value(QStringLiteral(\"fps\")).toDouble() >= 20' returned FALSE. (5.2)"
)


def suite(number, name, *lines, totals=None):
    body = [f"{number}/8 Testing: {name}", f"{number}/8 Test: {name}", 'Command: "/x/tests/' + name + '"', "Output:",
            "----------------------------------------------------------", f"********* Start testing of {name} *********"]
    body += lines
    if totals:
        body.append(f"Totals: {totals[0]} passed, {totals[1]} failed, {totals[2]} skipped, 0 blacklisted, 100ms")
    body += ["<end of output>", "Test time =   1.00 sec", "----------------------------------------------------------"]
    return "\n".join(body) + "\n"


PASSING = suite(1, "tst_world", "PASS   : TstWorld::initTestCase()", "PASS   : TstWorld::oneCase()", totals=(2, 0, 0))
SKIPPING = suite(2, "tst_agent", "PASS   : TstAgent::oneCase()",
                 "SKIP   : TstAgent::theRealProviderIsReachedOverTls() Set QQ_LIVE_ENDPOINT=1 to reach the real provider.",
                 "   Loc: [tst_agent.cpp(827)]", totals=(1, 0, 1))
FLOOR = suite(3, "tst_player", "PASS   : TstPlayer::oneCase()", FLOOR_FAILED, "   Loc: [tst_player.cpp(107)]", totals=(9, 1, 0))
OTHER_FAILURE = "FAIL!  : TstPlayer::itsFirstFrameIsTheGame() 'colours.size() > 200' returned FALSE. (3)"


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
        self.assertIn("failed: " + FLOOR_FAILED[len("FAIL!  : "):], said)

    def test_a_suite_that_stopped_says_so_and_what_it_reached(self):
        stopped = suite(4, "tst_games", "PASS   : TestGames::initTestCase()", "PASS   : TestGames::oneCase()")
        said = report.report(report.read_suites(stopped))
        self.assertIn("tst_games: STOPPED before its end", said)
        self.assertIn("2 passed by then", said)

    def test_what_comes_before_the_first_suite_is_not_counted(self):
        self.assertEqual(report.read_suites("Start testing: Nov 01\nPASS   : Nobody::nothing()\n"), [])

    def test_failed_suites_are_read_by_name(self):
        self.assertEqual(report.read_failed("5:tst_player\n6:tst_agent\n"), ["tst_player", "tst_agent"])


class TheFloor(unittest.TestCase):
    def stands(self, log, failed):
        return report.only_the_floor_failed(report.read_suites(log), failed)

    def test_it_stands_when_it_is_all_that_failed(self):
        self.assertTrue(self.stands(PASSING + FLOOR, ["tst_player"]))

    def test_it_does_not_with_another_failure_in_its_suite(self):
        log = suite(3, "tst_player", FLOOR_FAILED, OTHER_FAILURE, totals=(8, 2, 0))
        self.assertFalse(self.stands(log, ["tst_player"]))

    def test_it_does_not_with_another_suite_failed(self):
        self.assertFalse(self.stands(FLOOR + suite(6, "tst_agent", "FAIL!  : TstAgent::a() no", totals=(0, 1, 0)),
                                     ["tst_player", "tst_agent"]))

    def test_it_does_not_when_ctest_failed_another_suite_that_printed_no_failure(self):
        self.assertFalse(self.stands(PASSING + FLOOR, ["tst_player", "tst_world"]))

    def test_it_does_not_when_the_player_crashed_after_it(self):
        self.assertFalse(self.stands(suite(3, "tst_player", FLOOR_FAILED), ["tst_player"]))

    def test_it_does_not_when_another_suite_stopped(self):
        self.assertFalse(self.stands(FLOOR + suite(7, "tst_play", "PASS   : TstPlay::a()"), ["tst_player"]))

    def test_it_does_not_for_the_two_dimensional_games_check(self):
        other = "FAIL!  : TstPlayer::aTwoDimensionalGamePlaysToo() 'm.value(QStringLiteral(\"fps\")).toDouble() >= 20' returned FALSE. (5)"
        self.assertFalse(self.stands(suite(3, "tst_player", other, totals=(9, 1, 0)), ["tst_player"]))

    def test_nothing_failed_is_not_the_floor(self):
        self.assertFalse(self.stands(PASSING, []))


class TheProgram(unittest.TestCase):
    def folder(self, log, failed=None):
        made = tempfile.TemporaryDirectory()
        self.addCleanup(made.cleanup)
        temporary = pathlib.Path(made.name) / "Testing" / "Temporary"
        temporary.mkdir(parents=True)
        (temporary / "LastTest.log").write_text(log, encoding="utf-8")
        if failed is not None:
            (temporary / "LastTestsFailed.log").write_text(failed, encoding="utf-8")
        return made.name

    def test_it_answers_by_its_exit_code(self):
        self.assertEqual(report.main(["--only-the-floor", self.folder(FLOOR, "5:tst_player\n")]), 0)
        self.assertEqual(report.main(["--only-the-floor", self.folder(PASSING + FLOOR, "1:tst_world\n5:tst_player\n")]), 1)

    def test_no_list_of_failures_is_no_floor(self):
        self.assertEqual(report.main(["--only-the-floor", self.folder(PASSING)]), 1)

    def test_no_log_is_said(self):
        with tempfile.TemporaryDirectory() as empty:
            self.assertEqual(report.main([empty]), 1)


if __name__ == "__main__":
    unittest.main()
