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
  which was suspected to reflect a GPU power state but was not established.
  Retain every run, report the spread and compare alternating baseline/candidate
  series under the same recorded conditions; do not select only faster runs.

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

The diagnostic gate validates these distributions through
`tools/dev/prism_benchmark.py`, also included in its bundle. A result can meet
the timing criteria only with **40 recorded / 40 posted inputs**, matching the
collector, plus nonempty frame data on every discovered output. Dropped input
samples cannot make a deceptively short latency tail pass. Valid partial or
zero-sample `--try` runs remain diagnostic successes with failed criteria;
malformed reports fail even in trial mode. Old reports missing required fields
remain historical evidence and need a fresh measurement for current acceptance.

The reader bounds reports to one MiB, 64 outputs, 16 levels and 16,384 values;
rejects duplicate JSON fields and non-finite values; checks every distribution's
ordering, sample counts, and agreement of missed-frame percentages/frame rates
with their underlying counts/intervals. The accepted targets remain under 1%
missed frames and input-to-swap p95 within two refresh periods of output one.
This validates internal consistency, not a report's provenance or a hardware claim.

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

### QQ Agent callback timing (2026-11-15, isolated Pi software rendering)

The unchanged `logicThatThrowsInPlayIsInTheLog` case reproduces at source
`439b5a8`, while `aLongWaitLastsWhatItWasAskedFor` passes. The logging case
requests 600 ms through the Agent's wall-clock `wait` tool and requires one
`TypeError` entry with more than five repetitions. Its logic throws from
`onFrame`, which is driven by Qt's [FrameAnimation](https://doc.qt.io/qt-6/qml-qtquick-frameanimation.html),
after the first scene step. Elapsed wall time does not guarantee that many
animation updates.

Temporary observation slots preserve that 600 ms request and inspect scene
readiness, frame swaps, logic steps and log delivery. The original failing
method is unchanged. Both observations use the original native coordinator,
private display/device/network namespaces, commercial Qt 6.12.0, llvmpipe,
two renderer workers and the unchanged 3,072-MiB guard.

| Observation | First trace | Trace with logging control |
| --- | --- | --- |
| Play returns; scene and behaviour ready | 37 ms | 37 ms |
| Requested wait | 600 ms | 600 ms |
| Time at wait return, measured from play call | 1,221 ms | 1,197 ms |
| Window frame swaps / natural logic-frame callbacks | 3 / 1 | 3 / 1 |
| Scene / logic steps | 12 / 12 | 11 / 11 |
| First observed natural QML warning | Not timestamped | 1,195 ms |

The natural callback's exception reaches the log. In the second diagnostic,
six explicit callbacks after the wait produce six more exceptions immediately:
the log contains one entry with `times: 7`, exactly matching seven callbacks.
This confirms delivery and repeated-error counting for the exercised path.
The explicit callbacks are a positive control, not drawn frames or a passing
acceptance test. Neither diagnostic is a replacement for the original suite.

Those session-09 receipts establish insufficient natural callback delivery,
not a logger defect. They remain sealed in
`build/development-2026-11-15/session-09/`.

**Controlled follow-up at source `564f032`, session-10.** The same isolated Pi,
commercial Qt 6.12.0, existing `RelWithDebInfo` cache, 1600 x 960 Studio window,
private display/keyring, two llvmpipe workers and standard limits remain.
Temporary instrumentation records monotonic render-phase timestamps, process
and render-thread CPU time, natural warning timestamps/counts and scene state.
Both client and server Wayland traces match surface frame requests to commits
and callback IDs; 100 ms samples observe only the test and its compositor.
No callback is injected. Qt's [frameSwapped signal](https://doc.qt.io/qt-6/qquickwindow.html#frameSwapped)
means queued presentation, not physical scanout.

| Observation | Normal viewport | Diagnostic viewport opacity zero |
| --- | --- | --- |
| Play returns | 38 ms | 37 ms |
| Play-to-wait-return elapsed time; request remains 600 ms | 1,220 ms | 639 ms |
| Natural logic callbacks / logged repetitions | 1 / 1 | 20 / 20 |
| Window swaps / logic steps | 3 / 11 | 21 / 29 |
| First frame: after render recording to queued presentation | 1,056.341 ms | 207.230 ms |
| Client CPU consumed in that interval, summed across threads | 1,990.864 ms | 224.684 ms |

The control suppresses viewport pixels after normal startup while retaining
its 1010 x 904 layout area and live scene. It changes the draw workload; it is
not an acceptance run or a quality change to ship. Its natural error timestamps,
in milliseconds from play, are 307, 318, 336, 352, 367, 383, 399, 415, 431, 452,
469, 484, 505, 521, 543, 560, 581, 597, 615 and 637. The normal trace records
its single error at 1,218 ms. Both use the same 600 ms tool request.

In the normal run the display sends callbacks 10.256-10.554 ms after receiving
the associated commits. Across a 1,204 ms sampled interval, the client consumes
2,220 ms of CPU time and the compositor about 10 ms (10 ms counter resolution).
The next two client frames take 45.259 and 44.775 ms between recording and
presentation. This locates the dominant delay in client viewport rendering,
not the private display withholding an already submitted frame. The control's
callbacks arrive 9.477-12.024 ms after commits, with twenty natural logic
callbacks during its wait. It also shows the display can deliver the required
callback count when the draw workload is lower.

A separate first/second-play observation keeps one Studio process. Its first
trial independently reproduces the delay: 1,206 ms elapsed, one natural logic
callback, and 1,042.559 ms in the first render-to-present interval. Immediate
replay remains slow (929 ms, first interval 889.606 ms), with zero new-scene
frame/step signals despite two QML errors. `SceneIO::discard()` defers deletion;
this is consistent with old-scene interference. The replay is not a clean
cold/warm comparison and does not establish a shader-cache cause or a separate
production defect. Its raw evidence is retained.

The result establishes a workload/capacity bottleneck for this software
configuration, but does not identify which driver setup, resource allocation
or drawing work is excessive. No production fix or regression test is justified
at that checkpoint. The next step was a bounded call-stack profile in the same
sandbox, recorded below. For expected software rendering cost, the defensible
environment correction is to qualify a sufficiently fast isolated renderer
with the original workload and timing gate. Do not prewarm acceptance, hide
the viewport, inject callbacks, reduce quality or relax `times > 5`/600 ms.

All 1,414 source hashes and the original executable hash are restored. The
original focused logging/wall-clock run reports three passes and one failure:
the log now captures one error and `times > 5` fails with value `1`. The
wall-clock case passes. Native build and 16-program hardening pass
without compiler/licence diagnostics. The original lock is free; no test workers
remain; native samples stay at or above 6,359 MiB against the 3,072-MiB floor.
Broader acceptance and sanitizers were not rerun because the focused failure
remains open and production code is unchanged. Existing full-suite/lint and
sanitizer receipts retain their earlier scope. Current raw traces, CPU samples,
matched callback analysis and restoration evidence:
`build/development-2026-11-15/session-10/`.

Earlier session-09 restored all 1,414 original sources and the executable hash,
then ran the full suite: 22 passes, the logging failure and one optional provider
skip in 154.467 seconds. Build, hardening and aggregate/structured lint passed;
470 samples stayed at or above 5,859 MiB. This remains earlier full-suite evidence.

**Bounded first-frame profile at source `8f0f52a`, session-11.** The same Pi,
commercial Qt 6.12.0, Mesa 26.2.6/LLVM 19.1.7, two llvmpipe workers, original
1600 x 960 Studio window, private display/keyring and device/network fences
remain. The native `q-pi-release` cache is RelWithDebInfo, not CMake Release.
No packages, security settings, physical devices or production sources change.

`perf` is absent; installed GNU gprofng 2.44 supplies timer-based stack sampling
without repeatedly stopping all threads in GDB. The collector command uses
`-p 10 -t 60s -F off -a off -S off -j off`: 10 ms CPU sampling, a 60-second
collection ceiling, no child following, source/binary archiving, periodic
resource samples or Java profiling. This ceiling does not replace the unchanged
600 ms request, 1,800,000 ms watchdog, 5,400-second suite limit, 5,600-second
outer limit or 7,200-second guard. Temporary test-only markers bracket the first
`beforeSynchronizing`/`frameSwapped`; no callbacks are injected.

The first `-y SIGUSR2` attempt produces **zero usable samples**, reports a
pause/resume-handler conflict and a timer change, and slows the frame by 11.4%.
It is preserved, not counted as a valid profile. A follow-up resolves the
installed `collector_resume`/`collector_pause` API with `dlsym`, using `-y 34`.
The latter option does not pause startup collection in the observed run.
Therefore the unfiltered 10.460 CPU-seconds are **not** first-frame evidence.
`gprofng display text -sample_select 2` selects the interval labelled
`collector_resume` to `collector_pause(API)`: experiment time
8.110021014-9.235382661 seconds, or 1.125362 seconds. The interval's function,
call-tree, hot-address and thread reports are retained, alongside the full raw
experiment and its six file hashes. Report generation reuses that experiment.

| Observation | First after-recording-to-presentation interval | Process CPU in that interval | Render-thread CPU | Natural callbacks / logged repeats | Wait return from play start |
| --- | ---: | ---: | ---: | ---: | ---: |
| API diagnostic binary, no profiler | 1,036.716 ms | 1,968.802 ms | 207.135 ms | 1 / 1 | 1,201 ms |
| Same binary, API-controlled profiler | 1,114.708 ms | 2,054.427 ms | 281.525 ms | 1 / 1 | 1,286 ms |
| Same binary, only `LP_NO_RAST=1` | 205.132 ms | 206.473 ms | 204.954 ms | 24 / 24 | 640 ms |
| Same binary, Vulkan/lavapipe, full viewport | 929.079 ms | 1,666.013 ms | 0.325 ms | 1 / 1 | 1,280 ms |
| Earlier session-10 viewport-opacity control | 207.230 ms | 224.684 ms | Not recorded | 20 / 20 | 639 ms |

Times are observations, not repeated statistical benchmarks. CPU time sums
across threads and can exceed elapsed time. The sampled first-frame interval
is slightly wider than the table's after-recording interval. Compared with the
same API binary without profiling, elapsed overhead is 77.992 ms (7.52%) and
process CPU overhead 85.625 ms (4.35%). Resume/pause calls themselves take
46/52 microseconds. The profiler still warns about blocked SIGPROF and a timer
change; its raw log places the timer warning after the pause event. Qt QML's
deleted JIT memory mapping is unavailable. These warnings remain limitations,
not a clean-profiler claim.

The selected interval contains 2.050 CPU-seconds of samples; independent
collector resource counters show 2.070 CPU-seconds. Two sampled threads account
for 0.880 seconds each, with 0.290 on the third. **81.95% of exclusive sampled
CPU time has unresolved addresses**, so the profile alone cannot identify a
dominant shader. Resolved stacks include `QRhi::endFrame` through Mesa Gallium
to `LLVMGetPointerToGlobal`, `llvm::MCJIT::finalizeObject`, LLVM compilation
passes, and a small Qt scene-graph pipeline-creation path. Those stacks prove
some compilation/setup occurs; they do not explain the unresolved majority.

The one-variable rasterization control resolves that broader uncertainty:
disabling rasterization removes 831.584 ms (80.2%) of the first-frame interval
and 1,762.329 ms (89.5%) of process CPU, while the render thread's own cost stays
near 205-207 ms. The dominant delay is software drawing, consistent with the
earlier viewport-suppressed control, rather than render-thread setup alone.
The control leaves the original viewport and live scene present but omits
pixels; it is **never acceptance**. The exact shader/pass and whether its work
can safely be avoided remain unresolved. No logger, shader-cache or production
rendering defect is established. Original-reference server callbacks follow
commits by 10.077-10.548 ms; suppressed-raster callbacks take 7.042-8.103 ms.
The display remains much smaller than the first-frame client cost.

