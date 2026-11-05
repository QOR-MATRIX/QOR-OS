# Where QOR OS stands

For Andrew, 4 November 2026. Local branch: `integration/owner-brief`.

All 14 worktrees (Git checkouts) remain; this session owns implementation. One verified QOR-local build lock keeps
heavy jobs sequential. No cross-project prerequisite exists. Nothing was pushed.

## Verified checks

Tests use commercial Qt 6.12 hidden in WSL (Linux inside Windows), software graphics, a graphics processor fence,
two processors and one worker. ASan/UBSan check memory and undefined behavior; leaks are off. Latest runs have a
1.75 GiB hard memory cap; earlier runs used 1.25 GiB.

- Integrated QQ code at `39b0768`: fresh unfiltered **91 asset-library/195 game-build/3 Store-to-Player Qt cases pass
  per build**, zero failures/skips. Every test function, runner record and 18 source fingerprints are verified; limits
  are recorded. Recovery, change tags, trust, checked previews and listing provenance retain their regression checks.
- Startup checks: **52 Scene/5 real Player startup cases pass per build**, zero failures/skips. Supplied problems display
  literally. The preceding startup fix also passes capture, report-write failure, ordinary play and record/replay checks.
  Earlier tighter-cap sanitizer failures remain recorded; their cause was not established conclusively.
- **The complete first journey now passes once per build**, zero failures/skips: two Studio builds, three real Player
  launches and current Prism (compositor and shell) Library discovery/request, including playback after deleting the
  temporary project.
  Fresh records and all source/program identities are verified; Release takes 113.07 seconds, QQ sanitizers 113.31.
  Prism starts a recording stand-in; the real Player runs separately. Prism itself is not sanitizer-instrumented.
- The journey helper passes **15 Python regressions each on Windows/Linux**, zero skips. Failed children, stale reports
  and missing first-frame markers are refused.
- Hardening (exploit mitigations): **16 Release/13 sanitizer product programs and 25 main-build programs pass**. The selected
  current simulator build links with its pinned core; its wrapper exits 1 for 12 inherited Wayland metadata warnings.

QQ lint (Qt's interface-language checker) still fails: **166 console messages/191 structured warnings**, none newly
introduced in the retained comparisons. Against the first retained 381-warning snapshot, 190 are removed, none added;
this does not reconstruct the older reported 439. The **12 Wayland warnings** match their retained baseline exactly.
Listing/planning still run on the window thread; no latency budget is established.

## Next and limits

Next: refresh simulator/device command-line restrictions and the selected core bridge (C interface), then reproduce runtime
warnings. Exact commands, records, harness limits and resume point: `build/autonomous-2026-11-04/HANDOFF.md`.

Full OS/core suites, actual Windows products, native Linux graphics/sound/controllers and image/install proof remain
unrun; zero-warning gates are unmet. VM (virtual machine) Create/Prove commands are prepared in `docs/CI.md`; neither ran.
Next owner decision: separate Create go-ahead. Licensing prerequisites and actual proof still gate the guest Qt kit and runner.
No runner settings, isolation flags, licences or accounts changed; nothing was merged, deployed or published.
