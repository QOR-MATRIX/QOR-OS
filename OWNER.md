# Where QOR OS stands

For Andrew, 4 November 2026. Local branch: `integration/owner-brief`.

All 14 worktrees (Git checkouts) remain; this session owns implementation. One verified QOR-local build lock keeps
heavy jobs sequential. No cross-project prerequisite exists. Nothing was pushed.

## Verified checks

Tests use commercial Qt 6.12 hidden in WSL (Linux inside Windows), software graphics, a graphics processor fence,
two processors and one worker. ASan/UBSan check memory and undefined behavior; leaks are off. Latest runs use a
1.75 GiB hard memory cap; earlier runs used 1.25 GiB.

- Player startup problems now show filenames and supplied markup literally, retaining later changes and existing
  styling. **52 Scene/5 real Player startup Qt cases pass per build**, zero failures/skips. Fresh records/source hashes
  are verified. Production C++, Game and SceneIO are unchanged from `ad4aa3f`.
- Startup fix at `ad4aa3f`: **51 Scene/11 selected Player cases pass per build**, zero failures/skips. Malformed roots
  report a problem, are discarded and allow retry. Status 3 reports startup failure; 1 reports failed output. Ordinary
  play stays open after its report; capture and record/replay pass. Earlier tighter-cap sanitizer failures remain recorded;
  paging was observed, without a conclusive cause claim.
- Fresh hardening (exploit mitigations): **16 Release/13 sanitizer programs pass**. Sandbox, confinement and watchdog
  sources are unchanged. No atomic report-write claim is made.
- Current refresh at `39b0768`: **91 asset-library/195 game-build/3 Store-to-Player Qt cases pass per build**, zero
  failures/skips. All ordinary functions, fresh native records and 18 source hashes are verified.
- Historical, separate checks: **28 complete Studio** at `b2b084c`, **56 complete play** at `89877cf` and **5 program
  journeys** at `6f14fe2`, all zero failures/skips per build. They precede later changes.

QQ lint (Qt's interface-language checker) still fails: **166 console messages/191 structured warnings**, none added
by either latest fix. Against the first retained 381-warning snapshot, 190 are removed and none added; this does not
reconstruct the older reported 439. Six spatial-audio findings remain a Qt metadata constraint. **12 Wayland type-metadata
warnings** remain unresolved. Listing/planning still run on the window thread; no latency budget is established.

## Next and limits

First-journey helper fixes pass 15 Python tests each on Windows/Linux, zero skips: failed children, old reports and
missing first-frame markers are refused. Next: current simulator prerequisite validation and the actual Qt/Prism journey.
Exact records, harness limits and resume point: `build/autonomous-2026-11-04/HANDOFF.md`.

Full OS/core, actual Windows products, native Linux graphics/sound/controllers and image/install proof remain unrun;
the zero-warning gate is unmet. VM (virtual machine) Create/Prove commands are prepared in `docs/CI.md`; neither ran.
Next owner decision: separate Create go-ahead. Licensing prerequisites and actual proof still gate the guest Qt kit and runner.
No runner settings, isolation flags, licences or accounts changed; nothing was merged, deployed or published.
