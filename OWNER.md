# Where QOR OS stands

For Andrew, 12 November 2026. Owner-authorised checkpoint on
`integration/owner-brief`, based on `1b9c485`: Prism manager fixes, native Linux
job coordination, prepared Q Pi tooling, editor settings and reconciled guides.
All 52 incoming file fingerprints and 150 saved evidence files were rechecked
before preparing this commit. Build evidence remains local under `build/`.

## Prism's existing Linux software checks pass

Prism is the compositor and shell. Its saved candidate builds without warnings
in Release and with ASan/UBSan (address and undefined-behavior checks).
**Each configuration passed 42 pointer-capture results, with no failures or
skips, and all four affected suites.** These include unique announcements and
bind/destroy/rebind for all five protocol managers, including cursor-shape v1/v2.

Root QML (Qt interface language) lint reports zero warnings in both builds.
Hardening (compiled exploit defenses) passes for 15 Release and 36 sanitizer
programs. All 18 exercised program identities were verified; all nine sanitizer
programs link both runtimes. Leak detection was off.

Windows tooling checks pass **203 tests**, with 15 Linux-only skips. The initial
missing-Ninja failure and successful rerun with the installed build tool on PATH
are preserved under `build/commit-checkpoint-2026-11-12/session-01/`.

## Host recovery and remaining work

Owner-requested cleanup stopped six optional services and eight background
workers without changing startup settings. Windows Search and Phone Link had
returned at verification. The first build attempt stopped at 3,040 MiB available;
additional cleanup allowed completion under the original job lock and unchanged
3,072-MiB floor, with WSL (Linux on Windows) resident, graphics hardware access
blocked and software/virtual devices only.

Next: add the deferred configure-acknowledgement size regression and direct
null/wrong-object cases, using production logic without a test hook. These
focused builds exclude the core library; full-OS, hardware and isolated CI
(automated build service) acceptance remain separate.

Commands, failures, successful checks and preserved files:
`build/prism-completion-2026-11-12/session-02/`.
Q Pi and Qt Account/ARM64 work remain **on hold**. This checkpoint does not
establish an image, media-write, hardware or release result.
