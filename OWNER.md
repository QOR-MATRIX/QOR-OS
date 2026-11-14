# Where QOR OS stands

For Andrew, 14 November 2026. QQ, the game engine, now creates its initial
procedural sky once for a batch of colour changes. The new regression first
reproduced duplicate construction, then verified colours, texture ownership,
replacement and disposal. The Store playback test also waits for its own
timed-out Player to exit, removing the running-process warning.

The full optimized QQ build on the Raspberry Pi 5 has no compiler or
license-client diagnostics. All 84 scene results and all 11 world-rendering
results pass without failures or skips. All 84 scene results also pass offscreen
under ASan/UBSan, the memory and undefined-behavior checks, with leak detection
enabled. Hardening, the exploit-mitigation checks, passes 16 QQ programs and
the sanitizer program. QML, Qt's interface language, still has 53 runtime lint
warnings; that gate remains failed.

The Store test still exceeds its unchanged 60-second deadline. Separate
before/after diagnostics reach a first frame at 64.204/63.827 seconds and exit
cleanly at about 20 frames per second. This is finite software-renderer startup,
not a proven speedup or a passing Store gate. These hidden software checks use
virtual devices and a private network; full product, Windows, Q Pi image,
physical hardware and full OS acceptance remain open.

Both registered Qt runners are offline and their isolation-enabling variables
are absent. CI, automated build and test, stays paused for Qt. There is no local
provisioning/proof record for the proposed isolated virtual machine, and this
terminal lacks its administration access. The original build coordinators and
3,072-MiB free-memory floor remain mandatory.

The continuation is for draft pull request #10 on `fix/native-play-clock-waits`.
Current evidence is in `build/development-2026-11-14/session-06/`; prior receipts
remain preserved. Next: profile the renderer startup cost and satisfy the Store
deadline, then the Orb timer, remaining product, lint, isolated-build and
upstream-audit gates. The earlier Wayland display-library leak remains open.

QOR-0028 records the owner's long-term direction: one canonical development
repository for QOR OS, Q Pi, Demiurge Chain, ARQADE and associated systems.
A separate evidence-based consolidation proposal follows a reviewable milestone.
Current history, licences, source ownership, security and deployment boundaries
remain in force; this milestone authorizes no migration or restructuring.
