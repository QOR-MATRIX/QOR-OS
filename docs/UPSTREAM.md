# Upstream: what QOR OS shares with demiurge-chain

demiurge-chain (`QOR-MATRIX/demiurge-chain`, public, Apache-2.0) is
Demiurge's repository: the chain, QOR ID, Social, ARQADE's website and the QOR
Launcher. QOR OS shares three things with it:

- the QOR core, built from it at a pinned commit
  ([QOR-0003](decisions/QOR-0003-the-qor-core-at-a-pinned-commit.md));
- ARQADE's rules and recorded runs;
- Demiurge's one look.

QQ, ARQADE's app and games, and QOR Media came from it with their history
([QOR-0002](decisions/QOR-0002-one-repository-from-qtos-and-demiurge.md)).
They are developed here since 29 October 2026
([QOR-0015](decisions/QOR-0015-the-products-are-developed-here.md)), and
demiurge-chain's copies of them are frozen (its ADR-126).

## What is shared

| Path (the same here and upstream) | What it is | How | Upstream record |
|-----------------------------------|------------|-----|-----------------|
| `products/arqade/tests/fixtures/` | Recorded runs that ARQADE's games replay here and its rules replay upstream | carried by `git subtree`, with history | ADR-102 |
| ARQADE's rules: `logic/rules.mjs` of Orbital, Void Runner, Synapse and Rift Survivor, and the arenas' `logic/arena.mjs`, in `products/arqade/games/` | The rules and nothing else, which ARQADE's Node tests upstream import and its service is to confirm runs by | kept identical, changed in both at once ([below](#changing-a-games-rules)) | ADR-102, ADR-116 (proposed) |
| QOR OS's own games ([QOR-0017](decisions/QOR-0017-arqades-new-games.md)): Ever Rift's and Darkveil's `logic/rules.mjs`, their Node tests in `products/arqade/tests/` (and Darkveil's pilot) and their fixtures | Written here, under Apache-2.0; its Node test runs in QOR OS's lint workflow too | taken there by one pull request once the owner names the game ([QOR-0019](decisions/QOR-0019-ever-rift-and-darkveil.md)), then kept identical like the rules above. These two went there in [#117](https://github.com/QOR-MATRIX/demiurge-chain/pull/117), merged on 1 November 2026 (`74a1014`). Bellwick's stay here until the owner names it, so `demiurge.sh status` lists its rules as differing until then | ADR-129 |
| `docs/design/tokens.json` | Demiurge's one look: the products' own colours outside QOR OS, and what their tests read. In QOR OS they wear Lumen's roles under its names ([QOR-0016](decisions/QOR-0016-the-products-wear-lumen.md)) | copied at the same commit | ADR-108 |
| The QOR core (`tools/qor-launcher/core/` upstream) | Keys, profiles, amounts, the C bridge | built at a pinned commit, never copied | ADR-103, ADR-104 |

Everything Demiurge runs as a service stays upstream: the chain, QOR ID,
Social, ARQADE's website and the QOR Launcher for Windows and
Android.

The homepage moved to this repository's `site/` on 13 November 2026 under
[QOR-0027](decisions/QOR-0027-the-homepage-lives-in-qor-os.md), carried from
`d4c51d7e99848a3789cbe46f5f1c3de1ed0aca4e`. Original hashes are in
`site/upstream-source.json`; source, SDK and font licences are preserved. It is
developed here, without an ongoing subtree sync. Vercel's existing Astra Matrix /
qor project targets QOR-MATRIX/QOR-OS `main`, root `site`.

## The products, developed here

| Path | What it is | Upstream record |
|------|------------|-----------------|
| `products/qq/` | QQ, the QOR Engine: the runtime, QQ Studio, the QQ Player, the agent's tools, tests | ADR-081, ADR-083, ADR-105 |
| `products/arqade/qt/` | ARQADE's Qt app | ADR-102 |
| `products/arqade/games/` | ARQADE's games and arenas, rebuilt in QQ | ADR-102 |
| `tools/qor-media/` | QOR Media: player, viewer, editor, PDF reader | ADR-094 |

