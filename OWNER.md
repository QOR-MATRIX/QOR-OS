# Where QOR OS stands

For Andrew, 7 November 2026. Image and compiler source: feed626; host launcher recorded separately by checksum.

The private installer ISO (boot-media image) is 3,601,666,048 bytes. Its checksum
and complete private evidence are in build/vm-install-2026-11-06/.
It uses Fedora bootc (whole-image deployment), a checked offline OCI payload
(content-addressed image), a locked local qortest account and Qt's existing interface.

## Private installation and reboot persistence pass

Normal review and Cancel preserve the entire 16-GiB disk checksum. A separate
corrupt-payload image is refused and preserves its complete test disk through shutdown.
Stock installation then exits 0, with a complete, checked 2,051-byte backend log.

The installed disk boots with no installation media. The real welcome starts
pending; completing its three pages saves the expected record. Two further
installed reboots find that same record before welcome runs. All five actual
virtual-machine phases exit 0; installation and the three installed boots have
four distinct boot identifiers. Resource interruptions are preserved separately;
the same installed disk continues without a reset. Reboots use the restricted
guest agent. The installer's UI reboot button remains untested.

The host QEMU emulator now limits file-I/O workers to four within the unchanged
64-task container bound. SELinux, the kernel's access-control system, uses the
standard installer context and matching executable label, with enforcement enabled.
Installation writes target the assigned virtual disk. Host boot, security,
network and runner settings are unchanged.

## Checks and remaining work

All 62 portable cases pass on Windows and Linux, plus eight Linux process/file
cases. Native QEMU confirms the four-worker setting. Both image branches pass
14 warning-fatal checks, with zero warnings and one skipped. All 63 bundled
programs, libraries and plugins resolve their dependencies. C++ and QML are unchanged.

Current Kintrek services are running or restarting; PostgreSQL is healthy.
Container replacements and the failed lookups of removed identities are recorded.
All prior disks, outputs and checkouts are preserved. Exact commands, failures,
screenshots, hashes and resume history remain in the private evidence directory.

The full OS/product, hardware, Secure Boot, encryption, authentication, recovery,
rollback, repeatable-build, CI-isolation and release gates remain open.
The next image milestone is the complete device session and its declared hardware tests.
