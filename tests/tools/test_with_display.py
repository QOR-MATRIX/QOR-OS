"""Tests for tools/ci/with_display.py."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("with_display", REPO / "tools" / "ci" / "with_display.py")
with_display = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(with_display)

Transition = with_display.Transition
ASLEEP = Transition("506", "2026-11-01T06:39:26", "Idle Timeout")
AWAKE = Transition("507", "2026-11-01T06:50:00", "Input Mouse")

# What wevtutil printed on the runner, shortened; its line ends and the entities in its message are its own.
PRINTED = (
    "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Microsoft-Windows-Kernel-Power'/>"
    "<EventID>506</EventID><TimeCreated SystemTime='2026-11-01T07:54:23.3888707Z'/></System>"
    "<EventData><Data Name='Reason'>12</Data></EventData><RenderingInfo Culture='en-US'>"
    "<Message>The system is entering Modern Standby &#13;&#10;&#13;&#10;Reason: Idle Timeout.</Message></RenderingInfo></Event>\r\n"
    "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Microsoft-Windows-Kernel-Power'/>"
    "<EventID>507</EventID><TimeCreated SystemTime='2026-11-01T07:53:51.4198073Z'/></System>"
    "<RenderingInfo Culture='en-US'><Message>The system is exiting Modern Standby &#13;&#10;&#13;&#10;Reason: Input Mouse.</Message>"
    "</RenderingInfo></Event>\r\n"
)


class FakeMachine:
    """A laptop, driven by what the test says it does."""

    def __init__(self, log=(), wakes=True, reachable=True, code=0, sleeps_during=None):
        self.log = list(log)  # newest first, as the query asks
        self.wakes = wakes
        self.reachable = reachable
        self.code = code
        self.sleeps_during = sleeps_during
        self.did = []
        self.said = []

    def hold_display(self):
        self.did.append("hold")

    def wake(self):
        self.did.append("wake")
        if self.wakes:
            self.log.insert(0, AWAKE)

    def newest(self, *events):
        return next((t for t in self.log if t.event in events), None)

    def desktop_is_reachable(self):
        return self.reachable

    def now_utc(self):
        return "2026-11-01T07:00:00"

    def wait(self, seconds):
        pass

    def run(self, command):
        self.did.append(command)
        if self.sleeps_during:
            self.log.insert(0, self.sleeps_during)
        return self.code

    def say(self, line):
        self.said.append(line)


class RunWithDisplay(unittest.TestCase):
    def test_awake_it_holds_the_display_and_runs_the_command(self):
        machine = FakeMachine(log=[AWAKE, ASLEEP])
        self.assertEqual(with_display.run_with_display(machine, ["ctest"]), 0)
        self.assertEqual(machine.did, ["hold", ["ctest"]])
        self.assertEqual(machine.said, [])

    def test_the_commands_failure_is_its_own(self):
        machine = FakeMachine(log=[AWAKE], code=8)
        self.assertEqual(with_display.run_with_display(machine, ["ctest"]), 8)

    def test_a_machine_that_has_never_slept_has_no_events(self):
        machine = FakeMachine(log=[])
        self.assertEqual(with_display.run_with_display(machine, ["ctest"]), 0)
        self.assertNotIn("wake", machine.did)

    def test_asleep_it_holds_first_then_wakes_it_and_runs(self):
        machine = FakeMachine(log=[ASLEEP])
        self.assertEqual(with_display.run_with_display(machine, ["ctest"]), 0)
        # Awake with nothing holding it, the laptop slept again within the minute.
        self.assertEqual(machine.did, ["hold", "wake", ["ctest"]])
        self.assertIn("Idle Timeout", machine.said[0])

    def test_a_machine_that_will_not_wake_runs_nothing(self):
        machine = FakeMachine(log=[ASLEEP], wakes=False)
        self.assertEqual(with_display.run_with_display(machine, ["ctest"]), 1)
        self.assertNotIn(["ctest"], machine.did)
        self.assertTrue(machine.said[-1].startswith("::error::"))
        self.assertIn("Nothing was run", machine.said[-1])

    def test_a_locked_desktop_runs_nothing(self):
        machine = FakeMachine(log=[AWAKE], reachable=False)
        self.assertEqual(with_display.run_with_display(machine, ["ctest"]), 1)
        self.assertNotIn(["ctest"], machine.did)
        self.assertIn("locked", machine.said[-1])

    def test_woken_to_a_lock_screen_runs_nothing(self):
        machine = FakeMachine(log=[ASLEEP], reachable=False)
        self.assertEqual(with_display.run_with_display(machine, ["ctest"]), 1)
        self.assertIn("wake", machine.did)
        self.assertNotIn(["ctest"], machine.did)

    def test_sleeping_during_the_command_is_said_and_keeps_its_code(self):
        lid = Transition("506", "2026-11-01T07:30:00", "Lid")
        machine = FakeMachine(log=[AWAKE], code=8, sleeps_during=lid)
        self.assertEqual(with_display.run_with_display(machine, ["ctest"]), 8)
        self.assertTrue(machine.said[-1].startswith("::warning::"))
        self.assertIn("Lid", machine.said[-1])

    def test_sleeping_before_the_command_is_not_said(self):
        machine = FakeMachine(log=[AWAKE, ASLEEP])
        with_display.run_with_display(machine, ["ctest"])
        self.assertEqual(machine.said, [])


class ParseEvents(unittest.TestCase):
    def test_what_wevtutil_prints(self):
        self.assertEqual(with_display.parse_events(PRINTED), [
            Transition("506", "2026-11-01T07:54:23", "Idle Timeout"),
            Transition("507", "2026-11-01T07:53:51", "Input Mouse"),
        ])

    def test_nothing_printed_is_nothing_found(self):
        self.assertEqual(with_display.parse_events(""), [])

    def test_a_message_without_a_reason_still_counts(self):
        printed = "<Event><EventID>506</EventID><TimeCreated SystemTime='2026-11-01T07:54:23.1Z'/></Event>"
        self.assertEqual(with_display.parse_events(printed), [Transition("506", "2026-11-01T07:54:23", "not given")])


class Arguments(unittest.TestCase):
    def test_a_command_is_needed(self):
        self.assertEqual(with_display.main([]), 2)
        self.assertEqual(with_display.main(["--"]), 2)
        self.assertEqual(with_display.main(["ctest"]), 2)


if __name__ == "__main__":
    unittest.main()
