# Where QOR OS stands

For Andrew, 5 November 2026. Local branch: `integration/owner-brief`.

All 14 worktrees (Git checkouts) remain; this session owns implementation. The verified QOR-local build lock keeps
heavy jobs sequential. No cross-project prerequisite exists. Nothing was pushed.

## Verified checks

Commercial Qt 6.12 runs hidden in WSL (Linux inside Windows), with software graphics, a graphics processor fence,
two processors and one worker. ASan/UBSan catch memory errors and undefined behavior; leak checking is off.
Latest jobs have a 1.75 GiB hard memory cap; earlier jobs used 1.25 GiB.

- QQ code at `39b0768`: unfiltered **91 asset-library/195 game-build/3 Store-to-Player Qt cases pass per build**, zero
  failures/skips. Recovery, tags, trust, checked previews and listing provenance retain regression checks.
- Store **format 2/legacy/Library/pages: 621/11/23/18 cases per main profile**, zero failures/skips; all **153/9/9/16 functions** ran.
  Fresh records, source/input fingerprints and sanitizer flags are verified. Hardening (exploit mitigations) passes
  **30 normal/28 sanitizer main-build programs** and **16/13 product programs**.
- Shape's private factories use their actual owner, preserving public interfaces and 2D axis locks.
  **52 Scene/10 selected physics cases pass per product build**, zero failures/skips. Group waits use existing software
  game clocks; GPU waits, position tolerances and frame-rate checks remain. Earlier failed runs are retained.
- **The complete first journey at `3421d35` passes once per product build**, zero failures/skips: two Studio builds,
  three real Player launches and Prism's Library request, including playback after deleting the temporary project.
  Prism, the compositor and shell, starts a recording stand-in; the real Player runs separately. Prism is not instrumented.
  This result precedes the Shape batch and needs refreshing.
- **10 simulator/11 device-option cases pass per main profile**, zero failures/skips. The selected pinned-core C interface
  passes **9 normal-profile cases**; the sanitizer profile intentionally keeps the core off.
- Imported-model startup/reload and animation/picking: **5 Play/3 Studio cases per build**, zero failures/skips.
  Journey helper: **15 Python regressions each on Windows/Linux**, zero skips.

## Remaining and next

QQ lint (Qt's interface-language checker) still fails: **153 console/178 structured warnings**. This batch removes
13 existing findings, none added. **12 Wayland metadata warnings** match their baseline. An unchanged pinned Rust
dependency fails its warning gate; repair belongs upstream. Listing/planning remain on the window thread without a latency budget.

Next: refresh the complete journey, then check the Store's C++ storefront boundary.
Exact commands, failures and resume point: `build/autonomous-2026-11-04/HANDOFF.md`.

Full OS/Rust suites, Windows products, native Linux graphics/sound/controllers and image/install proof remain unrun.
Zero-warning gates are unmet. VM (virtual machine) Create/Prove commands are prepared in `docs/CI.md`; neither ran.
Next owner decision: separate Create go-ahead. Licensing prerequisites and proof gate the guest Qt kit and runner.
No runner settings, isolation flags, licences or accounts changed; nothing was merged, deployed or published.
