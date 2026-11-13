# Where QOR OS stands

For Andrew, 13 November 2026. PR #5 is merged on main at `ed3526e`.
Continuation is on `feat/native-qq-qualification`. Linux tooling passes 720 tests
with 124 platform skips; Windows passes 746 with 98 skips. All 23 Linux harness
fixtures and 63 ARQADE rule tests pass. Windows stayed above 3,314 MiB available;
the original 3,072-MiB memory floor is unchanged.

## Verified progress

The Linux harness checks its fake keyring before starting test services. Media
selects pinned x64/ARM64 PDFium, its PDF editing library, in separate caches and
refuses incomplete extraction. Its ARM64 archive and library layout pass static
checks. Qt PDF modules are absent on the Pi, so Media is not built there.

Q Pi's broader QQ procedure uses a private software display and isolated device
and network views. World, scene and Studio pass 11, 56 and 29 Qt Test results;
play tests are running. The earlier full publishing suite passes 11 results.
The complete creator/player journey is prepared against verified Prism programs
and waits for the shared lock. Product memory-error, hardware and image checks
remain open. Four Prism source differences were line endings only.

Demiurge-chain draft PR #118 patches certificate/compression dependencies.
All 213 chain tests pass with WebAssembly, portable runtime code, built; core
and Launcher pass 245/109 tests with 10/five ignored. The chain audit retains
38 vulnerability findings and 26 warnings. Both service audits still fail on
SQLx's SQLite dependency. The QOR OS core pin remains `6a4822d`. Further workflow
pins are being checked; the upstream Actions allowlist is restricted to its nine
used action repositories.

The public homepage and QOR ID's health, database and cache checks pass. Read-only
chain requests show runtime 11 and advancing finalized blocks. Vercel project
access, the live homepage repository link, Social/ARQADE domains and client
migration remain unresolved. GitHub's homepage points to QORSYNC.DEV.

## Continuation

Finish native products and the creator/player journey; resolve upstream audits
before richer asset/agent bridge commands, the shell-to-core interface. The
roadmap owns the remaining gates. Native Qt CI, automated checks, stays paused
pending isolated workers. No image, package, release or production deployment
was published. Receipts: `build/development-2026-11-13/session-06/`.
