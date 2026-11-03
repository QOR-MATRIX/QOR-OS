# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""tools/dev/products.sh's attempts, with stand-in suites run by the real CTest: its build stood in for, the script itself real."""

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

_REPO = pathlib.Path(__file__).resolve().parents[2]
_SCRIPT = _REPO / "tools" / "dev" / "products.sh"

SUITE = textwrap.dedent(
    """\
    import os, signal, sys
    name, how, marks = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(os.path.join(marks, "ran"), "a") as ran:
        ran.write(name + " " + os.environ.get("QOR_EXPECTED_RENDERER", "unset") + "\\n")
    flaked = os.path.join(marks, name + ".flaked")
    again = os.path.exists(flaked)
    fails = how == "fails" or (how in ("flaky", "kills-its-ctest") and not again)
    open(flaked, "w").close()
    if how == "kills-its-ctest" and again:
        os.kill(os.getppid(), signal.SIGKILL)
        sys.exit(0)
    if fails:
        print("FAIL!  : Stand::aCase() 'it' returned FALSE. ()")
    print("Totals: 1 passed, %d failed, 0 skipped, 0 blacklisted, 1ms" % fails, flush=True)
    sys.exit(1 if fails else 0)
    """
)

# The product's build, stood in for: configuring makes the stand-in suites, and building does nothing.
CMAKE = textwrap.dedent(
    """\
    #!/bin/bash
    [ "$1" != --build ] || exit 0
    while [ $# -gt 0 ]; do
        case $1 in -B) build=$2; shift 2 ;; *) shift ;; esac
    done
    exec "$REAL_CMAKE" -S "$STAND_IN" -B "$build" > /dev/null
    """
)

# CTest, as it is, or as it could go wrong at a retry.
CTEST = textwrap.dedent(
    """\
    #!/bin/bash
    case " $* " in
        *" --rerun-failed "*)
            case ${RETRY_GOES:-} in
                answers-zero) exit 0 ;;
                forgets) for list in "$2"/Testing/Temporary/LastTestsFailed*; do : > "$list"; done ;;
            esac ;;
    esac
    exec "$REAL_CTEST" "$@"
    """
)


@unittest.skipUnless(
    os.name == "posix" and all(shutil.which(tool) for tool in ("bash", "cmake", "ctest", "make", "python3")),
    "needs bash, CMake and make")
