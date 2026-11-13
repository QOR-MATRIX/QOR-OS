# Where QOR OS stands

For Andrew, 13 November 2026. Work continues on
`feat/native-publishing-validation`. Main contains the Linux test-runtime fix.
The current change detects a missing fake-keyring test fixture before starting
its private bus, display, build or suites. Its new regression awaits Linux CI,
automated checks. Repository-wide checks are being refreshed for this change.

## Verified progress

The previous native QQ publishing run passes all 11 Qt Test results with no
failures or skips. It used a private software display and isolated device/network
views, two rendering workers and the original 3,072-MiB memory guard; available
memory stayed at least 6,956 MiB. All 18 earlier harness fixtures pass on ARM64.
Those results do not cover this session's new missing-fixture regression.

Q Pi's full QQ product procedure initially stopped before tests: its filtered
snapshot lacked the tracked in-memory fake keyring. A private tools overlay now
supplies it, and the broader software run is in progress under the original
coordinator. It has not passed; hardware and image qualification remain open.

Demiurge-chain draft PR #118 has further certificate/compression patches at
`3d71009`. Its local audit reports 38 vulnerability findings and 26 maintenance
warnings, from 44 and 36. New hosted tests are in progress. The chain framework
requires the affected logging version exactly, so that upgrade was refused.
The framework pin and runtime source remain unchanged.

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
