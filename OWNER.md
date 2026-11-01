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
5. **You merged demiurge-chain #117** (*a request to merge*). Ever Rift's and
   Darkveil's rules are the same there and here, and its checks passed.

## Checked

**CI passes at `3bf213a`** on both systems. On Windows, QQ and **ARQADE passed
first time, Bellwick's tests included;** QOR Media passed on its automatic second
attempt, after a test window did not get the keyboard once. **Two things to know.**
That run first failed one test, because Windows' clipboard refused every program
on this laptop for at least 1¾ hours. It let go in the minute a mouse woke the
laptop, just before you merged #117, with no restart. What held it is not known. I
suspected `msrdc.exe`, the viewer for Linux windows, and tried to end it; the
permission system refused, and it was still running afterwards. And nobody has
played or heard Bellwick, Ever Rift or Darkveil.

## Needs you

1. **Name Bellwick.** Nothing of it goes public until then. Near it are *Bell
   Kingdom* and *Curse Crackers*. A search is not trademark clearance.
2. **Play Bellwick, Ever Rift and Darkveil.**
3. **Move the Store's signing keys off this PC.** A CI job here could read them.
4. **During CI:** on AC, lid open, unlocked, mouse and keyboard left alone. One
   test also clears what you had copied, on every Windows run.
5. **The Linux machine:** a USB-C SSD of 500 GB or more and a USB stick of 8 GB
   or more, then BUILDING.md's steps.
6. **Legal, still open:** trademark advice, the legal entity, the Qt letter,
   replacing the 5 October key.

## Next

The racer, then the pixel-art and arcade games; and Phase 1's goal on the Linux
machine: Orb Run in Prism with a DualSense.
