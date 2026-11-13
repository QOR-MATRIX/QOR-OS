# Where QOR OS stands

For Andrew, 13 November 2026. Q Pi development is reopened alongside QOR OS.
The owner authorised committing and pushing the complete reviewed checkpoint on
`integration/owner-brief`, based on `01ba206`. It includes the tested Prism
boundary regressions, benchmark validation and documentation. Preserved local
receipts distinguish each platform's checks from the remaining release gates.

## New native core and Windows evidence

The Pi 5 builds the pinned QOR core, the Rust library reached through a C
interface, using Rust 1.98.1 and the owner's Qt 6.12.0 ARM64 kit. Its dependency
lockfile is unchanged. The serial build took 23 minutes 6 seconds. The interface
suite passes nine Qt Test results, including setup/cleanup, with no failures or
skips. Its program passes exploit-hardening checks and checks for the Pi's
16-KiB memory pages. The three approved D-Bus development packages are installed
at their exact versions. The old receipt's package-name check failed; a fresh
package audit passes without repeating installation.

The core-enabled Windows build completes in the separate `ci-windows` folder.
All 22 selected software suites pass without failed/skipped suites. Hardening
passes 33 programs; QML static analysis reports zero warnings. Tests use private
settings, offscreen software rendering, silent audio and virtual controllers.

**The warning-free gate remains open on both hosts:** Rust reports a future
compatibility warning in pinned dependency `proc-macro-error2` 2.0.1. A successful
build exit does not clear it. The fix belongs upstream, followed by a reviewed
core-pin update. No check or compiler setting was weakened.

All 51 performance-gate and bundle fixture tests now pass on native Linux with
zero failures/errors/skips, closing the 25 cases skipped on Windows. They use
inert programs and hardware metadata, not actual graphics or input measurements.

## Preserved results and next work

All 66 saved native Prism/QQ evidence files and the 1,404-file source snapshot
were reverified. The 12 November Prism Release and ASan/UBSan results (address
and undefined-behavior checks) remain seven selected suites passing per build;
leak checking was off. QQ Studio, Player and tests already compile, with six
selected non-rendering cases passing. These results were not rerun or expanded
into full product acceptance. Stale roadmap and guide claims are reconciled.

Next: resolve the upstream warning; complete native core-enabled OS and product
checks, including product sanitizers and the whole creator/player journey;
then prove runner isolation before Qt CI (automated builds) resumes. Physical
graphics, sound, controller, complete dependency/image qualification and release
gates remain open. Lumen's visual, accessibility and measured-performance rules
remain unchanged. No image, publication, media write or service change occurred.

Both original build locks are free; Qt helpers exited naturally. All-file hooks,
the working-tree secret scan and local document-link checks pass. Current
receipts and continuation: `build/development-2026-11-13/session-01/`.
Earlier native receipts: `build/q-pi-connect-2026-11-12/session-01/`.