The alternative backend is tested separately with the installed
`/usr/share/vulkan/icd.d/lvp_icd.json`, selected through `VK_DRIVER_FILES`, and
`QSG_RHI_BACKEND=vulkan`. Inside the same sandbox, `vulkaninfo --summary`
reports a CPU device, llvmpipe/LLVM 19.1.7, Mesa 26.2.6, Vulkan API 1.4.354;
Qt logs confirm the Vulkan backend and the same software device. Vulkaninfo's
X11 surface-info skip is recorded; the actual Qt Wayland swapchain works.
Its shorter after-recording interval does not mean a faster complete wait:
the wait returns at 1,280 ms with one callback. The original quality and two
worker setting remain. No GPU acceleration or replacement acceptance is claimed.

All 1,414 original sources and original Agent executable SHA-256
`5e754f6ee300b52801b6833a46f7d72919e0ff22d496e792f95902b6824ebd10`
are restored. The original focused logging/wall-clock run reports **3 passes,
1 failure, 0 skips in 13.263 seconds**: `times > 5` is false with value `1`;
the wall-clock wait passes. Build and 16-program hardening pass without
compiler/licence diagnostics. All 503 native resource samples stay at or above
6,394 MiB; the original coordinator and 3,072-MiB floor remain. The final lock
is free and no QOR test workers remain. A transient SSH timeout before restore
launch created no job; readback preceded the successful launch.

No production change or new regression test is justified by these findings.
Sanitizers and broader acceptance were not rerun; earlier failures and skips
remain open. The next step at that checkpoint was a bounded Mesa generated-code
symbol-mapping diagnostic to associate hot addresses with shader work before
selecting an image-preserving optimization. If the workload is unavoidable,
qualify a faster isolated renderer against the original gate. The tested CPU
Vulkan backend is insufficient; reducing quality, prewarming, suppressing
pixels, injecting callbacks or relaxing assertions cannot qualify it.