They came by `git subtree`, with their history, at the paths they have
upstream. They keep those paths, so that ARQADE's build still finds QQ's
runtime at `../../qq/runtime`, its games at `../games` and its fixtures at
`../tests/fixtures`, and each product's tests still find
`docs/design/tokens.json`.

Since QOR-0015:

- **They are not pulled.** Only the shared ARQADE rules, tests/pilots and recorded
  runs listed above move back upstream; product application code stays here.
- **Licence.** Their files from demiurge-chain stay Apache-2.0; they are
  listed in [`LICENSES/from-demiurge-chain.txt`](../LICENSES/from-demiurge-chain.txt),
  and one changed here says so near its top. Other work on them is QOR OS's,
  under [`LICENSE`](../LICENSE).
- **The Windows launcher** ships demiurge-chain's frozen copies, the same code
  as its 0.1.17. Only the owner releases a fix into them, by a pull request
  there.
- **Their open items** from Demiurge's roadmap are in [ROADMAP.md](ROADMAP.md).

## State

The carry dates and upstream merge results below are historical checkpoints,
not a fresh upstream audit. The core pin is checked against the current
[`core/CMakeLists.txt`](../core/CMakeLists.txt). Remote equality and open upstream
work need their own current check; the dated checkpoints do not establish either.

