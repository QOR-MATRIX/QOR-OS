# Where QOR OS stands

For Andrew, 2 November 2026. One page, rewritten every session (`AGENTS.md` §2). Checkpoint, mid-session.

## What QOR OS is

The operating system for players, game developers and game designers: the Prism shell (*the compositor and desktop in
one*) and Lumen design, with QQ, ARQADE and QOR Media, on Linux. Code: `QOR-MATRIX/qor-os`, private, at `X:\Repoz\QOR-OS`.
The work is on the branch `integration/owner-brief`; `main` has only your merge of pull request #2.

## Shown working, and where

1. **The first journey, in software.** Open a QQ project, bring in a model, edit, save, build a game, and play that copy from
   Prism's Library; two projects kept apart; a restart; unsaved work kept through a crash. At `7a5e0e0`, hidden, under WSL
   (*Linux inside Windows*), drawn in software: `tst_workspace` 235 passed, `tst_gamebuild` 149, `first_journey` passed,
   `tst_library` 32 under the memory checkers. **Not yet:** native Linux, a graphics card, Windows. Prism there starts a stand-in
   for the Player, and the real Player plays what it asked for. An import records no permission. A recheck found gaps still being
   fixed, the worst that a hidden copy of unsaved work can stop being kept, and that the Studio's view never shows a new sky.
2. **The Store's engine**, format 2: player and creator views kept apart, free items only, installs all or nothing. 550 cases on
   Linux and Windows; no pages yet. A recheck found one medium fault, being fixed.
3. **CI** (*the automatic build machines, both this laptop*): both runners stopped, no isolation setting made. A replacement VM
   (*a separate machine inside this one, with its own kernel*) is written, reviewed twice, and has never run.
4. **ARQADE's and QQ's suites in software:** ARQADE 4 of 4; QQ 9 of 10, the one failure the Player's floor of 20 frames a
   second, at 6.2 to 6.3.

## Needs you

1. **Run the VM's first Create and Prove** (`docs/CI.md`, "The owner's first run", at `c9513c6`). Tell me first: I pause builds.
2. **Merge pull request #3** (*a request to change `main`*): Build then fails while nothing was built, which is the truth.
3. **Decide the floor and the eased software checks** (`docs/CI.md`, "What Linux runs"). I recommend letting them stand in
   software only, with a graphics-card run required before a release.
4. **A Windows test run** of `tools/dev/products.ps1` while you are away from the screen: the only graphics-card evidence here.
5. **Send the Qt letter** (`docs/release/QT.md`), question 15 on CI. No Qt kit goes on the VM before the answer.
6. **Move the Store's signing keys and their backup off this PC**, and **limit the Vercel and Render GitHub Apps.**
7. **The Linux machine:** a USB-C SSD of 500 GB or more and a USB stick of 8 GB or more, then `docs/BUILDING.md`.
8. **Later, when that work starts:** your accepted Unreal licence version, a Fab export and your Claude data setting, the Unity
   question, and Bellwick's name.

## Next

Fix the two rechecks' findings and merge them; the Store's pages (paused); the asset library that brings a pack from the Store
into a project, which with the pages finishes the second journey; review Prism's pointer capture, which is built. Workers run two
at a time: four at once reached the usage limit three times.
