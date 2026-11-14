# Where QOR OS stands

For Andrew, 14 November 2026. QQ, the game engine, now moves its 3D Player only
through an actual character controller, the physics body for walking and jumping.
An ordinary node, static body or trigger volume no longer receives controller
updates. Seven new regression rows cover body types, activation, pause state,
selected-player ownership and parent changes. Public properties, movement
calculations and existing test assertions, durations and tolerances are unchanged.

The full native QQ build on the Raspberry Pi 5 has no compiler or license-client
diagnostics. Its mode is RelWithDebInfo, optimized with debug information. All
76 scene results pass. Keyboard walking, running, turning, jumping, landing and
camera behavior, virtual-controller movement/routing and trigger entry pass six
results including setup/cleanup. The same 76 scene results pass offscreen under
ASan/UBSan, the memory and undefined-behavior checks, with leak detection enabled.
Executable hardening, the exploit-mitigation checks, passes all 16 QQ programs
and the sanitizer test program. These are hidden software tests with virtual devices.

QML, Qt's interface language, has 67 runtime lint warnings: all nine Player
findings removed, none added. Shape, Trigger and Player have none; the whole
lint gate still fails. The older Store/Orb timing, Wayland display-library leak,
ARQADE diagnostics and upstream dependency findings remain open and were not
rerun here. Full product, Windows product, hardware and isolated CI,
automated-build/test, acceptance remain unproven. Required Qt jobs remain paused;
release readiness fails while required jobs do not run.

Both original single-job coordinators remain in use with the 3,072-MiB memory
floor. The final native audit verifies 1,404 source fingerprints, current build
output, tested executable identities and sanitizer instrumentation. Native
available memory stayed above 6,310 MiB during this increment's final runs.

This Player continuation follows pushed Trigger commit `9a687d2` on
`fix/native-play-clock-waits` for draft pull request #10. Evidence is in
`build/development-2026-11-14/session-04/`, including two corrected fixture import
errors and three reproduced original-runtime failures. Earlier receipts remain
in their session folders. No image, package, release or production deployment
is part of this change.

Next: continue remaining QQ lint and playback repairs, then complete the
roadmap's product, isolated-build and upstream audit gates.
