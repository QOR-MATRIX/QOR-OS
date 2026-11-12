# Performance

QOR OS treats frame pacing as a feature. This document records the budgets,
the measured baseline, how to measure, and the techniques that keep the
shell fast. Early measurements use QTOS's shell; later entries state the QOR OS
change and environment measured.

## Budgets

From [VISION.md §12](../VISION.md#12-performance-budgets-targets-on-the-reference-laptop).
QTOS's Phase 0 measured what the simulator can measure: frame pacing and idle
behaviour.

| Metric | Target | Status (measured in simulation) |
|--------|--------|----------------|
| Compositor frame, shell animating | GPU ≤ 3 ms, CPU ≤ 2 ms | Historical **2.79 ms average frame interval at 360 Hz** on the RTX 4060; includes vsync and does not independently measure GPU or CPU render cost |
| Idle wakeups with nothing animating | ~0 | Design goal: springs stop on settle and the clock wakes once a minute; no current whole-process wakeup measurement |
| First-party app launch → first frame | ≤ 300 ms | Apps are in-process QML in simulation; device measurement with Qt Application Manager remains planned for Phase 3 |
| Beam keystroke → results | ≤ 16 ms | Search is synchronous JS over about 60 entries; not yet instrumented |
| Pointer input → photon | ≤ 1 frame + scanout | Historical input to swap: 5.9 ms p50 on the RTX 4060 at 360 Hz (about 2.1 frames, through the Windows compositor); 9.2 ms on Intel UHD. Device/photon measurement remains planned for Phase 3 |
| Boot / login budgets | ≤ 5 s / ≤ 700 ms | Phase 3 (image) |

## Baseline (2026-10-23)

`qor-shell --open files,music,notes --benchmark 8`: an 8-second tour that
cycles Overview → Beam → Control Center → Space 2 → Space 1 every 750 ms.
Window 1539×844 logical at 125% scaling (1924×1055 physical), Direct3D 11,
`windows-release`.

| GPU | Average | p50 | p95 | p99 | Max |
|-----|---------|-----|-----|-----|-----|
| NVIDIA GeForce RTX 4060 Laptop (360 Hz panel) | **2.79 ms · 358 fps** | 2.77 ms | 3.07 ms | 3.57 ms | 15.6 ms |
| Intel UHD Graphics (integrated) | 7.30 ms · 137 fps | 8.20 ms | 9.11 ms | 11.36 ms | 16.5 ms |

The RTX 4060 holds the panel's 360 Hz refresh through the whole tour. The
single max outlier is first-use pipeline creation when Beam opens.

Re-measured on 2026-10-24 with the shared benchmark driver:

- **RTX 4060:** 2.79 ms average, p95 3.06 ms, with 0.3% of frames missing a
  360 Hz refresh.
- **Intel UHD:** bimodal between runs. Two runs in five reproduce the
  baseline (avg 7.2 ms, p95 9.1 ms); the others sit at p95 about 11.1 ms,
  which looks like a GPU power state. Compare the better of several runs.

Re-measured on 2026-10-30, before and after Prism's windows took the size
their clients ask for (`c2dd379` and `866aa18`, both `ci-windows`, three
runs each, alternating, on the default high-performance GPU at 360 Hz). The
medians:

| Build | Average | p50 | p95 | p99 | Missed | Input to swap p50 / p95 |
|-------|---------|-----|-----|-----|--------|-------------------------|
| Before (`c2dd379`) | 2.78 ms | 2.77 ms | 3.03 ms | 3.32 ms | 0.2% | 5.8 / 6.0 ms |
| After (`866aa18`) | 2.79 ms | 2.77 ms | 3.02 ms | 3.36 ms | 0.3% | 5.9 / 6.1 ms |

The same within run-to-run noise: the change does its work when a client's
window opens, and the tour opens none.

Re-measured on 2026-10-30, before and after the products took Lumen's colours
(QOR-0016: `Theme.productColors` in the Kit, a `theme` for ProductHost). This
was Prism on Linux, `prism --size 1600x1000 --benchmark 8`, hidden: Qt's
offscreen platform and the software scene graph, under WSL, fenced from the
GPU, so no window reached the desktop. It is the same build folder, rebuilt
without and with the change. The medians:

| Build | Runs | Average | p50 | p95 | p99 | Missed | Input to swap p50 / p95 |
|-------|------|---------|-----|-----|-----|--------|-------------------------|
| Before (`3650c4d`) | 3 | 7.35 ms | 7.24 ms | 8.52 ms | 9.22 ms | 0% | 8.00 / 9.76 ms |
| After, straight after its rebuild | 5 | 7.36 ms | 7.23 ms | 8.51 ms | 9.27 ms | 0% | 7.80 / 10.41 ms |
| After, the machine idle | 5 | 7.37 ms | — | 8.54 ms | — | 0% | 8.03 / 9.97 ms |

Frame times are the same. Input to swap counts 40 inputs a run, so its p95 is
the second slowest. The first five runs after the change put it at 10.04 to
11.33 ms; five more on an idle machine put it at 9.51 to 10.62 ms, against 9.53
to 9.86 ms before. The colours are worked out when the theme changes, and the
tour changes none.

Re-measured the same way the same day, after Home and the Library (the
Library's folders read on a worker thread as the shell starts; Home made
but hidden): five runs, median 7.30 ms average (7.22 to 7.36), p50 7.19 ms,
p95 8.46 ms, p99 8.97 ms, no frame missed, input to swap 7.80 / 10.21 ms,
against the ten runs at `7f7f544` above (7.36 and 7.37 ms). The same within
noise: the tour opens neither.

Re-measured on 2026-10-31 for the exploit mitigations and the security batch
([QOR-0018](decisions/QOR-0018-the-security-and-quality-bar.md)), the same way:
three alternating runs of the same code built with and without
`QOR_HARDENING`. Without: median 8.15 ms average (8.05, 8.15, 8.23). With:
8.16 ms (8.16, 8.15, 8.22). The same within noise. Both are higher than the
runs above because the batch's other changes are in both, and the machine was
busier. Ubuntu's compiler adds some of these flags even with `QOR_HARDENING`
off, so this shows the added cost on that compiler, not the whole of it.

### Script failure diagnostics (2026-11-05, software only)

Before and after omitting the failed step's complete JSON request, three runs each of
`prism --size 1600x1000 --benchmark 8 --benchmark-out <fresh-file>` on this machine, before the rebuild and after it.
Both use the existing `ci-linux` normal profile (RelWithDebInfo), commercial Qt 6.12, a hidden offscreen platform and
`QT_QUICK_BACKEND=software` in GPU-fenced WSL. Each run has private HOME/XDG/data directories, null audio and virtual
controllers, a two-processor quota, one renderer worker and a 1.75 GiB hard memory cap. The reports say
`graphicsApi: "unknown"`; these are software observations and establish no native hardware budget or speed improvement.
All six reports are fresh, each with 40 posted and 40 measured input samples. Medians:

| Version | Runs | Average frame interval | p95 frame interval | Input to swap p50 / p95 |
|---------|------|------------------------|--------------------|-------------------------|
| Before | 3 | 8.29 ms | 9.63 ms | 8.19 / 11.27 ms |
| After | 3 | 8.10 ms | 9.32 ms | 7.96 / 10.82 ms |

The tour measures ordinary shell behavior; failure-output correctness has its own process regression. The runs are
sequential. Exact binary/report fingerprints, commands, context and individual observations are retained under
`build/autonomous-2026-11-04/script-privacy-bench-*-record.json` and `script-privacy-bench-verified.json`.

### Benchmark report-write checks (2026-11-05, software only)

The subsequent report-write fix is measured in the same private WSL/software context above. The preceding three after
runs serve as this change's before baseline: their exact binary matches the actual failed-write reproducer, and the
benchmark source was unchanged. Three new after reports use the rebuilt normal compositor. All reports are fresh with
40 posted/measured input samples; the report's graphics API remains `unknown`. Medians:

| Version | Runs | Average frame interval | p95 frame interval | Input to swap p50 / p95 |
|---------|------|------------------------|--------------------|-------------------------|
| Before | 3 | 8.10 ms | 9.32 ms | 7.96 / 10.82 ms |
| After | 3 | 8.07 ms | 9.19 ms | 7.94 / 10.99 ms |

Report I/O happens after the measured tour and input samples. These sequential observations establish no speed or
native hardware budget result. Exact source/binary/report fingerprints and baseline provenance are retained in
`build/autonomous-2026-11-04/benchmark-report-bench-verified.json`. Process regressions separately verify failed output
returns exit 1 and successful fresh reports remain complete. No atomic-write or power-loss guarantee is implied.

### Input-to-swap latency (2026-10-24)

`inputToSwap` times a pointer move over the Shelf. The clock starts when the
event is posted and stops at the swap of the first frame synchronised after
the event was handled. Display scan-out and input-device latency are not
included.

| GPU | p50 | p95 | Max |
|-----|-----|-----|-----|
| NVIDIA GeForce RTX 4060 Laptop (360 Hz) | **5.9 ms** | 6.3 ms | 6.5 ms |
| Intel UHD Graphics (360 Hz panel) | 9.2 ms | 18 ms | 54 ms |

### Prism nested under WSLg (2026-10-24, indicative)

`prism --benchmark 8` on the same laptop, inside WSLg. That means Ubuntu
26.04, OpenGL on Mesa d3d12, a 60 Hz output paced by WSLg's Weston, and a
1600×1000 window. These numbers describe the nested development setup, not
the device.

| Scenario | Average | p95 | Missed frames | Input to swap p50 / p95 |
|----------|---------|-----|---------------|-------------------------|
| Shell tour | 17.1 ms (60 Hz) | 17.8 ms | 0.2% | 26.4 / 33.9 ms |
| Shell tour + `weston-terminal` + Qt Widgets client | 18.2 ms | 21.8 ms | 1.1% | 26.8 / 34.8 ms |

### Optimisation history

| Change | Intel UHD p95 | Intel UHD avg |
|--------|---------------|---------------|
| Initial shell | 31.3 ms | 14.4 ms |
| Wallpaper zoom and parallax as transforms of a cached layer (not shader uniforms) | **9.1 ms** | **7.3 ms** |

## How to measure

```powershell
.\tools\dev\run.ps1 -- --open files,music,notes --benchmark 8 --benchmark-out bench.json
.\tools\dev\run.ps1 -- --gpu low-power --open files,music,notes --benchmark 8   # integrated GPU
```

```bash
prism --benchmark 8 --benchmark-out prism.json     # Linux: every output of the compositor
```

The tour runs for the given number of seconds; then the driver takes 40
latency samples over about 6 seconds. The driver is `prism/shell/shellbenchmark.cpp`, shared by
`qor-shell` and `prism`.

| Field | Meaning |
|-------|---------|
| `tool` | `qor-shell` or `prism` |
| `frames`, `averageMs`, `averageFps`, `p50Ms`, `p95Ms`, `p99Ms`, `maxMs` | Frame intervals of the first output during the tour |
| `refreshHz`, `missedFrames`, `missedPercent` | The output's refresh rate, and intervals longer than 1.5 refresh periods |
| `graphicsApi`, `size`, `devicePixelRatio`, `screen`, `seconds` | Context |
| `outputs[]` | The same frame fields for every output (one window per `WaylandOutput` under Prism) |
| `inputToSwap` | `samples`, `posted`, `averageMs`, `p50Ms`, `p95Ms`, `p99Ms`, `maxMs` (see above) |

Intervals come from `QQuickWindow::frameSwapped` through `qor::FrameStats`,
which never requests frames itself. While the tour is measured, the benchmark
keeps every output drawing (each swap asks for the next), so an output with
nothing animating is measured at its refresh rate rather than by its idle
gaps; the latency phase runs on a still scene. Continuous benchmark probes
include intervals of 200 ms or more, so a long stall reaches the report. The
interactive Monitor still excludes these idle-classified gaps. Keeping outputs
busy originally left the single-output numbers above
unchanged (2.78 ms average, 3.05 ms p95 and 5.9 ms input to swap on the
RTX 4060).

On 4 November 2026, `tst_framecapture` reproduced the omitted-stall defect
through the real benchmark collector: a hidden window receives synthetic
10 ms frame signals, interrupted for 500 ms during the measured tour. The
before/after runs used the same machine, commercial Qt 6.12, ASan/UBSan
(leak checking off), fenced WSL, one worker and a 1.25 GiB memory limit.

| Measurement | Before (`24d3b09` plus the regression) | After the sampler fix |
|-------------|--------------------------------------|-----------------------|
| Standalone report maximum | 10.40 ms | 510.38 ms |
| Missed intervals in that report | 0 | 1 |
| Probe reset and two synthetic signals, Qt Test microbenchmark | 0.00028 ms/iteration | 0.00028 ms/iteration |

The five sanitizer cases pass without failures or skips, including the
unchanged interactive idle filter. The microbenchmark's standard deviations
were 0.00096 and 0.0020 ms, so these observations establish no speed change.
No window was shown, no real rendering or input latency was measured, and
this is not hardware or compositor-gate evidence. Exact records and commands:
`build/autonomous-2026-11-04/HANDOFF.md`.

A separate check of the private WindowLayer ID rename used the real shell
at 1600 × 1000: three 3-second tours before and after, on the same machine
and sanitizer build, with the offscreen software scene graph explicitly
configured and the GPU fenced. Settings, Store and document/media folders
were temporary and empty. The reports label `graphicsApi` as `unknown`.

| Median of three runs | Before (root inputs `56ccb1e`) | After the ID rename |
|----------------------|-------------------------------|---------------------|
| Average interval | 9.63 ms | 10.50 ms |
| p95 interval | 13.59 ms | 13.58 ms |

These small software-only series establish no speed improvement or hardware
budget result. The existing `shell_e2e` application-controls/window-management
script passes both before and after, with unchanged assertions and timeout;
fresh records are in the same handoff's `windowlayer-*-e2e-record` folders.

### Frame probe idle wakeups

The probe's publisher previously kept waking every 250 ms after reporting
idle. A real Qt event-filter regression counted three further timer events
in a 750 ms quiet interval. It now stops at idle and queues one GUI-thread
restart when frames resume. The 250 ms sampling cadence, 200 ms idle filter,
benchmark stall capture and sample cap are retained. A status label's idle
redraw gets one final sample without sustaining an idle/active feedback loop.

Seven ASan/UBSan cases pass without failures or skips, covering those paths,
resume from another thread and detach before a queued restart. Leak checking
is off. The existing shell UI script also passes; five root programs pass
hardening. Synthetic probe-cost observations were 0.00034/0.00036 ms per
iteration before/after, with standard deviations 0.0026/0.0029 ms.

The first separate short timing series was slower after the change, so it
was retained and investigated. The committed sampler was rebuilt with the
same flags in a private namespace: read-only source with its two committed
files overlaid, and a copy-on-write build cache. Hashes verified the real
source, executable, build log and CMake cache were preserved. Both exported
executables pass hardening. Alternating old/new/new/old/old/new runs use the
same executable directory and a fresh private profile each time.

| Series, medians of three per version | Before average / p95 | After average / p95 |
|-------------------------------------|----------------------|---------------------|
| Initial separate 3-second runs | 8.88 / 11.92 ms | 11.34 / 16.19 ms |
| Separate quiet repeat | — | 11.80 / 17.12 ms |
| Alternating 8-second runs | 8.46 / 10.62 ms | 8.51 / 11.13 ms |
| Alternating 3-second runs | 8.74 / 11.91 ms | 8.77 / 11.41 ms |

The large gap was not reproduced in the controlled comparisons. These are
fenced WSL software/sanitizer observations, with `graphicsApi` reported as
`unknown`; they establish neither a speed improvement nor a hardware budget
result. All raw series, executable fingerprints, commands and fresh records
are retained in `build/autonomous-2026-11-04/HANDOFF.md`.

### Compositor decision gate

**Deferred, not failed**
([QOR-0005](decisions/QOR-0005-every-display-at-its-own-refresh.md), amending
[QTOS-0003](decisions/qtos/QTOS-0003-compositor-strategy.md)).

The gate was meant to confirm Qt Wayland Compositor with a bare-metal run of
`prism --benchmark` on Qt's KMS integration, with two outputs at different
refresh rates and Wayland clients running.

The owner ran it on 26 October 2026 from an Ubuntu live USB, with the G15's
360 Hz panel and a 1080p 60 Hz television. Both outputs appeared on the
television, the runs did not pass, and the results were lost to GitHub's
refusal of `.tgz` files. QTOS's ADR-0011 was never written.

The question is now answered by building Prism's own KMS backend (Phase 3)
against the owner's requirement:

- every display at its own native mode and refresh rate, 60 Hz and 360 Hz
  together;
- VRR where supported;
- NVIDIA RTX, Intel and AMD, including laptops whose displays sit on different
  GPUs.

Its acceptance targets are the gate's proposed criteria:

- under 1% missed frames on each output at its own refresh rate;
- input to swap within two refresh periods at p95.

The product target stays at one frame plus scan-out (VISION §12). The nested
numbers above validate the tooling only.

#### Running the decision gate

The gate kit stays as a diagnostic for probing a machine's GPUs and displays,
and for comparing runs. The backend's acceptance runs will come from an
installed QOR OS image rather than a live USB. Prism drives several outputs
(`--outputs` nested, every screen on eglfs), and `tools/dev/prism-gate.sh`
runs the whole procedure in one command. The machine
under test needs neither a Qt kit nor a build: `tools/dev/prism-gate-bundle.sh`
packs Prism, the test client, the Qt runtime they use and the gate script
into one archive (about 100 MB) that runs on a stock Ubuntu desktop, a live
USB session included. Build the bundle on the Ubuntu release it targets,
against that release's ISO manifest:

```bash
tools/dev/prism-gate-bundle.sh --build-dir ~/qor-build/linux-release     --manifest ubuntu-26.04.1-desktop-amd64.manifest --out qor-gate.tar.gz
```

The packager checks file/dependency inventories and their filters before writing an archive; a failed command or missing
dependency fails the package. `BUNDLE.txt` records the launcher checkout, packaging time and exact executable SHA-256 digests.
It leaves the executables' build revision unverified: a checkout's current commit does not establish which revision built a binary.
The archive is created and read back beside its destination, then replaces it in one rename. A failed or empty archive write
leaves the earlier bundle intact; normal exit cleanup removes temporary output.

On the machine under test, booted into Ubuntu (a live USB session is enough)
with two displays at different refresh rates on one GPU:

1. Unpack the bundle, and join the groups for the displays and input:
   `sudo usermod -aG video,render,input $USER`. A live session's user needs
   a password for the console: `sudo passwd $USER`.
2. `qor-gate/prism-gate.sh --probe` lists the GPUs and the displays on
   each. Prism drives one GPU, so both displays must be on it; on a laptop,
   the panel is usually on the integrated GPU and some video ports are wired
   to the discrete one.
3. `qor-gate/prism-gate.sh --try` runs a trial inside the desktop session,
   as two windows: it checks the bundle on this machine (its numbers are not
   gate data).
4. Switch to a text console (<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>F3</kbd>),
   log in, and run `qor-gate/prism-gate.sh`.

It picks the GPU with both displays (`--device` overrides), starts Prism on
Qt's eglfs KMS integration (atomic modesetting, one output per display, the
built-in panel first), opens an animating Wayland client on each output,
runs the benchmark, and checks the criteria above. It writes `report.json`,
`summary.md` (machine, GPU, outputs and their refresh rates, per-output
results, input to swap, and the verdict) and the logs into a `gate-<time>`
folder, with a `.zip` of it to attach to the issue (GitHub refuses `.tgz`
attachments); a live session forgets them at power off. It exits 0 when
every criterion passes. <kbd>Ctrl</kbd>+<kbd>C</kbd> stops a run, and a
watchdog ends one that hangs.

