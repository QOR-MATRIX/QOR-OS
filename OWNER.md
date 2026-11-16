# Where QOR OS stands

For Andrew, 15 November 2026. The Agent's per-frame notification check still fails.
The bounded Pi investigation now identifies generated texture-sampling and
fragment-shader code, which draws pixels on the processor, as the dominant
sampled work. Valid symbols explain 1.86 of 1.87 previously unknown processor
seconds, or 82.67% of the whole profile. Ten invalid symbol objects are excluded.
The owning Studio material/pass is still unknown; no image-preserving fix is
established. Profiling adds 7.4% elapsed time in one matched comparison.

Disabling only the Sun shadow does not improve responsiveness: one notification
still arrives and the wait returns later. Earlier missing-pixels controls
improve timing, but none counts as acceptance. Vulkan/lavapipe, an alternative
processor renderer, also delivers one notification. Host inventory finds a V3D
render node, an interface to the Pi graphics processor; no GPU was opened or
qualified, and the existing isolation remains unchanged.

The failing test mixes repeated-error logging with frame throughput. Its
600 ms timer request promises no frame count and can return late. A proposed
deterministic test would send six real errors in QML, Qt's interface language,
through the complete logging path and check one counted entry plus later reads. It would supplement
the unchanged natural-frame gate. The measured software-renderer stall remains
a performance issue; no assertion, deadline, quality or production change is made.

All 1,414 original source fingerprints and the original Agent executable are restored.
Original focused cases report three passes, one failure and zero skips in
13.296 seconds: `times > 5` fails with one error; wall-clock waiting passes.
Build and hardening, checks of program exploit mitigations, pass. The native
preset uses RelWithDebInfo, optimized code with debugging information, not
CMake Release. All 370 native samples exceed the unchanged 3,072-MiB (mebibyte) floor,
with at least 6,343 MiB available. No native test workers remain; the lock is free.

Next, qualify the V3D candidate in a separate sandbox exposing only its render
node against the unchanged focused workload. Keep the software-renderer
failure open. Broader acceptance remains closed; no new sanitizer or full-suite
pass is claimed. Earlier complete Agent remains 22 passes, one failure and one
optional provider skip; Studio passed 29 and the packaged-game journey passed
in 131.581 seconds. Those results retain their earlier scope.

Draft pull request #10 remains on `fix/native-play-clock-waits`. Current receipts:
`build/development-2026-11-15/session-12/`; the session-11 seal is preserved.
At the starting revision, hosted source checks passed, required Qt builds and
core audit skipped, and Release readiness failed. Full products, isolated CI,
Player frame-rate failures, the earlier Wayland memory leak, ARQADE diagnostics,
hardware and Q Pi image acceptance remain open. Consolidation planning is separate.
