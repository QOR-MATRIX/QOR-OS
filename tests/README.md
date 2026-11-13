# tests

Selected suite entry points follow, not an exhaustive test inventory or pass
report. `tests/CMakeLists.txt` registers the root CTest suites; product CMake
files register their own. Python tooling tests are in `tests/tools/` and run
through `unittest` discovery. See [CI.md](../docs/CI.md) for configured gates
and [OWNER.md](../OWNER.md) for evidence and tests not run.

| Suite | Path | Framework | Notes |
|-------|------|-----------|-------|
| `tst_spring` | `kit/tst_spring.cpp` | Qt Test | Spring physics; deterministic stepping via `Spring::step` |
| `tst_oklch` | `kit/tst_oklch.cpp` | Qt Test | Color math against reference values |
| `tst_services` | `services/tst_services.cpp` | Qt Test | End-to-end Qt Remote Objects, on a private endpoint |
| `tst_shell` | `shell/tst_search.qml` | Qt Quick Test | Beam search and calculator (pure JS) |
| `tst_producthost` | `prism/tst_producthost.cpp` | Qt Test | Prism's side of the QOR host protocol, and products it only starts; starts `tst_producthost_product`, which has no window |
| `tst_pointercapture` | `prism/tst_pointercapture.cpp` | Qt Test / raw Wayland client | Hidden software Prism; pointer lock/confinement and hostile clients. Includes manager advertisement/rebinding and delayed configure acknowledgement. Native ARM64 Release and ASan/UBSan each have 43 passing results after explicit Shelf activation in the acknowledgement fixture. The earlier 42-result Release/ASan passes at `01ba206` remain separate. |
| `tst_windowmanagement` | `prism/tst_windowmanagement.cpp` | Qt Test (Linux with Prism) | Compiles Prism's actual window-management source; null/wrong-object input through direct and invokable calls. Native ARM64 Release and ASan/UBSan each have six passing results. |
| `prism_products` | `prism/products.e2e.json` | `prism --script` (Linux) | Prism hosting stand-ins for ARQADE, QOR Media, QQ Studio and the QQ Player, offscreen and in software, through `tools/dev/prism-products.py` |
| `tst_kit_qml` | `kit/tst_kit.qml` | Qt Quick Test | Kit in a real window; label `gpu` |
| `tst_golden` | `golden/tst_golden.cpp` | Qt Test | Golden images of Kit and Lumen controls (light, dark, high contrast, RTL), perceptual OKLab diff, one reference set for all platforms; label `gpu`. See [DEVELOPMENT.md](../docs/DEVELOPMENT.md#golden-image-tests) |
| `tools_generate_tokens` | `tools/test_generate_tokens.py` | Python unittest | Generator math, output freshness and WCAG contrast guarantees |

```powershell
ctest --preset ci-windows                 # configured/built separate Windows test tree
ctest --preset ci-windows -LE gpu         # excludes GPU-labelled suites; not full acceptance
ctest --preset ci-windows -R spring -V
```

All suites set `QT_FORCE_STDERR_LOGGING=1` so QtTest output is visible under
ctest on Windows.

On 13 November all **22 core-enabled Windows suites selected without the `gpu`
label** pass, with zero failed/skipped suites. The current local tree builds,
33 programs pass hardening and QML lint reports zero warnings. Tests use private
profile/settings folders, offscreen software rendering, silent audio and virtual
controllers. The Rust dependency future-compatibility warning remains an unmet
warning-free gate; these results do not cover full products or GPU suites.
Receipts: `build/development-2026-11-13/session-01/`.

The Pi's separate pinned native core build also passes `tst_qorcore`: nine Qt
Test results including setup/cleanup, zero failures/skips. It checks version,
units, exact amount strings, excess-precision refusal, unknown commands and
malformed requests. The executable passes hardening and ARM64/16-KiB checks;
private folders and disconnected buses keep tests away from live accounts.
The core's Cargo.lock is unchanged; Rust's dependency warning remains. This
focused bridge result is not a full native OS/product or Rust sanitizer run.

The later 13 November native core-enabled build passes all **27 non-GPU root
suites**, without failures or skips, after correcting ARM64 metadata in the
signed demonstration fixtures. The initial 25/27 result and four failing Store
cases are preserved. Hardening passes 42 programs, ARM64/16-KiB layout checks
pass 44 linked files, and QML lint has zero warnings. The fixture generator's
ARM64 regression fails before the correction; all 15 generator tests and three
affected Windows Store/Library suites pass afterward. QQ's complete `tst_scene`
and `tst_playerwall` suites pass 56 and three Qt Test results, including setup/
cleanup, without failures/skips. These are isolated software checks, with the
Rust dependency warning, full product/sanitizer and hardware gates still open.
Receipts: `build/development-2026-11-13/session-02/`.

`tests/tools/test_prism_benchmark.py` runs the actual gate's embedded Python
evaluator with inert metadata and directly tests its shared production validator.
All 26 portable cases pass on Windows; 25 Linux launcher/bundle process tests in
the related suites are platform-skipped in that Windows run. On 13 November all
51 cases pass natively on the Pi, with zero failures/errors/skips, under its
original coordinator. The 25 Linux process cases use inert programs and hardware
metadata; they run no compositor or GPU. Native receipts:
`build/development-2026-11-13/session-01/native-gate-20261113/`.
Six preserved real collector
reports also validate unchanged as software diagnostics. This evidence neither
launches a compositor nor supplies the separate Prism boundary results below.

On 12 November the native Pi built the actual Prism and its test harnesses with
Qt 6.12.0, warnings as errors and hardening. The new acknowledgement case first
failed because opening a different client deliberately cannot steal focus; the
fixture now asserts that policy and activates it through the normal Shelf path.
The corrected case and all seven selected Release suites pass without skips:
both parser suites, product stand-ins, untrusted clients, script privacy,
window management and pointer capture. All 11 built programs pass hardening and
ARM64/16-KiB layout checks; Release QML lint has no warnings. The same seven
native ASan/UBSan suites also pass without skips or sanitizer diagnostics. Actual
instrumentation is verified in Prism and the tests; all 11 sanitizer programs
pass hardening/layout checks. The existing preset disables leak checking.
Failures, correction and results remain in
`build/q-pi-connect-2026-11-12/session-01/`. No production behavior or timeout
changed; the core, full products and real graphics/device gates remain separate.

The separate QQ native preset also builds Studio, Player and its test programs
without warnings; all 16 programs pass hardening and ARM64/16-KiB layout checks.
Only six non-rendering cases were executed: number formatting, player material
bindings, tile-map geometry, and three fake-launcher protocol cases. Both selected
executables report five passes including setup/cleanup, zero failures/skips.
Private HOME/XDG/TMP, virtual controllers and inaccessible audio/D-Bus sockets
keep them away from the desktop. Renderer expectations are not overridden. This
is neither the complete QQ test preset nor physical graphics/product acceptance.
Exact commands and logs: `build/q-pi-connect-2026-11-12/session-01/native-qq-20261112/`.

The earlier Windows/WSL Prism continuation fixed a CMake import-scope failure in the new test
target. Release configuration/generation succeeded and the extracted production
source compiled, but the build did not finish; neither new test has run and
ASan/UBSan has not started. The original lock's host-memory guard stopped the
first admitted compilation at 2,956 MiB and a more tightly constrained incremental
continuation at 3,060 MiB against the unchanged 3,072-MiB floor. At that point neither the new
cases nor the production source extraction had runtime acceptance. The raw client uses normal focus-change
configures, commits different geometry while replies are pending, then redraws
unchanged geometry after acknowledging the latest serial. It checks the actual
configured size and frame, not the unchanged buffer's dimensions. Evidence and
exact continuation: `build/prism-boundary-2026-11-12/session-02/`; the earlier
2,384-MiB pre-build refusal remains in `session-01/`.
The next continuation, `session-03/`, verified the preserved artifacts and resumed
the same incremental Release build after sustained resident admission. The guard
stopped it at 3,008 MiB, 64 MiB below its floor, before another compilation unit
completed. No focused/full regression or sanitizer was run in that continuation.
The later authorised resource review in
`build/prism-resource-2026-11-12/session-01/` preserved both caches, but the actual
worker scope stopped at 2,869 MiB before admission. No compiler or test ran;
the observer-resident check and portable research tests are not Prism runtime proof.

`pi/CMakeLists.txt` is the small native ARM64 Core/Test entry point for the
existing `tst_prismoptions` and `tst_prismoptions_device` parser suites. It uses
the real Prism source; neither test launches Prism or simulated services.
Release and sanitizer presets retain the native kit gate and hardening. Run
only through the [guarded native sequence](../image/README.md#native-qor-application-build).
On 12 November both native configurations built and passed 23 Qt Test results
each, with no failures/skips; all four programs passed hardening. These parser
smokes are separate from the later real-compositor run. The two `test_pi_build.py`
checks cover preset syntax and wrong-host refusal (including direct `-S`), not
compilation. Their older Windows results remain distinct from native execution.

Q Pi's `test_pi_*.py` suites exercise portable metadata, ELF, preflight and
headless receipt/connection-plan behavior. ELF/payload fixtures include hostile
GNU version-label chains and exact per-provider label agreement; they do not
contain real Qt code or prove symbol ABI compatibility. These suites do not execute a Pi image or
establish native ARM64, real Wi-Fi/SSH or hardware acceptance. Follow the
[native qualification sequence](../image/README.md#native-qor-application-build)
for those separate gates; preserve the existing heavy-job lock/resource guards.

`test_heavy.py` separates portable policy/supervision mocks from Linux-only
temporary-file/process fixtures (flock, descriptor inheritance, subreaper,
interruptions and incomplete-run markers). The latter skip on Windows and mock
filesystem qualification even on Linux. Follow the
[coordinator qualification procedure](../image/README.md#native-linux-heavy-job-coordination);
a passing fixture is not native builder, production lock or Pi acceptance.
