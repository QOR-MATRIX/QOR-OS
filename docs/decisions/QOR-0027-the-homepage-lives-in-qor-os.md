# QOR-0027: The homepage lives in QOR OS

- **Status:** Accepted
- **Date:** 2026-11-13
- **Deciders:** the owner, explicitly naming QOR-MATRIX/QOR-OS and Astra Matrix / QOR
- **Relates to:** [QOR-0002](QOR-0002-one-repository-from-qtos-and-demiurge.md),
  [QOR-0026](QOR-0026-qorsync-is-the-public-domain.md); upstream ADR-100 and ADR-131

## Context

The homepage and Analytics were still maintained in demiurge-chain under
QOR-0002 decision 4. The owner directed that QOR OS's GitHub repository and the
existing QOR Vercel project be aligned. Authenticated CLI inspection confirms
project `prj_pUNenZCSC3eh2tyJdHbHCt9rUlW0` in team
`team_gq3IWtz1kK0aO7kzMrrk6N6a` (`astramatrix`) is named `qor`, owns QORSYNC.DEV,
and still links to demiurge-chain with no-op install/build overrides. The connected
app's 404 response did not establish that the project was absent.

## Decision

1. **QOR-MATRIX/QOR-OS owns the homepage in `site/`.** Carry the static site,
   assets, tests and pinned Analytics 2.0.1 from upstream `d4c51d7`. Preserve
   hashes and licences. This supersedes only QOR-0002 decision 4's homepage ownership.
2. **Use the existing Astra Matrix / qor project.** Connect this repository's
   `main`, root `site`, Node 24, locked installation with scripts disabled,
   `npm run build`, and output `dist`. Keep its project ID and domain assignments.
   Do not create another project or publish the repository root.
3. **QORSYNC.DEV remains canonical**, with `www` redirecting to the apex. Primary
   project, contributor, progress and masterplan links name QOR OS. Accurate
   upstream network, licence and signed-release asset links remain intact.
4. **Network/core ownership is unchanged.** The chain, QOR ID, Social, ARQADE web
   application and shared core stay upstream. The core pin does not move.
5. **Verify connection and deployment separately.** `hosting.json` records expected
   non-secret settings. Remote Git repository ID, production branch, build settings,
   deployment commit and live page must agree. Homepage checks do not bypass native Qt gates.

## Consequences

The homepage has a small Node build separate from the device build. Carried source
stays Apache-2.0, fonts retain OFL notices and Analytics retains MIT. No operating
system code becomes public. Upstream homepage ownership and duplicate CI retire
once the replacement exists here; historical source and signed release URLs remain.
