# Where QOR OS stands

For Andrew, 5 November 2026. Branch: `integration/owner-brief`.

This session alone implements QOR. All 14 worktrees (Git checkouts) remain; heavy jobs use QOR's local lock.
No cross-project prerequisite. Commit/push authorised to the verified `origin/integration/owner-brief`; final refs go in the local handoff.

## Startup branding

The existing QorMark introduces the shared Prism compositor/shell session on Lumen's dark background.
Its first valid frame releases a 280 ms fade; reduced motion, Escape, text fallback and a 1.5-second watchdog remain.
Zero-size surfaces wait for layout. The new regression fails before the fix and passes unchanged afterward.
The logo's seven source/recipe hashes and bounded image data are checked before shell builds.

Firmware stays vendor-controlled. No bootloader is packaged. The optional image target stages/verifies seven Plymouth
(OS-loading splash) and dracut (initial filesystem builder) files under `build/`, preserving existing files.
There is no bootable image builder or actual OS boot evidence. The theme's runtime, initial filesystem inclusion,
Secure Boot and splash-to-greeter transition remain unproven. Host boot settings, disks, firmware and VM are untouched.

## Verified checks

Commercial Qt 6.12 runs hidden in WSL (Linux inside Windows), graphics-fenced, two processors/one worker, 1.75 GiB cap.
ASan/UBSan check memory errors/undefined behavior; leaks unchecked. Root profiles include debugging information;
the sanitizer preset excludes the QOR core and has compiler-warning promotion off, with separate zero-warning log guards.

- Linux startup: **12 cases per normal/sanitizer build**, zero failures/skips; existing Kit: **10 normal cases**.
- Real logo exporter: **three tests per build**; asset/payload: **16 per Windows/Linux host**, zero failures/skips.
- Incremental root interface-language checks pass on both hosts. Hardening (exploit mitigations): **34 normal/32 sanitizer programs** pass.
- Actual private host/two-output simulated Prism captures are inspected. Three before/three after software benchmarks
  contain fresh frames and all input samples; they clear no hardware budget.
- Native Windows startup: **12 cases**, zero failures/skips; hardening: **25 programs** pass. Earlier QQ recovery data:
  **78 native cases; 69 per Linux product build**, zero failures/skips.
- Public source-hash scanner matches have exact-line classifications and negative controls; no credential exception.

## Remaining and next

Actual Studio crash-only check: **2 pass/1 fail**, exit 8 before crash/restore; sanitizer crash case unrun.
Checkout-local Windows Store: **613 pass/1 fail**, zero skips, exit 8; isolated many-file case passes.
Earlier staging rename returned Access denied (5); cause unresolved, no permission/security changes.
QQ lint: **121 console/136 structured warnings**, failed; 12 earlier Wayland type-generation warnings were not regenerated. The pinned Rust warning gate fails.
Full product/OS suites, native Linux graphics/sound/controllers, image/install and CI isolation remain unproven.

Next owner decision: VM (virtual machine) Create go-ahead using `docs/CI.md`, then actual Prove results.
Guest Qt licensing prerequisites still apply. Runner unchanged; no release, deployment or merge.
Exact commands, failures, evidence and resume point: `build/autonomous-2026-11-04/HANDOFF.md`.
