# Where QOR OS stands

For Andrew, 15 November 2026. QQ Sprite motion now refuses incompatible physics
handles without a script error; two new regressions fail before the fix and
pass afterward. Music, Sprite and TileMap preserve current-parent play, pause,
replay and pixel-scale behavior. Existing assertions and gameplay timing stay
unchanged. All 14 focused results pass.

Structured lint, machine-readable checks of Qt's interface language, corrects
the earlier console-only count: Runtime had 20 warnings and Studio had 18.
The source fixes remove 14 with none added, leaving six Runtime and 18 Studio
warnings; Player has none. Adding the missing SpatialAudio dependency in a
temporary metadata copy makes Runtime lint pass, while invalid-property and
invalid-type controls still fail. The installed Qt development kit is unchanged;
normal lint still fails. The correction is proved but not integrated.

The native Pi passes all 109 scene results normally and under address/undefined-
behavior sanitizers, checks for memory and invalid-operation errors, with leak
checking. Complete gameplay passes 58 results; Store launch passes three.
The packaged-game creator journey passes in 201.365 seconds under its original
300-second limit, including edit/rebuild and playback after source removal.
Prism, the display compositor and shell, verifies Library arguments through a
test launcher; real Player playback runs separately on a private display.
Other QQ suites and the complete Studio graphical journey retain their
14 November evidence and were not all repeated for this increment.

Agent retains 21 passes, its identical log-timing failure and one optional
provider skip. Player retains 13 passes, two failures against the 20-frames-
per-second floor at 0.5 and 19.5, and an unavailable Landlock kernel-confinement
check. ARQADE's earlier private-Qt compatibility warning remains unrerun.

Native builds emit no compiler or licence diagnostics. Hardening, checks of
program exploit mitigations, passes 16 QQ programs and the sanitizer program.
Original isolation and memory guards remain: all 2,305 samples stay at or above
5,353 MiB available against the 3,072-MiB floor. Final readback verifies 1,412
sources and eight executables; no test workers remain and the original lock
is free. Evidence: `build/development-2026-11-15/session-04/`. All 273 files of
the preceding sealed receipt are unchanged. Complete repository hooks now pass
on a separate Pi checkout using the pinned tools and original guard. Windows
remains below its memory floor, so its heavy product checks remain incomplete.

Branch `fix/native-play-clock-waits` carries this checkpoint for draft pull
request #10; the reconciliation, hook and publication receipts are under
`build/development-2026-11-15/session-05/`. Its preceding hosted source checks
pass, but required Qt builds and core audit are skipped and Release readiness
fails. Qt CI, automated build and test, stays paused pending runner isolation.
Full-product, Windows, hardware and Q Pi image acceptance remain open.
QOR-0028's canonical-repository direction still requires a separate proposal;
ownership boundaries remain.
