# Where QOR OS stands

For Andrew, 13 November 2026. Main remains `3002419`; current work is on
`feat/core-audit-remediation`. Upstream repairs are pushed at `042941c` in
Demiurge-chain draft PR #118. Neither the QOR OS core pin nor a release changed.

## Repairs and developer access

The Launcher test helper now uses Cargo's reported executable path. Patched
core and Launcher dependencies report zero vulnerability advisories locally,
with 13 and 23 maintenance warnings retained. Both services still fail on an
SQLite dependency that even stable SQLx cannot update to the patched version.
Compatible blockchain patches remove nine advisory findings; 44 remain, plus
36 warnings. No audit exception was added.

At `042941c`, hosted chain format/lints and all 213 tests pass with WebAssembly
built. QOR ID, Social and ARQADE build/test jobs pass. Launcher validation is
still running; the service audits keep upstream's security job red. The pin
stays at `6a4822d` until upstream and QOR integration are validated.

Linux product tests now create a private short runtime directory and clean it
up without touching the desktop's directory. Its new regression fails against
the old script; all 18 harness fixtures pass on the native Pi with the fix.

GitHub's homepage remains QORSYNC.DEV; the README links progress, the masterplan,
developer guidance and security. Offline discovery covers 14 contribution areas.
The core bridge, the shell's command interface, still has four version/amount
commands. Broader DRC-369 asset and agent capabilities remain gated upstream.

## Q Pi and remaining acceptance

Q Pi development is reopened. Its approved SSH route, secure remote access,
works with strict host checks. Native trials verified private device/display
and network namespaces, with at least 6,970 MiB available throughout rendering.
Seven publishing protocol cases and the unversioned-folder window case pass;
the second window case times out with both render loops. Neither run passes
the complete suite. Evidence is in `build/development-2026-11-13/session-05/`.

Qt CI, automated native builds, remains paused pending isolated workers. Full
native products/sanitizers, hardware, image and release acceptance remain open.
GitHub private vulnerability reporting is enabled upstream; QOR OS's public
report-routing policy is still open. Vercel confirms domain assignments but
project access remains unresolved. No image, package, release or production
deployment was published.
