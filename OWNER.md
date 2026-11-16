# Where QOR OS stands

For Andrew, 16 November 2026. A reviewable Pi-kernel backport is prepared for
the sleeping zero-timeout buffer poll. It is checked as source and compiled
objects, not installed, booted or accepted. The original frame test still fails.

The production patch changes two lines in V3D, the Pi graphics driver: zero
remains a poll; positive waits and error accounting retain their original code.
It is a narrow backport of upstream Linux `226666556a9b`, pinned to the running
6.18.50 source and configuration. Patch application and exact reversal pass.
Both original and patched driver objects compile without warnings against the
matching headers; the patched instructions bypass conversion for zero.

A separate, default-off patch supplies six KUnit tests, Linux's kernel unit
tests. A private buffer with an unsignaled completion fence exercises the real
wait handler and records whether the task voluntarily schedules away. It also
checks idle polls, positive expiry/success and invalid padding/handles. The
device and handle fixture are synthetic; no graphics work is submitted.
The test object compiles, but none of these kernel tests has executed.

The running kernel has KUnit disabled. The parser generators bison/flex and
the QEMU virtual machine tool are absent. Full kernel builds, unpatched failure
and patched pass evidence, kernel sanitizers, interruption/completion integration
and hardware qualification remain pending. No dependency is installed.

The rollback plan distinguishes running and next-boot state: normal boot files
already match 6.18.60, although 6.18.50 is running. Neither reboot nor restoring
today's boot files proves a return to this baseline. Verify a recovery boot
before any candidate trial; keep candidate modules and boot files separate.
No kernel, boot file, module, library, service or desktop setting changes.

All 1,414 original QOR source hashes, both binaries and the prior image are
intact. The guarded preparation's 61 memory samples stay at or above 7,153 MiB
(mebibytes), above the 3,072-MiB floor. Locks are free and no QOR worker remains.

Historical focused results remain three passes, one failure, zero skips;
`times > 5` fails with three. Later notice/cursor assertions were unreached.
The software-renderer failure and 97 image outliers remain open. No new QOR
Release, sanitizer, full-product, hardware or image acceptance is claimed.
Required hosted Qt/core-audit jobs skipped; Release readiness failed at admission.

Next: qualify an isolated kernel-test build environment and run the unpatched
negative control plus patched KUnit suite before proposing installation.
PR #10 stays draft; isolated CI (automated integration checks), Player frame
rates, the earlier Wayland leak and ARQADE diagnostics remain open.
Review: `image/pi/kernel-wait-bo/`; runbook: `image/README.md`.
Receipts: `build/development-2026-11-16/session-06/`; session-05 remains sealed.
Consolidation planning is separate.
