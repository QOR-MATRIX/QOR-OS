# Where QOR OS stands

For Andrew, 1 November 2026. One page, rewritten every session (`AGENTS.md` §2). Checkpoint, mid-session.

## What QOR OS is

The operating system for players, game developers and game designers: the Prism shell (*the compositor and desktop in
one*) and Lumen design, with QQ, ARQADE and QOR Media, on Linux. Code: `QOR-MATRIX/qor-os`, now really private, at
`X:\Repoz\QOR-OS`. Your two briefs of today are in records QOR-0021 and QOR-0022 and the roadmap's order of work.

## This session

1. **The repository was public from 27 October,** and is private since today, as you chose. Treat the code as copied. Nothing
   found shows outside code ran on your laptop.
2. **Both CI runners (*build machines; both are this laptop*) can reach your signing keys and credentials.** The fix that
   stops their jobs is pull request #2, waiting for you: I may not merge my own. Until then they stay paused only because
   I start nothing on them.
3. **Workers are building** project workspaces, Prism's pointer lock, Linux product tests, the Store's catalogue, and an isolated
   Linux runner, on their own branches. None of it is reviewed or combined yet, and nothing reached `main`.
4. **QOR-0020 missed Epic's update of 30 October.** QOR-0022 proposes a rule for each operation. It is not in force.

## Checked

GitHub's records of all 117 runs; the runners' accounts; the signing files' timestamps, not their contents; what the Free plan
enforces; billing. **Not checked:** Epic's current licence (its pages refuse automated reading); anything on native Linux. **No
user journey is complete yet.** Nobody has played or heard Bellwick, Ever Rift or Darkveil.

## Needs you

1. **Merge pull request #2** (*a request to change `main`*), then **move the Store's signing keys and their backup off this PC.**
   Nothing planned needs them now.
2. **Limit two GitHub Apps:** Vercel and Render can write workflows in every organisation repository, which reaches the runners.
3. **For Unreal:** which licence version you accepted, and the text of its sections 6(e) and 8(a). Only when that work starts.
4. **For assets:** a Fab export, and your Claude data setting. Only when that work starts.
5. **For Unity:** one written question, drafted for you. Only when that work starts.
6. **The Linux machine:** a USB-C SSD of 500 GB or more and a USB stick of 8 GB or more, then BUILDING.md's steps and its checklist.
   Native graphics, sound and DualSense wait on this.
7. **Name Bellwick,** and **play** the three new games. **Legal, still open:** trademark advice, the legal entity, the Qt letter,
   replacing the 5 October key.

## Next

Review and combine the workers' branches. Then the build step that turns a QQ project into a package the Library can launch, which
finishes the first journey as far as WSL can show it. Then the two stores' pages, and the Game Design Document Studio.
