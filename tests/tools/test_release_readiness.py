# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""tools/ci/release_readiness.py: a commit is ready only when every required step ran and passed."""

import contextlib
import importlib.util
import io
import pathlib
import sys
import unittest

REPOSITORY = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("release_readiness", REPOSITORY / "tools" / "ci" / "release_readiness.py")
assert SPEC and SPEC.loader
release_readiness = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = release_readiness  # dataclasses looks its module up there.
SPEC.loader.exec_module(release_readiness)

ALL_PASSED = {
    "WINDOWS_RUNNER": "self-hosted kit",
    "LINUX_RUNNER": "self-hosted kit",
    "WINDOWS_JOB": "success",
    "WINDOWS_GPU": "success",
    "WINDOWS_PRODUCTS": "success",
    "WINDOWS_PRODUCTS_CHANGED": "true",
    "LINUX_JOB": "success",
    "LINUX_GPU": "success",
    "LINUX_PRISM": "success",
    "LINUX_PRODUCTS": "success",
    "LINUX_PRODUCTS_CHANGED": "true",
    "IS_RELEASE": "false",
}
OUTCOMES = [item.outcome for item in release_readiness.REQUIRED]


def run_main(env):
    with contextlib.redirect_stdout(io.StringIO()):
        return release_readiness.main(env)


def ready(env):
    return release_readiness.report(env)[1]


def text(env):
    return "\n".join(release_readiness.report(env)[0])


class ReleaseReadinessTest(unittest.TestCase):
    def test_every_required_step_passed_is_ready(self):
        self.assertTrue(ready(ALL_PASSED))
        self.assertEqual(run_main(ALL_PASSED), 0)

    def test_any_required_step_that_did_not_pass_is_not_ready(self):
        for outcome in OUTCOMES:
            for result in ("skipped", "failure", "cancelled", "", "neutral", "SUCCESS", "true"):
                with self.subTest(outcome=outcome, result=result):
                    self.assertFalse(ready({**ALL_PASSED, outcome: result}))

    def test_a_required_step_with_no_word_at_all_is_not_ready(self):
        for outcome in OUTCOMES:
            with self.subTest(outcome=outcome):
                env = dict(ALL_PASSED)
                del env[outcome]
                self.assertFalse(ready(env))

    def test_nothing_said_is_not_ready(self):
        self.assertFalse(ready({}))
        self.assertEqual(run_main({}), 1)

    def test_runners_paused_and_jobs_skipped_is_not_ready(self):
        # Where things stand while the runners are not isolated: Build's only job that ran is the check for Qt.
        paused = {
            "WINDOWS_RUNNER": "self-hosted kit, paused: not isolated",
            "LINUX_RUNNER": "self-hosted kit, paused: not isolated",
            "WINDOWS_JOB": "skipped",
            "LINUX_JOB": "skipped",
        }
        self.assertFalse(ready(paused))
        self.assertIn("paused: not isolated", text(paused))
        self.assertIn("| Linux: build, mitigations, qmllint, tests, sanitizers | not run |", text(paused))

    def test_a_runner_being_allowed_does_not_make_a_build_pass(self):
        allowed = {"WINDOWS_RUNNER": "self-hosted kit", "LINUX_RUNNER": "self-hosted kit"}
        self.assertFalse(ready(allowed))
        self.assertFalse(ready({**allowed, "WINDOWS_JOB": "failure", "LINUX_JOB": "skipped"}))

    def test_a_failure_and_a_step_not_run_are_said_apart(self):
        env = {**ALL_PASSED, "WINDOWS_JOB": "failure", "LINUX_JOB": "skipped"}
        self.assertIn("| Windows: build, mitigations, qmllint, tests | FAILED |", text(env))
        self.assertIn("| Linux: build, mitigations, qmllint, tests, sanitizers | not run |", text(env))

    def test_products_unchanged_stand_down_only_outside_a_release(self):
        for side in ("WINDOWS", "LINUX"):
            unchanged = {**ALL_PASSED, f"{side}_PRODUCTS": "skipped", f"{side}_PRODUCTS_CHANGED": "false"}
            with self.subTest(side=side):
                self.assertTrue(ready(unchanged))
                self.assertIn("not run: unchanged by this commit", text(unchanged))
                self.assertFalse(ready({**unchanged, "IS_RELEASE": "true"}))

    def test_products_that_changed_and_did_not_run_are_not_ready(self):
        for side in ("WINDOWS", "LINUX"):
            for changed in ("true", "", "FALSE"):
                with self.subTest(side=side, changed=changed):
                    self.assertFalse(ready({**ALL_PASSED, f"{side}_PRODUCTS": "skipped", f"{side}_PRODUCTS_CHANGED": changed}))

    def test_only_the_products_may_stand_down(self):
        # A step with no word on change cannot be excused by another's.
        for outcome in ("WINDOWS_JOB", "WINDOWS_GPU", "LINUX_JOB", "LINUX_GPU", "LINUX_PRISM"):
            with self.subTest(outcome=outcome):
                self.assertFalse(ready({**ALL_PASSED, outcome: "skipped", "WINDOWS_PRODUCTS_CHANGED": "false", "LINUX_PRODUCTS_CHANGED": "false"}))

    def test_the_summary_is_written_and_failure_is_reported(self):
        import tempfile

        with tempfile.TemporaryDirectory() as folder:
            summary = pathlib.Path(folder) / "summary.md"
            self.assertEqual(run_main({"GITHUB_STEP_SUMMARY": str(summary)}), 1)
            self.assertIn("Not ready for a release", summary.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
