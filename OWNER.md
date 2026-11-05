# Where QOR OS stands

For Andrew, 4 November 2026. Local branch: `integration/owner-brief`.

All 14 worktrees (Git checkouts) remain; this session owns implementation. One verified QOR-local build lock keeps
heavy jobs sequential. No cross-project prerequisite exists. Nothing was pushed.

## Verified checks

Tests use commercial Qt 6.12 hidden in WSL (Linux inside Windows), software graphics, a graphics processor fence,
two processors and one worker. ASan/UBSan catch memory errors and undefined behavior; leak checking is off.
Latest jobs have a 1.75 GiB hard memory cap; earlier jobs used 1.25 GiB.

- QQ code at `39b0768`: fresh unfiltered **91 asset-library/195 game-build/3 Store-to-Player Qt cases pass per build**,
  zero failures/skips. Every test function and source fingerprint is verified; harness limits remain recorded. Recovery,
  tags, trust, checked previews and listing provenance retain regression checks.
- **The complete first journey passes once per build**, zero failures/skips: two Studio builds, three real Player launches,
  and Prism's Library request, including playback after deleting the temporary project. Prism is the compositor and shell;
  it starts a recording stand-in here, with the real Player run separately. This journey's Prism is not instrumented.
  Fresh records/source/program fingerprints are verified; Release takes 113.07 seconds, QQ sanitizers 113.31.
- **10 simulator/11 device-option Qt cases pass in each main profile**, zero failures/skips. The selected pinned-core
  bridge (C interface) passes **9 cases in the normal profile**. The sanitizer profile intentionally keeps the core off.
  Fresh records, unchanged source/settings and actual sanitizer instrumentation are verified.
- Journey helper: **15 Python regressions each on Windows/Linux**, zero skips. Failed children, stale reports and missing
  first-frame markers are refused.
- Hardening (exploit mitigations) passes **16 normal/13 sanitizer product programs and 27/25 main-build programs**.
  Main profiles are existing `ci-linux`/`linux-asan`. Core-enabled configuration alone does not prove core linkage.

## Remaining and next

QQ lint (Qt's interface-language checker) still fails: **166 console/191 structured warnings**, none newly introduced
in retained comparisons. Against the first retained 381-warning snapshot, 190 are removed, none added. **12 Wayland
metadata warnings** match their baseline. An unchanged pinned Rust dependency also warns; the latest wrapper correctly
exits 1. Its repair belongs upstream. Listing/planning still run on the window thread; no latency budget is established.

Next: reproduce Prop (imported model) runtime warnings. Exact commands, earlier checks/failures, harness limits and resume point:
`build/autonomous-2026-11-04/HANDOFF.md`.

Full OS/Rust suites, Windows products, native Linux graphics/sound/controllers and image/install proof remain unrun;
zero-warning gates are unmet. VM (virtual machine) Create/Prove commands are prepared in `docs/CI.md`; neither ran.
Next owner decision: separate Create go-ahead. Licensing prerequisites and proof still gate the guest Qt kit and runner.
No runner settings, isolation flags, licences or accounts changed; nothing was merged, deployed or published.
