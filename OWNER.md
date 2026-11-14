# Where QOR OS stands

For Andrew, 14 November 2026. QQ, the game engine, now updates the 2D player's
visual only when it is an actual Sprite, the image used for that character.
Another scene object with a matching name stays untouched instead of being
moved or causing a script error. Seven new test cases cover valid and invalid
visuals, physics-carrier ownership and dimensions, player selection, activation,
clock interpolation, replay state and parent changes. Public property types,
named lookup and movement calculations remain unchanged.

The full native QQ build on the Raspberry Pi 5 has no compiler or license-client
diagnostics. Its mode is RelWithDebInfo, optimized with debug information. All
83 scene results pass. Four existing gameplay tests pass before and after the
fix: tiled-level platforming, top-down movement with its visual, recorded replay
and 2D triggers. Each run has six results including setup/cleanup, with no failures
or skips. Their source, assertions, tolerances and durations are unchanged.
All 83 scene results also pass offscreen under ASan/UBSan, the memory and
undefined-behavior checks, with leak detection enabled. Executable hardening,
the exploit-mitigation checks, passes 16 QQ programs and the sanitizer program.

QML, Qt's interface language, has 53 runtime lint warnings: all 14 Player2D
findings removed, none added. The whole lint gate still fails. These are hidden
software tests with virtual devices. Full product, Windows product, hardware
and isolated CI, automated-build/test, acceptance remain unproven. Required Qt
jobs remain paused. The older Store/Orb timing, Wayland display-library leak,
ARQADE diagnostics and upstream dependency findings remain open and were not
rerun for this increment.

The original build coordinators allow one heavy job at a time and retain the
3,072-MiB memory floor. The final native audit verifies 1,404 source fingerprints,
current build output, tested executable identities and sanitizer instrumentation.
Evidence, including both reproduced original failures, is in
`build/development-2026-11-14/session-05/`; earlier receipts remain in their folders.

This continuation follows pushed Player commit `8702213` on
`fix/native-play-clock-waits` for draft pull request #10. No image, package,
release or production deployment is part of this change.

Next: finish remaining QQ runtime lint and playback repairs, then complete the
roadmap's product, isolated-build and upstream audit gates.