Profiler controls and limitations follow the installed tool's help and
[GNU gprofng documentation](https://sourceware.org/binutils/docs/gprofng.html).
The targeted control is documented by [Mesa](https://docs.mesa3d.org/envvars.html#lp-no-rast);
the next symbol-mapping diagnostic is described in
[Mesa's llvmpipe profiling guide](https://docs.mesa3d.org/drivers/llvmpipe.html#profiling).
The alternative uses [Qt's supported backend selection](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html).
Raw logs, configurations, markers, callback timestamps, selected stacks,
overhead calculations and restoration receipts:
`build/development-2026-11-15/session-11/` (ignored local evidence, not in a fresh clone).

**Generated-code attribution at source `01eb67b`, session-12.** The investigation
was bounded to 17:44-18:04 local time and closed at 18:01; restoration, original
validation and publication followed. The session-11 seal (285 files), clean
branch and 14 worktrees, both original coordinator identities/locks, all 1,414
native sources and the original executable were verified before work. No other
jobs were interrupted. The same 1600x960 original viewport, private Wayland
display/keyring/network/device namespaces, commercial Qt 6.12, Mesa 26.2.6,
LLVM 19.1.7, two llvmpipe workers and standard suite limits were retained.

`perf` is absent. Mesa's matching debug files are not installed, and the old
process's anonymous generated-code addresses have no retained symbol mapping;
those old addresses cannot be resolved by guessing against a new address space.
The installed LLVM library exports the GDB JIT registration descriptor. A
temporary test-only overlay enables `GALLIVM_DEBUG=symbols` and snapshots its
registered objects after the observation. Process-global lookup initially
found no descriptor because the loaded LLVM symbols are library-local. One
bounded retry looks up the already-loaded library explicitly. There was no
third capture: `readelf` then rejected ten saved objects, which caused that
capture's post-analysis harness to fail despite the completed diagnostic test.
The failed attempt and raw data remain preserved; valid objects were analyzed
offline. Subsequent post-analysis records this error instead of claiming all
objects are readable. None of these diagnostic slot passes is product acceptance.

The retained snapshot contains 135 objects / 1,048,024 bytes. Exactly 125 are
valid little-endian ELF64 objects; ten fail the magic check and are excluded.
The cause of those invalid objects is not established. Matching sampled PCs
only to valid runtime section/symbol ranges yields no ambiguous matches:

| Exclusive sampled work | CPU-seconds | Share of 2.25 CPU-seconds |
| --- | ---: | ---: |
| `texfunc_res_1_sam_1_2020` | 0.740 | 32.89% |
| Other resolved `texfunc_*` texture functions | 0.240 | 10.67% |
| `fs_variant_partial` | 0.460 | 20.44% |
| `fs_variant_whole` | 0.420 | 18.67% |
| Other previously named/static work and remaining unknown PCs | 0.390 | 17.33% |

This resolves **1.86 / 1.87 unknown CPU-seconds (99.47%)**, newly attributing
82.67% of all sampled work. It does not mean 99.47% of the entire call stack is
symbolized. Saved disassembly places hot addresses in half-float texel loads,
conversion and interpolation instructions within `texfunc_res_1_sam_1_2020`.
The attribution is generated texture sampling and fragment shading, not a
proven cache-miss cause, a complete unwound shader call chain, or an identified
Qt material/resource binding. Those semantic pass details remain unresolved.

Collection retains gprofng 2.44, 10 ms timer sampling, a 60-second collector
ceiling, no descendants, Java, periodic resource sampling or archives. Only
sample interval 2, 6.783422773-8.011450550 seconds (1.228028 seconds), is used;
the unfiltered startup totals are excluded. SIGPROF/timer-change warnings and
the unavailable Qt QML JIT mapping remain limits on sampling accuracy. The
snapshot took 9.466 ms **after** the measured observation; marker calls took
62/47 microseconds. The same symbol-enabled binary without profiling supplies
the overhead reference; enabling symbol registration itself is not separately
benchmarked. A single pair is not a statistical performance claim.
The failed post-analysis did not write that profile run's executable hash.
Identical source manifests, an empty reference overlay and no intervening
compile/link support the same-binary comparison; the following reference and
shadow control independently record the identical executable hash.

| Same symbol-enabled diagnostic binary | First recorded-render-to-presentation interval | Process CPU | Render-thread CPU | Natural callbacks / repeats | Wait return from play start |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original viewport, no profiler | 1,133.941 ms | 2,151.869 ms | 160.751 ms | 1 / 1 | 1,312 ms |
| API-controlled profiler | 1,217.838 ms | 2,238.436 ms | 243.254 ms | 1 / 1 | 1,405 ms |
| Only Sun `castsShadow=false`, no profiler | 1,181.242 ms | 1,755.801 ms | 647.920 ms | 1 / 1 | 1,622 ms |

The matched profiling overhead is **7.40% elapsed / 4.02% process CPU**.
The shadow control changes one property after play, with its before/after
values recorded. It changes pixels and render state, and may incur variant
preparation; the increased render-thread cost prevents treating it as a clean
steady-state shadow benchmark. It does prove that this one-property removal
does not resolve the measured startup gate. It neither exonerates all shadow
work nor justifies a production quality change. Display commit-to-callback
latency remains about 10 ms. Earlier viewport-opacity suppression (207.230 ms,
20 callbacks) and `LP_NO_RAST` (205.132 ms, 24 callbacks) agree with drawing cost
being dominant, but remain non-acceptance controls from their earlier sessions.

**Test contract review.** `logicThatThrowsInPlayIsInTheLog` and QQ's documented
planted-fault coverage intend to prove repeated real logic errors appear once,
with a count. `StudioLog::append` coalesces consecutive identical level/text,
increments the repeat count and sequence; `since()` returns the changed entry.
The `wait` tool uses `QTimer::singleShot`, not a frame-count guarantee.
`Behaviour.qml` drives logic from `FrameAnimation`. The existing 600 ms request
and `times > 5` therefore combine logger correctness with startup throughput;
the test measures no upper elapsed-time bound and is not a precise FPS test.
The adjacent wall-clock check tests only `elapsed >= 1500`, so its pass does not
prove responsiveness. One observed callback produces one captured error;
session-09's six additional controlled callbacks produced six additional repeats.
No dropped-warning defect is demonstrated. The roughly one-second render stall
is real on this CPU configuration, but the reference-laptop first-app-frame
budget is not a defined Pi-software play-start service guarantee.

An evidence-backed correction is proposed, **not implemented**: add a controlled
fixture issuing six identical actual QML exceptions through QML warnings, the
production Qt message handler, `StudioLog` and MCP `log_read`. Assert exactly
one error entry with `times == 6`, correct filename/level/text, retained notices
and cursor advancement. Read an initial batch, then another repeat, and verify
that the already-read coalesced entry becomes visible at the new cursor. Keep
natural frames from adding uncounted fixture emissions. This protects dropped
errors, missing increments, duplicate entries and broken incremental reads.
Retain the original 600 ms / `times > 5` case as unresolved natural-delivery
evidence; any later reclassification needs an explicit acceptance boundary and
an independent natural-frame/startup-latency gate. Controlled emissions cannot
replace rendering acceptance, and the Player's existing 20-FPS checks remain.

**Renderer decision.** No image-preserving production optimization is supported
by this time box. Practical alternatives have different evidence:

| Renderer | Verified on this Pi | Decision |
| --- | --- | --- |
| OpenGL/llvmpipe | Full original viewport, one natural callback; generated sampler/fragment work dominates | Keep the unresolved software-support result; do not lower quality or timing requirements |
| Vulkan/lavapipe | Session-11 actual isolated Qt Wayland swapchain on a CPU device; one callback, 1,280 ms wait | Available, insufficient for this gate; no new duplicate run |
| V3D / V3DV | Session-12 host inventory only; session-13 below verifies isolated V3D access | Studio qualification fails on desktop GL shader compatibility and image correctness; V3DV remains unqualified |
| Qt Quick's 2D software adaptation | Qt documents no support for Quick 3D content | Not a replacement for the Studio viewport |

[Mesa documents V3D/V3DV on Raspberry Pi](https://docs.mesa3d.org/drivers/v3d.html),
and [Qt documents the Quick 3D graphics requirements](https://doc.qt.io/qt-6/qtquick3d-requirements.html).
The resulting V3D qualification is recorded in session-13 below, in a separate
render-node-only sandbox with private display/audio/keyring/input/network boundaries retained.
Verify the actual Qt renderer and original workload before calling it usable;
keep llvmpipe support and image/hardware acceptance separate. Node presence is
not GPU acceleration or acceptance evidence.

All original sources and the original executable hash above are restored.
The original focused cases report **3 passes, 1 failure, 0 skips in 13.296 s**:
`times > 5` still fails with value `1`; wall-clock waiting passes. Native build
and 16-program hardening pass without compiler/licence diagnostics. The cache
is RelWithDebInfo, not CMake Release. All 370 native samples exceed the unchanged
3,072-MiB floor (minimum 6,343 MiB); the original lock is free with no workers.
No production code remains changed, so there is no new regression or visual
equivalence claim, sanitizer run or broader-suite pass. Required isolated CI
gates remain unmet; draft PR #10 is not ready. The symbol snapshot uses the
[GDB JIT interface](https://sourceware.org/gdb/current/onlinedocs/gdb.html/JIT-Interface.html);
Mesa also provides [llvmpipe profiling guidance](https://docs.mesa3d.org/drivers/llvmpipe.html#profiling),
but `perf` is unavailable here; this capture uses LLVM's registered objects.
Commands, configurations, raw PCs/objects and hashes, coverage parser, failure
logs, matched overhead and callback evidence, test-contract review and restoration
receipts: `build/development-2026-11-15/session-12/` (local ignored evidence).

### V3D render-node qualification, session-13

Source `f0893ea8c18290b26fa31cf18dce7c56d0708eca`, original Agent binary
`5e754f6ee300b52801b6833a46f7d72919e0ff22d496e792f95902b6824ebd10`.
Qualification ran within a 20-minute window, 22:37:21 to 22:55:01 local time,
closing before 22:57:21. Four focused executions, no source overlay and no
profiler were used. Session-12's 360-file seal and all 1,414 native source hashes
remain intact. No new production change or test-contract correction is made.

**Device and isolation.** On this AArch64 Pi with 16,384-byte pages,
`/dev/dri/renderD128` is character device 226:128, mode 0660, owner 0/group 992,
bound through sysfs to `v3d`. The existing account belongs to group 992; an
actual read/write open succeeds inside the sandbox. Mesa is
26.2.6-1~bpo13+0~rpt2; commercial Qt is 6.12.0. The separate bubblewrap sandbox
exposes only that render node, with all primary graphics, audio, input, DMA-heap
and Pi control devices hidden. Private network, process, user, IPC and mount
namespaces, home/XDG directories, bus and test keyring remain verified. No host
permission, graphics setting, service, security control or desktop is changed.
The established software configuration is not edited.

A direct EGL 1.5 pbuffer probe reports Broadcom V3D 7.1.10.2 / OpenGL ES 3.1,
and all sixteen pixels of its 4-by-4 red readback are exact with no GL error.
That proves access and a small render/readback operation, not Studio correctness.
The first original-test attempt keeps the existing offscreen software compositor.
It cannot initialize its EGL display or advertise hardware buffers, so the client
falls back to llvmpipe. The unchanged renderer assertion correctly aborts:
**1 pass, 1 failure, 0 skips, 7.249 s**, SIGABRT; the second case is not run.

The next comparison changes only the private compositor backend to the installed
`eglfs_kms` plugin with `{"device":"/dev/dri/renderD128","headless":"1920x1080","hwcursor":false}`.
[Qt documents this render-node headless mode](https://doc.qt.io/qt-6/embedded-linux.html#headless-mode-using-drm-render)
without display-master access and without physical vsync throttling. No display
node is exposed. The compositor now reports V3D / ES 3.1 and advertises `wl_drm`;
the original 1600-by-960 Wayland client reports V3D **desktop OpenGL 3.1**.

**Unchanged focused result.** `logicThatThrowsInPlayIsInTheLog` and
`aLongWaitLastsWhatItWasAskedFor` give **3 passes, 1 failure, 0 skips, 28.341 s**.
Wall-clock waiting passes. Logging fails at `QCOMPARE(typeErrors, 1)` with 2;
the later `times > 5` assertion is not reached, and is not a pass. The log has
three intentional QML on-frame exceptions, with shader warnings between the
second and third. `StudioLog::append` coalesces consecutive identical text/level
only; two entries are consistent with that implementation. Across both cases,
45 missing-GLSL warnings and 15 graphics-pipeline failures remain recorded.
The baked shaders provide GLSL 150 and ES 300 variants, while this desktop
context searches GLSL 140, 130 and 120. No warning filter or version override is used.

Wire tracing records first surface creation at 348171.861 ms, first buffer
commit at 372222.828 ms, and first completed frame callback at 372235.837 ms:
**24,063.976 ms** from surface creation, including startup, context and shader
work. The first frame request-to-callback is **13.152 ms**. Five completed
surface-frame callbacks precede the logging failure (13.152, 5.693, 5.639,
5.568 and 5.974 ms request-to-callback); display-sync callbacks are excluded.
These are not the three QML on-frame error emissions. No play-start marker or
instrumented wait-elapsed value exists in this unchanged binary. The 100-ms
process samples cover 25.44 client CPU-seconds and 0.04 compositor CPU-seconds
over the complete two-case process, not a per-pass GPU profile. No profiler ran;
wire/CPU-observer overhead is unmeasured. Do not infer a precise startup hotspot
or compare this metric to session-12's after-rendering play interval.

**Images, not speed alone.** Run the existing
`theStudioWearsTheOneLooksToolRegister` once on V3D, then once with only the
client forced to llvmpipe (`LIBGL_ALWAYS_SOFTWARE=1`, `GALLIUM_DRIVER=llvmpipe`,
expected renderer `software`). Same headless V3D compositor, original Studio
binary `390d66647b83ecbb2053ae7aebaf963c37e6b30908d64df3d8274325ae83d981`,
viewport, test, private profiles and suite limits; actual client identities
are verified. Each gets a fresh private cache. This software image control
still exposes the render node for its compositor, so it is not the original
software-fence qualification. Existing `studio-look.png` is backed up and
restored; both new captures are retained separately.

| Client | UI test pass/fail/skip | Test elapsed | Surface creation to first frame callback | Pipeline failures |
| --- | --- | ---: | ---: | ---: |
| V3D, desktop GL 3.1 | 3/0/0 | 26.315 s | 24.044 s | 9 |
| llvmpipe, desktop GL 4.6 | 3/0/0 | 10.503 s | 6.941 s | 0 |

Both narrow button/focus assertions pass. Visual inspection shows a black sky
and missing environment lighting on V3D versus the orange sky and lit scene on
the matched software control. The static sky rectangle (300,80)-(1200,250) has
mean RGB 7.845/8.818/11.832 versus 193.858/88.880/69.290. These descriptive pixels
are not a new acceptance threshold or an exact-equivalence claim. Random project
titles, renderer arithmetic and possible animated state differ. The failed
shader pipelines and missing image content reject hardware qualification even
though some frames and UI checks work. A pre-existing screenshot was preserved
but is not used as the matched control because its complete provenance was not
re-established.

**Decision and next action.** Keep the original software failure open: its
sealed original run is 3/1/0 in 13.296 s, `times > 5` fails with 1. The new
compositor, buffer transport, client API and missing pixels prevent a direct
hardware/software speedup claim. The deterministic real-QML-error proposal above
is recommended as supplemental counting/cursor coverage, including intentional
interleaving that protects the consecutive-only rule. Retain the original
natural-frame requirement unchanged and separately reported. Neither shader
warnings nor rendering acceptance should disappear through a test rewrite.

The next bounded diagnostic should explicitly request ES 3.1 in the client
through Qt's supported [surface-format API](https://doc.qt.io/qt-6/qsurfaceformat.html#setRenderableType),
verify the actual context, and repeat image plus unchanged focused checks before
selecting any production policy. ES 3.1 works in the direct probe/compositor;
that client selection is not yet verified for Studio. Inspected Qt 6.12 RHI/Wayland source shows
`QSG_RHI_BACKEND=gles2` selects the same GL backend as `opengl`; it does not force
an ES context. No applicable environment-only ES selector was established in
this time box. Do not spoof Mesa versions or assume Vulkan/V3DV is qualified.
For the separate software blocker, map the already captured dominant texture
sampler's resource binding to its Qt material/pass before another reversible
single-variable probe. The missing V3D lighting is not proof that that pass owns
the llvmpipe hotspot, and disabling it is not an image-preserving fix.

All four native preset builds pass without compiler/licence diagnostics, using
RelWithDebInfo rather than CMake Release. All 216 resident-worker admission and
continuous samples exceed the unchanged 3,072-MiB floor (minimum 6,653 MiB).
Original sources and binary, free permanent lock and no QOR workers are checked
afterward. Qualification failed and code is unchanged, so conditional Release/
sanitizer acceptance and broader gates do not resume. No new hardening, complete
lint, full-suite or sanitizer pass is claimed. Hosted Qt builds/core audit remain
separate skipped gates at the starting revision, with Release readiness failed.
New commands, configs, wire/CPU logs, renderer proofs, images, helper correction,
analysis and final publication checks are in
`build/development-2026-11-15/session-13/`; PR #10 remains draft.

### Client-only ES 3.1 diagnostic, 2026-11-16

Source `8eda5ec361cd6e5721303cb811544ebe1a86648b`; session-13's 204-file seal is
verified unchanged. All 14 Windows worktrees and both complete Pi checkouts were
clean, permanent locks available and no QOR workers active before work. The
20-minute diagnostic window started at 02:20:15 local time and closed at
02:26:23, before 02:40:15. Three focused executions; no unchanged retry or broad suite.

**Request and actual path.** A diagnostic shared library, compiled against the
installed commercial Qt 6.12.0, runs a constructor in only the selected client.
It copies the default `QSurfaceFormat`, sets `OpenGLES` and version 3.1, then calls
`setDefaultFormat`. It logs that no application or current context exists yet.
[Qt recommends setting the default before application creation](https://doc.qt.io/qt-6/qsurfaceformat.html#setDefaultFormat).
Only that client's environment contains the temporary loader setting. This is
not a production delivery mechanism; no application source or binary is rebuilt
or altered. The same helper in observer-only mode leaves the default unchanged.

Actual Studio and both Agent contexts report **OpenGL ES 3.1**, Broadcom **V3D
7.1.10.2**, Mesa **26.2.6-1~bpo13+0~rpt2**, with Qt's **OpenGL QRhi backend**
(Qt's rendering abstraction). The requested/default enum is OpenGLES (2), version
3.1; application/current-context presence is zero at the request. The existing
`v3d` render-node identity (226:128, mode 0660, existing account group permissions)
and direct EGL probe are rechecked. Studio's actual context, not just the direct
probe, gates the Agent run. The private compositor remains EGLFS KMS headless,
1920-by-1080, V3D/ES 3.1, with only `/dev/dri/renderD128` exposed. Audio, input,
primary display, network, private home and keyring boundaries remain unchanged.
No desktop, service, permission or system graphics/security change is made.

**Original assertions, actual outcome.** The original Agent executable
`5e754f6ee300b52801b6833a46f7d72919e0ff22d496e792f95902b6824ebd10` runs
`logicThatThrowsInPlayIsInTheLog aLongWaitLastsWhatItWasAskedFor -v2` under the
unchanged 5,400-second suite and 1,800,000-ms Qt watchdog limits. Verbose Qt Test
output identifies each assertion reached; it changes logging, not the test body.
Result: **3 passes, 1 failure, 0 skips, 29.713 s**.

| Assertion/behavior | Observed result |
| --- | --- |
| Studio opens; log contains `oops.qml` and TypeError | Reached and passed |
| Exactly one TypeError entry | Reached and passed |
| Original `times > 5` after `wait(600)` | Reached and failed, value **3** |
| Playing notice and later cursor assertion in that case | Not reached after failure; not passing or Qt-reported skips |
| Wall-clock tool result and elapsed >= 1500 ms | Reached and passed |

There are no missing-GLSL or graphics-pipeline warnings. Three actual QML
on-frame exceptions become one counted entry, consistent with consecutive-only
coalescing. The two audio warnings (`pa_context_connect` and no output device)
remain visible in the private no-audio environment and precede those exceptions;
no real device is exposed and no warning is filtered. No full Agent pass is claimed.

**Timing and counts.** Wayland wire tracing and 100-ms process CPU sampling are
retained; there is no profiler and observer/preload/verbosity overhead is
unmeasured. Agent first surface creation to first completed frame callback is
**25,381.554 ms**, including startup/context/shader work. The first request-to-
callback is **10.614 ms**. Five completed surface-frame callbacks precede the
failed assertion; display-sync callbacks are excluded. They are different events
from the three QML on-frame exceptions. Exact timestamps, buffer commits and
every callback interval are in `derived.json`. Whole-process samples cover
26.80 client CPU-seconds and 0.02 compositor CPU-seconds, not a per-pass profile.
There is no play-start marker or measured `wait(600)` elapsed value in the
unchanged executable. No precise shader-compilation, GPU or synchronization
bottleneck can be assigned from these timestamps alone.

**Complete images.** The unchanged Studio executable
`390d66647b83ecbb2053ae7aebaf963c37e6b30908d64df3d8274325ae83d981` runs
`theStudioWearsTheOneLooksToolRegister` on ES/V3D and on a fresh matched llvmpipe
client. The latter uses the identical helper in observer-only mode, original
default format (type 0/version 2.0), actual desktop GL 4.6/LLVM 19.1.7, and the
same headless V3D compositor. Profiles and caches are fresh; source, test, viewport
and limits match. Only client renderer/API selection differs. This is not the
original GPU-hidden software fence, and neither timing is directly comparable
to the old offscreen-compositor play-frame interval.

| Client | UI test pass/fail/skip | Test elapsed | Surface creation to first callback | Shader/pipeline warnings |
| --- | --- | ---: | ---: | ---: |
| V3D / ES 3.1 | 3/0/0 | 27.663 s | 25.380 s | 0 |
| llvmpipe / desktop GL 4.6 | 3/0/0 | 10.470 s | 6.951 s | 0 |

Both full 1600-by-960 captures were visually inspected: sky, environment
illumination, objects, floor/shadows, ring, transform gizmo and interface are
present. Quantitative comparison uses raw RGB with **no registration, rescaling
or masking of viewport pixels**, plus separately reported regions. The complete
frame mean absolute channel difference is 0.244/255. The entire viewport
(270,56)-(1280,960) is 0.062224/255, RMS 0.363409, 99th percentile one and maximum
136. Exactly 195 of 913,040 viewport pixels (0.02136%) have any channel difference
above eight; outliers cluster at the sphere/ring edges, not missing sky or floor.
The sky region maximum is six; the floor/shadow region maximum is eight.
Toolbar and inspector differences are retained: their mean differences are
2.351 and 0.367 respectively, including random project-label/layout and text
rendering differences. These are descriptive measurements, not a newly chosen
pass tolerance. The images are not exactly equal; one fixture is not exhaustive
image qualification. Both narrow button/focus checks passing alone proves less.
Full images, hashes, region metrics and outlier coordinates are retained.

**Supplemental logging proposal, not implemented.** Reuse the suite's existing
one-time `StudioLog::install`; do not install it recursively. Use a hidden inert
QML Window with the properties/signals consumed by `StudioTools`, the real
`QQmlEngine`, `McpServer`, `StudioTools` and `Client::tool("log_read")`, without
opening the 3D Studio, starting play or scheduling natural frames. An actual QML
signal-handler fixture throws at one fixed source line when explicitly triggered;
its warning goes through Qt's normal handler, not direct `StudioLog::append`.
Setup/import warnings must be checked before taking the starting cursor, not
silently discarded as acceptable fixture noise. Exact sequence:

1. Emit a known Studio notice, then the identical real QML error twice. Read via
   MCP: one error entry, `times == 2`, source/text and warning level correct,
   notice retained, next cursor advanced.
2. Emit the same error once after that read. Read from the prior cursor: the same
   coalesced entry is visible with `times == 3` and a newer sequence. An immediate
   read without emissions returns no new entries and a stable cursor.
3. Emit a distinct real QML `console.warn` sentinel, then the same error three
   more times. Full-window read must show error runs **3 and 3**, separated by
   the sentinel, exactly six errors total, preserved ordering/notice and advancing
   cursors. The read from step 2's cursor sees only the sentinel and new error run.

This protects lossless warning capture, consecutive-only repetition, warning
interleaving, aggregate error counts and incremental visibility. It does not
assert one global error entry across unrelated warnings, promise a frame rate,
exercise GPU images, or replace natural on-frame delivery. Retain the original
600 ms / `times > 5` case unchanged and independently failing. This is a proposed
supplement, not an implemented test-contract change or acceptance waiver.

**Decision and bounded next step.** Explicit ES 3.1 is supported by the actual
client and resolves the observed desktop-GL shader/image mismatch. It does
**not** qualify the focused workload: three callbacks still fail the original
gate. The independent llvmpipe baseline remains `times > 5` with one. A minimal
production candidate for the image defect would be a shared early Qt-format
initializer selected only by an explicit supported Pi/V3D launch configuration,
before application/context creation, without changing default desktop/software
selection. It would replace the diagnostic loader, not ship it. Before adoption,
add deterministic full-scene image regressions covering sky, lit materials,
shadows and gizmos with fixed project labels/camera/animation state, approved
cross-renderer edge tolerances, actual API verification and unsupported-context
failure coverage. Validate other platform defaults and both software/hardware
paths, then relevant Release/sanitizer gates. No such production change is made.

The single next investigation is a bounded first-Play timeline/profile under
this verified ES configuration, separating client synchronization/shader work
from presentation waits and mapping natural callbacks to render phases. Reuse
the existing targeted render-phase diagnostic; do not infer the expensive path
from startup totals or retry broad suites. If a later cold/warm cache comparison
is needed, label and control cache state explicitly; warm-up is not an acceptance
workaround.

All 1,414 original sources, both executables and the pre-existing screenshot are
verified after the run. The subsequent observer-only client confirms no process-
local format persists; no global loader variable is set and all QOR workers exit.
All 168 native guard samples exceed 3,072 MiB, minimum 6,631 MiB. The only new
compile is the diagnostic library, warning-free with stack protection and eager
read-only relocations; the reused application cache remains RelWithDebInfo.
No new application Release build, hardening/lint suite, sanitizer or broader
acceptance result is claimed. Hosted Qt builds/core audit were skipped and Release
readiness failed at the starting revision; current publication outcomes remain
separate in the handoff. PR #10 stays draft. Receipts:
`build/development-2026-11-16/session-01/`; previous sealed evidence is unchanged.

### Earlier startup measurements

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

## Linux and platform optimization research (2026-11-12)

**Status: research candidates, not an accepted architecture change or measured
speedup.** This review changes no kernel, configuration, service, dependency or
resource guard. It is not a new roadmap. The order in [OWNER](../OWNER.md) and
[ROADMAP](ROADMAP.md) remains: Prism regressions, Release/sanitizer validation,
full core-enabled OS/product checks, runner isolation before Qt CI, then Q Pi
and single-image feasibility. Research can inform later work without admitting
another build or claiming a blocked milestone passed.

### Evidence and recommendation

Repository context reviewed: [VISION](../VISION.md), this performance record,
[architecture](ARCHITECTURE.md), [QOR-0004](decisions/QOR-0004-the-base-image-is-fedora-atomic.md)
and [QOR-0025](decisions/QOR-0025-q-pi-development-edition.md). At this review,
local work follows `01ba206`; the development handoff owns runtime status.
Existing observations above are historical and scoped to their binaries and
environments. They identify backdrop recapture as an integrated-GPU cost and
first-use pipeline creation as a hitch. They do not establish native Linux GPU
budgets, real input-to-photon latency or Q Pi acceptance.

**Current recommendation:** keep maintained Linux kernels with tested,
hardware-specific configurations and a shared QOR performance policy. Profile
Prism, QQ and their userspace dependencies before redesigning the kernel. This
matches VISION's principle of leveraging mature foundations rather than
rewriting them. A future bottleneck may justify changing that recommendation;
no candidate below is an assertion that a particular workload becomes faster.

| Option | Benefit to investigate | Cost and decision trigger |
|--------|------------------------|---------------------------|
| Maintained Linux plus configuration/userspace profiles | Existing drivers, Qt, graphics, audio, storage and developer ecosystem | Default path; compare against the distribution's supported baseline |
| Small downstream Linux patches | Fix a reproduced kernel bottleneck or missing platform behavior | Require a minimal reproducer, upstream assessment, security/driver testing, upgrade ownership and rollback |
| Deep Linux fork | More control over internals | Requires evidence that smaller changes cannot meet the product need and a funded maintenance/compatibility plan |
| Entirely new kernel | Architectural freedom | Linux applications and drivers do not automatically work; requires explicit scope and ownership for ABI, GPU/display, USB, storage, networking, power and security before replacing Linux |

Linux's userspace interface is stable, but its in-kernel driver interface is not
[source K8]. Custom kernels therefore increase module validation work. NVIDIA
modules must match their corresponding firmware and userspace release [G2].
Linux is GPL-2.0 with a syscall exception [K9]; kernel distribution compliance
and Qt distribution entitlement are separate questions. No approach guarantees
all games, engines, peripherals or anti-cheat systems will work.

### Prioritized experiments, not default-setting instructions

| Candidate | QOR/QQ benefit to measure | Tradeoff and boundary |
|-----------|--------------------------|-----------------------|
| Damage-aware rendering and bounded effects | Capture only changed regions behind visible glass; stop settled timers and hidden previews; keep UI threads responsive | Preserve trusted UI, accessibility, appearance and interrupted-animation correctness. Use QML profiling [Q1], not assumptions |
| QML ahead-of-time compilation and pipeline caches | Lower launch and first-use shader stalls; verify compiled bindings and cold/warm behavior [Q2, Q3] | Cache behavior is graphics-backend/device/driver dependent. Do not ship a cache as universally compatible |
| Native display/graphics path | Per-output pacing, direct scan-out where eligible, supported synchronization/VRR and efficient hybrid-GPU buffer transfer | Full-screen does not guarantee scan-out: overlays, effects and formats can require composition. WSL/software tests are not bare-metal proof; gamescope nested and embedded paths differ [G1] |
| Background-work resource policy | Bound imports, builds, indexing and thumbnails; compare CPU/I/O weights and parallelism while a game or editor is active [K1] | Prefer delaying restartable work to killing applications. Do not overprotect memory or weaken the existing build-admission guard |
| Pressure-aware memory and bounded caches | Measure pressure stall information (PSI), cache growth and reclaim; evaluate supported Multi-Gen LRU and modest zram [K2-K4] | Compression costs CPU and memory, not extra physical RAM. Aggressive reclaim/OOM policy can lose unsaved work. No blanket cache-dropping scripts |
| Power and thermal profiles | Compare balanced versus optional gaming/creation policies, including sustained Pi load [P1] | Peak clock is not sustained performance. Heat, fan noise and battery life are metrics; no blanket performance governor or default overclock |
| Storage scheduling | Compare stock with supported alternatives while loading a project/game during background I/O [K5] | BFQ trades CPU/throughput for responsiveness; do not impose it on every NVMe/SD device. Retain journaling and durability |
| Audio scheduling and buffers | Measure PipeWire quantum/rate, deadline misses and XRUNs for gameplay, QQ preview and media [A1, A2] | Lower buffers increase scheduling pressure; Bluetooth adds latency. Use supported bounded real-time handling, not unlimited priority for arbitrary apps |
| Kernel preemption/scheduler experiments | Compare supported preemption modes and task performance hints only after profiling [K6] | Availability and effects vary by architecture/kernel. PREEMPT_RT is not a generic gaming upgrade |
| sched_ext or targeted patches | Investigate a reproduced scheduling problem with a reversible experiment [K7] | Requires support in the actual kernel and regression/security/upgrade testing; fallback to the fair scheduler is not performance proof |

For QQ Studio, first measure asynchronous/cancelable asset import, bounded build
parallelism, inactive viewport rendering, texture/thumbnail budgets, scalable
preview effects and editor responsiveness during compilation. Current QQ uses
Quick 3D; a deliberately lightweight 2D edition is future product work, not an
existing feature toggle. Remote heavy work must remain optional and user-controlled.
Premium appearance means coherent typography, motion, accessibility and pacing,
not enabling every expensive effect on every device.

### Hardware profiles and compatibility

- **PC QOR OS:** qualify Intel, AMD, NVIDIA and hybrid layouts separately with
  supported kernel/Mesa/firmware/driver combinations. Keep security mitigations,
  signing and a known-good rollback. Hardware capability claims require testing.
- **Pi 5 first:** retain its supported Raspberry Pi OS hardware stack and the
  existing 16-KiB-page qualification. Pi 5 has a real GPU: Mesa V3D/V3DV render
  support and VC4 display support [G3]. Check correct hardware acceleration,
  adequate power and cooling before interpreting software tuning results.
- **Wider Pi support:** model-specific boot/kernel/graphics/RAM profiles are
  necessary. Official firmware distinguishes `kernel_2712.img`, common ARM64
  `kernel8.img` and older 32-bit paths [P2]. Do not label every ARM device Q Pi
  or claim all Pis are supported; Pico microcontrollers are not Linux desktops.
- **One-download vision:** a combined QOR OS image may contain several
  architecture-specific boot paths and kernels. One package does not imply one
  universal kernel binary. Installer feasibility and hardware acceptance remain
  separate milestones, not consequences of this research.

### Reproducible evaluation and decision gates

1. Record source/binary/image digests, kernel/configuration, firmware, GPU driver,
   storage, RAM, display modes, power policy and ambient/thermal conditions.
2. Establish a stock **native hardware** baseline. Use the existing Prism
   benchmark for its measured scope; keep software, WSL and hardware data separate.
3. Change one factor at a time. Alternate baseline/candidate runs, repeat enough
   to characterize variance, retain outliers and failures, and report all runs
   rather than selecting only the best power state. Separate cold/warm caches.
4. Exercise idle/launch, glass/window motion, mixed-refresh displays, QQ editing
   plus imports/builds, a reproducible game replay with background load, media
   and audio under load, and sustained Pi thermal behavior.
5. Record frame p50/p95/p99 and long stalls, missed frames, input-to-swap separately
   from input-to-photon, CPU/GPU use, memory/PSI, I/O latency, audio XRUNs, power
   and temperature. Measure individual CPU/GPU work rather than treating frame
   intervals as rendering costs.
6. Define a user-relevant threshold before the experiment. Accept only repeatable
   gains beyond run-to-run noise without correctness, compatibility, security,
   accessibility, data-loss or sustained-power regressions. Keep a stock fallback.
7. Revisit a kernel change only when a trace identifies a kernel limitation,
   upstream/configuration/userland alternatives are assessed, and driver,
   licensing, recovery and ongoing maintenance are accounted for. A new accepted
   architecture decision must follow the existing decision process.

This review ran no performance tests. Documentation research neither resolves
the Windows/WSL RAM blocker nor authorizes changes to settings, services, kernels,
physical media, security boundaries or the running development session.

### Primary references

Reviewed on 12 November 2026. Live upstream documentation can change; recheck
against the exact package/kernel version before implementing a candidate.

- **Q1:** [Qt Quick performance and profiling](https://doc.qt.io/qt-6/qtquick-performance.html).
- **Q2:** [Qt Quick Compiler](https://doc.qt.io/qt-6/qtqml-qtquick-compiler-tech.html).
- **Q3:** [Qt graphics configuration and pipeline caches](https://doc.qt.io/qt-6/qquickgraphicsconfiguration.html).
- **K1:** [Linux cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html).
- **K2:** [Pressure stall information](https://docs.kernel.org/accounting/psi.html).
- **K3:** [Multi-Gen LRU](https://docs.kernel.org/admin-guide/mm/multigen_lru.html).
- **K4:** [zram](https://docs.kernel.org/admin-guide/blockdev/zram.html).
- **K5:** [BFQ I/O scheduler and tradeoffs](https://docs.kernel.org/block/bfq-iosched.html).
- **K6:** [Scheduler preemption models](https://docs.kernel.org/scheduler/sched-preemption.html).
- **K7:** [sched_ext](https://docs.kernel.org/scheduler/sched-ext.html).
- **K8:** [Linux kernel driver interfaces](https://docs.kernel.org/process/stable-api-nonsense.html).
- **K9:** [Linux kernel licensing rules](https://docs.kernel.org/process/license-rules.html).
- **G1:** [Valve gamescope](https://github.com/ValveSoftware/gamescope).
- **G2:** [NVIDIA open GPU kernel modules](https://github.com/NVIDIA/open-gpu-kernel-modules).
- **G3:** [Mesa V3D/V3DV](https://docs.mesa3d.org/drivers/v3d.html).
- **P1:** [Raspberry Pi 5 hardware and cooling](https://www.raspberrypi.com/products/raspberry-pi-5/).
- **P2:** [Raspberry Pi kernel selection](https://www.raspberrypi.com/documentation/computers/config_txt.html).
- **A1:** [PipeWire pw-top measurements](https://docs.pipewire.org/page_man_pw-top_1.html).
- **A2:** [PipeWire real-time module](https://docs.pipewire.org/page_module_rt.html).

### Implementation assessment against the current tree (2026-11-12)

The separately authorised implementation pass preserves the research above and
the ordered Prism milestones. It adds no dependency, kernel policy or Qt build.

- **Implemented now: trustworthy benchmark acceptance.** The previous real
  evaluator incorrectly accepted three inert reports: 1 recorded / 40 posted
  inputs, more recorded samples than posted events, and a missed percentage
  inconsistent with the missed count. The shared bounded reader/validator now
  rejects them. This is an evidence-quality improvement, not a rendering speedup.
  The real embedded report evaluator and its shared production validator pass
  26 portable tests on Windows, including malformed input and exact timing-target
  boundaries. That Windows run skips the 25 Linux launcher/bundle process tests;
  both Bash scripts pass syntax checks. Six unchanged, hash-verified historical
  software reports satisfy the new structural checks and remain single-output
  diagnostics, not hardware-gate passes. Evidence and the initial failures:
  `build/performance-gate-2026-11-12/session-01/`.
  On 13 November the unchanged sources ran under the Pi's original coordinator:
  all **51 tests pass, zero failures/errors/skips**, including those 25 Linux
  process cases. These fixtures use inert programs and hardware metadata; no
  compositor, GPU, real input or timing benchmark runs. Native receipts:
  `build/development-2026-11-13/session-01/native-gate-20261113/`.
- **Already present; retain and measure.** Prism's `qt_add_qml_module` build uses
  Qt's script compilation path, consistent with Q2. `AssetThumbnails` requests
  asynchronous image loading and uses `QImageReader::setScaledSize`; Studio's
  Project/Library images request 40-by-40 sources. Its signed-byte validation and
  image-size limits must remain. `StatsOverlay` samples only while visible, and
  `FrameStats` stops its publication timer at idle. Linux product builds already
  specify one compile job. Adding duplicate caches, global thread settings or
  another idle timer would not implement a missing research finding.
- **Next bounded experiments after Prism's runtime gates.** Profile asset imports
  and inactive QQ viewports with representative small/large projects; measure
  UI responsiveness, peak memory, cancellation and project-switch correctness.
  Compare baseline/candidate cold and warm runs on the same platform, retaining
  every result and source/binary digest. Before changing damage regions or effect
  budgets, measure backdrop recapture and check visual/accessibility equivalence.
  These candidates need actual Qt/product runtime tests and same-machine benchmarks;
  source inspection supplies neither a bottleneck measurement nor a speed claim.
- **Hardware/architecture-dependent work stays explicit.** Direct scan-out,
  multi-GPU pacing/VRR, driver pipeline caches, kernel/scheduler/MM changes, zram,
  power/thermal policy, I/O scheduling and audio buffer changes require the
  supported hardware's baseline, sustained-load tests and rollback. QOR-0005's
  installed-image backend acceptance and QOR-0025's native Pi qualification still
  apply. No setting is enabled from research alone, and neither ARM architecture
  nor a synthetic report establishes an edition or hardware support.

No native rendering benchmark or new performance-gain measurement ran in this
implementation pass. Full OS/product checks, runner isolation, Q Pi and the
single-download installer remain in the owner's existing order.

## First-Play phase profile, 2026-11-16

At `9357d3b`, a 20-minute window ran from 09:15:41 to 09:24:48 local time,
closing after three focused executions. Temporary client observers reuse Qt's
window synchronization/recording/swap signals and `playingChanged`, recording
monotonic timestamps, process/thread CPU and thread IDs into an 8,192-event buffer.
No graphics commands, frame requests or timers are added. Extended Quick 3D
statistics are enabled in the observed runs and sampled before synchronization,
while the GUI thread is blocked; values can describe an earlier frame. The original
test executable and all assertions run unchanged with `-v2`. No source overlay,
application rebuild or system configuration change is used.

Actual context on every measured process: Broadcom V3D 7.1.10.2 / OpenGL ES 3.1,
Mesa 26.2.6-1~bpo13+0~rpt2, Qt 6.12 OpenGL QRhi backend. RenderD128 remains the
only exposed graphics node, with its existing identity/permissions and v3d driver.
The same private EGLFS headless compositor runs at 1920x1080 without physical
vsync; Studio is 1600x960. Private namespaces, audio/input/network/home/bus/keyring
boundaries, 30 resident admission samples, 3,072-MiB guard, 5,400-second suite
limit and 1,800,000-ms test watchdog remain. This compares clients on one compositor,
not timing against the earlier software-compositor baseline.

### Measurements and limits

| Measurement | Fresh client cache, observer off | Fresh client cache, observer on | Seeded client cache, observer on |
|---|---:|---:|---:|
| Focused cases, pass/fail/skip | 3/1/0 | 3/1/0 | 3/1/0 |
| Total test time | 29.820 s | 29.782 s | 6.265 s |
| Window show to first swap | unobserved | 25.435 s | 1.793 s |
| First frame after recording to swap, wall / render-thread CPU | unobserved | 23,795.162 / 23,313.795 ms | 519.492 / 22.180 ms |
| First Play frame after recording to swap, wall / render-thread CPU | unobserved | 508.851 / 11.111 ms | 507.219 / 9.676 ms |
| Whole-process sampled CPU | 26.82 s | 26.84 s | 3.17 s |
| First-window QRhi lifetime pipeline creation | 415 ms | 435 ms | 97 ms |
| First surface creation to callback | 25.461 s | 25.446 s | 1.805 s |
| First request to callback | 11.425 ms | 11.109 ms | 11.625 ms |

Cold means an empty **dedicated client disk cache**, separate from the compositor's
cache. OS page-cache, GPU and thermal state were not reset. The warm comparison
changes only that disk-cache seed: all 73 files, 2,326,167 bytes, are under
`mesa_shader_cache` and hash-match the cold run's saved files. New private profiles
and the same observer/library, binary and compositor configuration remain. The
second wall-clock case shares its process/cache with the first case; it is not an
independent cold-start sample. This identifies a cache-dependent startup cost,
not the precise compiling shader or all driver internals.

The first cold frame spends 69.396 ms in Qt synchronization and 1,112.188 ms from
`beforeRendering` to main-pass recording. Most of its wall and CPU cost occurs
after `afterRendering`, before `frameSwapped`. During first Play, the corresponding
interval is instead predominantly waiting. Four of five cold first-Play samples
show `QSGRenderThread` sleeping in `dma_fence_default_wait` (one reports `0`);
the GUI thread sleeps in `futex_do_wait`. The warm trace retains the fence wait.
Compositor sampled CPU is 0.03 s in each whole-process trace. These locate a
client-side wait around submitted graphics work, but do **not** identify the
originating EGL/GL call, fence owner, GPU pass or GPU execution duration. Low
compositor CPU alone does not exclude its involvement in buffer/fence ownership.

Qt's [window signals](https://doc.qt.io/qt-6/qquickwindow.html#afterRendering)
separate command recording from submission; swap is not proof of physical scanout.
Its [timestamp documentation](https://doc.qt.io/qt-6/qquickgraphicsconfiguration.html#setTimestamps)
explicitly excludes OpenGL ES. Recorded zero GPU-time fields are unavailable data,
not zero GPU work. QRhi's 435-ms pipeline counter cannot account for or assign
the separate 23.8-second interval. No call-stack or per-pass GPU attribution is claimed.

Observer event handlers account for 0.854 ms cold / 0.793 ms warm across 197/175
events. This partial accounting includes double-counted marker time in statistic
snapshots and excludes event-filter setup/dispatch and Qt's extended-statistics
cost. The single cold on/off pair differs by -38 ms elapsed and +20 ms sampled CPU;
it does not isolate profiler overhead from variance or establish a confidence bound.
Common 100-ms process/thread sampling, Wayland tracing and `-v2` overhead remains
unmeasured. No profile timing is promoted into an acceptance threshold.

### Original assertions and callback accounting

All three executions pass Studio opening, source/TypeError presence and
`typeErrors == 1`, then fail unchanged `times > 5` with **3**. Playing-notice and
cursor assertions below that failure are **unreached**, not passing or framework
skips. Both wall-clock assertions pass. Each log retains two expected private-audio
warnings and three QML TypeErrors; no shader or pipeline warning occurs.

Each trace completes five Wayland **surface-frame** callbacks before failure,
excluding display-sync callbacks. Subsequent request-to-callback latencies are
5.575–5.974 ms across the three runs. Observed cold swaps after `playing=true`
occur at 519.735, 530.809, 542.495 and 748.611 ms; warm swaps at 516.694, 528.009,
539.989 and 681.998 ms. Four swaps are distinct from three QML error callbacks.
Playing-true to playing-false spans 758.741/692.074 ms; this includes play/stop
dispatch and is **not** a direct measurement of the wait-tool call. Natural-error
emission times were not separately instrumented. All raw phase and wire timestamps
are retained. The independent llvmpipe failure remains `times > 5` with 1.

### Outlier inspection and proposed visual contract

The sealed ES and matched llvmpipe full images are unchanged. New 3x nearest-neighbor
inspection crops cover every one of the 195 pixels above eight channel levels;
an amplified absolute-difference image and marked full image retain their positions.
Of those pixels, 192 have a control-image 3x3 gradient of at least 8 (150 at least
16). The other three differ by 9 and match nearby control values within 2–3 levels.
Only 98/195 have a control pixel within one pixel and eight channel levels; **97
do not**. Sphere/ring silhouettes and small coverage/shading differences are visible,
without missing sky, environment lighting, objects or shadows in this fixture.
This supports edge-localized differences, not a proof that every outlier is harmless
or that the images are equivalent. Mean difference alone is insufficient.

Proposed acceptance, requiring review before implementation:

- Fix size, device scale, camera, geometry, light, labels, selection, animation
  and readiness; compare the entire captured frame. Do not mask dynamic UI labels;
  make them deterministic. Preserve raw images and a same-renderer reference.
- Define sky, lit-material, ground/shadow and UI regions from the fixture. Require
  every sky/ground reference pixel to differ by at most 8 per channel. Outside a
  one-pixel reference-derived edge band, use the same maximum; a global mean cannot
  waive a regional failure. Report maxima, counts, locations and connected regions.
- Within the edge band, initially require a matching reference colour within one
  pixel and eight levels, plus unchanged silhouettes/features. **The present 97
  unmatched pixels do not satisfy this candidate criterion.** Inspect their coverage
  and repeatability before proposing any alternative antialiasing metric; do not
  fit a tolerance merely to make this pair pass.
- Run three deterministic captures per renderer to assess repeatability, with
  exact same-renderer goldens where stable and separately reviewed cross-renderer
  limits. Validate the checker against missing-sky, disabled-light, missing-object
  and shifted-geometry negative controls, then require full-image review.

The existing missing-sky capture from session-13 is an offline negative control:
all 153,000 pixels in the fixed sky region exceed 8 versus the matched control,
maximum 231. Thus the proposed regional bound rejects that known defect. Other
negative controls and repeatability checks are **not run**; no visual contract is
implemented or declared passed. Previous toolbar/inspector differences also remain
until the fixture is deterministic. No new image render was needed in this session.

### Supplemental logging coverage and production recommendation

Code review reconfirms `StudioLog::append` merges only the last identical text and
level and advances its sequence on every repetition; `since` returns entries whose
current sequence exceeds the cursor. QML exceptions are warning-level messages.
The existing handler must be installed once, not recursively reinstalled.

Recommend the previously designed inert-window/real-QML/MCP test as **additional**
coverage: emit two identical errors, read `times=2`; emit one more and read the same
entry with cumulative `times=3` and a newer cursor; a no-op read is empty with a
stable cursor. Insert a real warning then emit three more identical errors. Two
separate runs `[3,3]`, a retained intervening warning, six total errors, notices,
order and correct incremental visibility must result. Do not sum cumulative counts
across reads as if they were deltas. Use a stable QML source line and verify setup
warnings before taking the initial cursor. Exercise the actual message handler and
`log_read`, not direct appends. This protects capture, consecutive coalescing,
warning interleaving and cursor semantics; it proves neither frame rate, Play
responsiveness nor GPU image correctness. Retain the natural gate unchanged.
No assertion/timeout/test-contract change is recommended as a means of passing it.

The smallest production candidate remains an explicitly selected Pi/V3D startup
surface-format initializer before application/context creation, with other renderer
defaults preserved. It addresses the observed image defect only. Require supported
selection and actual-context checks, unsupported-context failure coverage, the
reviewed deterministic image suite above, and relevant Release/sanitizer/cross-path
validation before adoption. Do not ship the diagnostic loader or treat warmed-cache
behavior as a responsiveness fix. No production implementation is made here.

### Restoration, refusals and next evidence

All 1,414 original source hashes, Agent/Studio hashes and the pre-existing screenshot
are verified intact. A fresh no-application observer-only process confirms the
original default surface format (type 0/version 2.0); no loader variables persist,
QOR workers are absent and both permanent locks are available. The software setup
and sealed session-01/13 artifacts remain intact. All 250 native samples exceed
3,072 MiB; minimum 6,718 MiB.

Two setup attempts launched no test: an inherited old manifest was correctly
refused, then private-profile moc could not discover the existing Qt licence
service. The current 1,414-source manifest was reverified; code generation moved
to the existing guarded host licence context without a bypass. The resulting
library was reused byte-for-byte for both observed runs. A premature profile launch
was refused while the reference lock was held; its qtlicd child drained normally
over 23 samples, and the reference wrapper completed with zero exit status.
An intermediate status message incorrectly called this a timeout and was corrected
against the final receipt. No service configuration or manual stop was used.
An offline analyzer's Windows text-decoding error was corrected by using UTF-8;
it caused no test rerun. These receipts are retained, not presented as test passes.

No application Release/sanitizer/full-suite/lint/hardening acceptance was run anew;
the preserved cache remains RelWithDebInfo. Documentation hooks and hosted source
checks are separate from device acceptance. PR #10 remains draft while required
Qt/core-audit and other acceptance gates remain unmet.

**Next concrete action:** use the available `/usr/bin/strace` in a bounded,
client-only run alongside the same phase observer, tracing the relevant graphics
ioctls and waits to identify the first-Play DMA fence and originating API boundary.
Measure its perturbation and keep GPU-time attribution open without suitable data.
Do not change compositor, security settings or natural-frame requirements.
Receipts: `build/development-2026-11-16/session-02/`, especially `profile-analysis.json`,
`derived.json`, `image-review.json` and raw logs under `from-pi/`/`final-from-pi/`.

## Client-only graphics wait trace, 2026-11-16

Continuation at `489ddfe`, branch `fix/native-play-clock-waits`, draft PR #10.
Session-02's 236-file seal is verified unchanged. All 14 Windows worktrees,
four complete native checkouts, original locks and active QOR workers are checked
before work. The 20-minute investigation opens at 09:47:23 local and closes at
10:02:24 (cutoff 10:07:23): three focused executions and one preserved observer
compilation refusal that launches no test. Subsequent analysis/publication does
not run further graphics variants. Receipts live in
`build/development-2026-11-16/session-03/`.

### Controlled setup and bounds

Every execution uses the original `tst_agent` SHA
`5e754f6ee300b52801b6833a46f7d72919e0ff22d496e792f95902b6824ebd10`,
the unchanged `logicThatThrowsInPlayIsInTheLog` and
`aLongWaitLastsWhatItWasAskedFor` cases with `-v2`, and the existing client-only
ES/phase observer SHA
`e0b1b28a479ef306181eaaf1a5bf9174722bd8842fbe33e1874e638674e4cccf`.
The cache is RelWithDebInfo, not Release. The 5,400-second suite and
1,800,000-ms function limits, assertions and play/wait behavior are unchanged.

Private user/process/mount/network namespaces, home/data/config/cache/runtime,
bus/keyring/audio/input isolation and read-only source remain. Only
`/dev/dri/renderD128`, character device 226:128, original permissions and actual
sysfs V3D driver, is exposed; no primary DRM/display or real sound/input devices.
Actual client identity in both test contexts is V3D 7.1.10.2, OpenGL ES 3.1,
Mesa 26.2.6-1~bpo13+0~rpt2 and Qt 6.12 OpenGL QRhi. The private EGLFS KMS
headless compositor is unchanged at 1920x1080 with no physical vsync; client
1600x960. Each client cache starts empty and is separate from the compositor
cache. System page caches and thermal state are not reset. No desktop, system
graphics/security/service setting or software-renderer configuration changes.

Installed `strace` is 6.13 with no stack-unwind support. A one-syscall detach
probe inside the same sandbox verifies that daemonized tracing preserves client
PID and intentional exit 7. Missing `/proc/sys/kernel/yama/ptrace_scope` is recorded
as absent, not assumed permissive. Only the chosen test process and descendants
are traced; no compositor, desktop or unrelated process is attached.

The trace command is `strace -D -f -I 1 --absolute-timestamps=unix,ns -T -yy -v
-s 64 --syscall-limit=50000 -e
trace=ioctl,futex,poll,ppoll,epoll_wait,epoll_pwait,clock_nanosleep -o <private-log>`
with client-only preload/environment passed using `-E`, followed by the original
test/cases. The supervisor detaches only its identity-checked tracer at 60 seconds
or 16 MiB; these are capture limits, not test limits. The complete trace ends
naturally at client exit 1 after 30.757 seconds: 2,061,896 bytes, 5,838 completed
calls, 4,282 ioctls, 20 client TIDs, no unmatched/resumed calls or capture cap hit.
Wall-clock trace timestamps align with monotonic phases using paired CPU samples;
median epoch offset 1794525523047250176 ns, spread 18,037 ns. That spread is not
a bound on tracing overhead.

### Results and perturbation

| Client observer | Test elapsed | Show to first swap | First-Play after-render to swap | Render-thread CPU in interval | Natural error callbacks (`times`) | Wayland surface callbacks before failure |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Existing phase observer | 29.781 s | 25.407 s | 508.726 ms | 11.119 ms | 3 | 5 |
| Same plus `strace` | 30.535 s | 25.047 s | 521.452 ms | 13.694 ms | 1 | 4 |
| Same plus targeted ioctl observer | 29.811 s | 25.428 s | 510.661 ms | 14.861 ms | 3 | 5 |

All three report **3 passed, 1 failed, 0 skipped**. `times > 5` fails unchanged;
source/error presence and consecutive coalescing assertions pass. Later
notice/cursor assertions are unreached. The wall-clock case passes, including
`waited.elapsed() >= 1500`. Two private-audio warnings persist; QML TypeErrors
number 3/1/3. Shader/pipeline errors are absent. Surface callback timestamps and
latencies, every phase, and reached assertions are retained in raw logs and
`profile-analysis.json`; surface callbacks are not the test's error callbacks.

The ptrace run adds 754 ms overall and changes the callback count. Its first-Play
interval adds 12.726 ms. These single observations establish perturbation, not a
statistical overhead bound. The lower-overhead observer adds 30 ms overall and
1.935 ms to that interval, reproducing three callbacks. Its selected-call handler
accounting totals 28.430 ms across the process, excluding real ioctl time;
phase accounting is 1.014 ms (reference 0.811 ms, strace 19.442 ms). Unselected
hook dispatch, common CPU/wire observation and normal run variability are not
fully measured. No claim of zero observer overhead.

### Buffer waits and call-stack coverage

The targeted client library forwards requests/returns unchanged and preserves
`errno`. It observes only V3D WAIT_BO/CREATE_BO/SUBMIT_CL on character 226:128,
for 60 seconds, at most 8,192 events and eight backtraces of waits above 10 ms.
Payload copies read only the client itself; submission arrays are bounded at
256 handles. It adds no rendering command or frame request. It records 3,456
events (2,868 waits, 474 creations, 114 submissions), six stacks, zero dropped
events and zero truncated arrays. The initial compile refused kernel-source
`__user` annotations; the temporary userspace observer corrects that annotation
only, then compiles warning-free with `-Wall -Wextra -Werror` and hardening flags.
No original source or executable is rebuilt or replaced.

Observed request values are WAIT_BO `0xc0106441` (16 bytes), CREATE_BO
`0xc0106442` (16 bytes), SUBMIT_CL `0xc0486440` (72 bytes). Actual V3D identity
disambiguates strace's overlapping ioctl names. Installed UAPI header 6.18.60 is
newer than running kernel 6.18.50; ABI sizes/requests are verified, not an exact
source match. Both header hashes and compile configuration are retained.

The observed first Play starts at monotonic 326587005043160 ns; after-render is
326587016618423 ns and swap 326587527278962 ns. On render TID 31/FD 16, **417
WAIT_BO calls occupy 491.838 ms of this 510.661-ms interval**, with 0.969 ms CPU.
Sixty-three calls exceed 1 ms and sum to 491.351 ms. The dominant handles are:

| Handle | Calls in interval | Wall time | Creation request size | Relevant captured submission |
| --- | ---: | ---: | ---: | --- |
| 291 | 50 | 383.545 ms | 4,096 bytes | Event 946, 11 BO handles, out_sync 2 |
| 184 | 15 | 87.248 ms | 16,384 bytes | Events 373/897 |
| 289 | 3 | 11.730 ms | 8,192 bytes | Event 927 |

Handle 291 has 49 timeout 0 calls and one infinite-timeout call; 48 return
`-1/ETIME` (62). Successful-call errno values are stale and are not errors.
Submission 946 starts 40.468 ms after Play and includes handle 291. Call 982 on
that buffer lasts 11.853 ms, only 0.005 ms thread CPU. Its captured stack is
`QRhi::endFrame` -> unresolved Qt/Mesa frames -> `drmIoctl` -> observer. Call 933
on handle 289 gives the same enclosing path. Mesa offsets including `0x12ef550`,
`0x12ef6d0`, `0x12f2304`, `0x130726c`, `0x1307cdc`, `0x877af4`, `0x41e39c`,
`0x41f958`, and QtGui `0x596f64`/`0x597ec4` are preserved with ELF build IDs
and SHA256s. The Qt callsite is an indirect dispatch, not a resolved GL name.

The lower-overhead result confirms that expensive timeout 0 calls persist without
ptrace; attributing all such cost to strace would be wrong. It does not establish
why those nominal polls sleep or whether driver timeout granularity, resource
reuse or GPU work dominates. The [V3D driver source](https://raw.githubusercontent.com/raspberrypi/linux/rpi-6.18.y/drivers/gpu/drm/v3d/v3d_bo.c)
describes waiting through a buffer reservation and converting nanoseconds to
kernel timeout units; the live branch is not proven to match this running build.
No kernel defect is concluded from it.

Neither WAIT_BO nor the captured submission supplies a unique underlying fence
context/sequence. BO handles and sync object 2 may be reused; close/lifetime and
other submission types are not fully instrumented. These are correlated buffer
and submission records, not proof of a specific shader/pass or unique fence.
The traced infinite wait on handle90 (459.636 ms, starting Play+640.103 ms) is
later than the first-frame interval. The observer's corresponding later long
wait is 306.566 ms. Neither is substituted for the dominant first-frame cost.

Installed Mesa/Qt libraries lack `.debug_info`, `.symtab` and matching build-ID
debug files; adjacent/debuglink search locations also lack the named files.
Of each first-frame 18-frame stack, three frames have exact dynamic
symbol names (observer ioctl, drmIoctl, QRhi::endFrame); 15 retain object/offset
only. `addr2line` falls back to unrelated nearest exported names, so those names
are explicitly rejected. No download, package installation or security change
was made to obtain symbols. GPU timing/pass identity remains unknown; unsupported
Qt OpenGL ES GPU timestamps are not evidence of negligible GPU execution.

### Preservation, decision and next action

All 1,414 sources, original Agent/Studio binaries and sealed screenshot verify
unchanged. No preload/observer variables remain in the parent environment, and a
fresh no-application probe again shows default type 0/version 2.0. Original locks
are free and QOR workers absent. All 215 diagnostic/setup guard samples exceed
3,072 MiB, minimum 6,736 MiB. The failed compiler setup remains in its own folder.
Earlier software failure, cold/warm evidence and image outliers remain sealed.
No new visual capture/equivalence, production fix or test-contract edit is made.

The next bounded action is to resolve these captured Mesa/Qt call sites using
matching debug information, identifying the GL operation and Studio pass that
reuse the dominant buffer. Do not select an optimization from buffer size or
nearest symbol names alone. Deterministic logging coverage stays supplemental;
the original natural-frame gate is retained. The 97 unresolved image outliers
and proposed regression criteria are unchanged. No application Release,
sanitizer, broad product suite or isolated CI acceptance runs anew. PR #10 stays
draft; hosted skips remain skips. Documentation hooks are a separate check, not
resolution of the product failure.

## Matched symbol attribution, 2026-11-16

At `c096c0b`, the offline continuation verifies session-03's 211-file seal,
14 clean Windows worktrees, five complete native checkouts, original locks,
source/binary/image hashes and absence of QOR workers. It resolves the saved
addresses without launching a graphics client, changing running libraries,
installing a package or adjusting system settings. The diagnostic window closes
before its 20-minute limit; exact start/close times are in `timebox.json` and
`halt-requested.json`. Receipts: `build/development-2026-11-16/session-04/`.

### Matching artifacts and resource refusal

The existing Pi package index supplies `mesa-libgallium-dbgsym`
`26.2.6-1~bpo13+0~rpt2`, ARM64, 80,450,020 bytes. The archive is downloaded from
the [Raspberry Pi package repository](https://archive.raspberrypi.com/debian/pool/main/m/mesa/)
into the diagnostic directory and checked against its recorded SHA256
`69e74e01fbee4f8e595d2641fffaead509e1331bf92e2de7226e53b77659f841`.
Only the matching debug file is extracted; no installation occurs.
Its AArch64 ELF build ID is `75d95637686f30eb26d51f6cc9ef87138bc160b2`, equal to
the captured loaded Mesa library. Its GNU debuglink CRC32 `5858e1df` independently
matches the original binary's link. Debug-file SHA256:
`b643d56e5cb847044416f698afaa4359e805a1462afeb18950900db04b0b944b`.
Original library SHA256 remains
`0330e2aec721436c28b67f78240055692b21fa3bcca7f864a87f5e6ef9e9f5ee`.

GNU `addr2line -a -f -C -i` crosses the unchanged 3,072-MiB floor at 2,992 MiB
available. The coordinator stops that task's symbolizer; no attribution is
accepted from its empty output. Its 56 retained successful samples reach a
minimum 3,416 MiB; the failing 2,992-MiB observation is recorded in the refusal,
not silently omitted from the outcome. No unrelated process is stopped.

A separate guarded job reads only ELF headers, `.symtab` and its string table,
checking `STT_FUNC` ranges (`start <= address < start + size`) rather than nearest
export names. It streams file hashing and avoids DWARF expansion. All 17 distinct
captured Mesa addresses resolve unambiguously among 52,852 function symbols;
the resolver step completes in 2.013 seconds. This job's 72 resource samples
stay at or above 7,232 MiB. It later refuses the mismatched Qt candidate, so the
overall job is not labelled passed. No identical large-symbolizer retry.

The official Qt 6.12 ARM64 component metadata advertises a qtbase debug archive
with `X86_64` in its filename. The candidate is inspected rather than assumed
compatible: 290,707,592 bytes, published SHA1
`11d39ba626027c4a73e2cdc69d9ceda44303482b`, local SHA256
`3a4ec2d0e7ddca8590e8701d533a902481ef5e29b2a9aeea7134fb5aa830bb67`.
The extracted QtGui file is actually **x86-64**, build ID
`aa16571707759b48ec6339b89d06e5b7c5f3a7c7`; the installed AArch64 QtGui requires
`fe6d433bf1bd02d07b8d41d33cf67a6d64276b79` and debuglink CRC `3d456ef0`.
It is rejected, never installed or used for attribution. The
[official component index](https://download.qt.io/online/qtsdkrepository/linux_arm64/desktop/qt6_6120/qt6_6120/qt.qt6.6120.debug_info.linux_gcc_arm64/)
and saved metadata document this candidate, not the absence of every possible
commercial ARM64 symbol source. No account credential or installer state is read.

### The captured operation

For event 982, first-Play render TID 31/FD 16 waits on buffer 291, timeout 0, from
monotonic 326587068153364 to 326587080006367 ns: 11.853003 ms wall and
0.005407 ms thread CPU, returning `-1/ETIME`. Its resolved Mesa chain, shown
from caller toward the wait, is:

```text
QRhi::endFrame
  unresolved QtGui frames (0x597ec4, 0x596f64)
  _mesa_DrawElementsBaseVertex             0x41f958
  _mesa_validated_drawrangeelements        0x41e39c
  u_vbuf_draw_vbo                          0x877af4
  v3d_draw_vbo                            0x1307cdc
  v3d_predraw_check_stage_inputs           0x130726c
  v3d_job_submit                          0x12f2304
  v3d_bo_alloc                            0x12ef6d0
  v3d_bo_wait                             0x12ef550
  drmIoctl -> client observer -> WAIT_BO
```

This is an **indexed draw via `glDrawElementsBaseVertex`**, entering pre-draw job
submission and allocation, not an identified swap/presentation call. Original
machine instructions at `0x12f2300` call `v3d_bo_alloc`; the third argument
points to the binary's literal `TSDA` at `0x1df6d40`. The preceding allocation
uses `tile_alloc` at `0x1df6d30`. In `v3d_bo_alloc`, the code at `0x12ef6c0`
and `0x12ef6c8` sets the wait's arguments to zero before the call at `0x12ef6cc`.
The cached-buffer idle check is compiled into this function; it is not a
separate frame in the captured stack. These observations come from the exact
hashed loaded binary, corroborated by matching full function ranges.

Upstream Mesa 26.2.6's [job code](https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-26.2.6/src/gallium/drivers/v3d/v3d_job.c)
associates `TSDA` with `job->tile_state`; its
[buffer manager](https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-26.2.6/src/gallium/drivers/v3d/v3d_bufmgr.c)
checks a cached buffer with a zero-timeout wait and falls back to a fresh
allocation if it remains busy. The source hashes and selected line numbers are
saved. These release sources corroborate the binary; downstream package patches
have not been exhaustively compared, and no source-line DWARF claim is made.

Thus the captured operation **attempts to reuse a cached buffer for tile-state
allocation**. Its `ETIME` result rejects that attempt; it does not prove that
buffer 291 was successfully reused, or that the buffer's previous use was also
tile state. The earlier trace correlates it with submission 946/out_sync 2 and
measures 383.545 ms across 50 calls. Only one of those calls has this captured
stack; assigning all 50 calls the same stack or one unique fence is unsupported.
The Studio draw/pass/material and underlying fence context/sequence remain
unidentified. Qt internal/Quick/thread frames remain unresolved by matching
symbols. Each first-frame stack improves from 3/18 to **11/18** named frames;
later saved stacks resolve 7/14, 11/18, 7/14 and 7/14. Full mappings are retained
in `attribution.json`, including the distinct later buffer-map/subdata and blit
paths. No new timing, profiler overhead or GPU execution measurement is made.

### Decision and validation scope

No QOR production change is justified yet. The evidence shifts the next bounded
investigation to the exact running kernel's **zero-timeout V3D WAIT_BO path**:
explain why a nominal cached-buffer idle probe sleeps before choosing a driver,
environment or application correction. Do not patch around it by weakening the
natural-frame test, replacing required software support or changing system
graphics/security settings. Qt symbols from the tested archive cannot close
the unresolved application-pass mapping.

All 1,414 original sources, both original test binaries and the prior screenshot
remain intact; the captured Mesa/Qt libraries are rehashed unchanged. Original
locks are free and QOR workers absent. No new graphics test, application build,
Release, sanitizer, image regression, broad suite or isolated-CI gate runs.
The prior reference/observer remain 3/1/0 with `times > 5` failing at 3; ptrace's
perturbed value 1 remains separate. Independent llvmpipe failure, 97 image
outliers and the unimplemented deterministic-logging/early-ES proposals remain.
PR #10 stays draft; documentation checks and hosted source checks cannot clear
the failed product/required-isolation gates.

## Exact running-kernel wait audit, 2026-11-16

At `d79c6ab`, the read-only audit verifies session-04's 102-file seal, 14 clean
Windows worktrees, six complete native checkouts, original guards/locks, no QOR
workers and all 1,414 original source hashes plus binaries/image. The bounded
investigation runs from 11:13:49 to 11:22:09 local, before its 11:33:49 cutoff.
No graphics client, package installation, reboot, module reload, kernel tracing
or system/security/service configuration change occurs. Receipts:
`build/development-2026-11-16/session-05/`.

### Exact running identity and compiled conversion

The live release is `6.18.50+rpt-rpi-2712`, package `linux` version
`1:6.18.50-1+rpt1`, built 11 September. Its installed changelog declares Linux
commit `cff533aec2fa601846766b32ff57204e0a61bed7`. Live kernel build ID is
`4d280647c3c1a9fb6fbd20827cd05bfe198d9c9f`. Copied module files, decompressed
only inside the diagnostic directory, match their live sysfs build IDs:

| Module | Loaded and file build ID |
| --- | --- |
| V3D | `d33271d9872dc6095701d0d70728a1c248ac8329` |
| DRM | `f022aa446a2cb31c617ad4db3576620eb27c6639` |

`kernel-inventory.json`, `kernel-detail.json`, collected `result.json` and ELF
identity logs retain source versions, architecture, hashes and configuration.
The actual V3D `v3d_wait_bo_ioctl` machine code loads timeout at `0x894`, calls
`nsecs_to_jiffies64` at `0x898`, unconditionally adds one at `0x89c`, clamps it,
and passes that value in `x3` to `drm_gem_dma_resv_wait` at `0x8c0`.
There is **no zero-timeout bypass**. DRM preserves that argument and calls
`dma_resv_wait_timeout` at `0x20894`; return zero becomes `-ETIME`.
These are original, build-matched instructions, not a rebuilt approximation.

### Why a poll sleeps

The package-pinned [V3D helper](https://github.com/raspberrypi/linux/blob/cff533aec2fa601846766b32ff57204e0a61bed7/drivers/gpu/drm/v3d/v3d_drv.h#L541)
adds one after nanoseconds-to-jiffies conversion. With the verified
`CONFIG_HZ=250`, zero converts to zero and becomes **one jiffy**, nominally 4 ms.
The [ioctl source](https://github.com/raspberrypi/linux/blob/cff533aec2fa601846766b32ff57204e0a61bed7/drivers/gpu/drm/v3d/v3d_bo.c#L276)
matches the binary sequence. A zero original timeout remains zero after elapsed
time accounting, so an expired wait returns `ETIME`, not `EAGAIN`.

The [reservation wait](https://github.com/raspberrypi/linux/blob/cff533aec2fa601846766b32ff57204e0a61bed7/drivers/dma-buf/dma-resv.c#L678)
iterates relevant fences and passes the positive timeout into
`dma_fence_wait_timeout`. Its internal `timeout ? timeout : 1` initializes the
success return convention; it does **not** turn a genuine zero argument into a
positive fence timeout. The defect is the earlier V3D conversion.

In the package-pinned [fence implementation](https://github.com/raspberrypi/linux/blob/cff533aec2fa601846766b32ff57204e0a61bed7/drivers/dma-buf/dma-fence.c#L501),
the default wait returns immediately for zero when the fence is unsignaled.
With the erroneous positive timeout it registers a completion callback, sets
the task interruptible and calls `schedule_timeout`. V3D and scheduler-finished
fence operation tables have no custom wait override in this source. This is the
normal V3D completion path; the trace does not identify the specific fence
context/sequence or rule out every imported fence type.

[`schedule_timeout`](https://github.com/raspberrypi/linux/blob/cff533aec2fa601846766b32ff57204e0a61bed7/kernel/time/sleep_timeout.c)
sets a jiffies-based timer and schedules the task away. The
[timer wheel](https://github.com/raspberrypi/linux/blob/cff533aec2fa601846766b32ff57204e0a61bed7/kernel/time/timer.c#L524)
rounds expiry upward to avoid firing early. Therefore one nominal 4 ms tick is
not a promise of exactly 4 ms elapsed; tick phase, timer processing and runnable
latency matter. The observed call982, 11.853003 ms elapsed and 0.005407 ms thread
CPU followed by ETIME, is consistent with this blocking path. No kernel
scheduler trace was taken: the exact wake-up time and division of that latency
are **not measured**, and GPU duration is not inferred from low client CPU.

This explains why Mesa's intended cache-idle poll can sleep before rejecting
cached buffer291 for TSDA tile-state allocation. It does not establish the
buffer's previous use, successful reuse, a unique fence, the Studio material/pass,
or that all 50 waits share the one captured stack. It also does not prove that
correcting the timeout alone will pass the unchanged natural-frame gate.

### Available correction and limits

The installed but unbooted `6.18.60+rpt-rpi-2712`, package version
`1:6.18.60-1+rpt1`, declares source `0b41724521c9adf5399409d02aece6c1df0a4432`.
A separate guarded static inspection finds **identical V3D wait-function
instructions**, including unconditional addition of one. Booting that installed
kernel is not a demonstrated correction and was not attempted.

Upstream Linux [commit `226666556a9b`](https://github.com/torvalds/linux/commit/226666556a9b3b51f04f9dad83662e5b9c78a325)
explicitly addresses this polling defect by using `drm_timeout_rel_to_jiffies`.
The [helper at that revision](https://github.com/torvalds/linux/blob/226666556a9b3b51f04f9dad83662e5b9c78a325/drivers/gpu/drm/drm_timeout.c)
returns zero before applying conversion/rounding; positive waits remain positive
and bounded. Pinned source URLs, full-file hashes and bounded function excerpts
are saved in `pinned-source.json` and `correction-source.json`. The source package
declares the running revision and module disassembly corroborates the critical
path; generic kernel functions were audited from source, not live stack samples
or matching vmlinux debug-symbol disassembly.

**Recommendation:** prepare a reviewable backport candidate for the supported
Pi kernel using this upstream correction, including helper/header dependencies,
build identity, original configuration and a rollback/qualification plan.
Do not install or boot it as part of this audit. Regression coverage should
exercise a valid busy buffer's zero-timeout ETIME poll, an idle buffer's success,
positive timeout completion/expiry and interruption semantics; record elapsed
time and kernel sleep evidence without redefining the QOR callback requirement.
Only a separately authorized isolated candidate run can establish improvement.
Then repeat the unchanged focused Agent test and matched full-image comparison;
retain the software-renderer failure independently. No QOR workaround selected.

### Outcomes and preservation

The running-module job's four identity/disassembly substeps succeed. Its search
of exported kernel headers has no helper match and exits 1, so the overall job
is recorded incomplete, not passed. The helper lives in the pinned V3D driver
header, resolved separately without repeating the job. Its 41 resource samples
have minimum 7,349 MiB. The newer-module comparison completes with 34 samples,
minimum 7,416 MiB. Both use the original heavy-job lock and 3,072-MiB floor.

No new test, image-equivalence, Release, sanitizer, lint/hardening or broader
acceptance result is claimed. Historical reference/observer remain 3 passed,
1 failed, 0 skipped (`times > 5` value 3); error/coalescing and wall-clock checks
passed, later notice/cursor checks unreached. The independent llvmpipe failure,
97 image outliers, unavailable matching Qt symbols, full products and isolated
CI remain open. PR #10 stays draft; hosted skipped Qt/core-audit gates are not
passes. Original source, binaries, image and session-04 seal are preserved.

## Pi kernel backport preparation, 2026-11-16

At `4937e1a`, `image/pi/kernel-wait-bo/` adds a pinned review candidate rather
than changing the installed driver. Patch 1 changes only the V3D ioctl's timeout
expression: zero bypasses the old conversion, while positive values retain the
original helper, clamping, fence path and elapsed/error handling. This is the
zero-poll semantic subset of upstream `226666556a9b`; it does not import the
shared DRM helper infrastructure or its separate overflow changes. The base is
Raspberry Pi source `cff533aec2fa601846766b32ff57204e0a61bed7` and the running
6.18.50 configuration SHA `8914d802785a04fbb68a00e88e90774db7aa2ec33b9fa1e28f3296be3ebbcff8`.

Patch 2, disabled by default, adds six KUnit cases. A private GEM buffer and
synthetic handle table hold a real, deliberately unsignaled DMA fence. The real
`v3d_wait_bo_ioctl`, GEM lookup, reservation iteration and default fence waiter
run in the test kernel. Sixteen zero polls must each return ETIME, keep the fence
unsignaled and leave the current task's voluntary context-switch counter
unchanged. Elapsed nanoseconds are reported but do not set a new tolerance.
CPU preemption uses the separate involuntary counter. This tests polling
semantics without assuming a GPU frame rate. Other cases cover idle zero,
busy positive expiry, signaled positive success, invalid padding and invalid
handle. No custom fence waiter substitutes for the kernel implementation.

The fixture is not a real allocated V3D hardware buffer, userspace ioctl dispatch,
GPU submission or image test. Kernel execution must still demonstrate that the
unpatched control fails the busy-poll requirement and the corrected variant
passes all six cases. Asynchronous completion while blocked and signal/restart
semantics remain separate integration gates; the signaled-success case alone
does not cover them. The original QOR callback assertion and timer are unchanged.

Native guarded checks apply both patches to hash-matched isolated source files,
verify exact postimages, compile the baseline and patched `v3d_bo.o` with `W=1`
and `-Werror`, and compile the KUnit object with those warning flags and a
test-only `CONFIG_KUNIT=1` compiler definition. The installed configuration is
not modified. This is **object compilation only**, not a linked test kernel.
The candidate instructions at `0x898` branch on nonzero timeout; zero reaches
the reservation wait with `x3=0`. Both patches reverse cleanly and restore exact
preimage hashes; the test source disappears from that disposable copy.

All 14 guarded substeps succeed. The 61 samples have minimum 7,153 MiB against
the original 3,072-MiB floor. The running kernel lacks KUnit; bison/flex and QEMU
are absent. No dependency was installed. Full production/test kernel builds,
negative/positive KUnit execution, kernel KASAN/lock checking, QOR Release/ASan,
focused original test, image comparison and broader acceptance are **unrun**.
There is no measured performance improvement from this preparation.

The boot inventory finds a separate recovery issue: normal `kernel_2712.img`
and `initramfs_2712` match installed 6.18.60 files, while 6.18.50 remains running.
Both older image/initrd files remain under `/boot`, but their presence is not a
verified recovery boot. The [rollback plan](../image/README.md#pi-kernel-wait-bo-backport-review)
requires a known recovery path before any candidate installation or reboot.
No boot/system file, module, original QOR source/binary/image or prior seal is
changed. The independent llvmpipe failure and 97 image outliers remain open.
PR #10 stays draft. Receipts: `build/development-2026-11-16/session-06/`.

The owner-authorized session-08 provisions 218 pinned Debian ARM64 dependency
payloads in a private root. Signed metadata and all archive hashes verify;
offline compiler/linker, bison/flex and paused QEMU startup checks pass.
The host package database, kernel and boot files are preserved. This is a
read-only tool root, not a configured Debian installation or kernel-test pass.
The original guard records 158 samples, minimum 7,214 MiB. The
[builder lock and runbook](../image/README.md#private-builder-dependencies-provisioned)
record the exact scope and reuse checks. All six control/corrected KUnit cases,
full kernel build, kernel sanitizers and physical recovery remain unrun.
Gameplay/software-renderer failures and image/Release/isolated-CI gates remain
open; PR #10 stays draft. Next: verify the complete pinned source/config and
build the isolated test kernel, then run the unchanged suites. Receipts:
`build/development-2026-11-16/session-08/`; session-07 remains sealed.
