# Where QOR OS stands

For Andrew, 5 November 2026. Branch: `integration/owner-brief`; session base `dad071d`.

This session alone implements QOR. All 14 worktrees (Git checkouts) remain; the other 13 are clean.
Heavy jobs run sequentially under QOR's local lock. Commit/push is authorised to
`origin/integration/owner-brief`; final references and resume point are in the local handoff.

## Installation and first run

There is **no bootable OS image, live environment or working installer**. Application packages and a desktop do not prove installation.
The image target stages 25 checked files. A data-only Docker image contains every source-matching file;
it cannot boot. The device recipe requires an approved existing Fedora device base and licensed runtime.

Calamares is the native Qt installer candidate, with the official logo and generated Lumen colours.
Its settings remain inactive, with no destructive choice/reboot selected. No partitioning backend,
final physical-disk review, bootc deployment adapter or bootloader installer is integrated.
QOR-0023 records that boundary; `image/README.md` maps capabilities and prerequisites.

The three-page `qor-setup` app uses Kit/Lumen and records only completion, preserving damaged/changed records.
Windows completion first failed; direct atomic temporary-file publication fixes it. Prism requests welcome once after
an external device-session unlock, excluding simulation/live media. Motion/contrast preferences reach the app at launch;
unknown motion hints keep it still. Real authentication services and installed-system first login remain unproven.

## Verified checks

Commercial Qt 6.12; WSL (Linux inside Windows) stays hidden and graphics-fenced, two processors/one worker, 1.75 GiB cap.
ASan/UBSan check memory errors/undefined behaviour; leaks unchecked. Root profiles include debugging information.
The sanitizer/device caches exclude the core; sanitizer compiler-warning promotion is off, with separate strict log guards.

- First run: **20 Qt cases per normal Linux, sanitizer Linux and Windows**, zero failures/skips.
- Image/archive: **15 + 3 Python tests per Windows/Linux host**, zero failures/skips; saved-image verification passes.
- New module/incremental root interface-language checks pass. Hardening (exploit mitigations): **37/35/28/5** programs
  pass in normal Linux/sanitizer/Windows/device caches. Device app rejects development captures.
- Three app captures inspected. Three before/three after software tours have fresh frames/all input samples;
  median frame p95 increased **10.009 → 10.401 ms**. No hardware budget cleared.
- Regenerated Wayland metadata still emits **12 existing warnings**; its warning guard failed. No suppression.

## Remaining and next

Existing Studio crash recovery, checkout Store staging, QQ lint and Rust warning gates remain unresolved; not rerun here.
Full OS/product suites, native graphics/sound/controllers, install/reboot/recovery and CI isolation remain unproven.
No host partition, boot/firmware, VM, licensing or runner changes. No release/deployment/merge.

Next owner decision: CI VM Create go-ahead under `docs/CI.md`, then Prove and guest Qt licensing prerequisites.
Installer proof additionally needs the approved device base, reviewed adapter and separately authorised disposable VM/disks.
Commands, failed attempts, evidence, final refs and resume: `build/install-experience-2026-11-05/HANDOFF.md`.
