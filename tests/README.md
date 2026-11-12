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
| `tst_pointercapture` | `prism/tst_pointercapture.cpp` | Qt Test / raw Wayland client | Hidden software Prism; pointer lock/confinement and hostile clients. Includes unique-manager advertisement and bind/destroy/rebind; 42 passed, zero failed/skipped in each focused Linux Release/ASan run on 12 November. |
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

`pi/CMakeLists.txt` is the small native ARM64 Core/Test entry point for the
existing `tst_prismoptions` and `tst_prismoptions_device` parser suites. It uses
the real Prism source; neither test launches Prism or simulated services.
Release and sanitizer presets retain the native kit gate and hardening. Run
only through the [guarded native sequence](../image/README.md#native-qor-application-build).
This is preparation, not a native pass. The two new `test_pi_build.py` checks
cover preset syntax and wrong-host refusal (including direct `-S`), not compilation;
their latest admission was refused by the unchanged RAM guard.

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
