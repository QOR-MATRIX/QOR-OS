# QOR-0023: Installation uses proven backends

- **Status:** Accepted for local development; no installer or storage mode is certified
- **Date:** 2026-11-05
- **Deciders:** the owner, authorising a branded installation journey and delegating its implementation choice
- **Relates to:** [QOR-0004](QOR-0004-the-base-image-is-fedora-atomic.md),
  [QOR-0008](QOR-0008-before-a-public-release.md),
  [QOR-0018](QOR-0018-the-security-and-quality-bar.md),
  [QOR-0021](QOR-0021-projects-engines-and-two-stores.md)

## Context

At `dad071d`, the repository has application builds and a seven-file startup-branding
payload, but no OS image builder, live session, partitioning backend, bootloader
installer or first-run application. A branded desktop does not demonstrate installation.
Fedora Atomic with bootc remains the base decision, and the Qt Line still applies.

## Decision

1. Use maintained installation backends for partitioning, encryption, deployment and
   bootloader installation. Do not implement replacement disk logic for appearance.
2. Calamares is the native Qt framework candidate. Original QOR branding data may be
   developed against its documented theme contract. Its partition module uses KPMcore;
   neither that module nor a bootc deployment adapter is integrated by this decision.
   Retain upstream attribution and review the chosen versions' licences before packaging.
3. Keep candidate settings inactive until a version-pinned backend and deployment adapter
   are reviewed and exercised together. A final review must identify the physical disk,
   stable identifier, capacity, affected partitions and exact data to be erased; stale or
   changed disk identity must invalidate approval. No destructive choice is preselected.
   Encryption, existing data, cancellation, failures and diagnostics need explicit tests.
4. Package the existing QorMark and generated Lumen colors through the checked image
   filesystem layer. A data-only container is evidence of packaging, never a bootable OS.
   Device assembly requires an explicit locally available image digest, a real device
   payload and the licensed Qt runtime. It must reject simulator builds and active
   unproved installer settings. It must not download a base implicitly.
5. The private `qor-setup` app supplies a small, skippable first-run introduction. Prism
   requests it once after a genuine device-session unlock, excluding simulation and live
   media. It changes no account or preference and records only completion. Unknown or
   damaged records are preserved. This hook cannot substitute for real authentication.
6. Bare-metal readiness requires disposable-disk install, reboot into the installed
   system, first-run, interrupted/failed installs, recovery and rollback evidence on
   declared firmware/storage modes. UEFI is intended; BIOS, Secure Boot, storage and
   hardware support are not established by configuration files or previews.

## Consequences

The repository can develop and test first-run behavior and package branding now. It
cannot offer a safe install button until the storage/deployment integration exists.
Anaconda's bootc support is useful backend research; its stock web/GTK interface is not
a replacement for QOR's Qt interface. Existing owner gates on VM creation, Qt licensing,
runner isolation, physical disks and publication remain in force.

The capability map and exact build/evidence boundaries live in
[image/README.md](../../image/README.md), not in a second roadmap.
