# Where QOR OS stands

For Andrew, 5 November 2026. Local branch: `integration/owner-brief`.

14 worktrees (Git checkouts) remain. This session owns implementation; the QOR-local lock serializes heavy jobs. No cross-project prerequisite exists. Nothing was pushed.

## Verified checks

Commercial Qt 6.12 runs hidden in WSL (Linux inside Windows), with software graphics and a graphics processor fence,
two processors, one worker. ASan/UBSan catch memory errors and undefined behavior; leak checking is off.
Hard cap: 1.75 GiB.

- QQ at `39b0768`: unfiltered **91 asset-library/195 game-build/3 Store-to-Player Qt cases pass per build**, zero failures/skips.
  Recovery, tags, trust, checked previews and provenance retain regression checks.
- Store **format 2/legacy/Library/pages/front-end: 621/11/23/18/39 cases per main profile**, zero failures/skips;
  all **153/9/9/16/28 functions** ran. Fresh records, fingerprints and sanitizer flags are verified.
  Hardening (exploit mitigations) passes **33 normal/31 sanitizer main-build programs** and **16/13 product programs**.
- Shape preserves public interfaces and 2D axis locks: **52 Scene/10 physics cases per product build** pass, zero failures/skips.
  Software waits use game clocks; GPU waits, tolerances and frame-rate checks remain.
- Failed scripts omit the complete request object while keeping their step number, reason and **exit 4**.
  An actual-process regression reproduced argument disclosure; **3 Qt cases plus client/text-privacy controls pass per
  main profile**, zero failures/skips. Earlier failures remain recorded.
- The complete journey at `f99e007` passes per product build, including real Player playback after project removal.
  Prism (compositor and shell) records a stand-in. Prism has no sanitizers in this journey.
- **17 service/10 simulator/11 device-option cases pass per main profile**, zero failures/skips; the core C interface
  passes **9 normal-profile cases**. The sanitizer profile keeps the core off.

- Benchmark output failures return **exit 1**. All **11 collector cases/8 functions per main profile** pass,
  zero failures/skips, alongside actual compositor failed/successful report checks.

Inspector: **5 Qt cases/3 functions per product build pass**, zero failures/skips.

## Remaining and next

QQ lint (Qt's interface-language checker) fails: **149 console/174 structured warnings**, four Inspector findings removed,
none added. **12 Wayland metadata warnings** match their baseline. A pinned Rust dependency fails its warning gate;
repair belongs upstream. Listing/planning remain on the window thread without a latency budget.

Readiness: **16 Python tests per platform pass**; workflow **Windows 14 pass/1 skip; Linux 15 pass**.
Windows collector: **9 cases/7 functions pass**, zero failures/skips; hardening reads 21 program headers.
Next: check Windows Store validation. Commands: `build/autonomous-2026-11-04/HANDOFF.md`.

Full OS/Rust suites, Windows products, native Linux graphics/sound/controllers and image/install proof remain unrun.
Software observations in `docs/PERFORMANCE.md` clear no hardware budget. Zero-warning gates are unmet.
VM (virtual machine) Create/Prove commands are prepared in `docs/CI.md`; neither ran. Next owner decision: Create
go-ahead. Licensing prerequisites and proof gate the guest Qt kit and runner. No runner settings, isolation flags,
licences or accounts changed; nothing was merged, deployed or published.
