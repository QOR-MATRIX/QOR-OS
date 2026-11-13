# Where QOR OS stands

For Andrew, 13 November 2026. Work continues on
`feat/native-publishing-validation`. Main contains the Linux test-runtime fix.
The missing fake-keyring check passes hosted Linux CI, automated checks:
720 Python tests pass with 124 platform skips, including all 23 harness fixtures;
all 63 ARQADE rule tests pass. An additional stale-cache retry check is pending.

## Verified progress

The previous native QQ publishing run passes all 11 Qt Test results with no
failures or skips. It used a private software display and isolated device/network
views, two rendering workers and the original 3,072-MiB memory guard; available
memory stayed at least 6,956 MiB. All 18 earlier harness fixtures pass on ARM64.
The new missing-fixture regression is covered by the hosted run above.

Q Pi's full QQ product procedure initially stopped before tests: its filtered
snapshot lacked the tracked in-memory fake keyring. A private tools overlay now
supplies it. World rendering and scene suites pass; Studio is running under
the original coordinator. The full procedure has not passed; hardware and image
qualification remain open.

Demiurge-chain draft PR #118 has further certificate/compression patches at
`3d71009`. Its local audit reports 38 vulnerability findings and 26 maintenance
warnings, from 44 and 36. All 213 hosted chain tests pass with the WebAssembly build included. The chain framework
requires the affected logging version exactly, so that upgrade was refused.
The framework pin and runtime source remain unchanged.

The Media helper now selects pinned x64/ARM64 PDFium archives in separate caches
and refuses checksum or extraction failures. Its ARM64 library passes the
16-KiB page-layout check. Qt PDF modules are absent on the Pi, so Media is not
built there. The four Prism source differences were only line endings; both
raw hashes are preserved without claiming a new build.

The earlier upstream repair at `042941c` passes 213 chain, 245 core and 109
Launcher tests; the latter two retain 10 and five ignored tests. Core/host audits
had zero vulnerability findings and 13/23 warnings. Both service audits still
fail on SQLx's SQLite database dependency. The QOR OS core pin stays `6a4822d`.

## Continuation

Complete the native product and sanitizer checks and creator/player journey;
resolve upstream service dependencies and warnings before a tested core-pin
update and richer DRC-369 asset or agent commands. The bridge, the shell-to-core
interface, still has four version/amount commands. The canonical roadmap owns
these gates. Evidence is under `build/development-2026-11-13/session-06/`.

GitHub's homepage is QORSYNC.DEV. Native Qt CI remains paused pending isolated
workers. Live service domains, Vercel project access, public security routing,
hardware, image and release gates remain open. No release or deployment occurred.
