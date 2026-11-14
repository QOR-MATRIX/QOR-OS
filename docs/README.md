# QOR OS documentation

The current entry points and references. The records and drafts indexed by the
decision and release indexes below are included; their dates and statuses still
apply. A document not indexed here (or a product's own README) is not current.
[`../AGENTS.md`](../AGENTS.md) says how they are kept true.

| Document | Read it to… |
|----------|-------------|
| [../OWNER.md](../OWNER.md) | see where QOR OS stands, on one page |
| [../README.md](../README.md) | enter the project and distinguish its targets from verified builds |
| [../AGENTS.md](../AGENTS.md), [../CLAUDE.md](../CLAUDE.md) | follow the project instructions; preserved independently of status updates |
| [../CONTRIBUTING.md](../CONTRIBUTING.md), [../CODE_OF_CONDUCT.md](../CODE_OF_CONDUCT.md), [pull-request template](../.github/PULL_REQUEST_TEMPLATE.md) | follow contribution and review practice |
| [../SECURITY.md](../SECURITY.md) | report vulnerabilities and understand known security limits |
| [../CHANGELOG.md](../CHANGELOG.md) | read unreleased changes and dated history, not infer a release or current test pass |
| [../THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md), [../LICENSE](../LICENSE) | read attribution and repository licensing; these do not prove a target-kit entitlement |
| [../VISION.md](../VISION.md) | understand what QOR OS is and where it is going |
| [ROADMAP.md](ROADMAP.md) | see what is done, what is next, and the release gates |
| [release/](release/README.md) | act on each release gate: who must act, the next step, and the drafts for the owner's advisers (Qt, trademarks, terms and privacy, signing, the security review, sign-in) |
| [decisions/](decisions/README.md) | read why decisions were made (QOR, QTOS and Demiurge records); propose new ones |
| [UPSTREAM.md](UPSTREAM.md) | know what QOR OS shares with demiurge-chain (ARQADE's rules and runs, the one look, the QOR core), how it moves, and where the products came from |
| [ARCHITECTURE.md](ARCHITECTURE.md) | understand how the code is organised and how the pieces talk |
| [BUILDING.md](BUILDING.md) | set up the toolchain, build, run, deploy and troubleshoot |
| [../image/README.md](../image/README.md) | Fedora image/lab evidence; Q Pi development preparation, first boot and hardware acceptance still required |
| [DEVELOPMENT.md](DEVELOPMENT.md) | follow conventions, use recipes (add a service, app or control), debug |
| [CI.md](CI.md) | understand paused Qt jobs, isolated-runner prerequisites and the checks CI is configured to require |
| [HOSTING.md](HOSTING.md) | verify the QOR OS repository, existing QOR Vercel project, build settings and canonical domain |
| [../site/README.md](../site/README.md), [../site/AGENTS.md](../site/AGENTS.md) | develop and validate the canonical homepage and preserve its source and privacy boundaries |
| [design/LUMEN.md](design/LUMEN.md) | apply the Lumen design language correctly |
| [design/tokens.md](design/tokens.md) | look up exact Lumen token values (generated) |
| [design/SHELL.md](design/SHELL.md) | know exactly how each shell surface behaves; keyboard reference |
| [SERVICES.md](SERVICES.md) | use or change service contracts; scenarios; `qorctl` |
| [AUDIO.md](AUDIO.md) | use the audio engine and the audio-reactive interface |
| [CONTROLLERS.md](CONTROLLERS.md) | use game controllers and the controller interface |
| [STORE.md](STORE.md) | publish to and verify the signed Store |
| [QT-MODULES.md](QT-MODULES.md) | see every Qt module and tool and its role in QOR OS |
| [PERFORMANCE.md](PERFORMANCE.md) | check budgets, measure frame pacing, keep the shell fast |

Subsystem entry points are also current references: [apps](../apps/README.md),
[Kit](../kit/README.md), [Prism](../prism/README.md),
[platform](../platform/README.md), [services](../services/README.md),
[simulator](../sim/README.md), [developer guide and SDK template](../sdk/README.md),
[tools](../tools/README.md) and [tests](../tests/README.md).
The [developer entry catalogue](../sdk/developer-tools.json) is machine-readable
navigation for the offline discovery command, not another roadmap or runtime API.
Image inputs are described by the image guide, including
[installer attribution](../image/installer/ATTRIBUTION.md) and
[Q Pi preparation metadata](../image/pi/preparation.json), which is planning data,
not a builder configuration or acceptance receipt.

The products document themselves in their own READMEs:
[QQ](../products/qq/README.md), its [samples](../products/qq/samples/README.md),
[ARQADE](../products/arqade/qt/README.md), its [games](../products/arqade/games/README.md)
and [QOR Media](../tools/qor-media/README.md). They came from demiurge-chain
([QOR-0015](decisions/QOR-0015-the-products-are-developed-here.md)), so their
references to `docs/decisions/ADR-*`, `docs/blueprints/`, `DIRECTION` and
`HANDOFF.md` describe upstream context, not additional QOR OS plans. Consult
only material permitted by AGENTS.md; obsolete pre-realignment documents must
not be retrieved to repair a reference. Where a
README and this repository disagree on what is planned, [ROADMAP.md](ROADMAP.md)
wins.

## Dates, handoffs and evidence

`OWNER.md` is the current session handoff and `ROADMAP.md` the only status and
release-gate list. Decision records retain their original decisions; the decision
index identifies proposals and supersession. `VISION.md` describes intended
architecture. The release pack preserves dated research and draft correspondence;
it does not establish current vendor terms, legal clearance or permission to act.

Ignored `build/` records are local, dated evidence, unavailable in a fresh clone.
Preserve original failures, hashes, patches and logs. Their old resume instructions,
permissions, resource snapshots and Git status are historical, even when a file
says "current" or "final". Reconcile them against today's tree and authorisation.
Do not add their test counts together when runs overlap, or transfer acceptance
between fixtures, Windows, WSL, native ARM64, a VM image and physical hardware.

The 10 November reconciliation inventory and link/receipt checks are under
`build/docs-audit-2026-11-10/review-01/`; these are audit evidence, not another
roadmap. Final Q Pi evidence remains in the qualification, bootstrap and headless
directories named by [the image guide](../image/README.md). The accepted private
VM result there remains scoped to its recorded x86-64 image/revision; it is not
a Q Pi image or full device-session acceptance.

`design/tokens.json` is Demiurge's one look, which the products' tests read. Lumen's own tokens are `kit/tokens/lumen.tokens.json`, documented in
`design/tokens.md`.

Screenshots in [`images/`](images/) are produced by
`tools/screenshots/capture.ps1`. Regenerate them when the UI changes.