class Attempts(unittest.TestCase):
    def run_script(self, suites, *arguments, **environment):
        made = tempfile.TemporaryDirectory()
        self.addCleanup(made.cleanup)
        root = pathlib.Path(made.name)
        for folder in ("kit/lib/cmake/Qt6", "shims", "stand-in", "marks"):
            (root / folder).mkdir(parents=True)
        (root / "kit/lib/cmake/Qt6/Qt6Config.cmake").touch()
        (root / "stand-in/suite.py").write_text(SUITE, encoding="utf-8")
        tests = "".join(
            f'add_test(NAME {name} COMMAND python3 -I "{root}/stand-in/suite.py" {name} {how} "{root}/marks")\n' for name, how in suites)
        (root / "stand-in/CMakeLists.txt").write_text(
            f"cmake_minimum_required(VERSION 3.16)\nproject(standin NONE)\nenable_testing()\n{tests}", encoding="utf-8")
        for name, text in (("cmake", CMAKE), ("ctest", CTEST)):
            (root / "shims" / name).write_text(text, encoding="utf-8")
            (root / "shims" / name).chmod(0o755)
        environ = dict(os.environ, PATH=f"{root}/shims:{os.environ['PATH']}", REAL_CMAKE=shutil.which("cmake"), REAL_CTEST=shutil.which("ctest"),
                       STAND_IN=f"{root}/stand-in", **environment)
        done = subprocess.run(["bash", str(_SCRIPT), "--qt", str(root / "kit"), "--build-root", str(root / "build"), "--display", "session",
                               "--product", "qq", *arguments], env=environ, capture_output=True, text=True, timeout=120)
        ran = (root / "marks/ran").read_text(encoding="utf-8").splitlines() if (root / "marks/ran").exists() else []
        self.last_line = done.stdout.strip().splitlines()[-1] if done.stdout.strip() else ""
        return done.returncode, done.stdout + done.stderr, ran

    def test_a_retry_reruns_the_failed_suite_alone_and_names_it_at_the_end(self):
        code, said, ran = self.run_script([("tst_fine", "fine"), ("tst_flaky", "flaky")])
        self.assertEqual(code, 0, said)
        self.assertEqual([line.split()[0] for line in ran], ["tst_fine", "tst_flaky", "tst_flaky"])
        self.assertIn("Failed once, and passed only on a later attempt: qq: tst_flaky", said)
        self.assertIn("what its suites said", said)

    def test_a_first_time_pass_names_no_retry(self):
        code, said, ran = self.run_script([("tst_fine", "fine")])
        self.assertEqual(code, 0, said)
        self.assertEqual(len(ran), 1)
        self.assertNotIn("later attempt", said)

    def test_failing_twice_fails(self):
        code, said, ran = self.run_script([("tst_fine", "fine"), ("tst_bad", "fails")])
        self.assertEqual(code, 1, said)
        self.assertEqual([line.split()[0] for line in ran], ["tst_fine", "tst_bad", "tst_bad"])
        self.assertIn("Failed: qq", said)
        self.assertNotIn("later attempt", said)

    def test_a_retry_that_answers_zero_having_run_nothing_has_not_passed(self):
        code, said, ran = self.run_script([("tst_flaky", "flaky")], RETRY_GOES="answers-zero")
        self.assertEqual(code, 1, said)
        self.assertEqual(len(ran), 1)
        self.assertNotIn("later attempt", said)
        self.assertIn("Failed: qq", said)

    def test_a_retry_with_its_list_of_failures_emptied_still_runs_what_failed(self):
        # CTest then runs them all, so what is reported as passing again did run.
        code, said, ran = self.run_script([("tst_fine", "fine"), ("tst_flaky", "flaky")], RETRY_GOES="forgets")
        self.assertEqual(code, 0, said)
        self.assertEqual([line.split()[0] for line in ran], ["tst_fine", "tst_flaky", "tst_fine", "tst_flaky"])
        self.assertIn("passed only on a later attempt: qq: tst_flaky", said)

    def test_a_ctest_stopped_part_way_has_no_old_record_to_lean_on(self):
        code, said, ran = self.run_script([("tst_fine", "fine"), ("tst_stops_it", "kills-its-ctest")])
        self.assertEqual(code, 1, said)
        self.assertEqual([line.split()[0] for line in ran], ["tst_fine", "tst_stops_it", "tst_stops_it"])
        self.assertIn("CTest left no record of how these ended", said)
        self.assertNotIn("later attempt", said)
        self.assertIn("Failed: qq", said)

    def test_a_filter_that_matches_nothing_is_no_pass(self):
        code, said, ran = self.run_script([("tst_fine", "fine")], "--tests", "no-such-suite", "--attempts", "1")
        self.assertEqual(code, 1, said)
        self.assertEqual(ran, [])
        self.assertNotIn("built and tested", said)

    def test_what_is_expected_to_draw_is_the_scripts_to_say(self):
        _, said, ran = self.run_script([("tst_fine", "fine")], QOR_EXPECTED_RENDERER="whatever-the-environment-says")
        self.assertEqual(len(ran), 1, said)
        self.assertIn(ran[0].split()[1], ("software", "gpu"))

    def test_the_last_line_says_how_it_was_drawn(self):
        code, said, ran = self.run_script([("tst_fine", "fine")])
        self.assertEqual(code, 0, said)
        expected = ran[0].split()[1]
        self.assertEqual(self.last_line, "Products built and tested, drawn in software: qq" if expected == "software" else "Products built and tested: qq")


if __name__ == "__main__":
    sys.exit(unittest.main())
