# Where QOR OS stands

For Andrew, 6 November 2026. Branch integration/owner-brief; starting pushed checkpoint 9e2e7b9.

This session alone implements QOR. All 14 worktrees (Git checkouts) are preserved.
Only one heavy QOR job runs under the verified project-local lock. Disposable VM (virtual-machine) work,
commits and pushes are authorised. Host disks, firmware, security/network/runner settings
and other projects remain outside scope.

## Actually observed

The private Fedora 44 bootc image (whole-image deployment) produces a real UEFI ISO
(firmware-bootable media), **1,932,019,712 bytes**, checksum
a3d3ea7518b57053ffb2c2199a51719b5e8d86222033a3331814db2719efaa2c.
Its recipe is c8ccc9b; its older welcome binary is traced to 9d6c7c9.
It automatically boots through the official-logo menu and OS-loading mark; Escape
shows diagnostics. Animation is unproved. Normal graphics failed on an absent default device.

A diagnostic boot proved the corrected VirtIO display selection, reserved guest console,
welcome interruption/restart and keyboard completion in RAM. The actual guest guard matched
the fresh 16-GiB raw file, sealed VM identifier, serial and hardware ancestry. Missing
firmware metadata caused refusal. Both runs shut down normally. **No install transaction,
installed-system welcome or second reboot has run.**

## Current implementation and checks

The private candidate connects the Qt installer to stock bootc through a complete,
checksum-checked OCI payload (content-addressed container image) on read-only media.
It defines a locked local qortest account, an honest limited introduction and bounded
public evidence. A narrow guest agent exposes policy inspection and shutdown/reboot;
the actual 10.2.2 process refused execution, file access and clock changes.
The launcher now rechecks ISO identity/checksum after pinning its file and ancestry.

- Linux Release and sanitizers: **21 welcome, 13 installer and 20 input Qt cases in each**,
  zero failures/skips; module lint has zero warnings; hardening (exploit mitigations)
  passes five programs in each. Actual instrumentation and identical source fingerprints are checked.
  The initial runner failed before tests on a wrong path; the corrected run passes.
- A separate, clean installer-runtime build passed with two programs and zero warnings.
  It predates the new introduction option and must be rebuilt before packaging.
- Portable guard/evidence checks last passed **56 cases each on Windows and Linux**,
  plus eight Linux process/file cases, with zero failures/skips.
- The exact tooling image is preserved in a checksum-recorded private archive.
  Restoration and repeated output remain untested. Full OS/product gates remain
  open, including 12 pre-existing compositor (display-server) metadata warnings outside this limited profile.

Next: commit/push this checked candidate and rebuild its runtime and ISO from that
clean revision. Prove cancellation/failure, installation,
media-free boot, welcome completion and a second guest reboot with persisted state.
Encryption and QOR ID are disabled. Hardware, Secure Boot, rollback, recovery, repeatable
output and release readiness remain unproved. No guest Qt kit or credentials enter images.
Exact jobs, logs and resume: build/vm-install-2026-11-06/LATEST.md.
