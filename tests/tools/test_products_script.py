# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""tools/dev/products.sh's attempts, with stand-in suites run by the real CTest: its build stood in for, the script itself real."""

import json
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
    import os, signal, sys, time
    name, how, marks = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(os.path.join(marks, "ran"), "a") as ran:
        ran.write(name + " " + os.environ.get("QOR_EXPECTED_RENDERER", "unset") + "\\n")
    if how == "skips":
        sys.exit(77)
    flaked = os.path.join(marks, name + ".flaked")
    again = os.path.exists(flaked)
    if how == "sequential" or (how == "retry-sequential" and again):
        activity = os.path.join(marks, "active")
        try:
            os.mkdir(activity)
        except FileExistsError:
            print("FAIL!  : Stand::aCase() 'one suite at a time' returned FALSE. (overlap)")
            print("Totals: 0 passed, 1 failed, 0 skipped, 0 blacklisted, 1ms", flush=True)
            sys.exit(1)
        try:
            time.sleep(0.6)
        finally:
            os.rmdir(activity)
    fails = how == "fails" or (how in ("flaky", "kills-its-ctest", "retry-sequential") and not again)
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

QML = textwrap.dedent(
    """\
    #!/usr/bin/env python3
    import json, os, pathlib, socket, stat, sys, time
    runtime = pathlib.Path(os.environ["XDG_RUNTIME_DIR"])
    address = runtime / sys.argv[-1]
    channel = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    channel.bind(str(address))
    pathlib.Path(os.environ["STAND_IN"]).parent.joinpath("marks/display.json").write_text(json.dumps({
        "runtime": str(runtime), "mode": stat.S_IMODE(runtime.stat().st_mode), "socket_bytes": len(os.fsencode(address))
    }))
    while True:
        time.sleep(1)
    """
)

# The product's build, stood in for: configuring makes the stand-in suites, and building does nothing.
CMAKE = textwrap.dedent(
    """\
    #!/bin/bash
    if [ "$1" = --build ]; then
        printf '%s\\0' "$@" > "$STAND_IN/../marks/build.args"
        [ "${BUILD_GOES:-}" != fails ] || exit 23
        exit 0
    fi
    printf '%s\\0' "${QOR_QT_DIR:-}" "$PWD" "$@" > "$STAND_IN/../marks/configure.args"
    [ "${CONFIGURE_GOES:-}" != fails ] || exit 17
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
        *) [ "${FIRST_GOES:-}" != answers-zero ] || exit 0 ;;
    esac
    exec "$REAL_CTEST" "$@"
    """
)


@unittest.skipUnless(
    os.name == "posix" and all(shutil.which(tool) for tool in ("bash", "cmake", "ctest", "make", "python3")),
    "needs bash, CMake and make")
