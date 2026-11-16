# Building and running QOR OS

Use Python 3.12 or later for repository tooling, matching the hosted CI baseline.
The [offline developer guide](../sdk/README.md#offline-discovery) can identify
source, prerequisites and action boundaries before installing a Qt toolchain.

QOR OS has two kinds of build:

- **The OS**: the CMake presets below. They build the Kit, services, shell,
  apps, compositor, tools, and the QOR core from demiurge-chain at its pinned
  commit.
- **The products**: QQ, ARQADE and QOR Media, each built by its own script,
  as it was upstream ([§6](#6-the-products-qq-arqade-qor-media)).

Normal shell builds check the approved startup rendering against its original logo
sources. The optional `qor_boot_payload` target stages the image's branding files
inside this checkout's `build/`; it does not build a bootable image, install a theme
on the host or regenerate an initramfs. Commands and remaining boot evidence are in
[image/README.md](../image/README.md#branding-the-startup-stages).

`qor-setup`, `tst_setup` and `qor_setup_qmllint` build and check the private first-run
app. Simulator builds expose development captures; device builds omit those options.
`qor-setup --build-capabilities` reports that distinction without graphics or user-state
access. `qor_image_payload` stages 25 checked image-data files under `build/image-payload`;
it refuses different existing content. Use a fresh output folder for changed inputs.
The data-only Containerfile can verify packaging with existing Docker; the device recipe
needs a separately approved existing device base and runtime. Neither is an installer ISO.
Exact commands and missing prerequisites are in [image/README.md](../image/README.md).

The private disposable-VM work is separate from that data target and the full
device recipe. `tools/image/build_vm_probe.py` builds an explicitly incomplete
UEFI live-display ISO from an exact local tooling image and a host-built runtime;
`tools/image/vmguard.py` and `vmrun.py` constrain its disposable raw disk and
private firmware variables. The opt-in Linux-only `QOR_BUILD_VM_INSTALLER`
requires `QOR_BUILD_SIMULATOR=OFF`; it is development work, not a readiness flag.
The earlier clean `feed626` image and its same-revision runtime pass actual stock
installation, media-free welcome and persistence through two installed reboots.
The host launcher is recorded separately by checksum; its QEMU main-loop I/O
pool is limited to four workers within the unchanged 64-task container bound.
Normal review/Cancel preserves the entire 16-GiB disk hash, and a separate
corrupt-payload image is refused without changing its test disk. Retry stays disabled.
SELinux pairs the existing `install_exec_t` executable label with the service's
`install_t` process domain. The packed executable label and unit are checked;
normal installation succeeds with enforcement enabled. The five actual VM phases
exit 0 across preserved resource interruptions, without resetting the installed disk.
That persistence sequence uses the restricted guest agent. Fresh actual UI reboot
tests on feed626 and 5b7d321 failed after successful installation. A diagnostic found
bootc's storage bind mount still active after exit 0; removing that confirmed mount
makes the unchanged strict guard and real reboot button succeed. The local backend
now runs bootc in a private mount namespace (a process-local filesystem view) and
requires the strict guard again before reporting completion. The separate collector
correction distinguishes unconfigured loop nodes from backed storage. The fresh clean
`66df3f7` image and same-revision compiler now pass the normal route on a new 8-GiB
disk: stock installation, actual UI reboot, media-free welcome completion and one
installed reboot retaining that record. All three native VM phases exit 0 with three
distinct boot IDs, without manual cleanup or debug overrides. The final verifier
accepts `build/vm-ui-reboot-2026-11-08/isolated-ui-proof/accepted-isolated-ui-proof.json`;
the earlier `feed626` proof and both failed UI attempts remain separate and unchanged.
The installation candidate requires a clean source revision and a fresh runtime
from that same revision. Every referenced offline OCI (content-addressed container
image) blob is checked on read-only media before stock bootc can write the assigned
disk. Virtual-port readiness starts the services; existing disk and RPC guards remain.
The limited welcome uses a locked local test account and supplies neither QOR ID
authentication nor the full desktop. Diagnostic overrides establish only their
explicitly recorded scope.
The full device-layer check additionally probes both `qor-setup --build-capabilities`
and `prism --build-capabilities`; a file named `prism` is insufficient. Prism's query
runs before graphics, audio, services or user state are initialised and reports its
compiled simulator mode and authentication availability. Current device builds
report `development:false, authentication:false`: the real authentication backend
and service daemons remain unfinished, so the full device layer is refused. The
structural test's future authenticated contract is an inert fixture, not a working
sign-in or device-session result. Missing probes, unknown fields and numeric values
in place of capability booleans are also refused.
Run the Windows lab memory preflight before its runtime/image pipeline:
`python tools/image/vm_resources.py`. Image phases and VM launch also check
the 3-GiB available-RAM budget and refuse to start below it. This is a read-only
resource check, with no process stopping, host setting change or retry loop.
See the [VM lab boundaries](../image/README.md#private-disposable-vm-lab),
including the accepted private evidence and remaining device/release gates. Do not install a
Qt kit in the VM or run an image/partitioning command against workstation disks.

### Q Pi preparation

On 14 November, direct command access was restored across networks through
Tailscale. The existing restricted SSH transport retains its saved key and
strict host verification using `HostKeyAlias=qpi-builder.local` for the private
network address. This is a verified connection to the existing builder, not
first-boot provisioning or an image acceptance result. Session-10 connection
receipts and session-11 execution evidence are under
`build/development-2026-11-14/`; credentials and sign-in links are excluded.

The native precomputed-sky diagnostic exports the default procedural texture
through Qt 6.12's public API and bakes it with the installed Balsam tool. Its
helper builds cleanly in a separate cache and passes hardening. Both minimal
scenes pass QML lint without warnings and use llvmpipe in the existing private
display/device/network namespaces. First-frame times are 61.529 seconds for
procedural lighting and 0.774 seconds for precomputed lighting, with identical
1280-by-720 image hashes and zero differing pixels. The original 30-sample
resident admission and continuous 3,072-MiB guard are unchanged; 1,404 source
hashes and the free lock afterward are verified. The initial component-loading
failure and its local-URL/bounded-wait correction are both retained in
`session-11/`. That diagnostic changed no production source and did not rerun
the Store acceptance case; production integration follows below.

The next native run embeds precomputed lighting for both palettes authored in
the bundled scenes, matched by their four colour values. Edited colours retain
procedural generation; explicit sky images retain precedence. The checked-in
EXR inputs, KTX textures and manifest are described in the
[QQ build guide](../products/qq/README.md#build-test-deploy). Normal application
builds only embed the reviewed textures; they do not launch the baker.

With `QOR_QT_DIR` set to the installed commercial ARM64 kit, `q-pi-release`
configures and builds all QQ targets without compiler/license diagnostics.
All 85 scene results and 11 world-rendering results pass; the unchanged
Store-installed Player suite passes three results in 8.371 seconds under its
original 60-second deadline. The `q-pi-asan` preset reuses the established
sanitizer cache with its `RelWithDebInfo` build type; all 85 scene results pass
offscreen with address/undefined-behavior sanitizers and leak checking enabled.
Hardening accepts 16 QQ programs and the sanitizer program. Runtime lint still
fails at exactly the previous 53 findings, with none added or removed. The first
configure attempt's missing-kit-environment refusal is preserved alongside the
corrected run in `build/development-2026-11-14/session-12/`. Final reconciliation
verifies 1,409 native source hashes, all four test binaries, sanitizer linkage
and the free original lock, with no remaining QOR workers. The 30-sample resident
admission and continuous 3,072-MiB guard remain unchanged; the two integration
runs stay above 5,537 MiB available. Windows heavy checks
remain stopped at a 2,501-MiB snapshot; all-file hooks and full product, Windows,
hardware and release acceptance remain incomplete.

The next Orb Run investigation uses the same native preset and private software
display. Its canonical case still fails startup; separate diagnostic and timing
proposal builds are preserved under `build/development-2026-11-14/session-13/`.
The complete revised proposal passes three Qt Test results in 184.714 seconds,
with no compiler/license diagnostics and hardening passing 16 programs. It
changes software wait semantics; [the CI guide](CI.md#what-linux-runs) gives the
exact limits. The original test source and build were restored before approval.
The owner subsequently approved the exact four-change proposal. Its applied
source, complete 1,409-file native manifest and rebuilt executable match that
passing run. The acceptance rebuild and hardening pass; the identical case was
not rerun. Integration receipts are in `session-14/` beside the probe. An initial local UTF-8
decoding error and the first proposal's remaining orb-replacement failure are
preserved. Windows now measured 2,791 MiB available, so local heavy checks and
all-file hooks remain incomplete; no unrelated services were changed.

The subsequent complete QQ run finds the bundled Player's separate HDR sky
still preparing its lighting during launch. Qt Balsam now precomputes that
lighting under the same native coordinator and isolated software renderer.
`products/qq/player/playground/assets/dusk-lighting.json` records its retained
Apache-2.0 source image, generator and hashes. Normal builds embed `dusk.ktx`;
they do not invoke the baker. The 1280-by-720 comparison is pixel-identical,
with first frames at 61.384 seconds from the HDR and 0.762 seconds from KTX.
The real Player's first frame is 5.279 seconds and its capture passes, resolving
the two original 60-second launch timeouts. The full Player run still fails
the unchanged 20-FPS floor at 0.5 and 18.9 FPS, with one unavailable-Landlock
skip. These are functional startup results, not real-time or kernel-confinement
acceptance. Original failures and the comparison are retained in
`build/development-2026-11-14/session-15/`.

That continuation completes all remaining native QQ suite attempts. Gameplay,
lockdown, publishing, Player network confinement, Store launch, Studio, workspace
and game builds pass completely; the asset library passes with its optional
benchmark skipped. The real Studio creator journey passes, as does the separate
Prism Library journey in 202.406 seconds under its unchanged 300-second limit.
The latter records launch arguments with the test launcher, then runs the real
Player on its private display. It does not prove hardware rendering in Prism.
Ordinary suites retain 5,400-second limits and Qt functions 1,800,000 ms. Builds
and hardening pass, but the agent's short log-timing check, the two Player frame
rates, unavailable Landlock support and the same 53 lint findings prevent full
QQ acceptance. Scene/world passes are retained by current binary fingerprints.
The final reconciliation verifies 1,412 native source files and 15 executables;
the original lock is free and no test worker remains. The complete suites never
fall below 4,923 MiB available. Windows remains below 3,072 MiB, so local heavy
checks and all-file hooks are still incomplete.

The 15 November Game interface follow-up uses the same `q-pi-release` build,
original coordinator, 30 resident-worker admission samples and continuous
3,072-MiB guard. Public scene/follower handles stay generic; private dynamic
accessors preserve compatible scene nodes. Baseline and candidate interface
checks each pass seven results, then the complete scene suite passes all 87
normally and under `q-pi-asan` offscreen with leak checking enabled. The latter
retains its established `RelWithDebInfo` cache. The complete native gameplay
suite also passes all 58 results under its unchanged standard limits. Store
launch passes three, and the packaged-game creator journey passes in 201.985
seconds under its registered 300-second limit. Player retains 13 passes, two
FPS failures (0.5 and 18.9 against 20) and one unavailable-Landlock skip.
All 24 Game lint findings are
removed; the other 29 warning lines are unchanged, so lint remains a failed
gate. No rendering quality, assertion or approved gameplay boundary changes.
Diagnostic traces of the unchanged Player and agent concentrate cost in software
rendering and synchronization; instrumentation timings do not replace ordinary
acceptance results. Receipts are in `build/development-2026-11-15/session-01/`.
The separate ARQADE caller rebuild exits zero but emits its existing Qt
`GuiPrivate` compatibility warning. Its strict build gate fails and caller tests
do not run; no diagnostic suppression or timing change is applied.

The following Behaviour increment retains that coordinator, isolation and
resource policy. Six focused results pass before and after the direct-accessor
change, and all 89 scene results pass normally and under the same offscreen
sanitizer configuration with leak checking. Eleven Behaviour lint findings
are removed with none added; the other 18 warning lines remain. Existing
gameplay sources, assertions and suite limits are unchanged. Complete gameplay
passes 58, Store launch three, and the packaged-game journey passes in 201.377
seconds under its 300-second limit. Agent reproduces its prior log-timing
failure; Player retains both FPS-floor failures and the Landlock skip. The
brief Tailscale outage interrupted the local polling queue; the original native
guard and test process continued, and the queue resumed against that same run.
All 1,966 resource samples stay at or above 5,427 MiB available. Final readback
verifies 1,412 native sources, eight executables, no remaining test workers and
the original lock free. The receipts are in `build/development-2026-11-15/session-02/`.

Start with the canonical [Pi 5 bootstrap procedure](../image/README.md#pi-5-builder-bootstrap-before-q-pi):
stock Raspberry Pi OS 64-bit Desktop Trixie establishes a possible native builder;
it is not the eventual Q Pi image. The current first-boot preparation is headless
Wi-Fi and key-only SSH, using the versioned Trixie `cloudinit-rpi` interface and
per-device provisioning; USB/HDMI is optional recovery. A unique account and normal
login with both autologin modes off remain required. Generic remote access stays
disabled; real provisioning/access needs separate action-time approval. The owner
authorises the eventual qualified Q Pi image
on the identified Samsung EVO 128-GB card, including its old Kali/data erasure;
the [action-time write gates](../image/README.md#authorised-q-pi-target-and-action-time-write-gates)
retain fresh capacity/USB identity and non-system/non-boot checks, image validation,
irreversible-erasure confirmation and completed-write verification. Stock bootstrap,
credentials and security/access changes remain separately gated. No write is ready.

`python3 -B tools/image/pi_preflight.py --bootstrap-only` observes bounded Pi 5
model/Trixie metadata, native ARM64/64-bit userspace, actual 16-KiB pages and the
existing 3-GiB RAM guard without Qt, Git or package-manager execution. Exit 0
is scoped to those observations; `builder_qualified` remains false. The guide
contains the read-only package-delta simulation, Qt entitlement/module questions,
single-card recovery boundaries and measured budget requirements. The current
Windows heavy wrapper remains unchanged. The repository's
[native coordinator](../image/README.md#native-linux-heavy-job-coordination)
adds a permanent per-account OS lock, incomplete-run marker and guarded RAM
admission. The 11 November resource/coordinator pass and subsequent unresolved
hostname diagnostic are historical; authorised key-based access was restored
on 12 November without weakening strict host checking. Initial trust remains
owner-approved first use, not independently verified identity.

The Pi now has CMake 3.31.6, the owner's Qt 6.12.0 Linux ARM64 kit and 18 reviewed
distro development packages; the latter changed no existing package versions
and removed none. `tests/pi/` built and passed both native parser configurations
(23 Qt Test results each, four hardened programs). Native resource/tooling checks
also recorded 196 passes and two wrong-host skips. These are distinct scopes.

The 12 November Prism snapshot built serially with warnings as errors, hardening
and the native 16-KiB gate. A test-only correction explicitly activates a second
client through the Shelf path instead of assuming it may steal focus. Corrected
Release passes all seven selected headless suites without skips; 11 programs
pass hardening/layout checks and Release QML lint has zero warnings. Native
ASan/UBSan also passes the same seven suites without skips or sanitizer diagnostics;
all 11 sanitizer programs pass hardening/layout checks and their instrumentation
is verified. The existing preset disables leak checking. Both configurations
exclude the QOR core. Evidence, exact source overlay and initial failures:
`build/q-pi-connect-2026-11-12/session-01/`.

On 13 November the pinned core and Corrosion fetches configured through
`ci-linux` in a separate native cache with the existing Pi kit/page gate.
Rust 1.98.1 built the core's `desktop` features serially with its Cargo.lock
unchanged. `tst_qorcore` passes nine Qt Test results without failures/skips;
the executable passes hardening and ARM64/16-KiB checks. Tests use private
HOME/XDG/TMP paths and disconnected buses/audio. The `proc-macro-error2` 2.0.1
future-compatibility warning remains, and only the core bridge target was built.
Full native OS/product and warning-free acceptance remain open. Qt helpers
exited naturally and the original coordinator returned cleanly. Receipts:
`build/development-2026-11-13/session-01/native-core-20261113/`.

The later 13 November continuation built the complete core-enabled development
configuration in that cache. After correcting missing `linux-arm64` metadata in
the signed demonstration fixtures, all 27 non-GPU suites pass without failures
or skips. Hardening passes 42 programs; 44 linked executables/libraries pass the
ARM64/16-KiB layout check; QML lint has zero warnings. The generated Corrosion
linker-library probe's relocatable object is not a product executable; the first
artifact-selection harness mistakenly selected it, and that failed receipt is
retained. QQ's complete scene-data and Player sandbox suites also pass (56 and
three Qt Test results, including setup/cleanup). These results retain the Rust
dependency warning and do not close full product, graphics or image gates.
Commands, source overlay and original failures:
`build/development-2026-11-13/session-02/`.

The next continuation passes the complete native QQ publishing suite: **11 Qt
Test results, no failures or skips**, including both Studio window cases, in
133.315 seconds. The existing binary ran on a private Wayland display with Mesa's
llvmpipe software renderer and two rendering workers, matching the Linux harness's
half-CPU default on this Pi. Test assertions and Qt's watchdog were unchanged;
the outer worker allowed 900 seconds. Earlier single-worker, 180-second diagnostic
runs and a debugger interruption are retained as incomplete results.

Bubblewrap 0.12.0 provided separate mount/process/network/user namespaces, a fresh
device directory, private `/tmp` and `/run`, and read-only source with only the
evidence directory writable. GPU, sound and input device paths were absent;
networking contained loopback only. The original native coordinator stayed in
charge, and the resident worker monitored the unchanged 3,072-MiB floor through
the run (minimum observed: 6,956 MiB). This is isolation of trusted product tests,
not qualification of a CI runner or a sandbox for untrusted applications.

The 1,404-file source snapshot was reverified. Its 187 QQ and 203 Kit files also
match the current tree. A later comparison of the four Prism compositor byte
differences found only CRLF versus LF line endings; their source text matches
after newline normalization. Both raw hashes are retained in session-06 evidence.
Prism remains outside this standalone publishing test's scope; its earlier
seven-suite Release/sanitizer evidence is described above. The original coordinator returned cleanly. Source,
binary and evidence hashes, commands and failed attempts are preserved under
`build/development-2026-11-13/session-05/native/`. This closes the publishing-suite
software check only; full products, sanitizers and the complete creator/player
journey remain required.

For QOR Media, `tools/dev/products.sh` selects the native Linux x64 or ARM64
PDFium archive from the existing pinned `chromium/8111` release. Each has its own
SHA-256 and extraction directory; unsupported architectures and failed extraction
refuse before CMake configuration. A successful-extraction marker prevents reuse
of a package whose entry point was written before an unpack failure. The old
shared cache is left untouched. This helper is for native builds, not cross builds.
The [official ARM64 asset](https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium/8111)
was downloaded and matched to digest
`042de247138f96341534cccf24ed21c2550a7245b936bb4a60d2340120000f05`;
its `libpdfium.so` is 64-bit AArch64 with 65,536-byte load alignment, compatible
with the selected 16-KiB page check. It was neither extracted nor executed.
The current native kit lacks `Qt6Pdf` and `Qt6PdfQuick`, so this is dependency
preparation, not a compiled or tested QOR Media application. Evidence is in
`build/development-2026-11-13/session-06/pdfium-arm64-review.json`.

The installed kit's bounded ELF checks and these software-only runs do not certify
all dependencies, commercial redistribution terms, real graphics/audio/input or
a measured application/image scratch budget. Enterprise entitlement is reported
by the owner; Trixie is not Qt's listed Pi reference OS. Ubuntu 24.04 remains an
unadopted reference alternative. The original Windows/WSL guard remains unchanged.

The Pi 5 ARM64 development edition is scoped by
[QOR-0025](decisions/QOR-0025-q-pi-development-edition.md). QQ now has native ARM64
`q-pi-release` and `q-pi-asan` configure/build/test presets in
`products/qq/CMakePresets.json`. They build Studio, Player and their tests, keep
hardening and warnings as errors, and link for the 2712 kernel's 16-KiB pages.
The presets reject x86-64 hosts before compiler or Qt-tool execution. The native
Release preset compiled Studio, Player and tests on 12 November without warnings;
16 programs passed hardening/layout checks and six selected non-rendering cases
passed. Full QQ, sanitizer-product and graphics acceptance remain open. Follow the
[native application procedure](../image/README.md#native-qor-application-build)
before using them; they neither install tools nor compose an image.

On the native host the presets also run the bounded, read-only
`pi_preflight.py --qt-only --qt-target <kit>` check before normal Qt package
discovery. This requires regular root config/version entry points, matching
literal Qt6/Core 6.12 versions, Linux desktop target metadata and Qt Core's
ARM64/16-KiB shared-library layout. Conflicting normal, cached or environment
package hints are refused; `Qt6_DIR` names the checked root config directory.
It does not execute
the kit's CMake toolchain or prove entitlement, other modules or binary
compatibility. An explicitly supplied CMake toolchain runs before CMake's
top-level includes and is outside this early gate's scope.

The [explicit payload audit](../image/README.md#static-target-payload-audit)
checks selected apps and dependencies using their supplied SHA-256 manifest,
without executing them or searching the host's libraries. It now validates GNU
version-label chains/hashes and requires exact agreement with each unique
dependency candidate, conservatively including weak requirements. It does not
resolve per-symbol bindings or relocations. Under the existing coordinator and
fresh memory admission, run focused portable regressions with
`python -B -m unittest discover -s tests/tools -p test_pi_elf.py -v` and
`python -B -m unittest discover -s tests/tools -p test_pi_payload.py -v`.
See [OWNER](../OWNER.md) for this increment's execution status. Passing a
synthetic inventory or label audit is not native ARM64, complete dependency ABI
or hardware evidence.

`tools/image/pi_preflight.py` remains a read-only preparation
probe, not an image builder. Its target-library check never executes ARM64 code.
Run its portable hostile-input tests with
`python -B -m unittest discover -s tests/tools -p test_pi_preflight.py -v`.
Follow [image/README.md](../image/README.md#q-pi-development-edition) for exact
probe commands, physical backing-volume requirements, Qt target qualification and
Bluetooth/bootstrap plans. A Windows/Android ARM64 Qt kit is not a Linux target
kit. Tool installation and namespace/privilege changes require their own identified
actions and approval; this preparation performs neither.

The probe optionally accepts `--scratch-root <existing-directory>` and
`--scratch-budget <measured-budget.json>` together. Its budget schema and capacity
limits are documented in the image guide. Omitting them leaves scratch space
unqualified. A passing capacity snapshot is not a measured builder requirement
or permission to begin an image build. QOR's shared memory guard permits higher
requirements, but a caller cannot lower its 3-GiB minimum.

Check Windows headroom with WSL already resident before admitting a WSL heavy
job. A cold-host snapshot does not budget WSL startup. A 10 November read-only
startup observation fell from 3,577 MiB to 1,419 MiB available without a compiler;
the separate monitored metadata attempt was stopped at 1,189 MiB. No QOR build
worker was available to reclaim. Do not reduce the floor or manage unrelated
services: free host memory outside this workflow or use another qualified builder.

The 12 November Prism continuation measured process private resident memory and
WSL cache use before reclamation. Clean guest page-cache reclamation admitted a
build, but subsequent host pressure stopped it at 2,956 MiB. A stricter temporary
`MemoryHigh=268435456` threshold on only the next owned scope, retaining its
1-GiB hard limit and the original Windows lock/3,072-MiB floor, still stopped at
3,060 MiB. Idle Search/Widgets working sets were then trimmed after process
identity, visibility and CPU checks. ChatGPT, the call, editors and active work
were excluded. These bounded actions are recorded in
`build/prism-boundary-2026-11-12/session-02/`; they are not automatic build steps
or an instruction to repeat cache trimming. MemoryHigh constrains the owned job,
not all WSL memory; a transient cold-host margin does not permit another build.
The subsequent `session-03/` held a lightweight guest observer resident throughout
a 30-second post-reclamation admission check. Even this admitted continuation
fell from about 3,519 MiB at submission to a 3,008-MiB guard stop while compiling.
Budget working headroom beyond the floor and preserve the incremental cache;
passing an idle admission observation does not guarantee a complete build.

The owner's separately authorised service/resource review is recorded in
`build/prism-resource-2026-11-12/session-01/`. Google Play Games Services was the
only confirmed stop candidate (about 26 MiB); the non-elevated session could not
open it for stopping, so no service/application stopped. Clean guest-cache
reclamation produced 3,860–4,067 MiB during an observer-resident 30-second check,
but the actual validation scope hit 2,869 MiB before admission and was stopped.
No compiler/test ran. Keep a lightweight observer alive through the transition
to the actual worker and qualify that worker before submitting a job; do not
treat the earlier idle measurement as permission. The 203-MiB measured deficit
only reaches admission, without compiler margin. This is a bounded maintenance
receipt, not a new automatic service-management or cache-reclamation build step.

On native Linux, the same probe uses the kernel's `MemAvailable` estimate and
visible cgroup v2 memory limits, including ancestor limits. Controller metadata
determines where memory counters exist; the stock Pi advertises no memory
controller and therefore uses host available memory. Missing advertised counters,
malformed/changing topology and hidden/nonstandard roots remain blockers. WSL
memory is not a Windows host-memory observation; the Windows guard is still
required there. The probe changes no limit and reclaims no memory.

`tools/image/pi_headless.py` validates bounded public host-identity receipts and
prepares strict Windows SSH argument arrays without connecting or reading private
keys. `publish_receipt()` is a fixture-tested library helper, not an installed
first-boot hook. Follow the [headless design](../image/README.md#q-pi-first-boot-and-bluetooth-bootstrap)
for physical host-key pinning, scoped authentication and interruption recovery.
Run its focused tests with `python -B -m unittest discover -s tests/tools -p test_pi_headless.py -v`.
Actual Q Pi image integration and its first-boot Wi-Fi/SSH/access flow remain
unverified; the stock builder's separately authorised SSH access is now observed.

## 1. Prerequisites

### Windows (primary development platform)

The 15 November qualified continuation at source `5d1359e` rebuilds QQ in
`build/ci-products/qq` with commercial Qt 6.12.0, MSVC 2022, `Release`, warnings
as errors and serial compilation. Compilation has no compiler/licence
diagnostics, hardening passes all 15 programs and aggregate QML lint has zero
warnings; Runtime, Player and Studio structured reports each confirm zero
warnings. All six metadata helper tests and six focused AgentPanel results pass.
The panel checks use inert controllers, a private profile, offscreen software
rendering and bypassed adapter probing; they instantiate no audio engine and
call no keychain or provider. Full product runtime suites remain unrun on this
host because their device/keyring boundary is not qualified. The core's null
audio switch does not disable [Qt Multimedia's Windows audio backend](https://raw.githubusercontent.com/qt/qtmultimedia/v6.12.0/src/multimedia/platform/qplatformaudiodevices.cpp).
An initial wrapper used the wrong executable path; its corrected continuation
passed, then exposed a tuple/list error in the result collector. Both wrapper
failures and the actual zero-exit test output are preserved without repeating
the passing test. Receipts: `build/development-2026-11-15/session-08/`.

The 14 November local QQ continuation uses `build/ci-products/qq`, the commercial
Qt 6.12.0 kit and MSVC 2022, with warnings as errors and serial compilation.
All targets build without compiler/license-client diagnostics; all 84 scene
results pass offscreen and hardening accepts all 15 programs. Runtime QML lint
still fails at 53 warnings. The scene run's missing-font and hardware-audio
warnings are preserved: `QOR_AUDIO_OUTPUT=null` does not isolate Qt Multimedia's
Windows backend. Do not infer device isolation from offscreen rendering or that
environment variable. Further runtime suites need a qualified audio boundary;
the full Windows product harness was not run. Receipts and the correction to
the evidence collector's assumed executable path are in
`build/development-2026-11-14/session-07/`.

| Tool | Version | Notes |
|------|---------|-------|
| Qt | **6.12.0**, kit `msvc2022_64` | Commercial install via Qt Maintenance Tool (see module list below) |
| Visual Studio 2022 or Build Tools | 17.14+ | Workload "Desktop development with C++" (MSVC v143, Windows SDK) |
| CMake | 3.27+ | Bundled with Qt: `C:\Qt\Tools\CMake_64` |
| Ninja | any recent | Bundled with Qt: `C:\Qt\Tools\Ninja` |
| Python | 3.10+ | Token generator, tests, tooling |
| Git | 2.40+ | |
| Rust | 1.91+ (1.98.1 in CI), from [rustup](https://rustup.rs) | The QOR core (`core/`), built by `cargo` through Corrosion |

### Required Qt modules

Install these for the `MSVC 2022 64-bit` kit in the Qt Maintenance Tool
(`C:\Qt\MaintenanceTool.exe` → Add or remove components → Qt 6.12.0):

| Component | Installer package | Needed for |
|-----------|-------------------|------------|
| Qt 6.12.0 MSVC 2022 64-bit | `qt.qt6.6120.win64_msvc2022_64` | Core, Gui, Qml, Quick, Quick Controls, Quick Effects, Quick Shapes, Svg, Test, Widgets |
| Qt Shader Tools | `qt.qt6.6120.addons.qtshadertools` | `qt_add_shaders` (`.frag` → `.qsb`) |
| Qt Remote Objects | `qt.qt6.6120.addons.qtremoteobjects` | Service IPC, `repc` |
| Qt Graphs | `qt.qt6.6120.addons.qtgraphs` | Monitor app |
| Qt Image Formats | `qt.qt6.6120.addons.qtimageformats` | WebP screenshots |
| Qt Quick Compiler extensions | `qt.qt6.6120.addons.qtqmlscriptcompiler` | `qmlsc` ahead-of-time QML compilation (commercial) |
| Qt Task Tree (TP) | `qt.qt6.6120.addons.qttasktree` | Dependency of Qt's own QML asset-downloader plugin; without it CMake prints a warning at configure time |

The products also need these, from the same kit:

- **Qt Multimedia**;
- **Qt Quick 3D**, with **Qt Quick 3D Physics** (`qt.qt6.6120.addons.qtquick3dphysics`), which every QQ scene imports;
- **Qt Spatial Audio**;
- **Qt PDF**, one of the installer's per-kit extensions
  (`extensions.qtpdf.6120.61400.win64_msvc2022_64`, or `…linux_gcc_64` on Linux);
- **Qt Network**.

Both are in [`.github/qt-packages.json`](../.github/qt-packages.json), so
`tools/ci/install_qt.py` installs them.

The QQ Player's browser build needs Qt for WebAssembly (multithreaded).
Recommended for upcoming phases (see [QT-MODULES.md](QT-MODULES.md)): Qt Quick
Timeline, Qt Lottie, Qt Virtual Keyboard, Qt WebEngine, Qt Positioning, Qt
TextToSpeech.

`cmake/QorLocateQt.cmake` finds Qt automatically, in this order:
`Qt6_DIR`/`CMAKE_PREFIX_PATH` from your IDE kit, the `QOR_QT_DIR`
environment variable, then `C:\Qt\6.12.0\msvc2022_64`.

### Linux

Qt 6.12.0 `gcc_64` with the same modules (plus **Qt Wayland Compositor** to
build `prism`), GCC 13+ or Clang 17+, CMake 3.27+, Ninja, and the
OpenGL/EGL/xkbcommon development packages:

```bash
sudo apt-get install ninja-build libgl1-mesa-dev libegl1-mesa-dev libxkbcommon-dev \
  libfontconfig1-dev libfreetype-dev libwayland-dev libpulse0 libssl-dev libdbus-1-dev pkg-config
```

`libpulse0` is needed by Qt Multimedia (the audio engine); desktops running
PipeWire are used through it automatically. `libssl-dev` (OpenSSL 3's
libcrypto) verifies Store signatures; Windows uses CNG instead.
`libdbus-1-dev` is for the QOR core, whose keychain uses the Secret Service.
Rust comes from rustup.

### Linux on WSL2 (Windows workstation)

WSL2 with WSLg is the quickest way to work on the Linux-only parts (`prism`,
the platform plugins) from a Windows machine. It is verified on Ubuntu 26.04
with GCC 15 and CMake 4.2.

```bash
# 1. Build dependencies, plus Weston's demo clients for testing Prism:
sudo apt-get install build-essential cmake ninja-build git python3 pkg-config \
  libgl1-mesa-dev libegl1-mesa-dev libgles-dev libdrm-dev libgbm-dev libvulkan-dev \
  libxkbcommon-dev libxkbcommon-x11-dev libfontconfig1-dev libfreetype-dev libdbus-1-3 \
  libwayland-dev wayland-protocols libinput-dev libxcb-cursor0 libpulse0 libssl-dev weston \
  libdbus-1-dev
#    and Rust, for the QOR core:
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain 1.98.1

# 2. The commercial Qt kit, unattended. This reuses the Qt Account sign-in
#    from Windows. Copy it once and keep it private:
install -D -m 600 "/mnt/c/Users/<you>/AppData/Roaming/Qt/qtaccount.ini" ~/.local/share/Qt/qtaccount.ini
python3 /mnt/x/path/to/qor-os/tools/ci/install_qt.py --platform linux --root ~/Qt

# 3. Build the Windows checkout from WSL into the Linux filesystem (fast I/O).
#    Turn .qmlls.ini generation off so the Windows editor setup is untouched:
export PATH=$HOME/.cargo/bin:$PATH
cmake -S /mnt/x/path/to/qor-os -B ~/qor-build/linux-release --preset linux-release \
  -DQT_QML_GENERATE_QMLLS_INI=OFF
cmake --build ~/qor-build/linux-release
ctest --test-dir ~/qor-build/linux-release        # GPU suites run on WSLg too
```

On Linux the executables sit in their target directories, for example
`prism/qor-shell`, `prism/compositor/prism` and `platform/lumen-widgets-gallery`.

### Linux on a machine of its own (Fedora on an external SSD)

WSL has no GPU QOR OS may use (the products' runs there are fenced from it)
and no controllers, so Phase 1's goal, Orb Run in Prism with a DualSense,
needs Linux on real hardware. The way that leaves Windows untouched is Fedora
Workstation on an external SSD, started from the laptop's boot menu.
[QOR-0004](decisions/QOR-0004-the-base-image-is-fedora-atomic.md) builds the
image on Fedora too.

**What it takes:** an SSD of 500 GB or more on a USB-C port of 10 Gbit/s or
faster (an NVMe drive in an enclosure, or a portable SSD), and a USB stick of
8 GB or more for the installer. Everything on both is erased.

1. **Before anything, in Windows:** if the internal drive uses BitLocker,
   save its recovery key (Control Panel → BitLocker Drive Encryption → Back
   up your recovery key), in case the firmware's boot menu makes Windows ask
   for it. Leave Secure Boot on: the steps below sign the NVIDIA driver for
   it.
2. **The installer:** write Fedora Workstation's current release to the USB
   stick with Fedora Media Writer (from fedoraproject.org).
3. **Install:** plug in both, restart, press **F12** at the Dell logo, and
   start the USB stick. Choose to install Fedora, and as its destination choose
   **only the external SSD**, erased whole. Check that the laptop's own drive
   is not selected: it holds Windows. Turn on disk encryption: the key that
   signs the NVIDIA driver, and later your Qt Account sign-in, live on this
   disk.
4. **Starting it:** Fedora puts its boot files on the SSD. With the SSD
   plugged in, F12 at the Dell logo lists it; without it, the laptop starts
   Windows as before. If the firmware puts Fedora first, move Windows Boot
   Manager back to the top in its boot sequence.
5. **First start:** finish GNOME's setup, then in a terminal:

   ```bash
   sudo dnf upgrade --refresh -y && systemctl reboot
   sudo dnf install -y gh git && gh auth login     # the private repository
   gh repo clone QOR-MATRIX/qor-os ~/qor-os && cd ~/qor-os
   tools/dev/setup-fedora.sh
   ```

`tools/dev/setup-fedora.sh` does the rest, one step at a time, each safe to
run again (`tools/dev/setup-fedora.sh check` shows what is in place):

- **packages:** the build tools and libraries, named by what pkg-config
  looks for, plus `dbus-daemon`, `python3-dbus` and `python3-gobject` for the
  products' hidden test runs.
- **nvidia:** RPM Fusion's driver (`akmod-nvidia`), as its NVIDIA and Secure
  Boot guides say. With Secure Boot on, it first makes this machine's signing
  key and asks you for a one-time password. At the next boot a blue screen,
  MOK management, appears: choose *Enroll MOK*, *Continue*, *Yes*, type the
  password (the keyboard there is US English), and reboot. Run the script
  again: it installs the driver, waits for the module to be built, and asks
  for one more reboot. The script stops at each reboot and says so.
- **rust:** rustup with Rust 1.98.1, as CI uses.
- **qt:** the commercial Qt 6.12 kit in `~/Qt`, through
  `tools/ci/install_qt.py`. It needs this machine signed in to your Qt
  Account once: run the Qt Online Installer for Linux, sign in, and close it
  at the component list.
- **build:** QOR OS with the `linux-release` preset, and the products with
  `tools/dev/products.sh --build-only`.

Then `tools/dev/setup-fedora.sh play` starts Prism in a window of GNOME's
session, with ARQADE, QOR Media, QQ Studio and the QQ Player where it finds
them and the QQ samples, Orb Run among them, in Home and the Library. A
DualSense connects by USB, or by Bluetooth from GNOME's settings (hold PS and
Create until the light bar flashes). The kernel's `hid-playstation` driver
serves it and logind gives the signed-in person its device, so
`build/linux-release/tools/gamepads/qor-gamepads` lists it with nothing to
set up ([CONTROLLERS.md](CONTROLLERS.md)). On this laptop GNOME runs Prism on
the Intel GPU; `QOR_PLAY_GPU=discrete tools/dev/setup-fedora.sh play` runs it
on the RTX through switcheroo-control.

Booting the laptop into Fedora makes its Windows environment unavailable.
Independently, Qt CI jobs remain paused until runner isolation is accepted;
returning to Windows does not authorise restarting runners ([CI.md](CI.md)).

#### What to check there

This is the optional reference-laptop route, separate from Q Pi's builder and
headless plan. It assumes the SSD and stick described above; no current physical
setup or media action is established by these instructions. The internal drive is neither repartitioned nor erased, and
erasing the two external drives is the owner's act. Each result goes in the
[roadmap's table](ROADMAP.md#alongside-the-phases--engines) as passed, failed or
blocked, with the commit. A suite passing in WSL settles none of this.

1. **Record the configuration:** the commit, `uname -r`, `nvidia-smi`, Mesa's
   version, the display path, and `tools/dev/setup-fedora.sh check`.
2. **The product suites on real hardware:** `tools/dev/products.sh --qt
   ~/Qt/6.12.0/gcc_64 --build-root ~/qor-build/products --attempts 1`. Note the
   QQ Player's frame rate against its floor of 20, and the Player's wall working
   with a real graphics card, which has only been tested in software.
3. **QQ Studio in Prism** (`tools/dev/setup-fedora.sh play`, on the Intel GPU, then
   with `QOR_PLAY_GPU=discrete`): it opens, and its menus, dialogs, focus, clipboard,
   scaling and fullscreen work.
4. **The first journey:** with two small projects, import an authorised asset, edit,
   save, build, run the packaged game from the Library, switch, restart and reopen.
   Unsaved work survives, and nothing crosses between them. Editor play and the
   packaged game are separate results.
5. **Sound:** Orb Run is heard, through the laptop and through a headset plugged in
   while it plays.
6. **A DualSense:** by USB and by Bluetooth, listed by `qor-gamepads`, playing
   Orb Run, with Guide bringing Overview.
7. **Pointer capture:** a first-person view holds the mouse, Prism's way out still
   works, and losing focus or locking the screen lets go.
8. **Displays,** if another is attached: each at its own rate, and hot-plug.

## 2. Configure and build

The Ninja presets need the MSVC environment. `tools\dev\Enter-QorShell.ps1`
loads it (via `vswhere` and `vcvars64.bat`) together with Qt's `bin`, CMake and
Ninja:

```powershell
. .\tools\dev\Enter-QorShell.ps1          # dot-source once per terminal
cmake --preset ci-windows
cmake --build --preset ci-windows
ctest --preset ci-windows
```

Or in one step: `.\tools\dev\build.ps1 -Preset ci-windows [-Test] [-Target all_qmllint]`.
Use the existing QOR heavy-job lock and resource preflight. Never rebuild/relink
a running `windows-release` shell; the helper's default still names that preset,
so select `ci-windows` explicitly on the owner's workstation.

| Preset | Generator | Build type | Use |
|--------|-----------|------------|-----|
| `windows-release` | Ninja | RelWithDebInfo | Existing shell/benchmark folder; protected while its shell runs |
| `windows-debug` | Ninja | Debug | Debugging (Qt debug DLLs) |
| `windows-vs` | Visual Studio 17 2022 | multi-config | Works without a developer shell |
| `linux-release`, `linux-debug` | Ninja | | Linux |
| `ci-windows`, `ci-linux` | Ninja | RelWithDebInfo | CI settings; `ci-windows` also isolates local work from the running release shell. Both require warnings as errors and the QOR core |

Build options (`-D<option>=ON|OFF`):

| Option | Default | Effect |
|--------|---------|--------|
| `QOR_BUILD_TESTS` | ON | Tests in `tests/` |
| `QOR_BUILD_TOOLS` | ON | `qorctl`, `qor-simd` |
| `QOR_BUILD_PLATFORM` | ON | QStyle plugin, platform theme, widgets gallery |
| `QOR_BUILD_COMPOSITOR` | ON on Linux | `prism` (skipped with a warning if Qt Wayland Compositor is missing) |
| `QOR_WARNINGS_AS_ERRORS` | OFF | `/WX` or `-Werror` |
| `QOR_QT_PINNED_VERSION` | 6.12.0 | Version used by Qt auto-location |
| `QOR_BUILD_CORE` | ON where `cargo` is found | The QOR core and `tst_qorcore` ([UPSTREAM.md](UPSTREAM.md)) |
| `QOR_BUILD_SIMULATOR` | ON | The simulated services, `qor-simd`, `qor-shell` and the tests. **OFF for a device's image**: Prism then has no simulator built in, and no unlock with nothing behind it |
| `QOR_HARDENING` | ON | Exploit mitigations on every program ([QOR-0018](decisions/QOR-0018-the-security-and-quality-bar.md)). Off only to compare |
| `QOR_STORE_DEMONSTRATION` | OFF | The Store's two storefronts on a catalogue signed with a throwaway key, for development (the `linux-demonstration` and `windows-demonstration` presets; [STORE.md](STORE.md#the-demonstration-build)). **Never in a release**: every other preset sets it off, it is refused without the simulator, nothing built with it installs, and `tools/ci/check_hardening.py` fails a folder that has it |
| `QOR_SANITIZE` | empty | Sanitizers for the suites, e.g. `address,undefined` (the `linux-asan` preset). The products take it from the environment |
| `QOR_CORE_SOURCE_DIR` | empty | A local demiurge-chain checkout to build the core from instead: to change the core and QOR OS together, or to try a commit before the pin moves to it |

The pin (`QOR_CORE_COMMIT`) and the core's Cargo features
(`QOR_CORE_FEATURES`, `desktop`) are set in `core/CMakeLists.txt` and written
on every configure, so they are not options. A build folder configured at an
older pin follows a new one and says so ("the pin moved to …").

The first configure with the core clones demiurge-chain at the pin and
Corrosion v0.6.1 into the build tree, and the first build compiles the core's
Rust dependencies. Both need network access and take a few minutes; with
`desktop`, the first build of the core takes about 3–4 minutes on the
workstation. Later builds are incremental.

Outputs (Windows): executables and test binaries in `build/<preset>/`, QML
modules in `build/<preset>/qml/`, plugins in `build/<preset>/plugins/`.

### IDEs

- **Qt Creator**: open `CMakeLists.txt`, choose the *Qt 6.12.0 MSVC2022 64bit* kit. Presets are detected.
- **VS Code**: open the folder and accept the recommended extensions (Qt
  extension pack, CMake Tools, clangd). Presets, launch configurations and
  tasks are in `.vscode/`.
- **Visual Studio**: use the `windows-vs` preset, or open the folder (CMake presets are supported).

The Windows presets supply `cmakeExecutable` for IDEs:
`C:/Qt/Tools/CMake_64/bin/cmake.exe`, the existing Qt-bundled CMake (3.30.5 on
the reviewed workstation). CMake Tools can therefore resolve it when VS Code
was launched normally without the developer shell's PATH. This is an
[IDE-only preset field](https://cmake.org/cmake/help/v3.30/manual/cmake-presets.7.html#configure-preset),
not a command-line executable locator. CLI commands still require the developer
shell above or the executable's full path. A different local Qt tools location
belongs in a derived `CMakeUserPresets.json` preset; Linux presets use their
own CMake and do not inherit this Windows path.

Choose `ci-windows` for guarded workstation validation. The three build/lint
tasks now use that separate folder, protecting a running `windows-release`
shell. Automatic configure on open/edit and build before debugging are disabled
in this workspace: first check QOR workers, the shared lock and memory guard,
then explicitly configure/build. Selecting an executable or parsing presets is
not a successful compiler, product or Prism test. No machine-wide PATH,
security setting, installation or WSL resource limit is changed by this setup.

## 3. Run

```powershell
.\tools\dev\run.ps1 -Preset ci-windows -- <qor-shell options>
```

`qor-shell` options:

| Option | Description |
|--------|-------------|
| `--unlocked` | Start on the desktop instead of the lock screen |
| `--open <apps>` | Comma-separated apps to open after unlock: `files`, `music`, `notes`, `terminal`, `monitor`, `settings`. `app/page` opens a page and `app/page/section` scrolls to a section, for example `settings/sound/reactive` |
| `--play`, `--seek <seconds>` | Start the music, at a position |
| `--surface <name>` | Show `beam`, `control-center`, `overview` or `media` after start |
| `--query <text>` | Initial Beam query (with `--surface beam`) |
| `--theme light\|dark` | Force appearance (writes to the settings service) |
| `--accent <hue>` | Force accent hue in OKLCH degrees |
| `--contrast standard\|high` | Force Increase Contrast off or on |
| `--size WxH` | Window size in logical pixels (clamped to the screen) |
| `--fullscreen` | Start full screen (<kbd>F11</kbd> toggles) |
| `--services embedded\|external` | Host the simulator in-process (default) or connect to `qor-simd` |
| `--services-url <url>` | Services endpoint |
| `--scenario <file>` | Simulation scenario JSON (default: built-in `sim/scenarios/default.json`) |
| `--ephemeral` | Do not persist preferences (implied by `--screenshot` and `--benchmark`) |
| `--gpu high-performance\|low-power\|default` | GPU preference (default `high-performance`) |
| `--screenshot <file> [--delay ms]` | Save a frame (`.webp`, `.png`, `.jpg`) and exit |
| `--benchmark <seconds> [--benchmark-out file]` | Scripted animation tour, prints frame-pacing JSON, exits |
| `--script <file>` | Drive the shell with a JSON script and exit ([DEVELOPMENT.md](DEVELOPMENT.md)) |

For both `qor-shell` and `prism`, benchmark report output is part of the command's result. A failed requested-file
open, incomplete write, flush/close failure or stdout write/flush failure returns exit 1 with a generic diagnostic.
Successful reports keep their JSON schema and replace an existing report. Automation must check the native exit
status and require its own fresh, regular report file; an older file does not establish a successful run. These writes
are not an atomic replacement or a power-loss durability guarantee. `tst_framecapture` covers directory failure,
Linux virtual-full-sink file/stdout failure and complete stale-file replacement through actual subprocesses.

`--screenshot`, `--benchmark` and `--script` render audio silently (the
engine's null output; see [AUDIO.md](AUDIO.md)), see only virtual game
controllers (`QOR_GAMEPADS=virtual`; see [CONTROLLERS.md](CONTROLLERS.md)),
leave the touchpad alone (`QOR_TOUCHPAD=off`; see
[SHELL.md](design/SHELL.md#gestures)), and install from the Store into a
temporary folder (`QOR_STORE_DIR`; see [STORE.md](STORE.md)).
`QOR_STORE_URL` points the Store at another signed catalogue.
`QOR_STORE_DEMONSTRATION_URL` names the demonstration's catalogues, and is read only by a build
with `QOR_STORE_DEMONSTRATION` ([STORE.md](STORE.md#the-demonstration-build)).

Persistent preferences (appearance, accent, motion, transparency, living
wallpaper, audio and the audio-reactive interface) are stored with `QSettings`
under `QOR-MATRIX/QTOS Shell` and Notes under the `notes` category.

### Separate service process

```powershell
.\build\ci-windows\qor-simd.exe --ephemeral                 # terminal 1
.\tools\dev\run.ps1 -Preset ci-windows -- --services external --unlocked # terminal 2
.\build\ci-windows\qorctl.exe watch                         # terminal 3
```

### Prism, the Wayland compositor (Linux)

`prism` hosts the same shell as a real Wayland compositor. It runs nested
inside any Wayland or X11 session, including WSLg:

```bash
B=~/qor-build/linux-release
$B/prism/compositor/prism --unlocked --socket wayland-qor &
WAYLAND_DISPLAY=wayland-qor weston-terminal &                  # client-side decorations
WAYLAND_DISPLAY=wayland-qor QT_QPA_PLATFORM=wayland \
  $B/platform/lumen-widgets-gallery &                           # Qt app, Prism decorations
```

![Prism nested in WSLg with weston-terminal and a Qt Widgets client](images/prism-nested.webp)

| Option | Description |
|--------|-------------|
| `--unlocked` | Start on the desktop instead of the lock screen |
| `--socket <name>` | Wayland socket for clients (default: the first free `wayland-N`, printed at start) |
| `--size WxH` | Output window size |
| `--outputs N` | Drive N outputs (up to 4), side by side, each a window of `--size`. On bare metal (`QT_QPA_PLATFORM=eglfs`) every connected screen is an output instead |
| `--spread-clients` | Open each new client on the output with the fewest windows |
| `--services embedded\|external` | As for `qor-shell` |
| `--screenshot <file> [--delay ms]` | Save the output and exit (used for automated checks) |
| `--benchmark <seconds> [--benchmark-out file]` | Shell tour plus input latency; per-output JSON report, then exit ([PERFORMANCE.md](PERFORMANCE.md)) |

How clients are handled:

- **Decorations.** Clients that support `xdg-decoration` (all Qt apps) get
  Prism's title bar and controls. Clients that draw their own (Weston demos,
  GTK) get none, so nothing is drawn twice.
- **Window geometry.** A client's window geometry is aligned with the frame,
  and client-drawn shadows are clipped, never scaled.
- **Size.** If a client picks its own size (a preferred size on first map, or
  whole character cells in a terminal), the frame adopts it.
- **Window state.** Focus, maximise and minimise follow `xdg_toplevel`
  requests and states.
- **Moves.** A move started from a client's own title bar uses the same
  springs and edge snapping as Prism's.

Under WSLg, Mesa's `libEGL` warnings and "no EGL_WL_bind_wayland_display" are
expected. Clients fall back to shared-memory buffers, which is fine for
development; on device, GPU buffer sharing is part of Phase 3.

## 4. Tests and lint

On 15 November, source `5d1359e` configured/built with `ci-windows` and the
required pinned QOR core. All 22 suites selected by
`ctest --preset ci-windows -LE gpu --parallel 1` passed in 129.266 seconds;
nine GPU-labelled suites were excluded. One desktop-entry case retains its
expected Windows skip. The image-decoder errors come from the passing malformed
preview checks. Hardening passes all 33 programs and QML lint reports zero
warnings. Temporary profile/settings folders, offscreen software rendering,
the core's null audio output and virtual controllers isolate this selection.
Rust's pinned `proc-macro-error2` 2.0.1 future-compatibility warning remains:
the successful build is not warning-free. All 550 core resource samples stay
at or above 3,422 MiB against the unchanged 3,072-MiB floor. The original Windows
coordinator holds its real lock throughout, with 30 resident-worker admission
samples and continuous monitoring. Full GPU/product, native Linux OS and
isolated-CI acceptance remain separate. Current receipts:
`build/development-2026-11-15/session-08/`; the preceding 13 November run remains
in `build/development-2026-11-13/session-01/`.

```powershell
ctest --preset ci-windows                           # all suites in this build
ctest --preset ci-windows -LE gpu                    # excludes GPU-dependent suites
cmake --build --preset ci-windows --target all_qmllint
python tools\tokens\generate_tokens.py --check
python tools\ci\check_headers.py
.\.venv\Scripts\pre-commit run --all-files
```

The repository's `.qmllint.ini` sets `MaxWarnings=0`, so a native `all_qmllint` command fails on warnings as well as errors.
It changes no diagnostic category or severity. Run it for a product's own build folder too:
`cmake --build <product-build-folder> --target all_qmllint --parallel 1`. A failing lint command is separate from passing
runtime tests; the existing warning backlog must be resolved before the zero-warning requirement passes.

The existing Qt 6.12 SpatialAudio metadata has a separate, reproduced dependency omission: its `qmldir` names
QtQuick and QtMultimedia but omits QtQuick3D, whose `QQuick3DNode` base is used by its types. This minimal input
reports two native Windows warnings with the installed kit:

```qml
import QtQuick3D
import QtQuick3D.SpatialAudio
Node { AudioListener {} }
```

`SpatialSound` reproduces the same omission. Byte-identical diagnostic copies retain it; adding only
`depends QtQuick3D` in a temporary metadata copy clears these examples. An unknown-property control still fails.
Using the actual QQ Runtime response file isolates exactly six `World.qml`/`Sound.qml`/`Voices.qml` findings,
with zero added; both whole-module diagnostic commands still fail. The installed SDK files and generated response
file retain their hashes. This is diagnosis, without a correction installed or connected to normal lint:
whole QQ failed at 142 console/167 structured warnings at that checkpoint, before later source cleanup.
The command records and controls are in
`build/autonomous-2026-11-04/spatial-runtime-diagnostic-verified.json`; this clears no runtime or hardware gate.

The 15 November native Pi reproduction confirms that the one-line metadata-copy correction removes exactly six
SpatialAudio findings and still rejects unknown properties and invalid inherited property types. The installed SDK
and generated response files stay unchanged. Structured output also exposes findings omitted from console output:
QQ Runtime had 20 warnings despite six printed lines, and Studio independently had 18. The subsequent Music/Sprite/
TileMap correction removes 14 runtime findings, leaving six runtime and 18 Studio findings; Player has none.
After those source fixes, the corrected temporary copy makes the complete Runtime lint command return zero with
zero warnings; the invalid-property and invalid-type controls still fail. Normal lint still uses the unchanged SDK
and fails at that diagnostic checkpoint.
Do not use console line counts as the complete lint result. Run each module's `qqruntime_qmllint_json`,
`qq-player_qmllint_json` and `qqstudio_qmllint_json` target separately and inspect its JSON; an aggregate target can
stop at the first failing module. Preserve every return code and diagnostic severity. Qt documents the
[module dependencies used by tooling](https://doc.qt.io/qt-6/qt-add-qml-module.html#module-dependencies)
and [structured lint output](https://doc.qt.io/qt-6/qtqml-tooling-qmllint.html#scripting).
Native receipts and controls: `build/development-2026-11-15/session-04/`.

The subsequent build integration uses `cmake/QorSpatialAudioMetadata.cmake` from QQ's runtime CMake file. It copies
only `qmldir` and `plugins.qmltypes` beneath that runtime's build directory, adds the missing dependency to the
local `qmldir`, and supplies the directory through Qt's documented `IMPORT_PATH` argument. The installed kit,
diagnostic severities and application import environment are unchanged. A kit already declaring the dependency
uses its original metadata. Incomplete metadata, unexpected module/base types and an unreviewed version still
missing the dependency stop configuration instead of silently applying the workaround.
The generated `qqruntime_qmllint` and `qqruntime_qmllint_json` targets both pass with zero warnings on the native Pi;
Player remains clean and Studio's 18 findings still fail the aggregate target. Six helper tests and real valid/
invalid-property/type controls pass. This is a build-tool correction, not Windows deployment or device acceptance.
Receipts: `build/development-2026-11-15/session-06/`.

The following AgentPanel correction removes the remaining 18 Studio findings.
An explicit accessor retains its required structural controller and reads the
current object in bindings and actions. Native QQ `all_qmllint` now exits zero;
Runtime, Player and Studio structured commands each report zero warnings.
No diagnostic category, severity or source coverage changed. Six focused
results pass against the unchanged panel before the correction and offscreen
under ASan/UBSan afterward, with leak checking. Complete Agent passes 22 with
the identical existing log-timing failure and optional provider skip; complete
Studio passes 29. The packaged-game creator journey passes in 131.581 seconds
under its original 300-second limit. All 1,351 validation samples stay at
or above 5,835 MiB against the unchanged 3,072-MiB floor. Native compilation
and hardening pass. Existing assertions, approved timing, the original guard
and private namespaces are unchanged. Receipts: `build/development-2026-11-15/session-07/`.

The 16 November client-only graphics trace at source `489ddfe` narrows first
Play to repeated V3D buffer waits inside Qt's `QRhi::endFrame`. In the targeted
ioctl observer, 417 `WAIT_BO` calls occupy 491.838 of the 510.661-ms
after-render-to-swap interval, with 0.969 ms of render-thread CPU. Buffer handle
291 accounts for 383.545 ms and is present in captured submission 946. This
identifies a dominant buffer reservation wait, not a unique fence sequence,
shader or Studio pass. Matching Mesa/Qt debug symbols are absent; raw offsets
are preserved. No production change follows from this evidence alone.

The same original Agent cases report 3 passed, 1 failed and 0 skipped in all
three executions. Reference/observer fail `times > 5` with 3; `strace` fails
with 1 and measurably perturbs scheduling. Error capture/coalescing and the
wall-clock case pass; later notice/cursor assertions are unreached. Actual
V3D/OpenGL ES 3.1/Mesa/Qt identities and private compositor are checked;
client caches start empty. There are no shader/pipeline errors. The independent
llvmpipe failure remains open, and no new image equivalence is claimed.

Original sources, binaries, screenshot, assertions and suite limits are intact.
All 215 diagnostic/setup resource samples exceed 3,072 MiB, minimum 6,736 MiB.
One observer compilation refusal is preserved and corrected before its test
ran. No application Release, sanitizer, broader product suite or isolated CI
acceptance was run anew. Prior 97 unexplained image outliers, deterministic
logging coverage and an early ES initializer remain unresolved/proposed.
Next: resolve the captured Mesa/Qt call sites with matching debug information
to identify the GL operation and pass that reuse the dominant buffer. Keep
PR #10 draft; skipped hosted Qt/core-audit jobs are not passes and required
Release readiness remains unmet. Receipts:
`build/development-2026-11-16/session-03/`; session-02 remains sealed.

The selected collector check also runs in the existing Windows kit without showing a window or using a graphics
card. Use `ci-windows`, leaving a running `windows-release` shell alone:

```powershell
cmake --build --preset ci-windows --target tst_framecapture --parallel 1
ctest --preset ci-windows --test-dir build/ci-windows -T Test -R "^tst_framecapture$" --no-tests=error --parallel 1 --verbose --output-on-failure --timeout 120
```

The explicit folder is needed for this recorded CTest dashboard invocation; the first preset-only dashboard command
selected the source folder and exited 8 with no tests. Its failed record is retained. The corrected test-only run
passes nine cases/seven functions with zero failures/skips; source hashes, compiler commands and the fresh program
are verified. Directory failure and complete report replacement run on Windows; the two `/dev/full` cases are
Linux-only and do not count as Windows passes or skips. This synthetic hidden software check, plus a 21-program
header hardening audit, proves neither hardware timing nor a full Windows or OS build.

## 5. Install and deploy

```powershell
cmake --install build\ci-windows --prefix dist\qor-os
```

The install step runs Qt's deployment script (`qt_generate_deploy_qml_app_script`,
which calls `windeployqt` on Windows) to produce a self-contained folder with
the required Qt libraries, plugins and QML modules. The result runs on a
machine without Qt installed (verified with an empty `PATH`).

Because the QOR OS QML modules are static and embedded in the executable,
`qor-shell` sets `QT_QML_IMPORT_SCANNER_EXTRA_ROOT_PATHS` to `kit/`, `apps/`
and `services/` so the deployment scan finds their imports (for example
`QtCore` and `QtGraphs`). Add new module directories there.

## 6. The products (QQ, ARQADE, QOR Media)

QQ, ARQADE and QOR Media came from demiurge-chain at the paths they had
there, and are developed here ([UPSTREAM.md](UPSTREAM.md)). Each keeps its own CMake project and
`build.ps1`, which configures with MSVC 2022 and Ninja, builds, and runs every
registered CTest suite. `tools/dev/products.ps1` runs them all:

```powershell
.\tools\dev\products.ps1                          # build\products\{qq,arqade,media}
.\tools\dev\products.ps1 -Product qq -Attempts 1  # one product, no retry
```

| Product | Script | Programs | Notes |
|---------|--------|----------|-------|
| QQ | `products/qq/build.ps1` | `qq-studio.exe`, `qq-player.exe`, `qq-mcp.exe` | `-Run` opens QQ Studio; `-Web` builds the Player for the browser (Qt for WebAssembly and Emscripten) |
| ARQADE | `products/arqade/qt/build.ps1` | `arqade.exe` | Builds QQ's runtime from `products/qq/runtime`; carries the games from `products/arqade/games` |
| QOR Media | `tools/qor-media/build.ps1` | `qor-media.exe` | Fetches the pinned PDFium release once (checked by SHA-256) into `%LOCALAPPDATA%\qor-media-pdfium` |

Their suites open real windows on the GPU and drive them with the keyboard
and mouse. Run them in a desktop session, and leave the desktop alone while
they run: a window that loses the keyboard to another application fails a
test. `products.ps1` tries a failing product once more. The look tests read
`docs/design/tokens.json`.

The Windows scripts use `tools/dev/products-tests.ps1`: old CTest records are invalidated before configuring/building and
before testing, and cleanup failure stops the run. CTest must run at least one suite and leave complete matching log/XML
evidence. Empty runs, failed suites and whole suites skipped by CTest cannot pass. `products-report.py` prints individual
case counts and skipped/failed cases; a skipped case is kept separate from the passing count. Python is required, as for
the repository's other checks. Suites run one at a time even with an inherited CTEST_PARALLEL_LEVEL. The scripts' explicit
`-Run` modes and QQ's `-Web` workflow retain their existing behavior.

### On Linux

demiurge-chain built the products on Windows only. QOR OS builds them on Linux
with its own script, outside the products: `tools/dev/products.sh` configures
each with CMake and Ninja against the `gcc_64` kit, builds it, and runs the
suites it registers with CTest. A product whose suites fail has its failed
suites run once more.

First attempts and retries explicitly use CTest's `--parallel 1`, so an inherited `CTEST_PARALLEL_LEVEL` cannot overlap
suite windows or input. Regression checks exercise both paths with real CTest and stand-in programs, without opening Qt.

```bash
tools/dev/products.sh                                 # build/products-linux/{qq,arqade,media}
tools/dev/products.sh --product qq --attempts 1
tools/dev/products.sh --qt ~/Qt/6.12.0/gcc_64 --build-root ~/qor-build/products
tools/dev/products.sh --display session               # on this desktop, to watch them
tools/dev/products.sh --display prism                 # inside Prism, as hidden as the default
tools/dev/products.sh --product qq --tests tst_agent  # the suites whose names match
tools/dev/products.sh --gpu                           # under WSL, on the GPU, which has frozen the PC
```

- **Display:** the suites open real windows and type into them. By default
  the script runs them on a display of the run's own,
  `tools/dev/products-display.qml`: a compositor that Qt's `qml` tool runs on
  the offscreen platform, which keeps each window at the size it asks for and
  gives the newest the keyboard, as a desktop does. There is no X display and
  the sound goes nowhere, so nothing reaches the screen or the speakers, and
  nothing on the desktop can take a test window's keyboard. It needs the
  kit's Qt Wayland Compositor, as Prism does. The display's log is in the
  build root. Hidden runs create a private, mode-0700 runtime directory below
  `/tmp/qor-products.*`, remove it on exit, and leave the desktop's runtime
  directory untouched. This keeps Wayland socket paths within Linux's limit even
  when the caller supplies long `TMPDIR` or `XDG_RUNTIME_DIR` paths.
  - `--display prism` runs them inside Prism, just as hidden, from
    `build/ci-linux` or `build/linux-*` or the one `--prism` names, with
    settings thrown away after the run. Prism opens each window at the size
    it asks for, within its work area. Run there on 30 October, QQ's suites
    failed only the cases that time movement in software (below), as they
    do on the run's own display; ARQADE's and QOR Media's have not been run
    there since.
  - `--display session` uses the session's own display and sound.
- **Keyring:** in the hidden modes the session bus is the run's own too, with
  `tools/dev/secret-service.py` on it: an in-memory Secret Service where QQ
  Studio's agent suite keeps its test key
  ([QOR-0011](decisions/QOR-0011-the-creators-provider-key-in-the-secret-service.md)),
  never touching your keyring. It needs `dbus-daemon`, `python3-dbus` and
  `python3-gi`; without them the script says so and the agent suite fails.
  With `--display session` the session's own keyring is used, and under WSL
  there is none.
- **PDFium:** the script fetches the release QOR Media's `build.ps1` pins
  (`chromium/8111`), for Linux, once into `~/.cache/qor-media-pdfium`, and
  checks it by SHA-256.
- **Graphics:** on a Linux machine the products draw on its GPU. **Under WSL
  the run is fenced from the GPU**, because on 28 October three runs that drew
  on it through Mesa's Direct3D 12 driver hung the Windows PC
  ([CI.md](CI.md#the-carried-products)). The script runs itself again in a
  mount namespace of its own where `/dev/dxg`, WSL's GPU device, is
  `/dev/null`. Mesa draws in software (llvmpipe) there, and nothing else
  (Vulkan, CUDA, video decoding) can reach the GPU either. Where systemd's
  user manager runs (`systemd=true` in `/etc/wsl.conf`), the run is also
  held to half the processors and three quarters of WSL's memory, so the PC
  stays usable. It needs unprivileged user namespaces, which WSL's kernel
  allows. In software the QQ Player's floor of 20 frames a second fails, and
  a pass says nothing of a real GPU, sound or controller
  ([CI.md](CI.md#what-linux-runs)). `--gpu` draws on the GPU through
  Direct3D 12 as before, and can freeze the PC.
- **Sound:** with `--display session` and no PipeWire daemon (WSLg), it sets
  `QT_AUDIO_BACKEND=pulseaudio`, as the shell's audio engine does.
- **Programs:** on Linux QQ's programs sit in their target folders
  (`studio/qq-studio`, `player/qq-player`, `agent/qq-mcp`, `tests/tst_*`);
  ARQADE's and QOR Media's sit in the build folder.
- **The first journey** (`first_journey`, `tools/dev/first-journey.py`) is the
  one place QQ and Prism meet: a project built by the real `qq-studio --build`,
  found in real Prism's Library and played by the real Player. Its Prism steps
  need a build of Prism with its stand-ins for the products, named by
  `QOR_PRISM_BUILD`; without it the run skips them and the test is reported
  as skipped (exit 77), which is not a pass. To run all of it:

  ```bash
  cmake --preset linux-asan
  cmake --build build/linux-asan --target prism prism-test-qq-player \
      prism-test-qq-studio prism-test-arqade prism-test-media
  QOR_PRISM_BUILD=$PWD/build/linux-asan tools/dev/products.sh --product qq \
      --tests '^first_journey$'
  ```

  Prism must be a simulator-enabled Linux build with those stand-ins: a device build correctly refuses
  `--script` and `--unlocked`. In software, Prism launches a recording stand-in; the real Player then plays
  the recorded arguments on the test display. This checks the Library/build contract, not a real Player
  window inside Prism or native hardware readiness.

  Each child must exit successfully. Studio must emit exactly one `QQ-BUILD` JSON object; a success-looking
  result never overrides a failed process. Each Player call requires an unoccupied report path before
  launch, then a fresh regular file containing a UTF-8 JSON object. Existing reports are refused without
  deleting or replacing them. Playback also requires a finite, nonnegative first-frame measurement;
  a missing marker, boolean, string or invalid number does not show that a frame appeared. Malformed/ambiguous
  results fail; missing Prism stages still return 77, which remains skipped rather than passed.

The samples are QQ scenes you can open in QQ Studio, for example
`products/qq/samples/orb-run`. Running them inside Prism, with Prism placing
their windows and speaking their launcher's protocol, is the next part of
Phase 1.

## 7. Troubleshooting

| Symptom | Fix |
|---------|-----|
| `Cannot open include file: 'rep_…_replica.h'` | Reconfigure; generated headers live in the build tree (`services/`, `tools/qorctl/`) |
| Configure can't find Qt | Set `QOR_QT_DIR` to the kit (`C:\Qt\6.12.0\msvc2022_64`) or pass `-DCMAKE_PREFIX_PATH=…` |
| `cl` not found with Ninja presets | Dot-source `tools\dev\Enter-QorShell.ps1`, or use the `windows-vs` preset |
| No console output from `qor-shell` | It is a GUI-subsystem app. Set `QT_FORCE_STDERR_LOGGING=1` and redirect stderr. Use `QT_LOGGING_RULES="qor.*=true"` for QOR OS categories |
| Test output missing in `ctest` | Tests set `QT_FORCE_STDERR_LOGGING=1`; run the binary with `-o -,txt` to see QtTest output directly |
| Low frame rate on a laptop | Check the log line `GPU: …`. Use `--gpu high-performance`, or force an adapter with `QT_D3D_ADAPTER_INDEX` (`QSG_INFO=1` lists adapters) |
| Shader or graphics issues | `QSG_INFO=1` prints the RHI backend and adapter; `QSG_RHI_BACKEND=vulkan\|opengl\|d3d12` switches backends |
| Services did not initialise in time | Another QOR OS instance holds the bus's socket (`$XDG_RUNTIME_DIR/qor-services` on Linux, `local:qor-services` elsewhere). The host falls back to a PID-suffixed endpoint; pass `--services-url` to `qorctl` |
| `QOR core: No QOR core at …` or Corrosion cannot find `cargo` | Install Rust from rustup (1.91+), open a new terminal, and reconfigure; or turn the core off with `-DQOR_BUILD_CORE=OFF` |
| The core's configure cannot fetch demiurge-chain | It needs network access to GitHub once per build tree; or point `QOR_CORE_SOURCE_DIR` at a local checkout |
| A product's suite fails on a busy desktop | Leave the desktop alone while the suites run, or run `products.ps1` again; a failure that repeats is real |