Each run requires a new output folder, including with `--out`; an existing folder and its evidence stay untouched.
A nonzero Prism exit or watchdog failure fails both the gate and a trial, even if a report was written. A gate pass requires
two outputs at different refresh rates as well as the numeric criteria. The launcher regressions use inert programs and a
stubbed hardware probe; they establish exit/evidence handling only, never hardware readiness.
Clients must start before measurement ends and remain running until Prism writes its measurement report. An early client
exit fails the run, including an exit with status zero; disconnecting during display shutdown after measurement is expected.
The report must cover every output in that setup. Counts and timing/rate/percentage fields must be finite and nonnegative
(refresh rates positive), with integer counts and ordered input percentiles. Invalid data fails even a trial; valid zero-sample
trial data stays diagnostic and cannot pass the hardware gate.

`--mode eDP-1=1920x1080@60` picks a display's mode, to separate frame pacing
from raw GPU throughput on a very fast panel; `--no-atomic` falls back to
legacy modesetting if a driver rejects atomic commits. With a Qt kit and a
`linux-release` build on the machine, `--build-dir` runs without a bundle.
Note that Qt's eglfs KMS integration drives one GPU, so it cannot test the
split-GPU laptops the backend must support.

The **Monitor** app shows the same probe live: frame time and fps while the
shell animates, and **Idle · Rendering on demand** when nothing changes.

