# Where QOR OS stands

For Andrew, 14 November 2026. QQ, the game engine, now keeps each private
collider and body factory with the shape that created it.

Playing and 2D/3D state still come from the immediate parent, whether it is a
scene, group or prefab (a reusable group). The new automated check covers live
changes, reparenting, detachment, restored movement restrictions and the same
body's identity. The text "true" cannot enable it. Public fields and all existing test assertions, durations
and tolerances are unchanged.

The full native QQ build is clean of compiler and license-client diagnostics.
Its actual build mode is RelWithDebInfo, optimized with debug
information, despite “release” in the cache directory's name. All 58 scene
results pass. Contact reporting, grouped motion and the three 2D plane-lock
rows pass seven results including setup/cleanup, without failures or skips.
ASan/UBSan, the memory and undefined-behavior checks, also pass all 58 scene
results offscreen with leak detection enabled. Executable hardening, the
exploit-mitigation checks, passes 16 QQ programs and the sanitizer test program.

QML, Qt's interface language, now has 88 warnings in the runtime's static checks:
14 removed, none added. Its warning gate still fails. Earlier Store/Orb timing failures,
the 96-byte Wayland display-library leak and ARQADE diagnostics remain open;
those suites and the Wayland sanitizer run were not repeated here. Full
products, hardware acceptance and qualified CI, automated builds/tests, remain
unproven. These checks use isolated software graphics and virtual devices.

The branch retains your approved six-wait software-clock patch and the separate
loaded-game property fix. Draft pull request #10 remains unmerged. The other
13 QOR checkouts were clean when checked. Upstream draft #118 stays at
`846d9ba` with its dependency audit failed; the fixed core-library revision remains
`6a4822d`. Both Qt CI workers remain offline. No gate was weakened.

The original job locks and 3-GiB memory floor remain. No unrelated service,
production deployment, device image, package or release was changed or published.
Current evidence is in `build/development-2026-11-14/session-01/`, including
the intermediate context-only result and the final parent-state correction.
Earlier scene and timing evidence remains in the 13 November session folders.

Next: resolve the remaining lint and playback findings, complete product
acceptance, qualify isolated CI workers and clear the upstream audit. The
roadmap remains the sole record of those gates.
