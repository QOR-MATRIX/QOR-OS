# QOR-0024: A confined disposable VM installation lab

- **Status:** Accepted for local development; installation is not yet proved
- **Date:** 2026-11-06
- **Deciders:** the owner, authorising a bootable image and complete disposable-VM installation
- **Relates to:** [QOR-0004](QOR-0004-the-base-image-is-fedora-atomic.md),
  [QOR-0008](QOR-0008-before-a-public-release.md),
  [QOR-0018](QOR-0018-the-security-and-quality-bar.md),
  [QOR-0023](QOR-0023-installation-uses-proven-backends.md)

## Context

The inspected starting revision `9d6c7c9` contains tested first-run software and
installation branding data, but no bootable artifact or exercised installation
backend. The owner now explicitly authorises a fresh disposable VM, install,
media-free boot, first-run completion and a second reboot proving persistence.
This does not authorise changes to host disks, EFI, firmware, security, networking
or runners. Hyper-V access is denied in the current ordinary terminal. Existing
Docker can run QEMU without KVM or changes to host configuration.

## Decision

1. Keep Fedora bootc as the device base. Pin its architecture manifest by digest.
   Package a private VM development profile separately from the full device
   recipe. Its limited Qt interface is not Prism, authentication or product
   readiness. A native Qt frontend may call stock bootc for partitioning,
   deployment and bootloader installation; it implements no replacement disk
   logic. This develops the backend requirement of QOR-0023 without activating
   its unresolved Calamares candidate configuration.
2. Build on existing licensed host tools, package only the required runtime for
   private testing, and retain notices. No Qt kit, account file or credentials
   enter the guest. Public distribution and isolated CI licensing remain separate
   prerequisites; this record accepts no new terms or third-party licence.
3. Use headless QEMU TCG in an existing restricted container: no network, host
   devices, KVM, host EFI/NVRAM, Docker socket or host directory passthrough.
   Permit only the individual test disk, its private firmware variables,
   immutable allowlist, ISO, entrypoint and evidence directory. Bound resource
   use and run one heavy QOR job at a time under the actual project-local lock.
4. Make the raw target a new regular file with no backing file. Record its file
   identity, capacity, parent ancestry, VM UUID and unique serial. Pin Windows
   ancestry/artifacts for the entire VM lifetime and pass open regular-file
   descriptors to QEMU. Private firmware variables come from a known image
   template, never the host. The guest must match UUID, serial, capacity, driver,
   fixed PCI ancestry, device node and kernel identity, and refuse additional,
   mounted, active, ambiguous or changed targets. Validate again immediately
   before every destructive backend invocation. A device name or VM detector
   alone never authorises writing. Test negative controls.
5. Require explicit target selection, an unchecked erase acknowledgement and
   complete serial confirmation. Show exact source and image identity. Use
   genuine backend completion/exit status, bounded diagnostics, no estimated
   progress and no automatic retry after a failed write. Disable encryption
   unless a separate encrypted install, unlock and reboot is actually proved.
   First run preserves a local-account path and does not offer functional QOR ID
   without tested authentication.
6. Retain the official mark across the controllable boot and UI stages. Firmware
   stays OVMF's; GRUB is a static menu, not an animation stage. Plymouth may
   animate OS loading only with diagnostic escape/fallback. Asset previews,
   configuration and application screenshots are not boot evidence.
7. Accept the milestone only with an exact committed source revision, image
   checksum, VM/target configuration, real boot/install logs and screenshots,
   cancellation/failure checks, media detached, installed-system boot, first-run
   completion and a second reboot proving state persisted. Keep untested modes
   explicit. UEFI without Secure Boot and one unencrypted VirtIO disk are the
   initial lab scope; physical hardware, BIOS, Secure Boot, storage variants,
   updates, rollback and full device readiness require separate proof.

## Consequences

Local implementation can proceed without changing host boot or runner settings.
Dependency bootstrap against mutable package repositories is not a reproducible
release input: freeze its resolved image and package identities, runtime hashes
and source inputs before accepting an artifact. A failed build or resource limit
remains a failed or blocked check, never a boot or installation pass. The current
capability map and commands stay in [image/README.md](../../image/README.md).
