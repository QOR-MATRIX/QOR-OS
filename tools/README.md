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
| `dev/developer.py` | Offline `list`, `show` and `check` for reviewed developer entry points; versioned JSON for agents, with no execution or network access ([usage and boundaries](../sdk/README.md#offline-discovery)) |
| `dev/prism-gate.sh`, `dev/prism_benchmark.py` | Existing display diagnostic and its bounded benchmark-report validator; [scope and evidence requirements](../docs/PERFORMANCE.md#how-to-measure), not hardware/release acceptance by themselves |
| `dev/heavy.py` | Native Linux per-account job coordination and existing RAM guard; [scope, qualification and incomplete-run recovery](../image/README.md#native-linux-heavy-job-coordination). Original Windows/WSL coordinator unchanged |
| `ci/check_headers.py` | Verifies SPDX headers |
| `ci/check_domains.py` | Refuses retired first-party hosts in supported tracked text formats; reports locations without contents ([QOR-0026](../docs/decisions/QOR-0026-qorsync-is-the-public-domain.md)) |
| `ci/install_qt.py` | Unattended commercial Qt install for CI |
| `github/bootstrap.py` | Applies repository settings, labels and milestones; optional ruleset failures exit nonzero. `--actions-only` applies and reads back only the reviewed Actions policy ([CI.md](../docs/CI.md#repository-settings-and-recovery)) |
| `github/check_actions_policy.py` | Offline workflow/policy check for reviewed action pins, token permissions, checkout credentials and privileged events |
| `image/pi_preflight.py` | Read-only host/Qt/resource qualification probes; [scope and commands](../image/README.md#q-pi-development-edition) |
| `image/pi_elf.py`, `image/pi_payload.py` | Bounded ARM64 ELF, explicit payload manifest and GNU version-label checks; no target execution, symbol binding or full ABI qualification |
| `image/pi_headless.py` | Public host-identity receipts and offline Windows SSH argument plans; no connection or provisioning |
| `image/vm_resources.py` | Existing memory floor and selected scratch-capacity checks; no resource-setting or service changes |
| `image/` | Confined private VM/image tooling; use only the [documented gates](../image/README.md#private-disposable-vm-lab) |
