# Where QOR OS stands

For Andrew, 8 November 2026. Latest tested image and compiler: 5b7d321; mount isolation awaits a fresh image.

The latest private installer ISO (boot-media image) is 3,601,670,144 bytes.
Its checksum and evidence are in build/vm-ui-reboot-2026-11-08/.
It uses Fedora bootc (whole-image deployment), a checked offline OCI payload
(content-addressed image), a locked local qortest account and Qt's existing interface.

## Private installation proof and UI reboot repair

The earlier clean feed626 image passes review/Cancel with the entire 16-GiB disk
unchanged, plus separate corrupt-payload refusal. Installation exits 0. The installed
disk boots without media, completes the real welcome and retains its record through
two guest-agent reboots. All five virtual-machine phases exit 0, with four distinct
installation/installed boot identifiers. That proof stays separate and unchanged.

Both images install successfully but fail the normal UI reboot. A diagnostic finds
bootc's storage bind mount still active after exit 0. Removing only that verified
mount makes the unchanged strict guard pass and the real button reboot. This is
diagnostic evidence, not normal acceptance.

The local backend now confines mounts to its supervised process and rechecks the
strict target guard before announcing completion. A fresh image and normal UI proof
remain pending. Unconfigured kernel loop nodes are separately distinguished from
backed storage; the real-kernel negative control still refuses writable backing.
SELinux, the kernel's access-control system, stays enforcing without new permissions.

## Checks and remaining work

All 67 portable cases pass on Windows and Linux, plus eight Linux process/file
cases. Regression failures on the old backend are preserved. The tested image has
1,437 runtime files and 63 programs, libraries and plugins with resolved dependencies.
Both image branches pass 14 warning-fatal checks, with zero warnings and one skipped.
The two native programs pass hardening and selected interface lint is clean.

QEMU, the emulator, uses four I/O workers within its unchanged 64-task bound.
The 3-GiB available-RAM preflight remains mandatory. Current Kintrek services are
running or restarting; PostgreSQL is healthy. Disks, outputs and checkouts are preserved.
Host boot, security, network and runner settings are unchanged.

The full OS/product, hardware, Secure Boot, encryption, authentication, recovery,
rollback, repeatable-build, CI-isolation and release gates remain open.
The immediate task is the repaired UI route; the full device session follows.
