# Where QOR OS stands

For Andrew, 16 November 2026. Studio's first Play still misses its natural-frame
requirement on the Pi. The new profile separates slow startup from a later wait
for submitted graphics work; neither is an accepted production fix.

Actual rendering remains OpenGL ES 3.1, a graphics interface, on Broadcom V3D,
the Pi graphics processor. The private compositor, which draws test windows,
is unchanged. A temporary client library observes phases without modifying the
original sources, executables, assertions or deadlines.

Cold startup takes 25.435 seconds from window show to first swap. Reusing only
73 Mesa shader-cache files reduces it to 1.793 seconds. First Play still spends
about 508 ms between recorded drawing commands and swap, using only 10–11 ms of
render-thread CPU. Samples show a DMA-fence wait, a wait for submitted work;
the interface thread waits on a thread lock. The owning graphics pass and fence
remain unknown. Qt cannot supply GPU timestamps on this ES path.

Reference, cold-profile and warm-profile runs each report three passes, one
failure and zero skips. `times > 5` still fails with three. Error capture and
coalescing pass; later notice/cursor assertions are unreached. Wall-clock waiting
passes. Warm caches are a diagnostic comparison, not acceptance.

The prior full images show sky and lighting restored. Of 195 viewport outliers,
192 lie at local image gradients; only 98 match within one pixel and eight colour
levels. No missing content is visible, but 97 need more than that simple edge
explanation. Visual criteria and regressions are proposed; equivalence is unproven.

Supplemental deterministic error/warning/cursor coverage and an early Pi graphics
initializer remain recommendations. The natural-frame gate stays unchanged.
The separate software-renderer failure remains open. All 1,414 source hashes,
both original binaries and the prior screenshot are intact; locks are free and
no QOR test workers remain. All 250 samples exceed the 3,072-MiB (mebibyte) guard,
minimum 6,718 MiB. No new application Release or sanitizer acceptance is claimed.

Next: trace the first-Play graphics-system calls alongside the phase observer
to identify the blocking fence. PR #10 remains draft. Full products, Player frame
rates, the earlier Wayland memory leak, ARQADE diagnostics, isolated CI (automated
integration checks), hardware and Q Pi image acceptance remain open. Required
hosted Qt/core-audit jobs were skipped and Release readiness failed at admission.
Current receipts: `build/development-2026-11-16/session-02/`; session-01 is sealed.
Consolidation planning is separate.
