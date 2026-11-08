# Where QOR OS stands

For Andrew, 8 November 2026. Tested image and compiler: feed626; the local inventory correction awaits a new image.

The private installer ISO (boot-media image) is 3,601,666,048 bytes. Its checksum
and complete private evidence are in build/vm-install-2026-11-06/.
It uses Fedora bootc (whole-image deployment), a checked offline OCI payload
(content-addressed image), a locked local qortest account and Qt's existing interface.

## Private installation proof and UI reboot repair

Normal review and Cancel preserve the entire 16-GiB disk checksum. A separate
corrupt-payload image is refused and preserves its complete test disk through shutdown.
Stock installation then exits 0, with a complete, checked 2,051-byte backend log.

The installed disk boots with no installation media. The real welcome starts
pending; completing its three pages saves the expected record. Two further
installed reboots find that same record before welcome runs. All five actual
virtual-machine phases exit 0; installation and the three installed boots have
four distinct boot identifiers. Resource interruptions are preserved separately;
the same installed disk continues without a reset. Those reboots use the restricted
guest agent.

A fresh installation on 8 November also exits 0, but its actual UI reboot button
returns an error. That failure and disk are preserved. A separate diagnostic boot
reproduces a guard defect: an unused kernel loop-device node is counted as a writable
disk despite having no backing storage. The local correction excludes only an
inactive, zero-size loop node with verified kernel ancestry and no loop attributes.
The real-kernel preview accepts that node and still refuses it when writable backing
is attached. A rebuilt image and successful normal UI reboot remain pending.

The host QEMU emulator now limits file-I/O workers to four within the unchanged
64-task container bound. SELinux, the kernel's access-control system, uses the
standard installer context and matching executable label, with enforcement enabled.
Installation writes target the assigned virtual disk. Host boot, security,
network and runner settings are unchanged.

## Checks and remaining work

All 65 portable cases pass on Windows and Linux, plus eight Linux process/file
cases. The omitted-mount Linux harness failure is retained. Native QEMU confirms four workers. Both tested image branches
pass 14 warning-fatal checks, with zero warnings and one skipped. All 63 bundled
programs, libraries and plugins resolve their dependencies.

Current Kintrek services are running or restarting; PostgreSQL is healthy.
Container replacements and the failed lookups of removed identities are recorded.
All prior disks, outputs and checkouts are preserved. Exact commands, failures,
screenshots, hashes and resume history remain in the private evidence directory.

The full OS/product, hardware, Secure Boot, encryption, authentication, recovery,
rollback, repeatable-build, CI-isolation and release gates remain open.
The immediate image task is the repaired UI reboot route; the full device session follows.
