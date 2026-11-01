#!/usr/bin/env python3
"""Runs a command that draws on the runner's desktop, with the display on for as long as it takes.

The Windows runner is a laptop whose only sleep is Modern Standby. When its display times out the machine goes on working
but its windows barely draw: animations hold still, a game's clock stops and the clipboard cannot be opened. A suite run
like that fails everywhere for no reason of its own, and takes an hour to say so (docs/CI.md).

So this holds the display on while the command runs, wakes the machine first if it is already asleep, and refuses to start
if it cannot draw: asleep and staying so, or locked. Afterwards it says whether the machine slept anyway (a lid, a power
button), because that is what a failure in such a run would mean. The command's exit code is its own.

Usage: python tools/ci/with_display.py -- ctest --preset ci-windows -L gpu
"""

from __future__ import annotations

import html
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from typing import NamedTuple, Protocol

ENTERING, EXITING = "506", "507"
WAKE_SECONDS = 20
SETTLE_SECONDS = 3


class Transition(NamedTuple):
    """A move in or out of Modern Standby, from Kernel-Power's events in the System log."""

    event: str
    utc: str  # to the second, so that two of them compare as text
    reason: str


def parse_events(xml: str) -> list[Transition]:
    """The transitions in what wevtutil prints as RenderedXml, in the order printed."""
    found = []
    for text in re.findall(r"<Event\b.*?</Event>", xml, re.DOTALL):
        event = re.search(r"<EventID[^>]*>(\d+)</EventID>", text)
        when = re.search(r"SystemTime='([^'.]+)", text)
        message = re.search(r"<Message>(.*?)</Message>", text, re.DOTALL)
        if not event or not when:
            continue
        reason = re.search(r"Reason:\s*(.*?)\.?\s*$", html.unescape(message.group(1)), re.DOTALL) if message else None
        found.append(Transition(event.group(1), when.group(1), reason.group(1).strip() if reason else "not given"))
    return found


class Machine(Protocol):
    def hold_display(self) -> None: ...
    def wake(self) -> None: ...
    def newest(self, *events: str) -> Transition | None: ...
    def desktop_is_reachable(self) -> bool: ...
    def now_utc(self) -> str: ...
    def wait(self, seconds: float) -> None: ...
    def run(self, command: list[str]) -> int: ...
    def say(self, line: str) -> None: ...


def asleep(machine: Machine) -> bool:
    latest = machine.newest(ENTERING, EXITING)
    return latest is not None and latest.event == ENTERING


def run_with_display(machine: Machine, command: list[str]) -> int:
    # Before the wake: awake with nothing holding it, a laptop goes back to sleep within the minute.
    machine.hold_display()

    if asleep(machine):
        since = machine.newest(ENTERING)
        machine.say(f"The machine has been in Modern Standby since {since.utc} UTC ({since.reason}); waking it.")
        machine.wake()
        for _ in range(WAKE_SECONDS):
            machine.wait(1)
            if not asleep(machine):
                break
        else:
            machine.say("::error::The machine stayed in Modern Standby, so nothing here can draw. Nothing was run.")
            return 1
        machine.wait(SETTLE_SECONDS)

    if not machine.desktop_is_reachable():
        machine.say("::error::The desktop is locked, or held by a system prompt, so no window here can draw or take the keyboard. "
                    "Nothing was run.")
        return 1

    started = machine.now_utc()
    code = machine.run(command)

    slept = machine.newest(ENTERING)
    if slept is not None and slept.utc >= started:
        machine.say(f"::warning::The machine went into Modern Standby at {slept.utc} UTC ({slept.reason}) while this ran. "
                    "Nothing draws then, so a failure here may be the machine's and not the code's.")
    return code


class Windows:
    """The real thing. Nothing here is imported or called off Windows."""

    QUERY = "*[System[Provider[@Name='Microsoft-Windows-Kernel-Power'] and ({})]]"

    def __init__(self) -> None:
        import ctypes
        from ctypes import wintypes

        self.ctypes = ctypes
        self.wintypes = wintypes
        self.kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self.user32 = ctypes.WinDLL("user32", use_last_error=True)

    def hold_display(self) -> None:
        # Held by this thread until this process ends, whatever becomes of it, and changes none of the owner's settings.
        ES_CONTINUOUS, ES_SYSTEM_REQUIRED, ES_DISPLAY_REQUIRED = 0x80000000, 0x1, 0x2
        self.kernel32.SetThreadExecutionState.argtypes = [self.wintypes.DWORD]
        self.kernel32.SetThreadExecutionState.restype = self.wintypes.DWORD
        if not self.kernel32.SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED):
            raise OSError("SetThreadExecutionState refused")

    def wake(self) -> None:
        # What wakes it is input: a request for the display does not. A move of no distance types nothing and moves nothing.
        ctypes, wintypes = self.ctypes, self.wintypes

        class MouseInput(ctypes.Structure):
            _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG), ("mouseData", wintypes.DWORD),
                        ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD), ("dwExtraInfo", ctypes.c_size_t)]

        class Payload(ctypes.Union):
            _fields_ = [("mouse", MouseInput), ("size", ctypes.c_byte * 32)]

        class Input(ctypes.Structure):
            _fields_ = [("type", wintypes.DWORD), ("payload", Payload)]

        MOUSEEVENTF_MOVE = 0x0001
        move = Input(type=0)
        move.payload.mouse = MouseInput(0, 0, 0, MOUSEEVENTF_MOVE, 0, 0)
        self.user32.SendInput.argtypes = [wintypes.UINT, ctypes.POINTER(Input), ctypes.c_int]
        self.user32.SendInput.restype = wintypes.UINT
        self.user32.SendInput(1, ctypes.byref(move), ctypes.sizeof(Input))

    def newest(self, *events: str) -> Transition | None:
        which = " or ".join(f"EventID={event}" for event in events)
        printed = subprocess.run(["wevtutil", "qe", "System", "/q:" + self.QUERY.format(which), "/c:1", "/rd:true", "/f:RenderedXml"],
                                 capture_output=True, text=True, encoding="utf-8", errors="replace", check=True)
        found = parse_events(printed.stdout)
        return found[0] if found else None

    def desktop_is_reachable(self) -> bool:
        # Locked, the input desktop is Winlogon's, which this session may not open.
        DESKTOP_SWITCHDESKTOP = 0x0100
        self.user32.OpenInputDesktop.argtypes = [self.wintypes.DWORD, self.wintypes.BOOL, self.wintypes.DWORD]
        self.user32.OpenInputDesktop.restype = self.wintypes.HANDLE
        self.user32.CloseDesktop.argtypes = [self.wintypes.HANDLE]
        desktop = self.user32.OpenInputDesktop(0, False, DESKTOP_SWITCHDESKTOP)
        if not desktop:
            return False
        self.user32.CloseDesktop(desktop)
        return True

    def now_utc(self) -> str:
        return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")

    def wait(self, seconds: float) -> None:
        time.sleep(seconds)

    def run(self, command: list[str]) -> int:
        try:
            return subprocess.run(command, check=False).returncode
        except OSError as error:
            print(f"::error::{command[0]} could not be run: {error}", flush=True)
            return 127

    def say(self, line: str) -> None:
        print(line, flush=True)


def main(argv: list[str]) -> int:
    if not argv or argv[0] != "--" or len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    if sys.platform != "win32":
        print("with_display.py is for the Windows runner.", file=sys.stderr)
        return 2
    return run_with_display(Windows(), argv[1:])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
