# QOR-0025: Q Pi starts on a supported Raspberry Pi desktop

- **Status:** Accepted for local preparation; no image or hardware accepted
- **Date:** 2026-11-09
- **Deciders:** the owner, requesting Q Pi for Raspberry Pi 5, ARM64, 8 GB RAM and 128 GB storage
- **Relates to:** [QOR-0004](QOR-0004-the-base-image-is-fedora-atomic.md)
  (limited development-edition exception to decisions 1 and 3),
  [QTOS-0002](qtos/QTOS-0002-linux-kernel-and-the-qt-line.md)
  (limited development-edition exception),
  [QOR-0018](QOR-0018-the-security-and-quality-bar.md),
  [QOR-0024](QOR-0024-disposable-vm-installation-lab.md)

## Context

At reviewed source `3a5ddc3`, the Fedora bootc lab is amd64/UEFI, not a Pi
image. Prism's non-simulator build reports authentication unavailable and refuses
unlock. Device services remain unfinished. Installing the simulator would not
provide a real device session. The owner instead requests a usable maintained
Pi login/desktop with QOR applications and development tools first.

The supplied preparation bundle is an unsigned planning input, not a builder,
licence grant or permission to install tools, change services or write media.
The live request authorises reversible preparation in the current project and
requires unrelated projects to remain separate. The initial checkout and thirteen
other worktrees were clean; no active QOR build worker was identified.

## Decision

1. Scope **Q Pi** as a private QOR OS development edition for Pi 5 ARM64.
   Start from Raspberry Pi OS **64-bit desktop, Debian Trixie**, as the base
   candidate. Use its supported login, desktop and hardware stack, requiring a
   real local account login with autologin disabled. This is a narrow exception
   to the Fedora/Qt-only host desktop rules, not a replacement for full QOR OS.
   QOR applications still use commercial Qt 6.12. No new licence is accepted.
2. Ship no QOR simulator, simulated services, development unlock or purported
   QOR ID authentication. Keep the full device Containerfile and capability
   guard unchanged. A base-only image is not the completed Q Pi milestone:
   native QOR applications and a small local build must actually work.
3. Prepare regular image files only. Pin and verify the chosen base artifact,
   builder, package inputs, source revision, Qt target/host tools, module and
   dependency closure before accepting a build. The `rpi-image-gen` Pi 5 layer
   is a builder candidate; its minimal example is not a usable desktop.
   Initial qualification targets the 2712 kernel's 16-KiB pages. No silent
   change to a 4-KiB kernel to conceal incompatible dependencies.
4. Keep the original QOR heavy-job lock and 3-GiB available host-RAM floor.
   Size scratch space on the physical backing volume, not WSL's virtual free
   space. Preserve QOR workers, worktrees and evidence. Leave unrelated projects
   and their services untouched. No tool
   installation, privilege grant, host configuration change, media operation,
   deployment, publication or paid service is authorised by this record.
5. Prepare two first-boot routes: temporary wired USB input, or separately
   authorised per-device supported Imager provisioning with Wi-Fi and key-only
   SSH. Bluetooth input cannot be assumed available before pairing. No shared
   credentials, private keys or network credentials enter the reusable image,
   repository or logs. Remote access stays off in the generic image.
6. Lightweight local 2D QQ Studio and optional user-controlled remote heavy
   builds are later milestones. Current QQ runtime, Player and Studio still
   depend on Quick 3D; no existing switch makes Studio 2D-only. Do not include
   third-party engines or operate their editors under this work.

## Consequences

Useful read-only probes and preparation can proceed on this Windows host.
Image composition is currently blocked; no ARM64 QOR application, image boot,
hardware rendering or Bluetooth pairing has been verified. A clean committed
source, qualified builder and actual Pi acceptance remain separate requirements.
The canonical commands, provisioning plan and acceptance sequence live in
[image/README.md](../../image/README.md#q-pi-development-edition).
Full Fedora/Prism work, its existing proof and all release gates retain their scope.
