# Where QOR OS stands

For Andrew, 13 November 2026. Main merged the development checkpoint at
`0c45307`, with current developer guidance, as requested by the owner.

## Main and the developer entry point

The older readiness change on main was reconciled with its tested successor.
The merged application and CI code matches the integration checkpoint; no
build gate or runner-isolation requirement was relaxed. The repository homepage
links current progress, the masterplan, the developer guide and security.
GitHub setup now reads UTF-8 (international text encoding) explicitly: the
Windows regression reproduced a failed milestone match before the fix. Setup
also keeps the homepage linked to the canonical documentation index.

The developer guide identifies source and tools across apps, services, Prism,
QQ, ARQADE, Media, assets and Q Pi. Model Context Protocol (MCP), the interface
for agents to call tools, already connects external clients to QQ Studio over a
local pipe. Scene mutation and logic execution retain their existing trust gates.

QOR Launcher and DRC-369 asset functionality belong upstream in demiurge-chain.
Their current source at `74a1014` was inspected. QOR OS's pinned core bridge
exposes only four version/amount commands; it does not yet expose those asset or
agent-authorisation operations. The frozen upstream root SDK is not a supported
integration route. No keys or signing authority are given to agents.

## Evidence and remaining gates

The preserved implementation evidence includes 22 Windows software suites,
33 exploit-hardened programs and zero interface-code warnings. Hosted Lint at
`3ad8954` passed 691 tooling tests with 124 platform skips. The Pi's 27-suite OS
check and seven additional QQ protocol cases are separate software evidence.
Full products, sanitizers, creator/player journey and physical devices remain open.
Rust's pinned dependency warning still prevents warning-free acceptance.

GitHub requires reviewed action repositories and full commit pins; job tokens
are read-only. Registered Qt workers are offline and no isolation flag is set.
Branch protection remains unavailable on this private repository's current plan.
A main merge is not an image publication, isolated CI proof or release acceptance.

Next, begin offline developer-tool discovery on a new branch, with structured
output for agents and tests that refuse unsafe metadata. The masterplan also
tracks upstream read-only Launcher/DRC-369 bridge work, trusted approvals and
reproducible app/asset onboarding. Native validation and isolation gates continue.
Receipts: `build/development-2026-11-13/session-04/`; earlier evidence is intact.
