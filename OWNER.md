# Where QOR OS stands

For Andrew, 5 November 2026. Local branch: `integration/owner-brief`.

All 14 worktrees (Git checkouts) remain. This session owns implementation. The QOR-local lock serializes heavy jobs;
no cross-project prerequisite exists. Nothing was pushed.

## Verified checks

Commercial Qt 6.12 runs hidden in WSL (Linux inside Windows), graphics-fenced, two processors/one worker, 1.75 GiB cap.
ASan/UBSan check memory errors/undefined behavior; leaks unchecked. Windows: two processors/one worker, no memory cap.

- Linux Store format 2/legacy/Library/pages/front-end: **621/11/23/18/39 cases per main profile** pass, zero failures/skips.
- The complete journey at `f99e007` passes per product build, including actual Player playback after project removal.
  Prism (compositor and shell) records a stand-in, without sanitizers in this journey.
- Failed scripts omit the complete request object while keeping **exit 4**. Benchmark output failures return **exit 1**;
  **11 collector cases per Linux profile/9 on Windows** pass, zero failures/skips.
- Windows Store: **614 format-2/11 legacy/21 Library/39 front-end/18 page cases** pass in private Temp, zero failures/skips.
  Unix-only: four format-2 functions/two Library rows, excluded.
- Hardening (exploit mitigations): **33 normal/31 sanitizer Linux main programs, 16/13 products, 24 Windows program headers** pass.
- QQ asset data: **Windows 78 cases/35 functions; Linux 69/32 per product build**, zero failures/skips.
  Folder identifiers replace Windows timestamps. Missing-folder/older journals stay; unresolved changes block new transactions.
- Gizmo (selected-object handles): **8 cases/6 functions per product build** pass, zero failures/skips.
- Recorder: **15 cases/9 functions per product build** pass, zero failures/skips; actual Player records/replays.

## Remaining and next

Studio recovery interface: **Release 37 pass/1 fail**; **37 sanitizer passes** cover six functions. The crash case is
unrun under sanitizers. Real Studio initialization replies remain unresolved; its private pipe accepts connections.

Checkout-local Windows Store: **613 pass/1 fail**, zero skips. Diagnostics read **Access denied (5)** after staging rename
failure. Two checkout-root controls failed; two outside-root controls passed. Cause unresolved; diagnostics restored.
No permissions/security settings changed; no other process interrupted.

QQ lint (interface-language checking): **140 console/156 structured warnings** on Windows/Linux; eleven recorder findings removed,
none added; still failed. Player lint passes. Windows root lint: **seven modules, zero warnings**. **12 Wayland metadata warnings** match baseline.
Six QQ findings come from Qt's metadata (type descriptions); the installed kit remains unchanged.
A pinned Rust dependency fails its warning gate; repair belongs upstream. Listing/planning remain on the window thread without a latency budget.

Next: qualify store metadata, then grouped-state reads. Commands: `build/autonomous-2026-11-04/HANDOFF.md`.
Full OS/Rust/Windows product suites, native Linux graphics/sound/controllers and image/install proof remain unrun.
Software observations clear no hardware budget. Zero-warning gates remain unmet.

VM (virtual machine) Create/Prove commands are prepared in `docs/CI.md`; neither ran. Next owner decision: Create go-ahead.
Licensing prerequisites and actual proof gate the guest Qt kit and runner. No runner settings, isolation flags, licences
or accounts changed; nothing was merged, deployed or published.
