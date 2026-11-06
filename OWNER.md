# Where QOR OS stands

For Andrew, 6 November 2026. Branch `integration/owner-brief`; session base `9d6c7c9`.

This session alone implements QOR. All 14 worktrees (Git checkouts) are preserved; the other 13 are clean.
Commits/pushes and disposable installation VM work are authorised. Host disks, EFI (firmware boot storage),
security/network/runner settings and other projects remain outside scope. One heavy QOR job runs at a time
under the verified project-local lock; no cross-project lock applies.

## Actual image and VM results

The private Fedora 44 bootc image (whole-image deployment) produces a real UEFI ISO.
The first artifact is 1,926,746,112 bytes; checksum and exact input hashes are in
`build/vm-install-2026-11-06/probe-build-v6/build-result.json`.
It reached the QOR OS Development live login console **after manual GRUB recovery**.
Clean boot failed on a module-prefix error; the logo was oversized. The Qt display service failed on
missing PulseAudio client libraries and a nonexistent legacy framebuffer. No installation has run.

Source fixes preserve embedded GRUB modules, derive a 112-pixel official mark, use software DRM
(direct rendering) and validate every packaged runtime library/plugin. All 62 ELF (Linux binary) dependencies
now resolve in the fixed target environment. A new ISO/real GUI boot is still required.
The app binary is explicitly traced to its older `9d6c7c9` compiler revision, not relabelled as new.

The VM used a new regular 16-GiB raw file, no backing file, private firmware variables and software CPU
emulation. It received no host devices, GPU, network, EFI/NVRAM or Docker socket. Actual guest inventory shows
one writable disk with the assigned serial/capacity. Host identities/ancestry are pinned; the guest guard
still needs its actual UUID/driver/PCI ancestry check before any stock bootc transaction.
Both live diagnostic runs shut down normally; no VM or heavy job is currently active.

## Checks and limits

- Windows: **13 installer + 20 first-run Qt cases**, zero failures/skips; module lint has zero warnings.
- Guard/provenance: **27 tests per Windows/Linux**; four additional Linux process tests and two Windows asset
  tests pass, zero failures/skips in these targeted suites. They are inert, not installation evidence.
- Windows hardening (exploit mitigations): **30 programs**, none short. Final staged hooks remain pending.

The Linux installer and sanitizer build remain blocked by Ubuntu WSL (Linux inside Windows):
`Wsl/Service/0x8007274c`. Restart permission is pending because it stops that distro's processes;
Ubuntu is preserved. Encryption and QOR ID are unavailable. First-run completion/persistence, installed
boot, recovery, rollback, hardware, Secure Boot and release readiness remain unproved. Existing Studio,
Store, QQ, Rust and Wayland gates are unresolved, not rerun.

Next: preserve/push a reviewed development checkpoint; rebuild/boot the corrected ISO, then finish guarded
installation and two installed-system boots. Exact commands, failures and resume: `build/vm-install-2026-11-06/LATEST.md`.
