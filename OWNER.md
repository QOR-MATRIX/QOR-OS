# Where QOR OS stands

For Andrew, 7 November 2026. Branch integration/owner-brief; tested image source fa17674.

The private installer ISO (boot-media image) is 3,601,666,048 bytes. Its checksum,
compiler source and runtime records are in build/vm-install-2026-11-06/.
It uses Fedora bootc (whole-image deployment), a checked offline OCI payload
(content-addressed image), a locked local qortest account and Qt's existing interface.
Encryption and QOR ID authentication remain unavailable.

## Startup and failure controls pass; installation is incomplete

The image and its 1,437 runtime files (application and libraries) record the same clean source. Both image
branches pass 14 warning-fatal bootc checks, with zero warnings and one skipped.
All 63 bundled programs, libraries and plugins resolve their dependencies.

Normal automatic boot now reaches the real installer. Its agent permits four
approved operations and disables 39 others. Review and Cancel preserve the entire
16-GiB disk hash. A separate corrupt-payload image is refused, keeps retry disabled,
and preserves its complete test disk through Cancel and shutdown.

The first actual stock installation exits 1 because its generic SELinux process
context cannot perform the required label operation. SELinux is the kernel's
access-control system. The full disk remains blank. Enforcement was never disabled.

A separate guest diagnostic pairs the existing installer process context with
its required executable label. The label capability succeeds; the real Qt frontend
shows the assigned target in that context with enforcement enabled. The recipe now
includes this pair, and a small native packing check confirms the label survives
filesystem packaging. This is diagnostic evidence; a fresh normal image still
needs to prove installation and persisted first run.

## Checks and resume

The new regression fails on the old configuration, then all 61 portable cases pass
on Windows and Linux, plus eight Linux process/file cases; none fail or skip.
Native udev syntax checks pass. C++ and QML are unchanged; earlier memory-error
checks keep their original chronology.
Full OS/product, hardware, Secure Boot, recovery, rollback, repeatable-build and
release gates remain open.

All ten temporarily stopped Kintrek services are restored. One transient port bind
failed during restoration and then recovered without a network-setting change.
PostgreSQL was never stopped and is healthy. All disks, outputs and 14 Git worktrees
(checkouts) are preserved. No VM or heavy job is active. Host boot, security,
network and runner settings are unchanged.

Next: validate and checkpoint the context/entrypoint correction, build its fresh
image, then prove stock installation, media-free welcome and reboot persistence.
Exact commands, hashes, failures and resume state: build/vm-install-2026-11-06/LATEST.md.
