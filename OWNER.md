# Where QOR OS stands

For Andrew, 13 November 2026. Main includes PR #4's Linux test-harness fix and
updated developer/security guidance. The next branch is
`feat/native-publishing-validation`, with a contributor handoff for native tests
and agent-tool discovery. Upstream repair code is validated at `042941c`; draft
PR #118 now has documentation head `3043551`. The QOR OS core pin remains `6a4822d`.

## Verified progress

Hidden Linux product tests now use a private short runtime directory and leave
the desktop's directory untouched. All 18 harness fixtures pass on the Pi; the
new regression also fails against the old script. Repository-wide hooks pass.
Hosted Lint passes 715 Python tests with 124 platform skips and all 63 ARQADE
rule tests at `1cdeef6`.

Q Pi development is reopened. The full native QQ publishing suite passes all
11 Qt Test results, including both Studio windows, without failures or skips.
It uses a private software display, two rendering workers and isolated views of
devices and networking. Hardware devices are hidden, networking is local only,
and the original resource lock was held throughout. Free memory stayed at or above 6,956 MiB
against the unchanged 3,072-MiB floor. Earlier diagnostic timeouts are preserved.
This is application evidence, not a Q Pi image or hardware qualification.

Upstream's repaired Launcher helper uses Cargo, the Rust build tool, to find its
executable. All four formerly failing spawn tests pass. Hosted checks pass 213
chain tests with WebAssembly, portable runtime code, built; 245 core tests pass
with 10 ignored and 109 Launcher tests pass with five ignored. Core and host
audits report zero vulnerability findings, retaining 13 and 23 maintenance warnings.
Both services still fail on an SQLite database dependency. The chain's patches
remove nine findings, leaving 44 and 36 warnings. No exception was added.

## Continuation

Continue the full native product/sanitizer and creator/player journeys, resolve
upstream service dependencies and warnings, then validate a core-pin update before
expanding DRC-369 asset or agent commands. The bridge, the shell-to-core interface,
still has four version/amount commands. Evidence and exact limits are in
`build/development-2026-11-13/session-05/` and the canonical roadmap.

GitHub's homepage is QORSYNC.DEV; offline discovery covers 14 contribution areas.
Qt CI, automated native builds, stays paused pending isolated workers. Public
security-report routing, live service domains, Vercel project access, hardware,
image and release gates remain open. No image, package, release or production
deployment was published.
