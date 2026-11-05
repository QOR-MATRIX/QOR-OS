# Where QOR OS stands

For Andrew, 5 November 2026. Local branch: `integration/owner-brief`.

All 14 worktrees (Git checkouts) remain; this session owns implementation. The verified QOR-local build lock keeps
heavy jobs sequential. No cross-project prerequisite exists. Nothing was pushed.

## Verified checks

Commercial Qt 6.12 runs hidden in WSL (Linux inside Windows), with software graphics and a graphics processor fence,
two processors and one worker. ASan/UBSan catch memory errors and undefined behavior; leak checking is off.
Latest jobs have a 1.75 GiB hard cap.

- QQ at `39b0768`: unfiltered **91 asset-library/195 game-build/3 Store-to-Player Qt cases pass per build**, zero failures/skips.
  Recovery, tags, trust, checked previews and provenance retain regression checks.
- Store **format 2/legacy/Library/pages/front-end: 621/11/23/18/39 cases per main profile**, zero failures/skips;
  all **153/9/9/16/28 functions** ran. Fresh records, fingerprints and sanitizer flags are verified.
  Hardening (exploit mitigations) passes **33 normal/31 sanitizer main-build programs** and **16/13 product programs**.
- Shape factories preserve public interfaces and 2D axis locks: **52 Scene/10 selected physics cases pass per product build**,
  zero failures/skips. Group waits use software game clocks; GPU waits, tolerances and frame-rate checks remain.
- Failed scripts omit the complete request object while keeping their step number, reason and **exit 4**.
  An actual-process regression reproduced argument disclosure; **3 Qt cases plus client/text-privacy controls pass per
  main profile**, zero failures/skips. A short private runtime fixes the new test's Unix
  socket-path limit. Failures remain recorded.
- The complete journey at `d111bdc` passes per product build, including real Player playback after project removal.
  Prism (compositor and shell) records a stand-in. Prism has no sanitizers in this journey.
- **17 service/10 simulator/11 device-option cases pass per main profile**, zero failures/skips; the core C interface
  passes **9 normal-profile cases**. The sanitizer profile keeps the core off.

- Benchmark output failures return **exit 1**. All **11 collector cases/8 functions per main profile** pass,
  zero failures/skips, alongside actual compositor failed/successful report checks.

## Remaining and next

QQ lint (Qt's interface-language checker) fails: **153 console/178 structured warnings**, 13 removed in the Shape batch,
none added. **12 Wayland metadata warnings** match their baseline. A pinned Rust dependency fails its warning gate;
repair belongs upstream. Listing/planning remain on the window thread without a latency budget.

Next: reduce remaining QQ metadata findings in small batches. Exact commands, failures, fingerprints
and resume point: `build/autonomous-2026-11-04/HANDOFF.md`.

Full OS/Rust suites, Windows products, native Linux graphics/sound/controllers and image/install proof remain unrun.
Software observations in `docs/PERFORMANCE.md` clear no hardware budget. Zero-warning gates are unmet.
VM (virtual machine) Create/Prove commands are prepared in `docs/CI.md`; neither ran. Next owner decision: separate Create
go-ahead. Licensing prerequisites and proof gate the guest Qt kit and runner. No runner settings, isolation flags,
licences or accounts changed; nothing was merged, deployed or published.
