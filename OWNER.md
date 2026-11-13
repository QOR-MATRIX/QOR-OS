# Where QOR OS stands

For Andrew, 13 November 2026. The native product tooling at `74d22e9` passes
hosted Linux CI, automated checks: 720 Python tests, 124 platform skips and all
63 ARQADE rule tests. All 23 harness fixtures pass, including missing test
support, architecture selection and incomplete dependency extraction.

## Verified progress

The Linux harness checks its fake keyring before starting test services. Media
selects pinned x64 or ARM64 PDFium, its PDF editing library, in separate caches;
failed extraction cannot keep an old completion marker. The official ARM64
archive's digest and library layout pass the 16-KiB page check. Qt PDF modules
are absent on the Pi, so Media is not built there.

Q Pi's broader QQ procedure is running with a private software display,
isolated device/network views and the original 3,072-MiB memory guard. World
rendering passes 11 Qt Test results and scene handling passes 56; Studio is
running. The full run has not passed. The earlier publishing suite passes all
11 results. Four Prism source differences were only line endings; raw hashes
and the earlier Release/sanitizer evidence remain separate from this run.

Demiurge-chain draft PR #118 has certificate/compression patches at `3d71009`.
All 213 hosted chain tests pass with WebAssembly, portable runtime code, built.
Its audit reports 38 vulnerability findings and 26 warnings, down from 44/36.
The chain framework pins the affected logging dependency exactly; that upgrade
was refused. Framework pin, runtime source and QOR OS core pin `6a4822d` remain
unchanged. Both service audits still fail on SQLx's SQLite database dependency.
The earlier core/Launcher repair passes 245/109 tests with 10/five ignored;
its audits retain maintenance warnings.

## Continuation

Finish native products, memory-error checks and the creator/player journey;
resolve service dependencies and warnings before a tested core-pin update and
richer DRC-369 asset or agent commands. The bridge, the shell-to-core interface,
still has four version/amount commands. The roadmap owns these gates; receipts
are in `build/development-2026-11-13/session-06/`.

GitHub's homepage is QORSYNC.DEV. Native Qt CI remains paused pending isolated
workers. Live domains, Vercel access, public security routing, hardware, image
and release gates remain open. No release or production deployment occurred.
