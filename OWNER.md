# Where QOR OS stands

For Andrew, 16 November 2026. Explicit OpenGL ES 3.1, a graphics interface,
restores Studio's observed sky and lighting on the Pi.
The natural-frame test still fails. This is diagnostic evidence, not a production
fix or completed hardware qualification.

A temporary library sets Qt's default format before application startup. The
actual renderer is Broadcom V3D 7.1.10.2, the Pi graphics processor, using ES 3.1.
The private compositor, which draws test windows, is unchanged. No desktop,
service, security, device permission or software-renderer setting changed.

The original focused Agent cases report three passes, one failure and zero skips
in 29.713 seconds. The errors now form one counted log entry without graphics-
program warnings, but `times > 5` fails with three. Its later notice and cursor assertions
are not reached; wall-clock waiting passes. The first window frame notification
arrives 25.382 seconds after surface creation, including startup; request to
notification is 10.614 ms. This is not the earlier software play-frame interval.

Complete image comparison against a software client on the same compositor
shows the sky, lighting, objects, shadows and interface restored. The viewport
differs by 0.062/255 per colour channel on average; 195 pixels differ by more
than eight, mostly at object/ring edges. It is not exact equality or an approved
image tolerance. The narrow button/focus checks pass on both clients.

Recommend supplemental deterministic counting and incremental-read checks with
real QML errors, including errors interleaved with warnings. QML is Qt's interface
language. Keep the original natural-frame gate. No production code or test
contract changed. Explicit Pi ES selection is a candidate for the image defect,
subject to image regressions and validation on the other rendering paths.

All 1,414 source hashes, both original executables and the previous screenshot
are preserved. The process-local format request is gone, no QOR workers remain
and the native lock is free. All 168 samples exceed the 3,072-MiB (mebibyte) guard,
minimum 6,631 MiB. No new application Release build or sanitizer pass is claimed.

Next, profile the first Play callbacks under ES to distinguish client rendering
work from presentation waits. The separate software failure remains one repeat
against `times > 5`. Full products, Player frame rates, the earlier Wayland memory
leak, ARQADE diagnostics, isolated CI (automated integration checks), hardware
and Q Pi image acceptance remain open. Required hosted Qt builds and core audit
were skipped and Release readiness failed at the starting revision.

PR #10 stays draft on `fix/native-play-clock-waits`. Current receipts:
`build/development-2026-11-16/session-01/`; session-13 evidence remains sealed.
Consolidation planning is separate.
