# Where QOR OS stands

For Andrew, 8 November 2026. Tested image and compiler: clean 66df3f7.

The latest private installer ISO (boot-media image) is 3,601,670,144 bytes.
Its checksum is 9251ca6d0799d987a30962faf32fb0278c9a03697210751cd377f7a2384fc575.
Evidence is in build/vm-ui-reboot-2026-11-08/isolated-ui-proof/.
It uses Fedora bootc (whole-image deployment), a checked offline OCI payload
(content-addressed image), a locked local qortest account and Qt's existing interface.

## Private installation and actual installer reboot

The earlier clean feed626 image passes review/Cancel with the entire 16-GiB disk
unchanged, plus separate corrupt-payload refusal. Installation exits 0. The installed
disk boots without media, completes the real welcome and retains its record through
two reboots through the restricted control service. All five virtual-machine (VM)
phases exit 0, with four distinct
installation/installed boot identifiers. That proof stays separate and unchanged.

The new image installs on a fresh 8-GiB disk and reboots through the actual
installer button. Two boots without installation media prove welcome starts pending,
completes through its three pages and remains complete after one installed reboot.
All three VM phases exit 0 with three distinct boot identifiers. The verifier accepts
the complete logs and record. No manual mount cleanup or debug override is used.

The preserved failures identify bootc's retained storage mount. The backend now
confines mounts to its supervised process and rechecks the unchanged strict target
guard before completion. Unconfigured kernel loop nodes are separately distinguished
from backed storage; the real-kernel negative control still refuses writable backing.
SELinux, the kernel's access-control system, stays enforcing without new permissions.

## Checks and remaining work

All 67 portable cases pass on Windows and Linux, plus eight Linux process/file
cases. Regression failures on the old backend are preserved. The tested image has
1,437 runtime files and 63 programs, libraries and plugins with resolved dependencies.
Both image branches pass 14 warning-fatal checks, with zero warnings and one skipped.
The two native programs pass hardening (exploit protections); selected interface lint is clean.

QEMU, the emulator, uses four I/O workers within its unchanged 64-task bound;
the installation peaks at 15 tasks with no memory or task-limit hits.
The 3-GiB available-RAM preflight remains mandatory. Paused Kintrek services are
restored, running or restarting; PostgreSQL is healthy. Disks and outputs are preserved.
Host boot, security, network and runner settings are unchanged.

The full OS/product, hardware, Secure Boot, encryption, authentication, recovery,
rollback, repeatable-build, CI-isolation and release gates remain open.
The next image work is the full device session.
