# Where QOR OS stands

For Andrew, 15 November 2026. V3D, the Pi graphics processor's renderer, is
available inside the separate sandbox, but Studio qualification fails.
The sandbox exposes only its render node, an interface without display control,
using existing permissions. No system graphics, desktop, service or security settings changed.
The original software-renderer configuration and evidence remain intact.

The private headless compositor, which draws the test windows, uses the OpenGL ES
3.1 graphics interface. Studio instead selects desktop OpenGL 3.1 and cannot load several packaged
shaders, programs that draw pixels. The unchanged focused Agent cases report
three passes, one failure and zero skips in 28.341 seconds. Shader warnings
separate three intentional errors into two log entries, failing the single-entry
assertion before its repeat-count check. Wall-clock waiting passes.

The first window frame callback, a display notification, arrives 24.064 seconds
after surface creation;
only 13.152 ms separates its frame request and callback. This includes startup,
not the earlier play-frame interval. The headless compositor has no physical
display pacing, so this is not a direct comparison with the software baseline.

The existing button/focus image check passes on both renderers, but the V3D
capture loses its sky and environment lighting. A matched software client on
the same compositor draws them correctly. Hardware qualification remains failed.
The preserved software case also remains failed: one repeat against `times > 5`.
Its dominant sampled work is reading texture pixels and drawing fragments;
the owning Studio drawing step is unresolved.

Recommend adding deterministic real-error counting and incremental-read coverage
while retaining the original natural-frame gate. No production code, assertion,
deadline or test contract changed. All 1,414 original source hashes and the
original Agent executable remain intact. Native preset builds pass without
diagnostics; they use RelWithDebInfo, optimized code with debugging information,
not CMake Release. All 216 samples exceed the 3,072-MiB (mebibyte) guard, minimum
6,653 MiB. The native lock is free and no test workers remain.

Next, make a bounded reversible OpenGL ES 3.1 client diagnostic to test the
shader mismatch, checking the whole image and unchanged focused assertions.
Software support remains a separate blocker. No new sanitizer or broader
acceptance pass is claimed. Earlier complete Agent is 22 passes, one failure and
one optional provider skip; Studio has 29 passes,
and the packaged-game journey passes in 131.581 seconds, each at its earlier scope.

Draft pull request #10 remains on `fix/native-play-clock-waits`. Current receipts:
`build/development-2026-11-15/session-13/`; session-12 is preserved. At the starting
revision, hosted source checks passed, required Qt builds and core audit skipped,
and Release readiness failed. Full products, isolated CI, Player frame-rate
failures, the earlier Wayland memory leak, ARQADE diagnostics, hardware and Q Pi
image acceptance remain open. Consolidation planning is separate.
