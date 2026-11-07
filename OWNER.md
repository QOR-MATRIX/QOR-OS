# Where QOR OS stands

For Andrew, 6 November 2026. Branch integration/owner-brief; starting pushed checkpoint 216ad48.

This session alone implements QOR. All 14 worktrees (Git checkouts) and unfinished work are
preserved; the other 13 were checked clean. Commits/pushes and disposable virtual-machine
work are authorised. One project-local heavy lock coordinates QOR jobs. No VM or heavy
job is active now. Other projects and host settings remain outside scope.

## Image milestone: incomplete

The earlier private UEFI ISO (firmware-bootable media) actually boots through the QOR menu
and loading mark, with Escape diagnostics. A diagnostic boot proves the corrected display
selection and welcome completion in RAM. It proves no installation or persisted first run.

The newer candidate connects the Qt installer to stock bootc (whole-image deployment),
a checked offline OCI payload (content-addressed image), a locked local qortest account
and bounded public evidence. It offers neither encryption nor QOR ID authentication.

At 216ad48, the fresh runtime, all 63 target-image programs/libraries/plugins, initramfs
(early boot filesystem), and OCI conversion/validation passed. Bootc then reported
**10 checks passed, four warnings and one skipped alternate-root check**, despite exiting zero.
The warnings concern generated package/runtime state and our retained build log.
The recipe now removes that state, declares runtime directories and makes warnings fatal;
a corrected image has not been built.

Composition was stopped only in QOR's identified container after Windows fell to about
317 MiB free out of 16 GiB. Partial outputs are preserved; the wrapper failed, and no ISO
receipt was accepted. A later read-only preflight found **102 MiB available**, below its
3-GiB budget. No unrelated process was stopped and no host setting changed.

## Checks and resume

- Linux Release and sanitizers: **21 welcome, 13 installer, 20 input Qt cases in each**,
  zero failures/skips; module lint has zero warnings; hardening (exploit mitigations)
  passes five programs in each. Actual instrumentation and identical source fingerprints checked.
- Latest Windows guard/resource tests: **59 passed, zero failures/skips**. Earlier Linux:
  56 portable plus eight process/file cases. The new image cleanup remains unverified.
- Full OS/product gates remain open, including 12 pre-existing compositor (display-server)
  metadata warnings outside this limited profile. Hardware, Secure Boot, recovery, rollback,
  repeatable output and release readiness are unproved.

Next owner action: free at least 3 GiB of RAM. Then run the prepared fresh image pipeline,
create a new allowlisted raw disk/private firmware variables, test cancellation/failure,
install, detach media, complete welcome and verify persistence through guest reboots.
Exact commits, jobs, commands and evidence: build/vm-install-2026-11-06/LATEST.md.
