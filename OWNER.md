# Where QOR OS stands

For Andrew, 6 November 2026. Branch `integration/owner-brief`; image evidence at `c8ccc9b`.

This session alone implements QOR. All 14 worktrees (Git checkouts) are preserved; the other 13 are clean.
Commits/pushes and disposable virtual-machine installation work are authorised. One heavy QOR job runs
at a time under the verified project-local lock. No cross-project prerequisite applies. Host disks,
firmware, security/network/runner settings and other projects remain outside scope.

## Actual image and guest results

The private Fedora 44 bootc image (whole-image deployment) produces a real UEFI ISO (firmware-bootable media):
**1,932,019,712 bytes**, SHA-256 checksum and exact inputs in
`build/vm-install-2026-11-06/probe-build-v7/build-result.json`.
It automatically boots through the correctly sized official-logo menu into the live OS, shows the QOR
loading mark and exposes readable diagnostics with Escape. The brief fade is not proved.
Its normal Qt display failed because the default graphics device was absent.

A temporary diagnostic boot proves the new VirtIO display selector, guest tty2 reservation and real
welcome UI. Interruption leaves welcome pending; restart and three Return presses complete it with
its checked public record and exit zero. **This is live RAM state, not installed-system persistence.**
The actual app binary remains traced to compiler revision `9d6c7c9`.

The real guest guard matched the new 16-GiB regular raw file, serial `qor-110e4251919b4c33`, sealed VM
identifier and VirtIO PCI (virtual hardware) ancestry. Missing firmware metadata caused refusal;
restoring it revalidated the same disk. Read-only ISO checks pass. No install transaction has run.
Private firmware variables and software CPU emulation receive no host devices, GPU, EFI/NVRAM,
network or Docker socket. Both v7 runs shut down normally. No VM/heavy job is currently active.

## Checks and next work

- **35 guard/provenance/display cases each on Windows and Linux**, seven Linux process/source cases,
  two Windows asset cases: zero failures/skips in final targeted runs. The first Linux harness was
  refused by the supervision guard; the corrected harness passes without weakening it.
- Earlier native Windows checks on unchanged C++: **13 installer + 20 first-run cases**, module lint
  without warnings; hardening (exploit mitigations) passes 30 programs. Full-project gates remain open.

The image-only PipeWire client dependency is added and its exact tooling image/package manifest frozen.
Ubuntu WSL (Linux inside Windows) connectivity recovered without restarting it; the existing commercial
Qt kit is verified. Fresh Linux installer/sanitizer builds are pending. No guest Qt kit or credentials enter images.

Next: finish checkpoint hooks/push, build/test the Linux frontend, then prove rebuilt automatic graphics,
package the offline deployment source, install, detach media and prove first-run persistence after a second reboot.
Encryption and QOR ID are unavailable. Hardware, Secure Boot, rollback, recovery and release readiness remain
unproved. Exact results, failures and resume: `build/vm-install-2026-11-06/LATEST.md`.
