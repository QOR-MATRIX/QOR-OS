# Architecture

This document describes the code as it exists today and how it maps onto the
device architecture in [`VISION.md`](../VISION.md). Decisions and their
rationale are recorded in [`decisions/`](decisions/README.md).

**Public homepage.** `site/` is the static QORSYNC.DEV homepage, built separately
with Node 24 by the existing Astra Matrix / qor Vercel project from QOR OS `main`
([QOR-0027](decisions/QOR-0027-the-homepage-lives-in-qor-os.md)). Only `site/dist/`
is public output. Device binaries, repository files and dependency directories
are excluded. The network and pinned core stay upstream; see [HOSTING.md](HOSTING.md).

**Standalone Qonsole source UI.** `apps/qonsole/Main.qml` opens an original Qt
Quick-only Window using `Launcher.qml`, with a local sample and discrete input
actions. `apps/qonsole/main.cpp` now supplies a dedicated C++/Qt host via the
independent CMake subproject, embedding the original QML module as resources.
Its local verification build uses installed Qt DLLs, with no deployment rule.
It imports no QOR services, core or simulated shell. The Windows host source
adds foreground-only Raw Input for original DS4 USB-shaped reports, reusing the
project's PlayStation parser and dispatching semantic actions without HID output
or controller/keyboard emulation. Its isolated fixtures pass, and a separate
normal Release candidate builds and passes the public Windows keyboard/focus
flow. Native foreground ownership is observed; actual Raw Input registration
readback and physical retest remain unverified. The owner's Cross failure in the
preserved old EXE remains open. Keyboard/visible evidence stays separate from
controller, full lifecycle, clean-PC and console acceptance.
This small source entry point complements the premium C++/Qt stack. Shared
services/apps/data/interfaces and parity across editions remain the goal; the
[canonical capability matrix](ROADMAP.md#alongside-the-phases--qonsole-device-edition)
owns readiness. Demiurge/QOR ID/Auth integration code is on hold; its current
ownership and core pin are unchanged.

**Q Pi development edition — execution on hold.** [QOR-0025](decisions/QOR-0025-q-pi-development-edition.md)
scopes a Pi 5 ARM64 exception: a maintained Raspberry Pi OS desktop and real base
login host native QOR application processes. It supplies no Prism authentication,
simulator or simulated device services. Existing embedded shell applications are
not automatically standalone apps; QQ still imports Quick 3D even for 2D content.
Recorded native ARM64 application builds and selected software suites passed; no Pi image
or physical-device acceptance is established. The Fedora full-device recipe and its
authentication guard are unchanged. See [the preparation and first-boot plan](../image/README.md#q-pi-development-edition).

QQ's `q-pi-release` and `q-pi-asan` presets are native ARM64 build entry points,
not build results. Their early host/Qt checks and the static payload audit reject
known unsuitable inputs without proving entitlement, complete dependency ABI or
hardware behavior. The public host-receipt helper prepares a headless trust path;
its first-boot hook, stable-key reboot, shutdown and access gate are not integrated.
See the [native build procedure](../image/README.md#native-qor-application-build)
and [headless design](../image/README.md#q-pi-first-boot-and-bluetooth-bootstrap).

## 1. Layers

```text
┌───────────────────────────────────────────────────────────────────────┐
│ Products       QQ (Studio, Player, runtime) · ARQADE · QOR Media      │  from demiurge-
│                each its own process and CMake project                 │  chain
├───────────────────────────────────────────────────────────────────────┤
│ Apps           QOR.Apps        Files · Music · Notes · Terminal · …  │
│ Shell          QOR.Shell       Desktop · Rail · Shelf · Beam · …     │
│ Platform API   QOR.Services    typed replicas + Intents              │
│ Design system  QOR.Kit, Lumen  tokens · Theme · Spring · materials   │
├───────────────────────────────────────────────────────────────────────┤
│ Hosts          qor-shell (any desktop OS)  │  prism (Wayland, Linux) │
├───────────────────────────────────────────────────────────────────────┤
│ Services       qor_sim / qor-simd (simulation)  │  planned qor-*d │
│                ── Qt Remote Objects contracts: services/interfaces ── │
│ QOR core       qor::core (Rust, demiurge-chain at a pinned commit)    │
│                ── C bridge: one JSON request, one JSON answer ──      │
└───────────────────────────────────────────────────────────────────────┘
```

The products are separate programs today. Prism hosts them as Wayland clients,
passes Lumen colors to them, and QQ's runtime uses QOR.Input's controller backends.
Native hardware acceptance remains open in [ROADMAP.md](ROADMAP.md). The QOR core
is linked into a test today. Phase 2 makes it the vault service, a process of its own.

**Startup identity.** The shared Desktop/DisplayDesktop path uses Kit's finite
`StartupSplash` over the existing lock/session surfaces. It releases on the first
swapped frame, skips replay on hotplug, respects reduced motion and has Escape and
a bounded watchdog. It starts no service and changes no sign-in decision. The same
reviewed QorMark rendering is checked before linking and staged for Plymouth through
`qor_boot_payload`; the image/bootloader/KMS handoff remains unproven
([image](../image/README.md#branding-the-startup-stages)).

**First run and installation.** `setup/` builds the private `qor-setup` application
(`QOR.Setup`, not a public SDK). It uses Kit/Lumen and the same official mark.
Prism requests it once after an external device session unlocks, excluding simulation
and a live-media marker. Closing leaves it pending; completion publishes a small
per-user record without replacing unrecognised data. It creates no account and performs
no installation. The real authentication services and full device installer
are still missing. The welcome receives a launch snapshot of the session's motion/contrast
preferences, defaults to no motion without a known hint, and changes no system preference.
`qor_image_payload` packages the startup theme, first-run source,
identity and inactive Calamares branding/settings. The explicit device Containerfile
requires an existing approved device base; the data-only image cannot boot.
[QOR-0023](decisions/QOR-0023-installation-uses-proven-backends.md) sets the backend
and disk-review boundaries; [the capability map](../image/README.md#installation-capability-map)
distinguishes code, configuration and missing runtime evidence.

The separate private VM lab ([QOR-0024](decisions/QOR-0024-disposable-vm-installation-lab.md))
uses a digest-pinned Fedora bootc base and restricted QEMU TCG. Its opt-in
`qor-install` frontend delegates whole-disk operations to stock bootc, with no
custom partitioner. Before writing, a sealed guest allowlist must match the new
regular-file disk's UUID, serial, capacity and driver/PCI ancestry; host files
are pinned and passed as descriptors. The offline OCI directory and receipt must also
be on the read-only installation ISO, not a mutable live overlay. Display
selection matches the sealed VM's VirtIO graphics slot rather than assuming
`card0`. The default live ISO is a display probe; the opt-in installation profile
adds the guarded offline backend. Port-ready events start its restricted agent and
frontend. The installer pairs SELinux's existing `install_t` process domain with
the executable's `install_exec_t` entrypoint label; no enforcement fallback or new
policy permission is added. The earlier clean `feed626` image passes actual stock
installation, media-free first run and persistence through two installed reboots.
The host QEMU main-loop pool uses four I/O workers within its 64-task container
limit; its source checksum is separate from the unchanged image/compiler identity.
The evidence keeps resource interruptions, distinct guest boot IDs and the
8 November UI reboot failures. An inventory correction excludes only inactive,
zero-size kernel loop nodes without backing attributes. A separate diagnostic finds
bootc's storage mount retained after successful exit. The backend now confines its
mounts to the supervised process and requires the unchanged strict target guard
before announcing completion. The clean `66df3f7` image and compiler now pass stock
installation and the actual UI reboot on a fresh 8-GiB target without diagnostic
cleanup. Two media-free installed boots prove the real welcome changes from pending
to complete, then retains the same record before the next welcome. All three native
VM phases exit 0 with three distinct boot IDs. The earlier `feed626` proof is unchanged.
These VM checks do not establish device services,
authentication, hardware, encrypted installation, rollback or CI isolation proof.
The full device-layer guard requires headless build contracts from both welcome
and Prism. Prism reports its compiled simulator mode before starting graphics,
audio or service connections. It currently reports authentication unavailable,
matching `Desktop.tryUnlock`'s refusal of non-simulated sessions. A simulator build,
missing authentication or unknown contract prevents structural image acceptance;
this contract check supplies no authentication backend or boot proof.

**Simulation and device share all QML.** The shell and the apps never talk to
a service implementation directly. They bind to Qt Remote Objects *replicas*.
In simulation the *sources* are the classes in `sim/`; the planned device sources
are the `qor-*d` daemons. The host program decides which sources it connects to.

## 2. Build targets

| Target | Kind | Module URI | Source | Purpose |
|--------|------|------------|--------|---------|
| `qor_kit` | static lib + QML plugin | `QOR.Kit` | `kit/` | Tokens, Theme, Motion, Materials, Spring, Oklch, SceneRect, FrameStats, Surface, Glass, Backdrop, RoundMask, Wallpaper, Icon, AppIcon, Artwork, QorMark, Spectrum, Sounds, building blocks |
| `qor_lumen_style` | static lib + QML plugin | `Lumen` | `kit/style/` | Qt Quick Controls 2 style (24 controls, Basic fallback) |
| `qor_gamepads` | static lib | — | `input/gamepads/` | Game controllers: XInput, HID and evdev backends and `GamepadManager`, with Qt Core and Gui only, so QQ links it too ([CONTROLLERS.md](CONTROLLERS.md#games)) |
| `qor_input` | static lib + QML plugin | `QOR.Input` | `input/` | The controllers (`qor_gamepads`) for QML: `Gamepads`, `ControllerNavigator`, `ControllerGlyph` ([CONTROLLERS.md](CONTROLLERS.md)) |
| `qor_audio` | static lib | — | `audio/` | The audio engine: real-time output, QOR Sessions synth, file decoding, EQ, limiter, analysis ([AUDIO.md](AUDIO.md)) |
| `qor_services` | static lib + QML plugin | `QOR.Services` | `services/` | `Services` singleton (replicas), `Intents`, `Reactive`, QML type info for replicas |
| `qor_sim` | static lib | — | `sim/` | Simulated sources (audio and media on `qor_audio`) + scenario loader + QtRO host |
| `qor_apps` | static lib + QML plugin | `QOR.Apps` | `apps/` | `AppRegistry` and seven embedded apps, including the Store |
| `qor_store` | static lib + QML plugin | `QOR.Store` | `store/` | Catalogue, package validation, installation and storefront models |
| `qor_setup` / `qor-setup` | static QML module / executable | `QOR.Setup` | `setup/` | Private first-run welcome; no account creation or installation |
| `qor-install` | executable, opt-in | — | `setup/` | Guarded private VM installer; requires `QOR_BUILD_VM_INSTALLER`, no simulator and Linux |
| `qor_shell` | static lib + QML plugin | `QOR.Shell` | `prism/shell/` | `Main`, `Desktop` and every shell surface |
| `qor-shell` | executable | — | `prism/host/` | Simulation host: window, sim services, CLI, screenshot, benchmark, GPU preference |
| `prism` | executable | `QOR.Prism` | `prism/compositor/` | Wayland compositor (Linux, needs Qt Wayland Compositor) |
| `qor-simd` | executable | — | `sim/simd/` | Services in a separate process |
| `qorctl` | executable | — | `tools/qorctl/` | CLI client of the services |
| `qor-gamepads` | executable | — | `tools/gamepads/` | Lists game controllers and streams their state |
| `qor_lumen_widgets_style` | plugin (`styles`) | — | `platform/style/` | Lumen `QStyle` for Qt Widgets |
| `qor_platformtheme` | plugin (`platformthemes`) | — | `platform/theme/` | `qor` QPA platform theme |
| `lumen-widgets-gallery` | executable | — | `platform/gallery/` | QStyle preview |
| `qor_core` (`qor::core`) | static lib (Rust) + header | — | `core/` | The QOR core from demiurge-chain at `QOR_CORE_COMMIT`, built by Corrosion; `qor_core.h` is its C bridge ([§10](#10-the-qor-core)) |
| `tst_*` | tests | — | `tests/` | See [§9](#9-testing) |

The products are not targets of this build. Each is its own CMake project,
built through `tools/dev/products.ps1` on Windows or the isolated
`tools/dev/products.sh` harness on Linux
([§11](#11-the-products)).

The root build's QML modules are static libraries linked into the executables
with their plugins. Qt's CMake integration compiles their QML; the observed
Windows commercial kit uses `qmlsc`. The import metadata lands in one directory
(`build/<preset>/qml`) for `qmllint` and `qmlls`.

### Module dependency graph

```text
QOR.Shell ──► QOR.Apps ──► QOR.Services
     │              ├──► QOR.Input
     │              ├──► QOR.Store
     └──────────────┴──► QOR.Kit ◄── Lumen (QQC2 style)
```

`QOR.Kit` has no dependency on other QOR OS modules. Apps depend on Kit,
Services, Input and Store, never on the Shell. The Shell hosts the apps.

## 3. Rendering model

### One scene graph

The desktop is a single Qt Quick scene. Windows (`AppWindow`) are items. In
simulation their content is an app component; under `prism` it is a Wayland
`ShellSurfaceItem`. Because windows are items, Overview, Spaces, minimise and
snapping are just item geometry driven by springs.

### Layering inside `Desktop`

```text
content        Wallpaper · focus-pull overlay · WindowLayer      ← sceneBackdrop source
chrome         SpaceSwitcher · Rail · Shelf · NotificationStack · MediaPopover · ControlCenter · Beam
LockScreen
display sim    Night Light tint · brightness dimmer
```

### Materials: shared backdrops

`Backdrop` renders its `sourceItem` once per change into a texture at
`downsample` (0.25) resolution, blurs it with `MultiEffect`, and exposes the
result as a texture provider. There are two:

| Backdrop | Source | Used by |
|----------|--------|---------|
| `wallpaperBackdrop` (`Materials.desktop`) | wallpaper only | Frost inside windows (`FrostSidebar`, Terminal), lock focus-pull |
| `sceneBackdrop` (`Materials.scene`) | wallpaper + windows | Shell chrome Glass (Rail, Shelf, Beam, Control Center, banners) |

Each `Glass` samples the shared texture at its exact on-screen rectangle.
`SceneRect` re-maps that rectangle on `QQuickWindow::afterAnimating`, once per
rendered frame and never on its own, so any number of glass panels costs one
blur, and only when their source changes. Frost samples the wallpaper-only
backdrop, so a window never samples itself (no recursion).

### Primitives are signed distance fields

`Surface`, `Glass` and `RoundMask` draw continuous-curvature rounded
rectangles (an Lᵖ-norm corner, exponent 4) analytically in one fragment shader
pass, including anti-aliasing, a hairline with specular falloff, an inner
sheen and a soft shadow. There are no textures or nine-patches, and they are
resolution-independent.

### Render on demand

Nothing in the shell requests frames continuously. Springs tick only while
moving (`QAbstractAnimation` driver stops on settle). The clock wakes once per
minute. The Aurora wallpaper is a cached layer (zoom and parallax are item
transforms of that texture). `FrameStats` measures frames passively. When the
desktop is idle, the render loop is idle.

Shaders live in `kit/shaders/*.frag` and are compiled to `.qsb` (SPIR-V, GLSL,
HLSL, MSL) by `qt_add_shaders`, so the same code runs on Direct3D, Vulkan,
Metal and OpenGL through QRhi.

## 4. Motion

`Spring` (C++) is the only animation primitive for motion that should feel
physical. It integrates the closed-form solution of a damped harmonic
oscillator (under-, critically- and over-damped cases), so it is exact at any
frame rate. Retargeting keeps velocity, and gestures can set velocity
directly. `Kit.reducedMotion` makes every spring jump to its target. Presets
(`snappy`, `smooth`, `bouncy`, `gentle`) come from the tokens. See
[design/LUMEN.md](design/LUMEN.md#motion).

## 5. Color and theming

Tokens define every color role in OKLCH for light and dark appearance
(`followAccent` roles take the accent hue as an offset). `Theme` resolves them
through `Oklch.themed()`, interpolating in OKLab by `Theme.darkness`, a spring
between 0 and 1. The accent hue is also a spring that takes the short way
around the hue circle. Every color in the system therefore morphs
continuously.

The shell binds `Theme.dark`, `accentHue`, `reduceMotion`,
`reduceTransparency` and `livingWallpaper` to the settings service, so
`qorctl theme light` restyles the running system.

The same tokens are generated into `platform/common/lumentokens.h` for the
Widgets `QStyle` and the QPA platform theme, and into `docs/design/tokens.md`.

## 6. Services and IPC

```text
                   services/interfaces/*.rep   (repc)
               ┌───────────────┴────────────────┐
         *SimpleSource (sim/)              *Replica (services/, tools/qorctl)
               │                                │
   QRemoteObjectHost  ◄── local:qor-services ──►  QRemoteObjectNode
   (qor-shell embedded, or qor-simd)          (Services singleton, qorctl, tests)
```

- Endpoint: on Linux a socket in the person's runtime folder,
  `$XDG_RUNTIME_DIR/qor-services`, which only they can enter
  (`services/endpoint/serviceendpoint.h`); `local:qor-services` (named pipe or
  socket) on Windows and macOS. If the default is taken, the host falls back to
  `…-<pid>`. It takes no authentication and trusts what it is sent, so nothing
  sandboxed is ever given it ([QOR-0018](decisions/QOR-0018-the-security-and-quality-bar.md)).
- `Services::connectTo()` acquires all replicas and waits until each has its
  initial state before QML loads, so the UI never flashes default values.
- Replicas are registered for QML with `QML_FOREIGN` (`NetworkService`,
  `PowerService` and so on). `cmake/QorRepc.cmake` generates them so AUTOMOC
  emits metatypes, which gives `qmllint` and `qmlsc` full type information.
- Apps request system actions through `Intents` (`app.open`, `session.lock`,
  `shell.overview`, `shell.beam`, `shell.space`), which only the shell
  fulfils.

Contracts are documented in [SERVICES.md](SERVICES.md).

## 7. Window management

`WindowLayer` owns the windows (`AppWindow` instances on a horizontal strip,
one screen-width per Space).

- **Open**: the window flies out of its Shelf icon (`origin`), driven by a
  `presence` spring. Close shrinks in place. Minimise flies back to the Shelf.
- **Move and resize**: `DragHandler`s write geometry and `snap()` the springs
  so tracking is 1:1. On release, `snapTargetFor()` applies magnetic halves or
  maximise.
- **Overview**: `relayout()` computes a grid and sets `overviewRect`/`overviewScale`.
  Springs animate there. Window layers enable mipmapping while scaled.
- **Spaces**: `spaceOffset` (a spring) slides the strip, and the wallpaper
  parallax follows.
- **Focus**: z-order counter, focused windows get elevation 5, others 3.

Under `prism`, `WindowLayer.surfaceDelegate` is a `ShellSurfaceItem` component
and `adopt()` wraps each xdg-toplevel in the same `AppWindow`.

- **A client's window opens at the size the client asks for.** Prism's first
  configure has no size (0 × 0), so the client chooses. The window stays
  unseen (`AppWindow.awaitingSize`) until the client's first commit says what
  it chose. Prism then plans the frame around that size, cascaded and within
  the work area (`planFrame()`), configures the client to fit, and the window
  flies in.
- **A product Prism hosts is known by its process,** not by what its toplevel
  says. Its window keeps the product's id and name from Prism's record. A
  product Prism places opens at the frame `planClient()` planned, the plan
  Prism sent the product when asking it to show. A product Prism only starts
  opens at its own size, like any other client ([§11](#hosted-by-prism)).

## 8. Hosts

### `qor-shell` (simulation)

1. Applies the GPU preference: high-performance by default via DXGI on Windows ([PERFORMANCE.md](PERFORMANCE.md#gpu-selection)).
2. `Kit::initialize()` registers Inter and JetBrains Mono and sets the application font.
3. Selects the `Lumen` Quick Controls style.
4. Starts `SimServices` in-process (or connects to an external host) and calls `Services::connectTo()`.
5. Loads `QOR.Shell/Main` with an `options` map from the command line.
6. Optionally runs the screenshot timer or the `--benchmark` tour.

### `prism` (device, experimental)

`Compositor.qml` creates a `WaylandCompositor` with one `WaylandOutput` whose
window hosts the same `Desktop`. It plugs a `ShellSurfaceItem` delegate into
the window layer and implements xdg-shell, xdg-decoration (server-side),
text-input and Qt's text-input-method. It drives one output per display
(`--outputs N` nested; every screen under eglfs). It hosts the products,
starting them as Wayland clients of its own ([§11](#hosted-by-prism)). It runs
nested today. Prism's
own KMS backend is Phase 3
([QOR-0005](decisions/QOR-0005-every-display-at-its-own-refresh.md),
[QTOS-0003](decisions/qtos/QTOS-0003-compositor-strategy.md)).

The local warning-removal candidate uses `QWaylandCompositorExtension` directly
for five custom managers, each returning its protocol interface, and checks
`configureAcked(QObject*)` with `qobject_cast` before accessing private toplevel
state. On 12 November, the focused Linux Release and ASan/UBSan builds and their
existing raw-client/integration suites passed in hidden, GPU-fenced software.
The raw-client suite includes unique global advertisement and manager
bind/destroy/rebind, including cursor-shape v1/v2 and last-binding destruction.
Root QML lint and hardening passed in both configurations at the `01ba206`
checkpoint; the focused builds exclude the core library and do not establish
hardware/full-OS acceptance. The later local increment moves the unchanged
`WindowManagement` class into `prism/compositor/windowmanagement.{h,cpp}`, shared
by Prism and `tst_windowmanagement`. That suite exercises null and unrelated
objects directly and through the Qt invokable interface. The raw-client suite
adds pending/older/latest acknowledgement and unchanged-geometry commit cases.
The new test target now resolves Qt's Wayland imports in its own CMake directory.
The Windows/WSL continuation stopped at the unchanged memory guard; its failed
attempts remain in `build/prism-boundary-2026-11-12/`. Subsequent native ARM64
Release and ASan/UBSan builds completed on 12 November. The acknowledgement
fixture first exposed its own invalid focus-stealing assumption; it now asserts
that policy and explicitly activates the second window through the normal Shelf
path. Each configuration passes all seven selected suites without skips,
including 43 pointer-capture and six window-management Qt Test results.
All 11 programs in each configuration pass hardening and 16-KiB layout checks;
Release QML lint has zero warnings. These focused builds exclude the core,
use software rendering and product stand-ins, and do not check memory leaks.
Their 50 preserved evidence files were hash-verified on 13 November, along with
the unchanged source snapshot and test overlay. Native receipts:
`build/q-pi-connect-2026-11-12/session-01/`.
See [the current Phase 1 work](ROADMAP.md#phase-1--one-stack).

## 9. Testing

| Suite | Framework | Covers |
|-------|-----------|--------|
| `tst_spring` | Qt Test | spring physics: damping regimes, settling, velocity preservation, presets, reduced motion, stability |
| `tst_oklch` | Qt Test | OKLCH/OKLab conversions, gamut mapping, themed tokens, perceptual mixing |
| `tst_services` | Qt Test | end-to-end QtRO: state replication, replica→source writes, remote slots and signals, Wi-Fi and media behaviour |
| `tst_audio` | Qt Test | the audio engine: rendering, sessions, decoding, EQ, limiter, analysis |
| `tst_input` | Qt Test | controller reports, CRCs, mappings, dead zones, the manager and the navigator |
| `tst_store` | Qt Test | catalogue and package signatures, installation, tampering refused |
| `tst_storefronts` | Qt Test | the two storefronts as pages have them: that only a demonstration build holds their key, no preset says otherwise, every query names its view, and what a catalogue hands over from the hostile side (hostile pictures, licences) |
| `tst_storefronts_qml` | Qt Quick Test | the Store's real pages over a catalogue of markup: each store's list, search, featured row and counts hold nothing of the other's, plain text throughout, the switch in Settings |
| `tst_store_format2` | Qt Test | catalogue format 2 and every package check from the hostile side: archive bombs, unsafe and colliding names, what a pack may hold, sequence and expiry, drafts, the two storefronts' views, creator features, dependencies |
| `tst_qorcore` | Qt Test | the QOR core through its C bridge: version, units, exact amounts, refusals ([§10](#10-the-qor-core)) |
| `tst_producthost` | Qt Test | Prism's side of the host protocol: the handshake, who may connect, the long poll, a real product process with its token on stdin only, nothing secret logged; products started with no channel, one run at a time ([§11](#hosted-by-prism)) |
| `store_widgets_qmllint` | qmllint | the Store's widgets against the built modules |
| `tst_shell` | Qt Quick Test | Beam fuzzy ranking, calculator (including injection rejection), section grouping |
| `tst_kit_qml` | Qt Quick Test (GPU) | tokens, theme roles, typography, springs in QML, Oklch singleton |
| `tst_golden` | Qt Test (GPU) | every Kit component in every variant against reference images |
| `shell_e2e` | scripted shell (GPU) | whole flows with synthetic input, controllers included |
| `prism_e2e` | Python + real clients (GPU, Linux) | the compositor with real Wayland clients: windows, Spaces, displays |
| `tst_pointercapture` | Qt Test + raw Wayland clients (Linux, offscreen, in software) | pointer lock, confinement and relative motion from the hostile side: who may, after what, what ends them, the way out, every protocol error, floods ([SHELL.md](design/SHELL.md#capturing-the-pointer)) |
| `prism_products` | scripted Prism (Linux, offscreen, in software) | Prism starting stand-ins for ARQADE, QOR Media, QQ Studio and the QQ Player, laying out and naming their windows, passing the Motion setting |
| `tools_generate_tokens` | Python unittest | color math, generated outputs current |

`all_qmllint` must report zero warnings. Build's baseline CTest steps exclude
the `gpu` label; separate GPU steps require their workflow opt-in. Qt CI jobs
remain paused pending isolation, so registered suites are not current CI passes
([CI.md](CI.md#what-ci-trusts)). The products' suites are listed in [§11](#11-the-products).

## 10. The QOR core

The QOR core is the Rust library that holds the keys and talks to the QOR
network. Its source is `tools/qor-launcher/core/` in demiurge-chain, and
QOR OS never copies it
([QOR-0003](decisions/QOR-0003-the-qor-core-at-a-pinned-commit.md)).

- `core/CMakeLists.txt` fetches demiurge-chain at the pin, `QOR_CORE_COMMIT`
  (or uses `QOR_CORE_SOURCE_DIR`), and Corrosion builds the `qor-launcher-core`
  crate as a static library from its `Cargo.lock`, with the `desktop` feature:
  every module demiurge-chain's L13.2 moved into the core, as the Windows
  launcher builds it. Those modules are QOR ID's and Social's clients, the
  chain client, the Library's index, the Mesh and Qontrol.
- The bridge is three C functions:
  - `qor_core_version()`;
  - `qor_core_call(request)`, which takes `{"command": …, "args": {…}}` and
    returns `{"ok": …}` or `{"error": {"kind", "message"}}`;
  - `qor_core_free(answer)`.

  Amounts cross as decimal strings of embers, and no command answers with a key.
- At the current pin the bridge answers `core_version`, `token_info`,
  `parse_amount` and `format_sparks`. The modules L13.2 moved in are compiled
  and linked but not yet reachable through it; the bridge grows upstream as
  Phase 2 needs them.
- `QOR_BUILD_CORE` is on where `cargo` is found, and the CI presets require it.

## 11. The products

QQ, ARQADE and QOR Media came from demiurge-chain with their history, at the
paths they had there, and are developed here since
[QOR-0015](decisions/QOR-0015-the-products-are-developed-here.md)
([UPSTREAM.md](UPSTREAM.md)). ARQADE's rules and recorded runs stay shared
with demiurge-chain, where ARQADE's server judges runs by them. Each product
is its own CMake project. `tools/dev/products.ps1` calls its Windows `build.ps1`;
`tools/dev/products.sh` configures and tests it in the isolated Linux harness.

QQ's `World` selects packaged, precomputed lighting when all four sky colours
match either palette authored in its bundled scenes. The default also serves
Orb Run; the other serves Ember Climb. Selection is by colours, so any world
using those values shares the same path. Other colours retain procedural sky
generation and batched edits, and explicit sky images retain precedence.
Exposure, rotation, skybox blur and rendering quality are unchanged. The KTX
textures are embedded in QQ's shared runtime; their source EXR images and
generation hashes are kept in `products/qq/runtime/skies/`. They are generated
explicitly under the existing native resource/isolation procedure, not rendered
as an implicit part of a host build. See [QQ's build notes](../products/qq/README.md#build-test-deploy).

| Product | Path | Targets | Suites |
|---------|------|---------|--------|
| **QQ** | `products/qq/` | `qqruntime` (QML module `QQ`), `qq-studio` (`QQ.Studio`), `qq-player` (`QQ.Player`), `qq-mcp`, `qqagent` | `tst_world`, `tst_scene`, `tst_studio`, `tst_play`, `tst_player`, `tst_storegame`, `tst_agent`, `tst_lockdown`, `tst_publish`, `tst_workspace`, `tst_gamebuild`, `tst_assetlibrary`; Linux also `tst_playerwall` and, with Python, `first_journey` |
| **ARQADE** | `products/arqade/qt/` (+ `games/`, `tests/fixtures/`) | `arqade` (QML module `ARQADE`), built on QQ's runtime from `../../qq/runtime` | `tst_arqadelink`, `tst_arqadeserver`, `tst_interface`, `tst_games` |
| **QOR Media** | `tools/qor-media/` | `qor-media` (QML module `QOR.Media`), with PDFium | `tst_medialink`, `tst_files`, `tst_textfile`, `tst_playback`, `tst_look`, `tst_reader`, `tst_pdfpages`, `tst_pdfmarks`, `tst_pdfforms` |

Their visual suites render real windows and drive them with synthetic input. The
Linux harness provides a hidden display and private session/keyring; WSL is GPU-fenced
by default. Software rendering is not native GPU evidence. The
look tests read `docs/design/tokens.json`, Demiurge's one look. They build on
Windows with their own scripts and on Linux with `tools/dev/products.sh`
([BUILDING.md](BUILDING.md#on-linux)). Prism hosts all of them
([below](#hosted-by-prism)); Lumen color propagation and QQ's QOR.Input integration
are already present. Remaining platform and hardware work is in the roadmap.

Current platform seams:

| Seam | Today | On QOR OS |
|------|-------|-----------|
| Placement | The launcher's `place` command parents the product's window into its own by native handle (`Dock`, `products/arqade/qt/src/dock.cpp`, `tools/qor-media/src/dock.cpp`), which Wayland cannot do | On Wayland the product's window stays a toplevel and Prism places it, speaking the launcher's side of the host protocol ([QOR-0010](decisions/QOR-0010-on-wayland-prism-places-the-products-windows.md), [below](#hosted-by-prism)); `Dock` stays for Windows |
| Controllers | QOR.Input's XInput/HID backends feed QQ's `Input` (`products/qq/runtime/input.cpp`) on Windows | QOR.Input's Linux backend feeds the same gamepad state; real-controller acceptance remains separate ([CONTROLLERS.md](CONTROLLERS.md#games)) |
| The agent's key | Windows Credential Manager (`products/qq/agent/keychain.cpp`) | The Secret Service, over Qt D-Bus, in the same file: the creator's own provider key, not a QOR key ([QOR-0011](decisions/QOR-0011-the-creators-provider-key-in-the-secret-service.md)). The image must ship a provider (Phase 3) |
| GPU choice | DXGI's high-performance adapter (`products/qq/runtime/gpu.cpp`, ARQADE's `main.cpp`) | Mesa and Prism's own outputs; nothing to do in the products |

The host protocol (ADR-046, ADR-102) is what the products already speak to a
launcher: a `--launcher-channel` local socket, a one-time token on standard
input, 4-byte length-prefixed JSON frames, and one long-poll request
(`arqade.next`, `media.next`) answered with a command: `place`, `hide`,
`theme`, `player`, `prefs`, `play`, `open` or `quit`.

### Hosted by Prism

`ProductHost` (`prism/shell/producthost.h`, the QML singleton `ProductHost`)
is Prism's side of that protocol, version 1, to the same contract as the QOR
Launcher's (demiurge-chain's `tools/qor-launcher/src-tauri/src/host/`):

- **Records.** Prism knows each product by a record of its own:
  - ARQADE (`arqade`, long poll `arqade.next`), which Beam offers;
  - QOR Media (`qor-media`, `media.next`), which opens a file Prism hands it
    (`Desktop.openInMedia()`);
  - QQ Studio (`qq-studio`), which Beam offers;
  - the QQ Player (`qq-player`), which plays the scene Prism hands it
    (`Desktop.playQQGame()`), one game at a time.

  A product's program is where its variable points (`QOR_ARQADE`,
  `QOR_MEDIA`, `QOR_QQ_STUDIO`, `QOR_QQ_PLAYER`), else beside Prism's own.
  Prism offers a product only where its program is and Prism has a display
  for it, so `qor-shell` offers none.
- **Only started.** QQ Studio and the QQ Player get no channel (`launch()`).
  QQ Studio's channel is for publishing (`qontrol.commit`,
  `cartridge.publish`), which Prism does not offer before the vault service
  (Phase 2); started without one, the Studio says that publishing needs the
  launcher. The QQ Player speaks no protocol. Each gets its file as an
  argument (`--project <folder>`, or `-- <scene>`), its stdin is empty, and
  its window opens at the size it asks for ([§7](#7-window-management)).
  Asked to stop, or when Prism ends, it is asked to end at once.
- **Start.** ARQADE and QOR Media run as Wayland clients of Prism
  (`WAYLAND_DISPLAY`, `QT_QPA_PLATFORM=wayland`) with
  `--launcher-channel <name>`, the person's Motion setting
  (`QOR_REDUCED_MOTION`), a trusted `XDG_ACTIVATION_TOKEN` for its first
  window, and a one-time token on its stdin, never in its arguments or
  environment. On Linux it gets SIGTERM if Prism dies.
- **The channel.** A local socket in a folder only this user can enter (a
  named pipe on Windows). It takes one connection, from the process Prism
  started as the operating system reports it, and then closes to everyone.
  The hello must carry the token within 30 seconds. The token is compared in
  constant time and used once. Prism offers one method, the product's long
  poll, and answers any other `unsupported_method`, so ARQADE's sign-in and
  website (`arqade.signin`, `arqade.web`) wait for Phase 2.
- **Commands.** `place` carries the content rectangle Prism planned
  (`WindowLayer.planClient()`) and no parent. `open` is QOR Media's. `prefs`
  goes to ARQADE when the Motion setting changes, and `quit` stops it.
  `theme` carries the colours the products wear, Lumen's roles under the
  names of Demiurge's look ([QOR-0016](decisions/QOR-0016-the-products-wear-lumen.md)),
  as a product connects and whenever they change; every product, QQ's with
  no channel too, is also given them as it starts (`QOR_THEME_COLORS`). A
  newer place, theme, player, prefs or play replaces one not yet sent. `play`
  asks ARQADE for one of its games, from Home or the Library, and `open`
  carries where a film or song resumes when the Library knows. What QOR
  Media reports on its poll, where a file stopped and how long it is, goes to
  the Library (`ProductHost.reported`), checked for its type and size.
- **The controller.** A product whose record `takesController` (ARQADE, the
  QQ Player) plays with it: while its window has the focus and nothing of the
  shell is open, the controller interface steps aside and Guide brings
  Overview ([CONTROLLERS.md](CONTROLLERS.md#games)). Any other product, QOR
  Media among them, gets the d-pad as the arrow keys, South as Return and
  East as Esc ([CONTROLLERS.md](CONTROLLERS.md#applications)).
- **Windows.** When a toplevel arrives, `productOf()` names its product by the
  client's process id. The window of a product Prism places opens at the
  planned frame; any other at the size it asks for ([§7](#7-window-management)).
  Closing it ends the product; one that fails says so in a toast.

The token and the session's id are never logged; `tst_producthost` looks for
both at every log level. `prism_products` runs Prism offscreen with stand-ins
for all four products.

## 12. Repository conventions

- Generated files are committed and checked for staleness: `kit/Tokens.qml`,
  `platform/common/lumentokens.h` and `docs/design/tokens.md` (from
  `tools/tokens/generate_tokens.py`).
- Every source file carries `SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary`,
  except the files from demiurge-chain, which keep their Apache-2.0 notices.
  The products keep their own formatting.
- Naming ([QOR-0009](decisions/QOR-0009-the-name-is-qor-os.md)): C++ in namespace `qor` (`qor::sim`,
  `qor::lumen`, `qor::host`), targets `qor_*`, programs `qor-*`, logging
  categories `qor.*`, and QML modules under `QOR.*` (beside QOR Media's
  `QOR.Media`) except the style module `Lumen`.
