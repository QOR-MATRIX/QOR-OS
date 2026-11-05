# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Native failures and fresh reports at the first journey's program boundary."""

import contextlib
import importlib.util
import io
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = pathlib.Path(__file__).resolve().parents[2] / "tools/dev/first-journey.py"
SPEC = importlib.util.spec_from_file_location("first_journey", SCRIPT)
journey = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(journey)
RUN = subprocess.run


class FirstJourneyPrograms(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="qor-journey-check-")
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name)
        self.out = self.root / "reports with spaces" / "measurement.json"
        self.out.parent.mkdir()
        self.env = {name: os.environ[name] for name in ("SystemRoot", "WINDIR") if name in os.environ}

    def child(self, body):
        path = self.root / "program.py"
        path.write_text("import json, pathlib, sys\n" + body + "\n", encoding="utf-8")

        def run(command, **kwargs):
            # A real child process receives the production helper's arguments.
            return RUN([sys.executable, str(path), *command[1:]], **kwargs)

        return mock.patch.object(journey.subprocess, "run", side_effect=run)

    def build(self, body):
        with self.child(body):
            return journey.build("studio-fixture", self.root / "project", self.env)

    def play(self, body):
        with self.child(body):
            return journey.play("player-fixture", ["scene.qml"], self.out, self.env)

    @staticmethod
    def report(payload):
        return (
            "out = pathlib.Path(sys.argv[sys.argv.index('--measure') + 1])\n"
            f"out.write_text({payload!r}, encoding='utf-8')"
        )

    def test_build_accepts_one_successful_result_with_other_output(self):
        result = {"ok": True, "build": 1, "compiled": False}
        self.assertEqual(self.build(f"print('startup note')\nprint({'QQ-BUILD ' + json.dumps(result)!r})"), result)

    def test_build_rejects_nonzero_exit_even_with_a_success_result(self):
        for code in (7, 77):
            with self.subTest(exit=code):
                with self.assertRaisesRegex(journey.Failed, f"exit {code}"):
                    self.build(f"print('QQ-BUILD {{\"ok\": true}}')\nsys.exit({code})")

    def test_build_rejects_missing_result(self):
        with self.assertRaises(journey.Failed):
            self.build("print('no build result')")

    def test_build_rejects_ambiguous_results(self):
        with self.assertRaises(journey.Failed):
            self.build("print('QQ-BUILD {\"ok\": true}')\nprint('QQ-BUILD {\"ok\": false}')")

    def test_build_rejects_malformed_json_as_a_reported_failure(self):
        with self.assertRaises(journey.Failed):
            self.build("print('QQ-BUILD {broken')")

    def test_build_rejects_a_non_object_result(self):
        with self.assertRaises(journey.Failed):
            self.build("print('QQ-BUILD []')")

    def test_player_accepts_a_fresh_report(self):
        result = {"problem": "", "logicErrors": [], "unloaded": [], "scene": "file:///fixture.qml"}
        self.assertEqual(self.play(self.report(json.dumps(result))), result)

    def test_player_rejects_zero_exit_without_a_report(self):
        with self.assertRaises(journey.Failed):
            self.play("pass")

    def test_player_rejects_nonzero_exit_even_with_a_fresh_report(self):
        for code in (1, 3):
            with self.subTest(exit=code):
                with self.assertRaisesRegex(journey.Failed, f"exit {code}"):
                    self.play(self.report('{"problem": ""}') + f"\nsys.exit({code})")
                self.out.unlink()

    def test_player_preserves_an_existing_report_without_starting_a_child(self):
        before = '{"problem": "", "scene": "an earlier run"}'
        self.out.write_text(before, encoding="utf-8")
        started = self.root / "started"
        with self.assertRaises(journey.Failed):
            self.play(f"pathlib.Path({str(started)!r}).write_text('started', encoding='utf-8')")
        self.assertEqual(self.out.read_text(encoding="utf-8"), before)
        self.assertFalse(started.exists())

    def test_player_rejects_a_directory_instead_of_a_report(self):
        with self.assertRaises(journey.Failed):
            self.play("pathlib.Path(sys.argv[sys.argv.index('--measure') + 1]).mkdir()")

    def test_player_rejects_malformed_json_as_a_reported_failure(self):
        with self.assertRaises(journey.Failed):
            self.play(self.report("{broken"))

    def test_player_rejects_a_non_object_report(self):
        with self.assertRaises(journey.Failed):
            self.play(self.report("[]"))

    def checked_report(self):
        scene = str(self.root / "scene.qml")
        return scene, {"problem": "", "logicErrors": [], "unloaded": [], "scene": pathlib.Path(scene).as_uri()}

    def test_journey_accepts_a_measured_first_frame(self):
        scene, result = self.checked_report()
        result["firstFrameMs"] = 0
        with contextlib.redirect_stdout(io.StringIO()):
            journey.check_played(result, scene)

    def test_journey_rejects_an_unmeasured_or_invalid_first_frame(self):
        missing = object()
        for frame in (missing, None, -1, True, "12", float("nan"), float("inf")):
            with self.subTest(frame=frame):
                scene, result = self.checked_report()
                if frame is not missing:
                    result["firstFrameMs"] = frame
                with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(journey.Failed):
                    journey.check_played(result, scene)


if __name__ == "__main__":
    unittest.main()
