# Where QOR OS stands

For Andrew, 16 November 2026. Studio's first Play still fails its natural-frame
requirement on the Pi. Client-only tracing locates most of the delay in waits
for graphics buffers; it has not identified the owning Studio pass or a fix.

Actual rendering is OpenGL ES 3.1, a graphics interface, on Broadcom V3D, the
Pi graphics processor. The private compositor, which draws test windows, is
unchanged. Temporary observers leave original sources, binaries and timing intact.

First Play spends 510.661 ms after recording drawing commands before swap.
Of that, 491.838 ms is inside 417 V3D buffer-wait calls, using 0.969 ms of
render-thread CPU. Buffer handle 291 accounts for 383.545 ms and belongs to a
captured graphics submission. The call stack reaches Qt's end-of-frame method.
This waits for submitted work; no unique completion signal or shader is known.
Matching debug symbols for Mesa, the graphics driver, and Qt, the interface
toolkit, are absent. Nearby symbol names are unreliable.

The reference and lower-overhead observer each fail `times > 5` with three.
The system-call tracer changes scheduling and records one. Each execution has
three passes, one failure and zero skips. Error capture/coalescing and wall-clock
waiting pass; later notice/cursor assertions are unreached. Neither tracing nor
the earlier warm-cache startup improvement is acceptance.

The independent software-renderer failure remains open. Prior image review
leaves 97 of 195 high-difference pixels unexplained by the proposed edge rule;
no new image capture or equivalence is claimed. Supplemental deterministic
error/warning/cursor coverage and an early Pi graphics initializer remain
recommendations. The natural-frame requirement stays unchanged.

All 1,414 source hashes, both original binaries and the prior screenshot are
intact. Locks are free and no QOR test workers remain. All 215 diagnostic/setup
samples exceed the 3,072-MiB (mebibyte) guard, minimum 6,736 MiB. One temporary
observer compilation refusal is preserved; its corrected build is warning-free.
No new application Release or sanitizer acceptance is claimed.

Next: resolve the captured Mesa/Qt call sites with matching debug information
to identify the graphics operation and pass that reuse the dominant buffer. PR #10
stays draft. Full products, Player frame rates, the earlier Wayland memory leak,
ARQADE diagnostics, isolated CI (automated integration checks), hardware and
Q Pi image acceptance remain open. Required hosted Qt/core-audit jobs were
skipped and Release readiness failed at admission. Current receipts:
`build/development-2026-11-16/session-03/`; session-02 is sealed.
Consolidation planning is separate.
