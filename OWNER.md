# Where QOR OS stands

For Andrew, 16 November 2026. The exact running Pi graphics driver turns a
zero-timeout buffer poll into a sleeping wait. This explains the captured cache
probe's blocking behavior; correcting it has not yet been tested. The original
natural-frame requirement still fails.

The running kernel, `6.18.50+rpt-rpi-2712`, and its package-declared source are
identified. V3D, the Pi graphics driver, and DRM, Linux's graphics infrastructure,
have matching loaded/file build identities. Machine instructions turn zero
into one jiffy, a kernel timer tick. The configured tick is 4 ms; timer rounding
and scheduling can make the elapsed delay longer. The source path reaches the
sleeping fence wait, where a fence is a graphics completion signal.

Mesa, the user-space graphics driver, intended to check whether cached buffer
291 was idle for a tile-state allocation. One captured call spent 11.853 ms
elapsed and 0.005 ms on CPU, then reported a timeout. The kernel audit explains
why it can sleep; it does not identify the individual fence or divide that
elapsed time precisely. Successful buffer reuse and the Studio pass are unknown.

The installed newer `6.18.60` kernel's wait instructions retain the same defect.
Upstream Linux correction `226666556a9b` preserves zero as a poll. No corrected
Pi kernel is installed or qualified. Next: prepare a reviewable backport
candidate and busy-buffer regression plan before any installation or reboot.
No QOR production change or test-contract change is proposed to mask this bug.

This continuation runs no graphics or product tests. The sealed reference and
lower-overhead observer remain three passes, one failure, zero skips;
`times > 5` fails with three. Error capture/coalescing and wall-clock waiting
passed; later notice/cursor assertions were unreached. The separate software
renderer failure and 97 unresolved image outliers stay open.

Both static audit jobs stay above the 3,072-MiB (mebibyte) resource floor.
The first job's module checks succeed; a header-only search finds no helper,
so the whole job is not called passed. The pinned driver source resolves it.
All 1,414 original source hashes, both binaries and the prior image are intact.
No kernel, library, service or system setting changes. Locks are free and no
QOR worker remains. Supplemental logging and early graphics setup stay proposals.

PR #10 stays draft. Release, sanitizer, full products, Player frame rates,
the earlier Wayland memory leak, ARQADE diagnostics, isolated CI (automated
integration checks), hardware and Q Pi image acceptance remain open. Required
hosted Qt/core-audit jobs skipped and Release readiness failed at admission.
Receipts: `build/development-2026-11-16/session-05/`; session-04 remains sealed.
Consolidation planning is separate.
