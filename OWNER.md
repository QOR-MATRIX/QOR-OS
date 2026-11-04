# Where QOR OS stands

For Andrew, 4 November 2026. Autonomous local work continues; native lint now enforces the existing zero-warning rule.

QOR OS joins Prism (desktop and compositor), Lumen (design language), QQ, ARQADE and QOR Media. This run started clean at
`a79a754` on `integration/owner-brief`, preserving all 13 QOR worktrees. This session owns implementation. Heavy jobs use the
verified QOR-local lock sequentially; no cross-project prerequisite exists.

## Completed checkpoints

`be8bba3` fixes Windows-wrapper exit masking. `eed562b` requires registered CTest (test runner) suites, fresh evidence and
sequential execution; 13 PowerShell/real-CTest stand-in cases and 13 Linux workflow checks passed. Actual Windows Qt runtime
remains untested. The initial directory diagnosis was corrected: Qt places Windows executables in the build root.

`8ff17c9` declares Qt Quick 3D as QQ's C++ base-type dependency. Full QML lint (Qt's interface-language checker) fell from
431 to **429 diagnostics**, none added. Six spatial-audio findings reproduce in a Qt-only probe; the installed metadata lacks
the dependency owning its base type. The kit was not changed and no warning was suppressed.

`c1ed4f5` makes ScreenText and ScreenButton show supplied labels literally, preserving button capitalization. Two real-QML
regression rows first failed, then verified plain text, independent instances and later edits using benign strings.

The current change sets Qt's supported `MaxWarnings=0` policy and adds sequential product lint steps to both CI platforms.
The actual QQ CMake lint commands now fail (exit 255) on the same **429 diagnostics**, none added or removed. This closes
false-success reporting; the zero-warning requirement remains unmet. All 18 policy/workflow checks passed on Linux;
Windows passed 17 with one Linux-only check skipped. These use real Qt lint and native-command stand-ins, without windows.

Latest runtime evidence is from `c1ed4f5`'s text change: commercial Qt 6.12 under hidden WSL (Linux inside Windows), software
rendering, a graphics-processor fence, one compiler/rendering worker, two processors and a 1.25 GiB memory cap. Release
(optimized) and AddressSanitizer/UBSan (memory/undefined-behavior checks) each passed **45 scene cases**, none failed/skipped;
leak checking was off. Hardening (exploit mitigations) passed 16 Release programs and the sanitizer scene program.

## Next and limits

Checkpoint the lint gate, then reduce another small behavior-preserving lint batch with focused checks. Exact evidence,
commands and resume point: `build/autonomous-2026-11-04/HANDOFF.md`. No heavy job is active. Recovery, transaction tags, trust
and provenance code remain unchanged. Listing/planning still occupy the window thread.

Full OS/core, actual Windows product suites, native Linux graphics/sound/controllers and image/install testing remain
unrun. No release readiness is claimed. VM (virtual machine) Create/Prove is prepared in current `docs/CI.md`; neither ran.
Your separate go-ahead, Qt's licensing answer and successful proof precede the guest kit/runner. Runner settings and
isolation flags remain unchanged. Nothing was pushed, merged, deployed or published; no licences or accounts changed.