| Path | Upstream commit it was last split at |
|------|---------------------------------------|
| `products/arqade/tests/fixtures/` | demiurge-chain `28d9e97` (`main`, 26 October 2026). Unchanged upstream as of `8b4344e` (29 October, the merge of #113), so no pull was needed |
| The four products | `28d9e97`, identical in their paths to `db98ac0`, where demiurge-chain froze its copies. Developed here since |

| QOR core pin | Features | What that core has |
|--------------|----------|---------------------|
| `6a4822d10c93694d6d12abf33a75b4df6cac918a` (28 October 2026, the merge of #109) | `desktop` | The same code as `e99c7ce`. Its manifest declares Apache-2.0 instead of MIT (demiurge-chain's ADR-122) |
| `e99c7ce62045da08701a2ad6f7769a1a1d87afe6` (27 October 2026, the merge of #107) | `desktop` | Everything below, plus the modules L13.2 moved in from the Windows launcher: QOR ID's and Social's clients, the chain client, the Library's index, the Mesh and Qontrol. The bridge's commands are unchanged |
| `28d9e970fef3bcedb0d0cfbdcf71c74949fed5ec` (26 October 2026, the first pin) | none | Sparks and errors, the vault, profiles and their confirmations; bridge commands `core_version`, `token_info`, `parse_amount`, `format_sparks` |

`tools/upstream/demiurge.sh status` prints local subtree metadata and the current
core pin. Its rules comparison uses the local upstream clone's **last fetched**
`origin/main`; it neither fetches nor proves current remote equality. Update the
recorded state when an authorised upstream change actually lands.

On 13 November, Rust 1.98.1 on Windows and native ARM64 reports a future-compatibility
warning in `proc-macro-error2` 2.0.1, reached through `subxt-macro` 0.51.0. The
diagnostic concerns a private `proc_macro` re-export. A fresh upstream check at
`74a1014464f85e610f929ebadf8c199c82673db7` still finds that locked dependency.
The crate's latest published version remains 2.0.1 and its
[source repository is archived](https://github.com/GnomedDev/proc-macro-error-2).
A routine pin advance therefore does not clear the warning. The pin stays unchanged
until a reviewed upstream repair passes the core and QOR OS checks; no warning
suppression or dependency substitution was adopted. Receipts:
`build/development-2026-11-13/session-02/dependency-review.json`.

## Remediating core dependencies

This is the procedure for the open security task in [ROADMAP.md](ROADMAP.md),
not a second roadmap. The 13 November audit of pin `6a4822d` found four
vulnerability advisories and 13 warnings. The same affected versions remain in
upstream `74a1014`; advancing to that commit is not a fix. The repair in
[upstream PR #118](https://github.com/QOR-MATRIX/demiurge-chain/pull/118),
`042941c`, now patches the core and host locks. Their local audits report zero
vulnerability advisories, retaining 13/23 warnings. Hosted compilation/lints pass;
245 core tests pass with 10 ignored and 109 host tests pass with five ignored.
The QOR OS pin is unchanged. The core bridge must not grow around
an unresolved audit or unvalidated dependency selection.

The selected core update is rusqlite 0.41.1 with bundled libsqlite3-sys 0.39.1,
quinn-proto 0.11.19 and quinn-udp 0.5.16. QOR ID's QUIC lock is patched too.
Both services still fail on SQLx's optional SQLite lock dependency: stable
SQLx 0.9.0 rejects the fixed libsqlite3-sys version, so that trial was removed.
An isolated resolution with default SQLx features disabled and only PostgreSQL,
macros and migration support still retains the affected SQLite lock dependency;
feature trimming alone does not clear this gate.
The chain's compatible QUIC, libz-sys 1.1.31 and Wasmtime 36.0.20 patches first
reduce its report from 53 to 44 vulnerability findings, with 36 warnings. The
follow-up at `3d71009` selects rustls-webpki 0.103.17, webpki-root-certs 1.0.12,
zstd 0.13.5, zstd-safe 7.4.1 and zstd-sys 2.1.3+zstd.1.5.7: its audit reports
38 findings and 26 warnings. All 213 hosted chain tests pass with WebAssembly
built on that revision. The attempted tracing-subscriber update is refused
because sc-tracing 47.0.0 requires exactly 0.3.19. Its SDK pin
and runtime source are unchanged. No new exception was added. Service/chain
findings and core maintenance warnings remain upstream work, not a reason to
silently update this repository's pin.

The workflow follow-up at `97dcb13` pins all 32 external action references to
nine upstream commits across both workflows, disables persisted checkout
credentials and selects explicit audit/coverage tool versions with checksums
and no fallback installer. Weekly Actions updates are proposed for review.
A structured YAML comparison preserves the job commands, gates and triggers.
Hosted run `43519214320` passes all 213 chain tests with WebAssembly built,
245 core tests (10 ignored), 109 Launcher host tests (five ignored), frontend,
design/accessibility, service, ARQADE and coverage checks. Security still fails
the two service audits. The manual publishing workflow was not dispatched.
These source changes are in draft PR #118, not upstream main.
The live repository allowlist is restricted to the nine used action repositories;
read-only default tokens and no pull-request approval remain in force. Global
SHA enforcement waits for the source pins to merge so current main keeps running.
The separate CI dispatch `43522526902` is building the release node for the
existing two-validator agreement/finality/restart check. Its optional Rust
1.100.0 report fails on 66 `clone_on_copy` diagnostics at DRC-369 event/call
parameters expanded by SDK macros, the issue tracked by upstream ADR-072.
Required Rust 1.98.1 checks pass; no lint suppression or compiler/SDK pin change
was made. The report skips WebAssembly and is not runtime test evidence.

The official RustSec records identify these minimum patched versions; they are
review inputs, not a tested dependency selection:

- `libsqlite3-sys 0.38.2`: [RUSTSEC-2026-0459](https://github.com/rustsec/advisory-db/blob/main/crates/libsqlite3-sys/RUSTSEC-2026-0459.md)
  lists `>=0.39.0`. The core's `desktop` feature includes `library`, which uses
  `rusqlite 0.40.2` with `bundled`. Review the matching rusqlite requirement,
  database compatibility and actual SQLite features together. The advisory's
  SQLCipher qualification must not be lost by quoting the version alone.
- `quinn-proto 0.11.18`: [RUSTSEC-2026-0523](https://github.com/rustsec/advisory-db/blob/main/crates/quinn-proto/RUSTSEC-2026-0523.md)
  and [RUSTSEC-2026-0524](https://github.com/rustsec/advisory-db/blob/main/crates/quinn-proto/RUSTSEC-2026-0524.md)
  list `>=0.11.19`. Trace the selected transport features and all parent crates
  before choosing an update; the presence of a package in a lock file alone is
  not runtime reachability.
- `quinn-udp 0.5.15`: [RUSTSEC-2026-0538](https://github.com/rustsec/advisory-db/blob/main/crates/quinn-udp/RUSTSEC-2026-0538.md)
  lists `>=0.5.16,<0.6.0` or `>=0.6.3`; its affected receive path excludes Linux
  and Windows. Record that distinction while retaining the audit gate.

Work in a clean upstream `session/*` branch or isolated worktree, under its
`AGENTS.md`. Leave QOR OS's dependency checkout and pin alone. Before compiling,
use the real heavy-job coordinator and sustained headroom checks with the actual
worker resident. An insufficient-memory refusal is a stop, not permission to
lower the floor, bypass coordination or manage another project's services.

Capture the current manifests, lock-file hashes, selected features and target
platforms without credentials. Review the narrowest compatible changes, including
the desktop host's own lock file when its path dependency resolves differently.
Preserve approved pins and notices; do not add advisory ignores, loosen the
workflow or suppress the separate `proc-macro-error2` warning.

Run upstream's locked core/host tests and configured audits on the changed
revision. Test Library database open/read/write and transport refusal behavior
affected by the selected updates, using local fixtures and test doubles. Record
commands, environment, failures and skips; a successful dependency resolution or
format check is not a compiled/tested core. Other upstream audit failures remain
visible even if these four are repaired.

Only after the upstream repair is reviewed and integrated should QOR OS update
`core/CMakeLists.txt` and the pinned source references in
[`sdk/developer-tools.json`](../sdk/developer-tools.json) together. Revalidate the
discovery catalogue and required native/core checks for the new pin, update the
existing status/security documents and preserve the exact evidence. Do not copy
the core here or describe the full ecosystem as secure from one passing audit.

## Pulling from upstream

For an owner-authorised upstream update, from a clean working tree in Git Bash
or WSL. `pull` fetches upstream and creates merge/copy commits; it is not a
read-only check. Preserve existing work rather than stashing or discarding it
merely to make this command run.

```bash
tools/upstream/demiurge.sh pull            # the fixtures and the look, from demiurge-chain main
tools/upstream/demiurge.sh pull <commit>   # or a specific commit
pwsh tools/dev/products.ps1                # build and test the products against them
```

The script keeps a clone of demiurge-chain in `../_upstream/demiurge-chain`
(set `QOR_UPSTREAM_DIR` to change it). It splits the fixtures' upstream
history there (`git subtree split`) and merges it here (`git subtree pull`),
in a merge commit. It then copies `docs/design/tokens.json` at the same
commit, commits that file if changed, and says if ARQADE's rules differ from upstream's. Splitting is
deterministic, so every pull adds only what is new.

## Changing the products

A change to a product is made here, like any other in QOR OS. A file from
demiurge-chain that is changed gets a line near its top saying so, as
Apache-2.0 §4(b) requires:

```cpp
// Changed for QOR OS: <what changed and why>.
```

A file added to a product carries QOR OS's SPDX header
(`tools/ci/check_headers.py` checks it). The products keep their own build
scripts and formatting; don't reformat them.

## Changing a game's rules

ARQADE's rules live once, in each game's `logic/rules.mjs` (the arenas'
`logic/arena.mjs`), and are used in two repositories: by the game here, and
by ARQADE's Node tests and service upstream. Every change to a game's rules
fails its test until its fixture and the digest of the rules' state are made
again ([games/README.md](../products/arqade/games/README.md)). So a change to
the rules is made in both repositories at once:

1. **Here:** change the rules and the game, record the game's run again
   with ARQADE's games suite, and commit. For QOR OS's own games, update the
   digest their Node test pins here too (`node --test
   products/arqade/tests/<game>.test.mjs`).
2. **Upstream:** write the change as patches, which apply there because the
   paths are the same:

   ```bash
   tools/upstream/demiurge.sh patch <commit>   # the rules, fixtures, look and shared tests it changed
   # then, on a branch of demiurge-chain:
   git am /path/to/qor-os/build/upstream-patches/*.patch
   ```

   Update the digests its Node tests pin (`products/arqade/tests/`), and open
   a pull request there for the owner. Published there, the rules stay
   Apache-2.0, and reach the Windows launcher's frozen games at its next
   release, so the change has to suit those too.
3. **Once it is merged,** `tools/upstream/demiurge.sh status` shows the rules
   the same as upstream's again, and a pull takes the fixture back unchanged.

## Moving the core's pin

1. Pick a commit on demiurge-chain's `main` (never a branch commit).
2. Build and test against it from a dedicated checkout at that commit. Do not
   switch another worker's upstream checkout. On the owner's Windows machine,
   protect any running `windows-release` shell and use `ci-windows` under the
   existing heavy-job coordination:

   ```powershell
   . .\tools\dev\Enter-QorShell.ps1
   cmake --preset ci-windows -DQOR_CORE_SOURCE_DIR="X:/path/to/reviewed-checkout"
   cmake --build --preset ci-windows
   ctest --preset ci-windows -R tst_qorcore
   ```

3. Change `QOR_CORE_COMMIT` (and `QOR_CORE_FEATURES`, if the crate's features
   changed) in [`core/CMakeLists.txt`](../core/CMakeLists.txt) and the table
   above in one commit, named "Pin the QOR core at demiurge-chain
   `<short hash>`", saying what changed in the core. Both are written on every
   configure, so every build folder, CI's included, follows the move.
4. Clear the local source override when validating the final pin:
   `cmake --preset ci-windows -DQOR_CORE_SOURCE_DIR=`. Otherwise that cache still
   selects the checkout instead of testing the pinned fetch. Run the required
   build, test, hardening and lint gates for the final revision. CI acceptance,
   including `tst_qorcore`, remains required; while Qt jobs are paused, record
   local evidence and the unmet CI gate explicitly ([CI.md](CI.md#what-ci-trusts)).

### Moves so far

- **28 October 2026, to `6a4822d`:** the merge of
  [demiurge-chain #109](https://github.com/QOR-MATRIX/demiurge-chain/pull/109), which the owner merged
  that day. The core's code did not change: its manifest declares Apache-2.0
  instead of MIT (demiurge-chain's ADR-122, accepted by the owner on 27
  October), and two comments, in the manifest and in `.cargo/audit.toml`, now
  record ADR-121 as accepted. At the merge the core built into QOR OS from the
  `ci-windows` preset and `tst_qorcore` passed 9 of 9.
  [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) changed in the same
  commit.
- **27 October 2026, to `e99c7ce`:** the merge of
  [demiurge-chain #107](https://github.com/QOR-MATRIX/demiurge-chain/pull/107)
  (L13.2), with the `desktop` feature. That change moved QOR ID's client, the
  chain client, Social's client, the Library's index, the Mesh and Qontrol from
  the Windows launcher into the core, each behind a Cargo feature, and
  `desktop` turns on all of them, as the Windows launcher and QOR OS build it
  (demiurge-chain's ADR-120). It was checked ahead at the PR's head (`48e9bfa`)
  and again at the merge: the whole core compiles and links into QOR OS with
  warnings as errors, and `tst_qorcore` passes 9 of 9. The move also stopped
  the pin and the features being cache options, so a build folder follows the
  pin in the file.
- **26 October 2026, `28d9e97`:** the first pin, with no features.

### Open upstream

At the 29 October cut-over, no item remained for that transition. This is not a
current inventory of upstream pull requests. ADR-126, demiurge-chain's side of
the cut-over (below), is on its
`main`: the owner merged
[demiurge-chain #113](https://github.com/QOR-MATRIX/demiurge-chain/pull/113)
on 29 October 2026, as `8b4344e`. In the products' paths it added only a note
at the top of each one's README there, and it changed nothing that is shared.

The records QOR OS waited on for the core are accepted and on demiurge-chain's
`main` too. ADR-121 (#107, accepted by #108) has the core's audit ignore
RUSTSEC-2026-0344 in `ruzstd`, which only subxt's light client reaches and
nothing builds. ADR-122 (#109) has the six active crates, the core among
them, declare Apache-2.0, the repository's licence, instead of MIT.

## The cut-over, 29 October 2026

QOR-0002 kept demiurge-chain the products' upstream until a record moved
their development here. On 29 October 2026 the owner asked to "move forward
with their development transition to this repo entirely", and chose:

- the products move, and the core and both launchers stay upstream;
- new work on QQ is closed until the public SDK's record decides whether its
  source is published;
- the Windows launcher ships frozen copies.

[QOR-0015](decisions/QOR-0015-the-products-are-developed-here.md) records it
here and ADR-126 upstream. Nothing had changed upstream in the products'
paths since the carry, so nothing was pulled first. From then on, the
products' paths are QOR OS's own, except ARQADE's rules, which stay shared.