For pull requests that touch the shell or Kit, run the benchmark on the same
machine before and after and paste both results.

## GPU selection

On hybrid-graphics Windows laptops Qt's Direct3D backends use adapter 0,
usually the integrated GPU. `qor-shell` asks DXGI for the adapter matching
`--gpu` (default `high-performance`) and sets `QT_D3D_ADAPTER_INDEX`
accordingly before the first window is created (`prism/host/gpupreference.cpp`).
An explicit `QT_D3D_ADAPTER_INDEX` in the environment always wins. The chosen
adapter is logged as `GPU: …`.

On Linux, GPU selection belongs to the platform (PRIME offload / `DRI_PRIME`);
on a QOR OS device, Prism owns the KMS device.

## Techniques

| Technique | Where | Why |
|-----------|-------|-----|
| Shared blurred backdrops | `kit/Backdrop.qml`, `kit/Glass.qml` | One downsampled (¼) blur per source per change, shared by every glass panel |
| Exact backdrop mapping without wakeups | `kit/src/scenerect.cpp` | Re-maps on `afterAnimating` (only when frames happen) |
| SDF primitives | `kit/shaders/surface.frag`, `glass.frag`, `roundmask.frag` | One pass for shape, AA, hairline, sheen and shadow; no textures |
| Cached wallpaper | `kit/Wallpaper.qml` | Noise field evaluated only on hue, appearance or time change; zoom and parallax are transforms |
| Closed-form springs that stop | `kit/src/spring.cpp` | Exact at any frame rate; zero cost at rest |
| Mipmapped window layers only while scaled | `prism/shell/AppWindow.qml` | Crisp Overview thumbnails without permanent mip generation |
| Native text rendering | `prism/host/main.cpp` | Crisp static text; layers keep animated text smooth |
| AOT-compiled QML | `qt_add_qml_module` + `qmlsc` | No QML parsing at startup; typed bindings run as C++ |
| Pipeline cache | Qt Quick | Repeat runs avoid shader pipeline creation |