class Attempts(unittest.TestCase):
    def test_inherited_parallelism_cannot_overlap_the_first_attempt(self):
        code, said, ran = self.run_script([("tst_one", "sequential"), ("tst_two", "sequential")],
                                          "--attempts", "1", CTEST_PARALLEL_LEVEL="2")
        self.assertEqual(code, 0, said)
        self.assertEqual(len(ran), 2)

    def test_inherited_parallelism_cannot_overlap_the_retry(self):
        code, said, ran = self.run_script([("tst_one", "retry-sequential"), ("tst_two", "retry-sequential")],
                                          CTEST_PARALLEL_LEVEL="2")
        self.assertEqual(code, 0, said)
        self.assertEqual(len(ran), 4)
        self.assertIn("Failed once, and passed only on a later attempt", said)

    def run_script(self, suites, *arguments, old_record=False, blocked_record=False, script=None, **environment):
        made = tempfile.TemporaryDirectory()
        self.addCleanup(made.cleanup)
        root = pathlib.Path(made.name)
        for folder in ("kit/lib/cmake/Qt6", "kit/qml/QtWayland/Compositor/XdgShell", "shims", "stand-in", "marks"):
            (root / folder).mkdir(parents=True)
        (root / "kit/lib/cmake/Qt6/Qt6Config.cmake").touch()
        (root / "kit/qml/QtWayland/Compositor/XdgShell/qmldir").touch()
        self.old_record = root / "build/qq/Testing/prior/Test.xml"
        if old_record:
            self.old_record.parent.mkdir(parents=True)
            self.old_record.write_text(
                '<Site><Testing><Test Status="passed"><Name>tst_fine</Name><Results/></Test></Testing></Site>', encoding="utf-8")
            testing = self.old_record.parent.parent
            (testing / "TAG").write_text("prior\nExperimental\n", encoding="utf-8")
            (testing / "Temporary").mkdir()
            (testing / "Temporary/LastTest_prior.log").write_text(
                "1/1 Testing: tst_fine\nTotals: 1 passed, 0 failed, 0 skipped, 0 blacklisted, 1ms\n", encoding="utf-8")
            if blocked_record:
                protected = testing if blocked_record == "unreadable" else self.old_record.parent
                self.addCleanup(protected.chmod, 0o755)
                protected.chmod(0o111 if blocked_record == "unreadable" else 0o555)
        (root / "stand-in/suite.py").write_text(SUITE, encoding="utf-8")
        tests = "".join(
            f'add_test(NAME {name} COMMAND python3 -I "{root}/stand-in/suite.py" {name} {how} "{root}/marks")\n'
            f'set_tests_properties({name} PROPERTIES SKIP_RETURN_CODE 77)\n' for name, how in suites)
        (root / "stand-in/CMakeLists.txt").write_text(
            f"cmake_minimum_required(VERSION 3.16)\nproject(standin NONE)\nenable_testing()\n{tests}", encoding="utf-8")
        # No test starts the developer's real D-Bus/keyring. Other Python uses
        # still run normally, including the fake compositor's real Unix socket.
        python = ('#!/bin/bash\nif [ "$1" = -c ]; then '
                  '[ "${STAND_IN_KEYRING_MODULES:-}" = available ]; exit $?; fi\nexec "$REAL_PYTHON" "$@"\n')
        bus = '#!/bin/bash\ntouch "$STAND_IN/../marks/bus-started"\nexit 1\n'
        for name, text in (("cmake", CMAKE), ("ctest", CTEST), ("qml", QML), ("python3", python), ("dbus-daemon", bus)):
            (root / "shims" / name).write_text(text, encoding="utf-8")
            (root / "shims" / name).chmod(0o755)
        environ = dict(os.environ, PATH=f"{root}/shims:{os.environ['PATH']}", REAL_CMAKE=shutil.which("cmake"), REAL_CTEST=shutil.which("ctest"),
                       STAND_IN=f"{root}/stand-in", REAL_PYTHON=sys.executable, **environment)
        done = subprocess.run(["bash", str(script or _SCRIPT), "--qt", str(root / "kit"), "--build-root", str(root / "build"), "--display", "session",
                               "--product", "qq", *arguments], env=environ, capture_output=True, text=True, timeout=120)
        ran = (root / "marks/ran").read_text(encoding="utf-8").splitlines() if (root / "marks/ran").exists() else []
        self.last_line = done.stdout.strip().splitlines()[-1] if done.stdout.strip() else ""
        self.configure_args = (root / "marks/configure.args").read_bytes().decode().split("\0") if (root / "marks/configure.args").exists() else []
        self.build_args = (root / "marks/build.args").read_bytes().decode().split("\0") if (root / "marks/build.args").exists() else []
        receipt = root / "marks/display.json"
        self.display = json.loads(receipt.read_text(encoding="utf-8")) if receipt.exists() else None
        self.bus_started = (root / "marks/bus-started").exists()
        return done.returncode, done.stdout + done.stderr, ran

    def test_missing_keyring_fixture_refuses_before_starting_test_services(self):
        with tempfile.TemporaryDirectory() as source:
            script = pathlib.Path(source) / "tools/dev/products.sh"
            script.parent.mkdir(parents=True)
            shutil.copyfile(_SCRIPT, script)
            code, said, ran = self.run_script([("tst_fine", "fine")], "--display", "offscreen", "--attempts", "1",
                                              script=script, STAND_IN_KEYRING_MODULES="available")
            self.assertNotEqual(code, 0, said)
            self.assertIn("product test fixture tools/dev/secret-service.py is missing", said)
            self.assertFalse(self.bus_started, "a bus started before checking the fixture")
            self.assertIsNone(self.display)
            self.assertEqual(self.configure_args, [])
            self.assertEqual(ran, [])

    def test_hidden_display_uses_a_private_short_runtime_and_cleans_it_up(self):
        with tempfile.TemporaryDirectory() as outside:
            inherited = pathlib.Path(outside) / ("desktop-runtime-" + "x" * 100)
            inherited.mkdir()
            sentinel = inherited / "keep"
            sentinel.write_text("the desktop's runtime is not ours", encoding="utf-8")
            code, said, ran = self.run_script([("tst_fine", "fine")], "--display", "offscreen", "--attempts", "1",
                                              XDG_RUNTIME_DIR=str(inherited), TMPDIR=str(inherited))
            self.assertEqual(code, 0, said)
            self.assertEqual(len(ran), 1)
            self.assertIsNotNone(self.display)
            runtime = pathlib.Path(self.display["runtime"])
            self.assertNotEqual(runtime, inherited)
            self.assertEqual(self.display["mode"], 0o700)
            self.assertLess(self.display["socket_bytes"], 108)
            self.assertFalse(runtime.parent.exists(), "private display state survived cleanup")
            self.assertTrue(sentinel.is_file(), "the inherited desktop runtime was changed")

    def test_native_preset_uses_the_same_cache_for_configuration_build_and_ctest(self):
        for preset in ("q-pi-release", "q-pi-asan"):
            with self.subTest(preset=preset):
                code, said, ran = self.run_script([("tst_fine", "fine")], "--preset", preset, "--attempts", "1")
                self.assertEqual(code, 0, said)
                self.assertEqual(len(ran), 1)
                kit, source, *arguments = self.configure_args
                self.assertEqual(pathlib.Path(kit).name, "kit")
                self.assertEqual(pathlib.Path(source), _REPO / "products/qq")
                self.assertEqual(arguments[:2], ["--preset", preset])
                build = arguments[arguments.index("-B") + 1]
                self.assertEqual(self.build_args[:4], ["--build", build, "--parallel", "1"])
                self.assertNotIn("-DCMAKE_BUILD_TYPE=Release", arguments)

    def test_native_preset_rejects_other_products_and_unknown_presets_before_building(self):
        for arguments in (("--preset", "q-pi-release", "--product", "qq,media"),
                          ("--preset", "unknown"), ("--preset", ""), ("--preset",)):
            with self.subTest(arguments=arguments):
                code, said, ran = self.run_script([], *arguments)
                self.assertEqual(code, 2, said)
                self.assertEqual(ran, [])
                self.assertEqual(self.configure_args, [])

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

    def test_a_first_attempt_that_answers_zero_without_running_has_not_passed(self):
        code, said, ran = self.run_script([("tst_fine", "fine")], "--attempts", "1", FIRST_GOES="answers-zero")
        self.assertEqual(code, 1, said)
        self.assertEqual(ran, [])
        self.assertIn("Failed: qq", said)
        self.assertNotIn("Products built and tested", said)

    def test_a_failed_configure_or_build_leaves_no_old_passing_record(self):
        for phase in ("CONFIGURE_GOES", "BUILD_GOES"):
            with self.subTest(phase=phase):
                code, said, ran = self.run_script([("tst_fine", "fine")], old_record=True, **{phase: "fails"})
                self.assertEqual(code, 1, said)
                self.assertEqual(ran, [])
                self.assertFalse(self.old_record.exists(), "A failed build left a previous test record looking current")
                self.assertNotIn("Products built and tested", said)

    def test_a_record_that_cannot_be_invalidated_stops_the_run(self):
        for blocked in ("unreadable", "unwritable"):
            with self.subTest(blocked=blocked):
                code, said, ran = self.run_script([("tst_fine", "fine")], "--attempts", "1", old_record=True,
                                                  blocked_record=blocked, FIRST_GOES="answers-zero")
                self.assertEqual(code, 1, said)
                self.assertEqual(ran, [])
                self.assertNotIn("Products built and tested", said)

    def test_a_whole_suite_skip_does_not_pass_the_product(self):
        for suites in ([("tst_skipped", "skips")], [("tst_fine", "fine"), ("tst_skipped", "skips")]):
            with self.subTest(suites=suites):
                code, said, ran = self.run_script(suites, "--attempts", "1")
                self.assertEqual(code, 1, said)
                self.assertEqual(len(ran), len(suites))
                self.assertIn("tst_skipped: NOT RUN", said)
                self.assertNotIn("Products built and tested", said)

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
