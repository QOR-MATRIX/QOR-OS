# Where QOR OS stands

For Andrew, 14 November 2026. QQ, the game engine, now reads a trigger's
occupant count from its actual physics volume. Triggers detect objects entering
an area without blocking them. An ordinary object can no longer supply a false
count. Box and sphere ownership, moving between parents, live dimensions and
optional entity callbacks have regression coverage. Public properties and all
existing test assertions, durations and tolerances remain unchanged.

The full native QQ build has no compiler or license-client diagnostics. Its
actual mode is RelWithDebInfo, optimized with debug information. All 69 scene
results pass. Falling bodies, player entry, grouped movement and 2D triggers
pass six results including setup/cleanup, without failures or skips. All 69
scene results also pass offscreen under ASan/UBSan, the memory and
undefined-behavior checks, with leak detection enabled. Executable hardening,
the exploit-mitigation checks, passes 16 QQ programs and the sanitizer program.
These are hidden software tests with virtual devices.

QML, Qt's interface language, now has 76 runtime lint warnings: seven removed,
none added. Shape and Trigger have none, but the whole lint gate still fails.
The earlier Store/Orb timing, Wayland display-library leak, ARQADE diagnostics
and upstream dependency findings remain open and were not rerun here. Full
product, Windows product, hardware and isolated CI, automated-build/test,
acceptance remain unproven. Required Qt jobs remain paused; release readiness
failed at the preceding pushed checkpoint because required jobs did not run.

The original Windows coordinator, the single-job lock wrapper, was restored
byte-for-byte at its original path after idle-worker checks. Independent lock
exclusion and exit-status propagation pass. Both hosts retain the 3,072-MiB
memory floor. The final native audit verifies 1,404 source fingerprints, current
build output, tested executable identities and sanitizer instrumentation.

The Shape fix is pushed as `b8a5afd`. This Trigger continuation builds on it on
`fix/native-play-clock-waits` for draft pull request #10. Evidence, initial
failures, coordinator recovery and Git checks are in
`build/development-2026-11-14/session-03/`; Shape evidence remains in session-02.
No image, package, release or production deployment is part of this change.

Next: continue remaining QQ lint and playback repairs, then complete the
roadmap's product, isolated-build and upstream audit gates.
