# Where QOR OS stands

For Andrew, 6 November 2026. Branch integration/owner-brief; image evidence at c8ccc9b.

This session alone implements QOR. All 14 worktrees (Git checkouts) are preserved; the other 13
were checked clean. Commits/pushes and disposable virtual-machine work are authorised.
One heavy QOR job runs at a time under the verified project-local lock. Host disks, firmware,
security/network/runner settings and other projects remain outside scope.

## Actual image and guest results

The private Fedora 44 bootc image (whole-image deployment) produces a real UEFI ISO
(firmware-bootable media): **1,932,019,712 bytes**; exact inputs and SHA-256 checksum are in
build/vm-install-2026-11-06/probe-build-v7/build-result.json.
It automatically boots through the official-logo menu, shows the QOR loading mark and exposes
readable diagnostics with Escape. Animation is unproved. The normal Qt display failed because
its default graphics device was absent.

A temporary diagnostic boot proved the new VirtIO display selector, reserved guest console and
welcome UI. Interruption leaves welcome pending; restart and keyboard completion write the checked
public record and exit zero. **This is live RAM state, not installed-system persistence.**
That image's app binary remains traced to compiler revision 9d6c7c9.

The guest guard matched the new 16-GiB regular raw file, serial qor-110e4251919b4c33, sealed VM
identifier and virtual-hardware ancestry. Missing firmware metadata caused refusal. Read-only ISO
checks pass. No install transaction has run. Private firmware variables and software CPU emulation
receive no host devices, GPU, EFI/NVRAM, network or Docker socket. Both v7 runs shut down normally.

## Checks and next work

- Latest committed guard/display checks: **35 cases each on Windows and Linux**, seven additional
  Linux process/source cases, two Windows asset cases; final runs have zero failures/skips.
- Fresh Linux installer/runtime compilation linked both apps, but **failed acceptance on 13
  pre-existing QML registration warnings**. No accepted runtime receipt was produced.
- Explicitly naming the existing Gamepads QML export removes its warning: Linux module rebuild
  has zero warnings; the new Windows import/enum regression passes (three Qt cases including
  setup/cleanup). The first Windows test wrapper failed on a wrong executable path; corrected run passes.
- Twelve compositor metadata warnings remain. The private installer-only runtime will exclude that
  unused compositor; this does not clear its full-project gate. Full OS/product/sanitizer gates remain open.

Ubuntu WSL (Linux inside Windows) recovered without restart; the existing commercial Qt kit works.
No VM or heavy job is active at this checkpoint. No guest Qt kit or credentials enter images.
Next: checkpoint this tested input fix, rebuild the limited runtime, then package the offline deployment
source and prove installation, media-free first run and persistence after a second reboot.
Encryption and QOR ID remain unavailable. Hardware, Secure Boot, recovery, reproducibility and release
readiness are unproved. Exact logs and resume: build/vm-install-2026-11-06/LATEST.md.
