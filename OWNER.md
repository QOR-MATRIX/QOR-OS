# Where QOR OS stands

For Andrew, 1 November 2026. One page, rewritten every session (`AGENTS.md` §2).

## What QOR OS is

The operating system for players, game makers and creators: the Prism shell
(*the compositor and desktop in one*) and Lumen design (*QOR OS's look*), with
Demiurge's QQ engine, ARQADE and QOR Media, on Linux. Code:
`QOR-MATRIX/qor-os` (private), here at `X:\Repoz\QOR-OS`.

## This session

1. **The platformer is built:** Bellwick, a working title, ARQADE's seventh
   card, with eight levels in four regions. A search program finishes the whole
   game by the real rules. It and its tests caught my errors: gaps too wide to
   jump, a sealed floor, and the level's end bell ringing for a secret room.
2. **Ever Rift and Darkveil now face the camera.** "Up" ran diagonally across
   the screen. Their rules did not change.
3. **A real bug, found by accident:** the labels on Lumen's accent buttons, check
   marks and radio dots have been black since the first version, where the design
   says near-white. And Prism never passed your colours to the products. Both are
   fixed, and frames still take about 2.79 ms. Tests now catch this.
4. **CI no longer fails because the screen went off.** CI (*the automatic build
   and test run*) kept running in Modern Standby (*this laptop's sleep: working,
   screen off, windows barely drawing*) and failed for an hour across unrelated
   tests. `tools/ci/with_display.py` now keeps the screen on while tests draw,
   wakes the laptop first, and refuses to run if it is locked. It changes none
   of your settings.

## Checked

At `3bf213a`: Security, Lint, and both systems' builds and OS tests pass. On
Windows, QQ passed, and **ARQADE passed every test first time, Bellwick's
included.** **The Build is still red: QOR Media fails one test, on both
attempts.** It copies a PDF's text, and Windows' clipboard is refusing every
program on this laptop, with no QOR program running. So this is the machine and
not the code. The suspect is `msrdc.exe`, the viewer for Linux windows; that is
unproven. I tried to end it and the permission system refused, so it is yours to
do. demiurge-chain #117 still passes and can merge. Nobody has played or heard
Bellwick, Ever Rift or Darkveil.

## Needs you

1. **Free the clipboard.** If copy and paste fails for you too, this is why. In
   a terminal: `taskkill /IM msrdc.exe /F` (no Linux windows were showing), or
   restart the laptop. I am watching, and rerun the failed job when it frees.
2. **Merge demiurge-chain #117.** Nothing players see changes.
3. **Name Bellwick.** Nothing of it goes public until then. Near it are *Bell
   Kingdom* and *Curse Crackers*. A search is not trademark clearance.
4. **Play Bellwick, Ever Rift and Darkveil.**
5. **Move the Store's signing keys off this PC.** A CI job here could read them.
6. **During CI:** on AC, lid open, unlocked, mouse and keyboard left alone. That
   clipboard test also clears what you had copied, on every Windows run.
7. **The Linux machine:** a USB-C SSD of 500 GB or more and a USB stick of 8 GB
   or more, then BUILDING.md's steps.
8. **Legal, still open:** trademark advice, the legal entity, the Qt letter,
   replacing the 5 October key.

## Next

The racer, then the pixel-art and arcade games; and Phase 1's goal on the Linux
machine: Orb Run in Prism with a DualSense.
