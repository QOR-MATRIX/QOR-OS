# Where QOR OS stands

For Andrew, 15 November 2026. The Agent frame-callback failure remains open.
The latest isolated Pi comparison locates the main delay in Studio's viewport
rendering with llvmpipe, the processor-based graphics renderer. Its first play
frame takes 1,056 ms between recorded rendering and presentation, consuming
1,991 ms of processor time across threads. The private display sends callbacks
10.3-10.6 ms after receiving frames and uses about 10 ms of processor time
during the 1.2-second observation. The display is not the dominant delay.

A diagnostic control suppresses only viewport pixels, retaining layout and
the live scene. The same 600 ms request then delivers 20 natural logic callbacks
instead of one, all correctly counted in the log. This is evidence of rendering
cost, not an acceptance pass or an approved rendering change. Immediate replay
is also slow and has scene-lifecycle interference; it does not establish a
shader-cache cause. No specific production defect or safe fix is established.

All 1,414 original sources and the original Agent executable hash are restored.
The two original focused cases report three passes and one failure: the log
captures one error, failing `times > 5`. The wall-clock wait case passes.
Build and hardening, checks of program
exploit mitigations, pass. The original coordinator, private device/network/
display boundaries and 3,072-MiB floor remain. Native samples stay at or above
6,359 MiB available; the lock is free and no test workers remain.

Next, capture a bounded call-stack profile of the first viewport frame in the
same sandbox to distinguish driver setup from drawing cost. If the unchanged
workload exceeds this software builder's capacity, qualify a sufficiently fast
isolated renderer for the unchanged gate. No callback injection, prewarming,
quality reduction or deadline change qualifies the original test.

Earlier complete Agent remains 22 passes, one failure and one optional provider
skip; Studio passed 29 and the packaged-game creator journey passed in 131.581
seconds under its 300-second limit. Those broader suites and sanitizers were
not rerun: production code is unchanged and the focused failure remains open.
Earlier qualified Windows checks, clean interface-language lint and remaining
Player frame-rate failures retain their recorded scope.

Draft pull request #10 remains on `fix/native-play-clock-waits`. Current receipts:
`build/development-2026-11-15/session-10/`; preceding sealed receipts are intact.
At the starting revision, hosted source checks passed, required Qt builds and
core audit skipped, and Release readiness failed. Full products, isolated CI,
the earlier Wayland memory leak, ARQADE diagnostics, hardware and Q Pi image
acceptance remain open. Repository-consolidation planning stays separate.
