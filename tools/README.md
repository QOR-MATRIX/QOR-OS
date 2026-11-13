# tools

This is a navigation guide, not an exhaustive inventory or authorisation to run
installation, repository-administration or device commands. Follow
[AGENTS.md](../AGENTS.md) and the canonical build/image procedures.

| Path | Purpose |
|------|---------|
| `qorctl/` | `qorctl`, a CLI client of the system services ([docs/SERVICES.md#qorctl](../docs/SERVICES.md#qorctl)) |
| `tokens/generate_tokens.py` | Lumen token generator → `kit/Tokens.qml`, `platform/common/lumentokens.h`, `docs/design/tokens.md`. `--check` fails if stale |
| `icons/import_lucide.py` | Imports `kit/icons/icons.txt` from a `lucide-static` package, recoloured for GPU tinting |
| `screenshots/capture.ps1` | Deterministic screenshots of named scenes into `docs/images` (WebP) |
| `dev/Enter-QorShell.ps1` | Loads MSVC, Qt, CMake and Ninja into the current PowerShell |
| `dev/build.ps1` | Configure, build, optionally test or build a target with a preset |
| `dev/run.ps1` | Runs `qor-shell` with arguments |
| `dev/prism-gate.sh`, `dev/prism_benchmark.py` | Existing display diagnostic and its bounded benchmark-report validator; [scope and evidence requirements](../docs/PERFORMANCE.md#how-to-measure), not hardware/release acceptance by themselves |
| `dev/heavy.py` | Native Linux per-account job coordination and existing RAM guard; [scope, qualification and incomplete-run recovery](../image/README.md#native-linux-heavy-job-coordination). Original Windows/WSL coordinator unchanged |
| `ci/check_headers.py` | Verifies SPDX headers |
| `ci/install_qt.py` | Unattended commercial Qt install for CI |
| `github/bootstrap.py` | Applies repository settings, labels, milestones and (optionally) the branch ruleset |
| `image/pi_preflight.py` | Read-only host/Qt/resource qualification probes; [scope and commands](../image/README.md#q-pi-development-edition) |
| `image/pi_elf.py`, `image/pi_payload.py` | Bounded ARM64 ELF, explicit payload manifest and GNU version-label checks; no target execution, symbol binding or full ABI qualification |
| `image/pi_headless.py` | Public host-identity receipts and offline Windows SSH argument plans; no connection or provisioning |
| `image/vm_resources.py` | Existing memory floor and selected scratch-capacity checks; no resource-setting or service changes |
| `image/` | Confined private VM/image tooling; use only the [documented gates](../image/README.md#private-disposable-vm-lab) |
