# Where QOR OS stands

For Andrew, 15 November 2026. The Agent frame-callback failure remains open.
The isolated Pi investigation identifies software rasterization, drawing pixels
on the processor, as the dominant first-frame cost. With the original Studio
viewport, the interval from recorded rendering to presentation is 1,037 ms.
A diagnostic that disables only rasterization reduces it to 205 ms while the
render thread's processor time remains near 205-207 ms. It delivers 24 natural
logic callbacks instead of one. This missing-pixels control is not acceptance.

The bounded call-stack profile contains 2.05 seconds of sampled processor work
across threads in its 1.125-second first-frame interval. Most hot addresses lack
symbols; profiling adds about 7.5% to the measured frame interval. Warnings and
the failed initial collector attempt are preserved. The rasterization comparison
resolves the broad cause, but no particular shader or safe production fix is
established. The private display remains a small part of the delay.

Vulkan/lavapipe, an alternative graphics interface and processor renderer,
works inside the same Pi isolation. It still delivers one callback and returns
from the 600 ms request at 1,280 ms. It does not resolve this gate or qualify
hardware acceleration. No rendering quality, assertion or deadline changes.

All 1,414 original source hashes and the original Agent executable are restored.
The original focused cases report three passes, one failure and zero skips in
13.263 seconds: `times > 5` fails with one captured error; wall-clock waiting
passes. Build and hardening, checks of program exploit mitigations, pass.
The native preset uses RelWithDebInfo, optimized code with debugging information,
not CMake Release. All 503 native resource samples exceed the unchanged
3,072-MiB floor, with at least 6,394 MiB available. No native test workers remain.

Next, resolve the hot addresses with Mesa's generated-code symbols in a
bounded diagnostic before selecting an optimization that preserves the image.
A faster isolated renderer must pass the unchanged workload; the tested Vulkan
alternative is insufficient. Broader acceptance stays closed. Production code
is unchanged, so no new regression, sanitizer or full-suite pass is claimed.
Earlier complete Agent remains 22 passes, one failure and one optional provider
skip; Studio passed 29 and the packaged-game journey passed in 131.581 seconds.

Draft pull request #10 remains on `fix/native-play-clock-waits`. Current receipts:
`build/development-2026-11-15/session-11/`; the preceding seal is verified.
At the starting revision, hosted source checks passed, required Qt builds and
core audit skipped, and Release readiness failed. Full products, isolated CI,
Player frame-rate failures, the earlier Wayland memory leak, ARQADE diagnostics,
hardware and Q Pi image acceptance remain open. Consolidation planning is separate.
