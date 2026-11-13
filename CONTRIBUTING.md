# Contributing to QOR OS

Thank you for helping build QOR OS. This guide covers the workflow. The rules
that bind everyone, people and AI assistants alike, are in
[`AGENTS.md`](AGENTS.md). The technical details live in
[`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) and
[`docs/BUILDING.md`](docs/BUILDING.md).

## Before you start

- Follow the ordered reading list in [`AGENTS.md`](AGENTS.md#1-read-these-in-this-order),
  starting with current [`OWNER.md`](OWNER.md). The documentation index distinguishes
  current guidance from dated handoffs and evidence.
- Architectural changes need a decision record in
  [`docs/decisions/`](docs/decisions/README.md). Copy
  [`TEMPLATE.md`](docs/decisions/TEMPLATE.md), take the next `QOR-NNNN` number,
  index it in the README there, and open it with your change.
- Visual changes to Lumen start from the tokens
  ([`kit/tokens/lumen.tokens.json`](kit/tokens/lumen.tokens.json)) and the spec
  ([`docs/design/LUMEN.md`](docs/design/LUMEN.md)).
- QQ, ARQADE and QOR Media are developed here
  ([QOR-0015](docs/decisions/QOR-0015-the-products-are-developed-here.md)).
  A file of theirs from demiurge-chain that you change is marked
  `Changed for QOR OS:`; a change to ARQADE's rules is made upstream too
  ([`docs/UPSTREAM.md`](docs/UPSTREAM.md#changing-a-games-rules)).
- Changes to the QOR core are made in demiurge-chain, under its rules, and
  reach QOR OS when the pin moves
  ([QOR-0003](docs/decisions/QOR-0003-the-qor-core-at-a-pinned-commit.md)).

## One-time setup

```powershell
git clone https://github.com/QOR-MATRIX/qor-os.git
cd qor-os
python -m venv .venv                                         # repo-local, gitignored
.\.venv\Scripts\python -m pip install -r requirements-dev.txt   # clang-format 20.1.8, pre-commit, cryptography, pinned
. .\.venv\Scripts\Activate.ps1                         # hooks use Python from this environment
.\.venv\Scripts\pre-commit install                     # format, token and header checks on every commit
.\tools\dev\build.ps1 -Preset ci-windows -Test   # separate folder; preserve a running windows-release shell
.\tools\dev\products.ps1                         # build and test QQ, ARQADE and QOR Media
```

Rust (1.91 or later, from [rustup](https://rustup.rs)) is needed for the QOR
core.

Keep the virtual environment active when running hooks or committing; on Linux
activate it with `source .venv/bin/activate`. The system-language hooks use the
`python` on `PATH`, including the pinned YAML parser used by the Actions check.

On the owner's workstation, use the existing QOR heavy-job lock and resource
preflight before jobs; do not relink a running `windows-release` shell.
See [BUILDING.md](docs/BUILDING.md). Setup/install examples are instructions for
an authorised setup, not permission to change this machine or enable paused CI.

## Workflow

1. **Check the starting point.** Read `OWNER.md`, then inspect `git status`,
   the current branch, its upstream and `git log -5 --oneline`. Fetch the remote
   before comparing revisions. Start ordinary changes from `main`; when continuing
   an authorised integration, use the branch named in the current handoff. Preserve
   incoming edits and build evidence. Never reset, force-push or switch a shared
   checkout just to obtain a clean status. Name new branches by type:
   `feat/beam-unit-conversion`, `fix/shelf-hover-flicker`,
   `docs/services-reference`, `perf/backdrop-cache`.
2. **Commit and push when the owner has requested it.** Use plain words to say
   what changed and why. A conventional prefix is optional. Assistant commits
   end with their `Co-Authored-By` attribution, as `AGENTS.md` requires.

   ```text
   Keep Spring velocity when reduced motion retargets an animation
   Refuse Store packages whose signed platforms exclude the device
   ```

3. **Keep changes reviewable.** One concern per pull request; split large work.
4. **Open a pull request** using the template. Visual changes need before and
   after screenshots from `tools/screenshots/capture.ps1`.
5. **Merge ordinary changes** by squash or rebase. Use a merge commit when a
   reviewed upstream import must preserve `git subtree` ancestry
   ([UPSTREAM.md](docs/UPSTREAM.md#pulling-from-upstream)). A pushed checkpoint is
   not a merged change, a passing build or a release. Confirm the remote revision
   after pushing and report any remaining work.

## Definition of done

- [ ] `cmake --build --preset ci-windows` (or the appropriate Linux preset) builds without warnings
- [ ] `ctest --preset ci-windows` passes, `tst_qorcore` included
- [ ] `python tools/ci/check_hardening.py build/ci-windows` passes (use the matching build folder on Linux)
- [ ] `cmake --build --preset ci-windows --target all_qmllint` reports zero warnings
- [ ] `pre-commit run --all-files` is clean (clang-format, generated tokens, SPDX headers)
- [ ] Changes that touch QQ, ARQADE or QOR Media pass `tools/dev/products.ps1`
- [ ] New behaviour has tests: C++ in `tests/`, QML logic in Qt Quick Test, Python tools with `unittest`
- [ ] Changes that parse outside input include hostile-input tests and the applicable `linux-asan` run
- [ ] Shell and Kit changes keep `qor-shell --benchmark 8` p95 at or below the documented baseline
      ([`docs/PERFORMANCE.md`](docs/PERFORMANCE.md))
- [ ] Works in light and dark, with two accent hues, Reduce Motion and Reduce Transparency, and with a controller
- [ ] Documentation, [`CHANGELOG.md`](CHANGELOG.md) and, at the end of a session, [`OWNER.md`](OWNER.md) updated
- [ ] Nothing secret in the change, its logs or its screenshots; money only as integer amounts ([`AGENTS.md`](AGENTS.md))

Record the exact revision (and any uncommitted overlay), commands, environment,
outcomes and skips in the pull request. Mark an unmet check **failed**, **blocked**
or **not run**, with its reason; do not check a box on the strength of an older
receipt. A non-GPU subset is useful evidence, but does not satisfy the full visual
or hardware gate. Qt CI remains paused until runner isolation is proven; see
[CI.md](docs/CI.md#what-ci-trusts) for live-setting observations and limitations.

## Working with an assistant

`AGENTS.md` is the instruction entry point for every assistant; `CLAUDE.md`
redirects there. Follow [the documentation index](docs/README.md), use the
existing frameworks and pinned dependencies, and keep changes within their owning
repository. Do not create a competing roadmap or copy dated receipts as current
acceptance. Prefer small, reproducible commands and links to the canonical guide.

At handoff, update `CHANGELOG.md`, the affected guide and `OWNER.md`; state the
branch and commit, what was tested, what remains open and the next executable step.
Evidence under ignored `build/` is local: include enough commands and outcomes in
the review description for someone using a fresh clone. Never attach credentials,
personal identifiers or restricted third-party engine material.

## GitHub configuration changes

Run `python tools/github/check_actions_policy.py` before changing workflows.
The hook and Lint also run it. It checks reviewed action repositories, full commit
pins, read-only tokens, credential-free checkouts and privileged workflow events.
Its malformed-input tests use the existing pinned Python tooling; no Qt kit or
network access is needed. It does not audit an action's implementation.

Repository settings are separate from files in a branch. Follow
[CI.md](docs/CI.md#repository-settings-and-recovery) to apply and read back a
reviewed change. A ruleset file, CODEOWNERS or a successful setup command alone
does not prove branch protection is active. Never enable a runner's isolation
flag merely to make CI pass.

## Code review

Reviewers look for correctness, adherence to the Lumen spec, render-on-demand
behaviour (no new continuous frames when idle), accessibility (keyboard focus,
`Accessible` roles, controller navigation), and API stability of service
contracts in `services/interfaces/`. Changing a `.rep` file changes a
cross-process contract, and so does a new QOR core bridge command: say so
explicitly in the PR description.

## Licensing of contributions

QOR OS is proprietary (see [`LICENSE`](LICENSE)), except the code from
demiurge-chain, which remains Apache-2.0
([`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)). By contributing, you
confirm that you have the right to submit the work and you assign it to
QOR-MATRIX under the applicable project terms. Files carried from demiurge-chain,
including modifications to them, remain Apache-2.0: retain their notices and
add `Changed for QOR OS:` near the top when modifying one. New QOR OS source files
carry their SPDX header. The carried-file inventory and shared-rule boundaries
are in [UPSTREAM.md](docs/UPSTREAM.md). Do not add third-party code or assets
without recording them in `THIRD_PARTY_NOTICES.md`, and do not add anything
under copyleft licenses that would conflict with proprietary distribution.

## Conduct

Participation is governed by the [Code of Conduct](CODE_OF_CONDUCT.md).
