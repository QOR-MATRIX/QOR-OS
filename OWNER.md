# Where QOR OS stands

For Andrew, 1 November 2026. One page, rewritten every session (`AGENTS.md` §2).

## What QOR OS is

The operating system for players, game makers and creators: the Prism shell
(*the compositor and desktop in one*) and Lumen design (*QOR OS's look*), with
Demiurge's QQ engine, ARQADE and QOR Media, on Linux. Code:
`QOR-MATRIX/qor-os` (private), here at `X:\Repoz\QOR-OS`.

## This session

1. **How Unreal Engine comes to QOR OS is decided** (record QOR-0020), and nothing
   of it is built yet. Epic's own Linux builds, unchanged. Games run in gamescope
   (*Valve's game compositor*) in the sandbox; the editor is each person's own
   download from Epic; a plugin joins games to the Library and the look. The
   licence forbids the rest: no editor from us, no engine shared between games.
2. **Bellwick is built,** a working title: the platformer, eight levels, ARQADE's
   seventh card. A search program finishes the whole game by the real rules.
3. **Ever Rift and Darkveil now face the camera.**
4. **A real bug, found by accident:** labels on Lumen's accent buttons, check
   marks and radio dots had been black since the first version, not near-white,
   and Prism never passed your colours to the products. Both fixed.
5. **CI no longer fails because the screen went off.** CI (*the automatic build
   and test run*) now keeps the display on while tests draw.
6. **You merged demiurge-chain #117** (*a request to merge*).

## Checked

**CI passed at `3bf213a` and `c59286e`,** on both systems, before its jobs on this laptop were paused on 1 November (*until a
runner is shown to be isolated*): since then nothing is built, and Build reads red by design. ARQADE passed first time on
Windows, Bellwick's tests included. Windows' clipboard refused every program on
this laptop for at least 1¾ hours; what held it is not known. For Unreal: Epic's
5.8 documents, its licence of May 2023, and a running Prism. **Nobody here has read
Epic's current licence:** its pages refuse automated reading. Nobody has run Unreal
on QOR OS, or played or heard Bellwick, Ever Rift or Darkveil.

## Needs you

1. **Epic's licence, before anyone downloads Unreal.** With counsel: read the
   current text; choose who accepts it, since it does not transfer and the legal
   entity is open; ask Epic about plugins, our Store, CGT, and AI. It limits using
   an AI on engine code, so I keep away from the engine. Nothing is urgent: the
   first stage needs no engine until its last step.
2. **Name Bellwick.** Nothing of it goes public until then. Near it are *Bell
   Kingdom* and *Curse Crackers*. A search is not trademark clearance.
3. **Play Bellwick, Ever Rift and Darkveil.**
4. **Move the Store's signing keys off this PC.** A CI job here could read them.
5. **CI is paused.** Its runners are stopped until a replacement is shown to be
   isolated (`docs/CI.md`); nothing runs on this laptop until then.
6. **The Linux machine:** a USB-C SSD of 500 GB or more and a USB stick of 8 GB
   or more, then BUILDING.md's steps. Epic recommends 32 GB of memory for its
   editor, and this laptop has 16.
7. **Legal, still open:** trademark advice, the legal entity, the Qt letter,
   replacing the 5 October key.

## Next

Prism's pointer lock and relative motion, which gamescope needs before it will
start: Unreal and Phase 3's Steam both wait on them. Then the racer, the pixel-art
and arcade games, and Orb Run in Prism with a DualSense on the Linux machine.
