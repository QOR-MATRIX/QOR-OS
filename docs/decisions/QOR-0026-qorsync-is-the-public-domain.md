# QOR-0026: QORSYNC.DEV is the public domain

- **Status:** Accepted
- **Date:** 2026-11-13
- **Decider:** the owner; service subdomains explicitly retained
- **Relates to:** [QOR-0001](QOR-0001-one-rulebook-and-one-decision-log.md),
  [QOR-0003](QOR-0003-the-qor-core-at-a-pinned-commit.md),
  [QOR-0018](QOR-0018-the-security-and-quality-bar.md)

## Decision

1. **QORSYNC.DEV is the only first-party public domain.** Use
   `https://qorsync.dev` for the homepage and approved service subdomains beneath
   it. The owner confirmed that separate service hosts remain appropriate.
   Retired first-party hosts must not appear in product copy, links, defaults,
   examples or the current tracked documentation.
2. **The project repository is [QOR-MATRIX/QOR-OS](https://github.com/QOR-MATRIX/QOR-OS).**
   Its homepage field points to QORSYNC.DEV. Its README separately links the
   masterplan, current progress, developer guide and contribution/security paths.
   Exact upstream source, licence and third-party documentation links remain
   accurate; this does not move code between repositories.
3. **Do not fabricate services or redirect unsupported protocols.** A canonical
   URL is not evidence of deployment, API compatibility or an operational email
   sender. Frozen clients remain frozen. Source maintenance, released clients,
   live DNS, hosting and mail configuration require separate evidence.
4. **Preserve identity and provenance.** Reverse-DNS application/keychain
   identifiers and cryptographic domain separators are not website addresses.
   This decision does not migrate credentials, alter protocol signatures,
   rewrite git history or change copyright attribution. Historical prose may
   redact a retired host with an explicit note; it must not imply that an old
   deployment originally used the new name.
5. **Check new contributions.** `tools/ci/check_domains.py` scans supported tracked
   text formats, including encoded links, in commit hooks and hosted Lint. It
   reports file/line locations without echoing addresses or file contents. It
   refuses linked or malformed inputs and does not read restricted secret files.

## Consequences

QOR OS and its tooling can enforce their own source tree immediately. Launcher,
core and network changes belong in demiurge-chain under its rulebook. A tested
upstream change precedes any core-pin movement. Production verification and
replacement of already-installed clients remain distinct from merging source.
