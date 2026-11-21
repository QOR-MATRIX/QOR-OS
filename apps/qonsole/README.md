<!-- SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary -->
# Qonsole standalone local slice

The [authoritative roadmap and capability matrix](../../docs/ROADMAP.md#alongside-the-phases--qonsole-device-edition)
record the current Windows baseline, next source-provenance/staging requirements
and separate real-controller/console gates. Qonsole is an edition of one QOR OS,
not a replacement for its C++/Qt desktop or developer ecosystem. Demiurge/QOR
ID/Auth integration code and QPi remain on hold.

An original QOR OS launcher with a dedicated C++/Qt executable and the same
Qt Quick Home/sample UI. Its independent CMake subproject uses the repository's
compiler settings and hardening; it does not configure Prism, QOR.Apps, simulated
services, the core or product processes. The source-preview route still works.
It is not added to the main CMake build or the shell's application registry.

The Home page has Open sample and Exit, with an initial visible focus ring.
Arrows move selection; Enter, keypad Enter or Space confirms; Escape returns
from the local sample to the same Home choice. Back on Home stays there.
The original geometric sample lets arrows move a tile and confirm change its
colour. No game or external application is launched.

`Launcher.dispatch(action)` accepts discrete `left`, `right`, `up`, `down`,
`confirm` and `back` actions. Keyboard behavior is unchanged. The Windows host
now has a foreground-only Raw Input source path for original Sony DS4 USB-shaped
reports (VID 054c, PID 05c4/09cc, report 01, 64 bytes), using QOR's existing
PlayStation parser. Cross maps to confirm, Circle to back and the D-pad to
navigation. It invokes this action seam directly; it does not emulate a keyboard
or controller. It opens no HID device and sends no output/light/rumble commands.
No full gamepad manager, polling loop or new third-party dependency is added.

The reader requires an active, visible foreground Qonsole window and first
observes neutral navigation controls after activation. Held/repeated reports and
releases do not act; a fresh press after release does. Known device masks merge
before edge detection, and cancel wins over simultaneous confirm. Each report
rechecks activity; focus/visibility/application changes and removal reset the
latch. Report blocks are bounded before dispatch. This is edge latching, not a
time-based debounce claim. Bluetooth, other device IDs, DualSense, axes and
physical timing/reconnection qualification are outside this narrow change.

**Source/fixture result only:** the owner reports that Windows recognized a wired
DS4 and button 2, but physical Cross did not open the sample in the old standard
EXE (`b474280e...`); Circle was not tested. That exact preserved EXE contains no
adapter. New source and normal-host/native-reader objects compile; nine private
fixture functions pass (11 Qt results including setup/cleanup), using in-memory
reports and the actual QML dispatch/focus/keyboard path. No OS controller is
created or impersonated. The new application is not linked/launched in this
slice, and physical retesting requires a separate coordinated session and a new
candidate binary. Evidence: `build/development-2026-11-20/session-03/`.

## Current controller candidate, 20 November 2026

A separate normal Release build with `QOR_QONSOLE_VERIFICATION=OFF` and focused
fixtures ON produces `build/qonsole-windows-controller-release-retry-01/qor-qonsole.exe`,
SHA-256 `6de2f01db418afdbd742f8472e62470ec160dd7c2002ca036eeebc6a874dca69`.
Its compile records exclude verification-only sources/defines. Eleven Qt results
pass and hardening reads two programs with zero shortcomings. Ordinary external
Windows Enter/Escape events open the bundled sample and return Home with the
same accessible `openSample` ID. Native foreground ownership is observed; no
physical controller input was sent or requested. Underlying object identity is
not proved by the external accessibility observation. The window exits 0.

This supersedes the earlier source-only limitation below, retaining that evidence.
The original `b474280e...` executable and private placements remain unchanged.
Thirty admission samples have minimum 4,000 MiB; 360 live observations have
minimum 3,627 MiB against the unchanged 3,072 floor. Receipt:
`build/development-2026-11-20/session-06/`. Actual Raw Input registration readback
is unavailable in the current normal path; foreground ownership alone does not
prove registration/delivery. Physical Cross/Circle/D-pad and reconnect remain
unverified, as do clean-PC, Linux, distribution, CI/Release and console gates.

## Dedicated local Windows executable

The commands/results below describe the earlier verification-enabled milestone.
Preserve its build and the standard `build/qonsole-windows-normal-release`
baseline; use a fresh explicit `-B` for any later controller candidate. The
current source change has not replaced either verified executable. Verification
flow mode deliberately returns before native reader registration.

Use the existing QOR job coordinator, 3,072-MiB guard and an admitted serial slot
before building. Load the existing MSVC/Qt environment with
`tools/dev/Enter-QorShell.ps1`. From `apps/qonsole`, the scoped commands are:

```powershell
cmake --preset windows-verify
cmake --build --preset windows-verify --target qor-qonsole --parallel 1
```

This uses installed Qt 6.12.0, CMake/Ninja and MSVC only, writing to
`build/qonsole-windows`. It embeds the unchanged `Main.qml` and `Launcher.qml`
under `qrc:/qt/qml/QOR/Qonsole/`. No QML cache C++ is generated. The generated
module metadata and build-tree `qt.conf` are recorded with the build inputs.
No Qt runtime deployment, installation or package rule is provided.

`build/qonsole-windows/qor-qonsole.exe` can run normally with the installed Qt
runtime on PATH. The `windows-verify` preset is **Release with local verification
controls enabled**, not a distribution build. `QOR_QONSOLE_VERIFICATION` defaults
OFF outside this preset; that separate configuration has not been built/tested
in this milestone. Enabled controls are `--verify-flow <existing-absolute-dir>`
and `--verify-missing-qml`, mutually exclusive. The former injects Qt events and
writes new captures; the latter deliberately loads a nonexistent owned QML type.
Normal QML root creation failure exits nonzero in either configuration.

The exact local executable (42,496 bytes) has SHA-256
`a3a4943bb14ac904ef0279790be50b881de64a174b1d3b5c3d18fe69c59ea3c2`.
It launched once from a genuinely empty working directory: visible identity,
Home → original bundled sample → Back restored the same `openSample` item at
index 0 with fresh frames 1 → 2 → 3. Exit was 0. Its missing-QML control returned
1 with an actual engine load error; the PE hardening check passed. This is
injected input inside the actual executable, not a Qt host standing in for it.
Exact commands, binary/build/resource input hashes and captures are retained in
`build/development-2026-11-19/session-10/`. No physical controller was touched.

## Source-preview route on the existing Windows kit

From the repository root, using the installed kit without building or installing:

```powershell
$qorPreviousBackend = $env:QT_QUICK_BACKEND
try {
    $env:QT_QUICK_BACKEND = 'software'
    & C:/Qt/6.12.0/msvc2022_64/bin/qml.exe -f "$PWD/apps/qonsole/Main.qml"
} finally {
    $env:QT_QUICK_BACKEND = $qorPreviousBackend
}
```

For this preview route, the runnable **source payload** is `Main.qml` and `Launcher.qml` in this directory,
with their SPDX notices. Both must remain together. There are no image, sound,
network or model inputs. The source requires an existing entitled Qt 6.12 runtime
with Qt Quick and a working GUI platform plugin; it is not a self-contained EXE
or a redistributable Qt runtime bundle. The dedicated executable also relies on
the existing runtime rather than bundling Qt. The normal Windows window uses the
system font. No application version, package identity or PS4 title ID is assigned
by this source-only slice.

## Payload inventory, not a package

The existing `build/development-2026-11-19/session-08/staging-metadata-draft.json`
is reconciled in place. It retains the earlier loose-source proposal separately
from the new executable and embedded QML resource mappings. No content has been
staged. Version, package identity and icon stay unset; the Qt application name
is now `qor-qonsole`. The sample remains original inline QML geometry. Qt's own
resources/dependencies have an incomplete inventory and are not our assets.

The earlier source-mode **Qt launch host**, distinct from the new Qonsole EXE, is
`C:/Qt/6.12.0/msvc2022_64/bin/qml.exe`, SHA-256
`58dc1a370298e797b67164c02683eaeba59b09d759d19ea9c3328b585aa7bd05`.
Its current bytes match session-07. The draft also records the verification
host, exact source/test-font/notice hashes, base revision and uncommitted source
identity. Qt's kit supports debug and release; the prior QtTest runtime was
shared release/MSVC 2022. That documentation-only result is historical; session-10
records the separate application build: CMake 3.30.5, Ninja 1.12.1, MSVC
19.44.35228.0, Release, warnings-as-errors/hardening ON and local verification ON.

## Focused tests

From `apps/qonsole`, the isolated C++ fixture target is opt-in, with helpers OFF:

```powershell
# Existing MSVC/Qt environment, admitted serial slot; fresh directory only.
cmake --preset windows-verify -B ../../build/qonsole-controller-fixtures -DQOR_QONSOLE_VERIFICATION=OFF -DQOR_QONSOLE_BUILD_TESTS=ON
cmake --build ../../build/qonsole-controller-fixtures --target tst_qonsole_controller --parallel 1
```

Run that test EXE with process-only `QOR_GAMEPADS=off`, offscreen/software Qt,
QML disk cache disabled and existing `kit/fonts` as QT_QPA_FONTDIR. The fixture
does not link/instantiate the native reader or a gamepad manager. It checks
Cross/Circle data, bounds, edges/neutral rearming, inactive input, one action per
report, actual QML dispatch/focus and unchanged Qt keyboard behavior. This is
not physical input proof. The production `main.cpp` and Windows reader were
compiled separately to objects without linking or launching Qonsole.

`tests/qonsole/tst_navigation.qml` loads the actual Launcher component and sends
Qt key events, plus explicit adapter-action fixtures. It covers initial focus,
arrows and boundaries, confirm/back, tile movement/interaction, five repeated
returns, explicit exit and unknown actions. These are automated input fixtures,
not physical controller tests.

Run the installed `qmltestrunner.exe -input tests/qonsole -o <new-log-path>,txt`.
For hidden software checks, set `QT_QPA_PLATFORM=offscreen`,
`QT_QUICK_BACKEND=software`, `QML_DISABLE_DISK_CACHE=1` and
`QT_QPA_FONTDIR=<repository>/kit/fonts` for the test process only. The last value
uses existing OFL fonts instead of the offscreen plugin's absent default font
directory; the licences remain beside the fonts. Inspect the actual Qt log and
require all nine results (seven cases plus init/cleanup), with zero failures,
skips and warnings. A process exit with empty captured stdout is insufficient.

Windows Qt 6.12 verification and a bounded native Windows entry-window smoke
are recorded with source/tool hashes under
`build/development-2026-11-19/session-07/`. Session-08 adds one native Windows
entry-window flow with injected Qt Return/Escape events: `openSample` →
`samplePage` → the identical `openSample` object at index 0, with fresh frames
at each phase (three Qt results pass, no failures/skips/warnings). The original
seven-case suite was not rerun for that documentation reconciliation. Current
host: Windows 11 Pro Insider Preview 10.0.29708, 64-bit; PowerShell 7.7.0;
Qt/QtTest 6.12.0 x86-64 MSVC 2022 release, Windows QPA, software rendering.
Physical controller input remains untested. No Linux, PS4/PS5, firmware, packaging,
game overlay, Media/Settings, device or complete Qonsole milestone acceptance
is established by this local slice.
