# Where QOR OS stands

For Andrew, 7 November 2026. Branch integration/owner-brief; installer image source f132dc1.

The private installer ISO (boot-media image) is built: 3,601,666,048 bytes, with its
checksum and complete source/runtime records in build/vm-install-2026-11-06/.
It uses Fedora bootc (whole-image deployment), a checked offline OCI payload
(content-addressed image), a locked local qortest account and the existing Qt interface.
It offers neither encryption nor QOR ID authentication.

## Image milestone: incomplete

Both image branches pass warning-fatal checks: 14 passed, zero warnings, one skipped.
The fresh runtime (application code and libraries) records 1,437 files at clean f132dc1;
all 63 target-image programs/libraries/plugins resolve their dependencies.

The new ISO boots automatically through its branded menu into Linux on a newly
recorded 16-GiB virtual disk. The display helper succeeds, but the installer and
restricted guest agent lose their early device-wait jobs before virtual serial
ports become active. Normal automatic installer startup therefore fails.

A separate guest-only diagnostic boot confirms the exact port names and PCI
(device-bus) ancestry. Starting the services after readiness displays the real
installer and correct disk/source identity. Target review and Cancel leave the
entire disk hash unchanged. The actual guest agent permits exactly four approved
operations and disables 39 others; its shutdown works. These diagnostic overrides
prove neither automatic startup nor installation or persisted first run.

The recipe now starts those services from their assigned port-ready events and
copies service files with non-executable permissions. This correction needs a
fresh image and normal boot. Existing disk, device and guest-agent guards remain.

## Checks and resume

The startup regression fails on the old recipe, then passes with all 60 portable
cases on each of Windows and Linux, plus eight Linux process/file cases; none fail
or skip. Native udev syntax checks pass for both rule variants. All 20 retained
Release/sanitizer source fingerprints still match. Native C++/QML is unchanged.
The corrected image is pending. Full OS/product, hardware,
Secure Boot, recovery, rollback, repeatable-build and release gates remain open.

The owner authorised temporarily stopping ten Kintrek non-database services to
free RAM. All ten are restored after every completed job; PostgreSQL was never
stopped and remains healthy. All test disks, prior outputs and 14 Git worktrees
(checkouts) are preserved. No VM or heavy job is active now. Host boot, security,
network and runner settings are unchanged.

Next: validate and checkpoint this startup correction, build its fresh image,
then prove normal startup, cancellation/failure, stock installation, media-free
boot, welcome and reboot persistence. Exact jobs, commands, hashes, failures and
remaining work: build/vm-install-2026-11-06/LATEST.md.