## Known costs and next steps

Startup branding was compared locally on 5 November 2026: three sequential eight-second
`prism --size 1600x1000 --benchmark 8` tours before (`8b3bfb8`) and after the startup changes,
on the same Windows/WSL machine, commercial Qt 6.12, offscreen Qt Quick software, GPU access
masked, CPU quota two processors, one compiler/render worker and 1.75 GiB memory cap.
The root profile was `ci-linux` (`RelWithDebInfo`, warnings as errors); this was not native
GPU testing. All six fresh reports contain frames and all 40 input samples per run.

| Measurement, milliseconds | Before: median (range) | After: median (range) |
|---------------------------|------------------------|-----------------------|
| Frame interval p95 | 11.925 (11.570–12.689) | 11.386 (10.788–12.224) |
| Frame interval p99 | 13.645 (12.741–14.281) | 13.250 (11.792–14.565) |
| Input to swap p95 | 14.924 (13.927–16.316) | 12.522 (12.047–13.325) |

These small software samples describe the observed runs, not a causal speedup or a hardware
budget pass. The early forced-capture run and its intermediate benchmark remain archived
separately; the table uses the final zero-extent fix. Source/program hashes, fresh report hashes,
commands and limits are in the local `build/autonomous-2026-11-04/` checkpoint. Startup adds
no recurring QML animation after release; a first valid frame releases its 280 ms fade and
the watchdog stops when settled. Native display pacing, GPU, audio and controller gates remain.

The first-run welcome hook was compared separately on 5 November, from `dad071d` to
the frozen installer changes, with three tours on each side and the same software fence
and resource limits above. Median frame p95 was **10.009 → 10.401 ms**; median input-to-swap
p95 was **10.445 → 12.034 ms**. All six reports contain fresh frames and 40/40 input samples.
The after samples were slower; these observations establish neither a causal regression
nor a hardware budget pass. The simulated tour does not launch welcome, so it measures
neither first-run app launch nor a real device login. Exact binaries, reports and command
records are under `build/install-experience-2026-11-05/`: `install-bench-before-record.json`
and `install-final-bench-after-record.json`. The app's page fade is finite; unknown/manual
motion hints keep it still. Actual device startup/login timing remains unmeasured.

- On integrated GPUs the remaining p95 is dominated by re-capturing the scene
  backdrop while windows move. Planned work: capture only the region under
  visible glass, and skip the capture when no glass overlaps changed content.
- `Reduce Transparency` removes the glass sampling cost entirely.
- Phase 3 adds direct scanout for full-screen clients and overlay planes for
  video and cursor in the KMS backend.
