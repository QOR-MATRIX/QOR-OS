# Where QOR OS stands

For Andrew, 16 November 2026. Matching Mesa debug symbols now identify the
graphics operation behind a captured first-Play buffer wait. The original
natural-frame requirement still fails; no production fix is established.

The captured indexed draw, `glDrawElementsBaseVertex`, reaches Mesa's job
submission and requests a TSDA tile-state buffer, temporary data for drawing
tiles. Mesa checks whether cached buffer 291 is idle using timeout zero.
That check returns a timeout; it does not prove the buffer was reused.
The exact library's machine instructions confirm this allocation and probe.
The Studio pass and individual graphics completion signal remain unknown.

All 17 captured Mesa addresses now have matching function ranges. Mesa is the
graphics driver; its debug file matches both build identity and checksum.
Two first-frame stacks improve from 3 to 11 named frames out of 18. The Qt
interface toolkit's candidate symbols are rejected: the downloaded file is
x86-64, with a different identity, despite its ARM64 repository location.

The first symbolizer exceeded the 3,072-MiB (mebibyte) memory guard at 2,992 MiB
and was stopped. A smaller function-table reader completed the Mesa resolution;
its job's 72 samples stayed at or above 7,232 MiB. No running library or SDK changed.
All 1,414 source hashes, both original binaries and the prior image are intact.
Locks are free and no QOR test worker remains.

No test or graphics run occurred in this continuation. The sealed reference
and lower-overhead observer each remain three passes, one failure, zero skips;
`times > 5` fails with three. The system-call tracer recorded one and disturbed
scheduling. Earlier measurements total 383.545 ms of waits on buffer 291, but
only one of its calls has the resolved stack; do not assign every wait that stack.
Error capture/coalescing and wall-clock waiting passed; later notice/cursor
assertions were unreached. The separate software-renderer failure stays open.

The 97 unresolved image outliers, supplemental deterministic logging coverage
and early Pi graphics initialization remain unresolved or proposed. No new
image equivalence, Release, sanitizer or broader product acceptance is claimed.

Next: audit the exact running kernel's zero-timeout V3D buffer-wait path to
explain why a cache probe sleeps. PR #10 stays draft. Full products, Player frame
rates, the earlier Wayland memory leak, ARQADE diagnostics, isolated CI (automated
integration checks), hardware and Q Pi image acceptance remain open. Required
hosted Qt/core-audit jobs skipped and Release readiness failed at admission.
Receipts: `build/development-2026-11-16/session-04/`; session-03 remains sealed.
Consolidation planning is separate.
