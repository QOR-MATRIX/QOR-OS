# QOR-0028: Plan one canonical development repository

- **Status:** Accepted for planning only; no migration authorized
- **Date:** 2026-11-14
- **Deciders:** the owner
- **Relates to:** [QOR-0002](QOR-0002-one-repository-from-qtos-and-demiurge.md),
  [QOR-0003](QOR-0003-the-qor-core-at-a-pinned-commit.md),
  [QOR-0015](QOR-0015-the-products-are-developed-here.md),
  [QOR-0017](QOR-0017-arqades-new-games.md),
  [QOR-0025](QOR-0025-q-pi-development-edition.md),
  [QOR-0027](QOR-0027-the-homepage-lives-in-qor-os.md),
  [QTOS-0004](qtos/QTOS-0004-commercial-qt-and-proprietary-repository.md)

## Context

The owner wants one canonical development repository for QOR OS, Q Pi,
Demiurge Chain, ARQADE and associated systems. The immediate work remains
the current QOR OS/Q Pi milestone and reviewable pull request #10. Repository
consolidation must not displace that work or silently change its boundaries.

The current tree still implements the split: `core/CMakeLists.txt` fetches
the core at a pinned upstream commit; `tools/upstream/demiurge.sh` maintains
the shared-code relationship; the device products live under `products/`;
and the homepage has its own static build under `site/`. The following
conflicts require a later decision, rather than a code move in this milestone:

| Existing decision or contract | What consolidation must resolve |
| --- | --- |
| QOR-0002 and QTOS-0004: private QOR OS, public network upstream, commercial Qt, proprietary and carried Apache-2.0 files | Canonical repository identity, access and public-source distribution without changing licences or disclosing private history |
| QOR-0003: core changes upstream, fetched at a pin and reached through its bridge | Source ownership and dependency pinning while preserving the key-handling and separate-service boundaries |
| QOR-0015 and QOR-0017: products developed here, launcher copies frozen upstream, shared ARQADE rules and fixtures kept aligned | Existing launcher consumers, relative paths, rule equivalence, release inputs and attribution |
| QOR-0027: `site/` here, existing Astra Matrix / qor deployment from QOR OS `main` | Deployment continuity, project connection, build root and domain ownership |
| QOR-0025 and QOR-0018: Q Pi acceptance and isolated build requirements | Independent device, image, hardware and CI qualification; a common repository proves none of these |

These are conflicts with the desired future source arrangement, not findings
that the present decisions have ceased to apply. Current upstream references,
consumers and deployments need a fresh inventory before a migration proposal;
the records alone are not a live audit of every associated system.

## Decision

1. Record **one canonical development repository** as the owner's long-term
   direction for the named systems. The destination, visibility, directory
   layout and publication mechanism are undecided.
2. Finish a reviewable account of the current milestone first: focused
   changes, reproducible validation and explicit failed, blocked and untested
   gates. Reviewability does not mean release, hardware or CI acceptance.
3. Then propose a separate consolidation plan based on verified repository,
   history, licence, security, consumer and deployment evidence. It must
   preserve active work, original history and notices, identify cutover and
   rollback checks, and name the decisions requiring supersession.
4. **No code moves, repository restructuring, history rewriting, visibility
   changes, merges or deployment cutovers are authorized by this record.**
   The current source ownership, pin, shared-rule protocol, service boundaries
   and publication approvals remain in force. No earlier record is superseded.
5. Keep the work sequence in [ROADMAP.md](../ROADMAP.md); do not create a second
   roadmap. Do not recover, read or restore Demiurge's prohibited pre-realignment
   documentation while preserving permitted history.

## Consequences

The owner can review the source and operational tradeoffs before selecting a
migration. A shared repository need not imply shared runtime privileges,
credentials, CI workers, licensing or release schedules. Qt runner isolation
must still be proved before enabling its jobs. The current milestone changes
neither repository visibility nor upstream core ownership.
