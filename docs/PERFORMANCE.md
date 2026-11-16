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
